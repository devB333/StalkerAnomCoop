#!/usr/bin/env python3
"""Check a co-op session running the S08-T02 transfer fixture (docs/persistence-contract.md §6 `transfer`).

Usage: python3 tools/compare-coop-transfer.py <session logs> [--out compare-transfer.json]
           [--expect-restart]

Inputs (builds/logs/<task>/run-<run>/): server/ (and server-first-run/ after a restart):
coop-server.log, coop-transfers.csv; c1/, c2/: coop-c*.log, coop-transfer-client.csv,
coop-world-final.csv.

The fixture's rounds (CoopServerHost.cpp transfer_fixtures) and what each step must get:
  0 box race      steps 0, 1: one Accepted, the other ItemTaken (9)
  1 put           both Accepted
  2 cross         both Accepted
  3 stale         steps 0, 1 Accepted (take and put back); step 2 Stale (12)
  4 drop race     step 0 (drop) Accepted; steps 1, 2 (pickups): one Accepted, the other 9 (taken) or
                  10 (the item lies at the dropper's feet, which can be out of the other player's reach)
  5 equip         step 1 (the slot-2 item into the box) Accepted (step 0 is the slot action)
  6 refusals      step 0 OutOfReach (10), 1 NotContainer (13), 2 NotDroppable (14), 3 ItemTaken (9)
  7 corpse race   one Accepted, the other 9
  8 disconnect    step 0 Accepted (the request survives the dropped connection)

Checks:
  rounds              every round's results as above. With --expect-restart the restarted server
                      runs the rounds again: that second run must match exactly; in the first, a
                      step of the round the termination interrupted may instead be refused 8 (its
                      item rolled back with the checkpoint) or 5 (the store failed closed)
  linked              every driver step became a request from the client's own inventory event
                      (no `unlinked` row)
  duplicates_same     every same-id repeat of an Accepted step returned its original commit
  one_commit_per_op   no operation committed twice; no race item committed for both clients
  server_state        the server's last pickup-state: every touched item where its last record put it
  invariants          every ownership-invariants line is ok (recovered, checkpoints, final)
  clients_agree       each client's final replica shows every touched item under the server's owner
  no_pending          no client ended with an unanswered request
  disconnect          round 8's client dropped its connection after the request and got it Accepted
With --expect-restart also:
  restart_recovered   the restarted server recovered and replayed transfer records without failure
"""

import argparse
import csv
import json
import re
import sys
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def field(line, name, cast=str):
    m = re.search(r"\b" + re.escape(name) + r"=(\S+)", line)
    return cast(m.group(1)) if m else None


def rows(path):
    if not path.exists():
        return []
    with open(path, newline="") as f:
        return list(csv.DictReader(x for x in f if not x.startswith("#")))


EXPECT = {
    0: {"race": (0, 1)},
    1: {0: (1, 0), 1: (1, 0)},
    2: {0: (1, 0), 1: (1, 0)},
    3: {0: (1, 0), 1: (1, 0), 2: (2, 12)},
    4: {0: (1, 0), "race": (1, 2)},
    5: {1: (1, 0)},
    6: {0: (2, 10), 1: (2, 13), 2: (2, 14), 3: (2, 9)},
    7: {"race": (0, 1)},
    8: {0: (1, 0)},
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="compare-transfer.json")
    ap.add_argument("--expect-restart", action="store_true")
    a = ap.parse_args()
    logs = Path(a.logs)
    incarnations = [d for d in (logs / "server-first-run", logs / "server") if (d / "coop-server.log").exists()]
    server_logs = [lines(d / "coop-server.log") for d in incarnations]
    every = [x for log in server_logs for x in log]
    report = {"incarnations": [d.name for d in incarnations], "checks": {}}

    rounds_seen = sorted({field(x, "round", int) for x in every if " fixture-transfer-round " in x})
    report["rounds_announced"] = rounds_seen
    report["setup"] = [x.split(" ", 1)[1] for x in every if " fixture-transfer " in x]

    clients = sorted(p.name for p in logs.iterdir() if p.is_dir() and re.fullmatch(r"c\d+", p.name))
    results = {}  # (era, round, step) -> {client, ...}; era 1: the rounds the restarted server ran
    dups, unlinked, pending, disconnect = [], [], {}, {}
    for c in clients:
        crows = rows(logs / c / "coop-transfer-client.csv")
        firsts = {}
        era_of, era, last_round = {}, 0, -1
        for r in crows:
            rs = (int(r["round"]), int(r["step"]))
            if r["event"] == "step":
                # the restarted server announces its rounds from 0 again
                if rs[0] < last_round:
                    era += 1
                last_round = rs[0]
                era_of[rs] = era
            key = (era_of.get(rs, 0),) + rs
            if r["event"] == "result" and int(r["step"]) >= 0:
                results.setdefault(key, {"client": c, "outcome": int(r["outcome"]), "reason": int(r["reason"]),
                                         "commit": int(r["commit"]), "operation": r["operation"],
                                         "action": int(r["action"])})
                firsts.setdefault(r["operation"], r)
            if r["event"] == "unlinked":
                unlinked.append({"client": c, "era": key[0], "round": key[1], "step": key[2], "action": r["action"]})
        for r in crows:
            if r["event"] == "dup-result":
                o = firsts.get(r["operation"])
                dups.append({"client": c, "round": int(r["round"]), "step": int(r["step"]),
                             "same": bool(o) and (o["outcome"], o["reason"], o["commit"]) ==
                             (r["outcome"], r["reason"], r["commit"]),
                             "accepted": bool(o) and o["outcome"] == "1"})
        log = lines(logs / c / f"coop-{c}.log")
        summary = [x for x in log if " transfer-summary " in x]
        pending[c] = field(summary[-1], "pending", int) if summary else None
        disconnect[c] = sum(1 for x in log if " transfer-step-disconnect " in x)

    eras = sorted({k[0] for k in results}) or [0]
    final_era = eras[-1]
    verdicts, missing = {}, []
    for era in eras:
        for rnd, exp in EXPECT.items():
            got, ok, lenient = {}, True, True
            present = any(k[0] == era and k[1] == rnd for k in results)
            if not present:
                if era == final_era:
                    missing.append(rnd)
                continue
            for step, want in exp.items():
                if step == "race":
                    pair = [results.get((era, rnd, s)) for s in want]
                    outs = sorted((r["outcome"], r["reason"]) for r in pair if r)
                    got["race"] = [(r["client"], r["outcome"], r["reason"]) if r else None for r in pair]
                    losing = ((2, 9), (2, 10)) if rnd == 4 else ((2, 9),)
                    good = len(outs) == 2 and outs[0] == (1, 0) and outs[1] in losing
                    lenient = lenient and (good or all(o in ((1, 0), (2, 5), (2, 8), (2, 9)) for o in outs))
                    ok = ok and good
                else:
                    r = results.get((era, rnd, step))
                    got[str(step)] = (r["client"], r["outcome"], r["reason"]) if r else None
                    good = bool(r) and (r["outcome"], r["reason"]) == want
                    lenient = lenient and (good or not r or (r["outcome"], r["reason"]) in ((2, 5), (2, 8)))
                    ok = ok and good
            interrupted = a.expect_restart and era < final_era and not ok and lenient
            verdicts[f"{era}:{rnd}"] = {"ok": ok or interrupted, "exact": ok, "interrupted": interrupted, "got": got}
    rounds_ok = all(v["ok"] for v in verdicts.values()) and not missing and \
        sum(1 for v in verdicts.values() if v["interrupted"]) <= 1
    report["checks"]["rounds"] = {"pass": rounds_ok and bool(verdicts), "eras": len(eras), "rounds": verdicts,
                                  "missing_in_last_run": missing}
    report["checks"]["linked"] = {"pass": not unlinked, "unlinked": unlinked}
    acc_dups = [d for d in dups if d["accepted"]]
    report["checks"]["duplicates_same"] = {"pass": bool(acc_dups) and all(d["same"] for d in acc_dups),
                                           "repeats": len(dups), "accepted_repeats": len(acc_dups),
                                           "bad": [d for d in acc_dups if not d["same"]]}

    commits = []
    for x in every:
        if " transfer-committed " in x or " pickup-committed " in x:
            commits.append({"revision": field(x, "revision", int), "operation": field(x, "operation"),
                            "slot": field(x, "slot", int), "item_entity": field(x, "item_entity", int)})
    ops = {}  # operation ids are scoped per player: (slot, operation)
    for c in commits:
        ops.setdefault(f"{c['slot']}:{c['operation']}", set()).add(c["revision"])
    doubled = {k: sorted(v) for k, v in ops.items() if len(v) > 1}
    race_items = {}
    for rnd in (0, 7):
        for s in (0, 1):
            for era in eras:
                r = results.get((era, rnd, s))
                if r and r["outcome"] == 1:
                    race_items.setdefault(f"{era}:{rnd}", []).append(r["client"])
    report["commits"] = len(commits)
    report["checks"]["one_commit_per_op"] = {"pass": not doubled and all(len(v) == 1 for v in race_items.values()),
                                             "doubled": doubled, "race_winners": race_items}

    def state(log, tag):
        return {field(x, "item_entity", int): {"parent_entity": field(x, "parent_entity", int),
                                               "expected": field(x, "expected_owner", int),
                                               "ok": field(x, "ok") == "1"}
                for x in log if " pickup-state " in x and f" tag={tag} " in x}
    last = server_logs[-1] if server_logs else []
    final = state(last, "final") or state(last, "recovered")
    report["server_state"] = {str(k): v for k, v in final.items()}
    report["checks"]["server_state"] = {"pass": bool(final) and all(v["ok"] for v in final.values()),
                                        "items": len(final), "wrong": [k for k, v in final.items() if not v["ok"]]}

    inv = [x.split(" ", 1)[1] for x in every if " ownership-invariants " in x]
    report["checks"]["invariants"] = {"pass": bool(inv) and all(" ok=1 " in x for x in inv),
                                      "checks": len(inv), "bad": [x for x in inv if " ok=1 " not in x],
                                      "tags": sorted({field(x, "tag") for x in inv})}

    agree = {}
    for c in clients:
        world = {int(r["entity"]): r for r in rows(logs / c / "coop-world-final.csv") if r.get("in_set") == "1"}
        bad, seen = [], 0
        for item, s in final.items():
            w = world.get(item)
            if not w:
                continue
            seen += 1
            parents = (int(w.get("parent_entity") or 0), int(w.get("level_parent_entity") or 0))
            if parents != (s["expected"], s["expected"]):
                bad.append({"item": item, "client_parent": w.get("parent_entity"),
                            "client_level_parent": w.get("level_parent_entity"), "server_owner": s["expected"]})
        agree[c] = {"items_seen": seen, "bad": bad}
    report["checks"]["clients_agree"] = {"pass": bool(agree) and all(v["items_seen"] > 0 and not v["bad"]
                                                                     for v in agree.values()), "clients": agree}
    report["checks"]["no_pending"] = {"pass": all(v == 0 for v in pending.values()), "pending": pending}
    r8 = results.get((final_era, 8, 0))
    report["checks"]["disconnect"] = {"pass": sum(disconnect.values()) >= 1 and bool(r8) and r8["outcome"] == 1,
                                      "disconnects": disconnect, "result": r8}

    if a.expect_restart:
        second = server_logs[-1] if len(server_logs) > 1 else []
        rec = [x.split(" ", 1)[1] for x in second if " recovery-" in x]
        replay = [x.split(" ", 1)[1] for x in second if " transfer-replay" in x or " pickup-replay" in x]
        report["restart"] = {"replay": replay, "answered_from_index": sum(1 for x in second if "-duplicate " in x)}
        report["checks"]["restart_recovered"] = {
            "pass": len(server_logs) > 1 and any("recovery-replayed" in x for x in rec) and
            not any("recovery-replay-failed" in x for x in rec) and
            all(" ok=1" in x for x in replay if " ok=" in x),
            "replayed": len(replay)}

    report["pass"] = all(v["pass"] for v in report["checks"].values())
    out = Path(a.out)
    out.write_text(json.dumps(report, indent=2) + "\n")
    for k, v in report["checks"].items():
        print(f"{'PASS' if v['pass'] else 'FAIL'} {k}")
    print("overall", "PASS" if report["pass"] else "FAIL", "->", out)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

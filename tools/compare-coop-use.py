#!/usr/bin/env python3
"""Check a co-op session running the S08-T07 equip and use fixture (docs/persistence-contract.md §6
`equip`, `use`).

Usage: python3 tools/compare-coop-use.py <session logs> [--out compare-use.json] [--expect-restart]

Inputs (builds/logs/<task>/run-<run>/): server/ (and server-first-run/ after a restart):
coop-server.log, coop-inventory.csv; c1/, c2/: coop-c*.log, coop-transfer-client.csv.

The fixture's rounds (CoopServerHost.cpp use_fixtures) and what each step must get:
  0 equip        both Accepted (p1 detector to slot 9, p2 pistol to slot 1)
  1 unequip      both Accepted
  2 refusals     0 PlaceRefused (20), 1 NotDroppable (14: not the player's item), 2 NotUsable (21),
                 3 Malformed (2), 4 the artefact to the belt: PlaceRefused (20) when the actor's
                 outfit has no belt slots, else Accepted
  3 eat          both Accepted (at the same tick)
  4 boosters     0 p2's medkit Accepted (p1's active medkit booster is p1's own), 1 p1's second
                 medkit UseRefused (22: its own booster is active)
  5 use vs move  p1: use Accepted, the move of the used-up item ItemGone (8); p2: move Accepted, the
                 use of the moved item 9 or 14
  6 quick-use    Accepted (through GEG_PLAYER_ITEM_EAT)
  7 equip again  both Accepted
  8 last use     Accepted (the medkit used up)
  9 disconnect   Accepted (the request survives the dropped connection)
 10 pickup       Accepted (p1 picks up the bread at its feet: a pickup record)
 11 eat it       Accepted (that bread used up: after a restart it must not come back, L39)

Checks:
  rounds              every round's results as above. With --expect-restart the restarted server
                      runs the rounds again: that second run must match exactly; in the first, a
                      step of the round the termination interrupted may instead be refused 8 or 5,
                      and an expected 22 may be Accepted (the booster memory is not saved)
  linked              every driver step became a request from the client's own inventory path
  duplicates_same     every same-id repeat of an Accepted step returned its original commit
  one_commit_per_op   no operation committed twice (equip-committed, use-committed)
  server_state        the server's last inventory-state: every item an equip or use record named is
                      released (used up) or held by its actor in the recorded place with the
                      recorded remaining uses
  healing             every medkit use below full health raised the user's health on the server
                      within 5 s (use-followup: the medkit heals through its booster over time)
  layouts             every layout point on each client matched the server's layout hash
  item_graph          every item-graph point on each client matched (S08-T03)
  local_blocked       each client blocked its local CInventory::Eat at least once (the installed UI's
                      path became an intent, nothing was used locally)
  presented           each client played an accepted use's presentation at least once
  adapter             the server's item adapter is installed (fx and booster memory wrapped)
  no_pending          no client ended with an unanswered request
  disconnect          round 9's client dropped its connection after the request and got it Accepted
With --expect-restart also:
  restart_recovered   the restarted server recovered and replayed its records without failure, and
                      its inventory-state at recovery was all ok
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


A = ((1, 0),)
EXPECT = {
    0: {0: A, 1: A},
    1: {0: A, 1: A},
    2: {0: ((2, 20),), 1: ((2, 14),), 2: ((2, 21),), 3: ((2, 2),), 4: ((2, 20), (1, 0))},
    3: {0: A, 1: A},
    4: {0: A, 1: ((2, 22),)},
    5: {0: A, 1: ((2, 8),), 2: A, 3: ((2, 9), (2, 14))},
    6: {0: A},
    7: {0: A, 1: A},
    8: {0: A},
    9: {0: A},
    10: {0: A},
    11: {0: A},
}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="compare-use.json")
    ap.add_argument("--expect-restart", action="store_true")
    a = ap.parse_args()
    logs = Path(a.logs)
    incarnations = [d for d in (logs / "server-first-run", logs / "server") if (d / "coop-server.log").exists()]
    server_logs = [lines(d / "coop-server.log") for d in incarnations]
    every = [x for log in server_logs for x in log]
    report = {"incarnations": [d.name for d in incarnations], "checks": {}}
    report["rounds_announced"] = sorted({field(x, "round", int) for x in every if " fixture-use-round " in x})
    report["setup"] = [x.split(" ", 1)[1] for x in every if " fixture-use " in x]

    clients = sorted(p.name for p in logs.iterdir() if p.is_dir() and re.fullmatch(r"c\d+", p.name))
    results = {}
    dups, unlinked, pending, disconnect, summaries, layout_points, graph_points = [], [], {}, {}, {}, {}, {}
    for c in clients:
        crows = rows(logs / c / "coop-transfer-client.csv")
        firsts = {}
        era_of, era, last_round = {}, 0, -1
        for r in crows:
            rs = (int(r["round"]), int(r["step"]))
            if r["event"] == "step":
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
        inv = [x for x in log if " inventory-summary " in x]
        summaries[c] = {k: field(inv[-1], k) for k in ("equips", "uses", "local_eats_blocked", "presented",
                                                        "layouts", "moves", "layout_points")} if inv else {}
        layout_points[c] = [{"seq": field(x, "seq", int), "match": field(x, "match") == "1",
                             "waited_ms": field(x, "waited_ms", int)} for x in log if " layout-point " in x]
        graph_points[c] = [field(x, "match") == "1" for x in log if " item-graph seq=" in x]

    eras = sorted({k[0] for k in results}) or [0]
    final_era = eras[-1]
    verdicts, missing = {}, []
    for era in eras:
        for rnd, exp in EXPECT.items():
            present = any(k[0] == era and k[1] == rnd for k in results)
            if not present:
                if era == final_era:
                    missing.append(rnd)
                continue
            got, ok, lenient = {}, True, True
            for step, wants in exp.items():
                r = results.get((era, rnd, step))
                got[str(step)] = (r["client"], r["outcome"], r["reason"]) if r else None
                good = bool(r) and (r["outcome"], r["reason"]) in wants
                # the interrupted round may also find the booster memory gone: it is a script-local
                # table the installed scripts never save (single player's load does the same)
                lenient = lenient and (good or not r or (r["outcome"], r["reason"]) in ((2, 5), (2, 8)) or
                                       ((2, 22) in wants and (r["outcome"], r["reason"]) == (1, 0)))
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

    ops = {}
    commits = 0
    for x in every:
        if " equip-committed " in x or " use-committed " in x:
            commits += 1
            ops.setdefault(f"{field(x, 'slot')}:{field(x, 'operation')}", set()).add(field(x, "revision", int))
    doubled = {k: sorted(v) for k, v in ops.items() if len(v) > 1}
    report["commits"] = commits
    report["checks"]["one_commit_per_op"] = {"pass": commits > 0 and not doubled, "doubled": doubled}

    def state(log, tag):
        return [{"item_entity": field(x, "item_entity", int), "section": field(x, "section"),
                 "ok": field(x, "ok") == "1", "line": x.split(" ", 1)[1]}
                for x in log if " inventory-state " in x and f" tag={tag} " in x]
    last = server_logs[-1] if server_logs else []
    final = state(last, "final")
    report["server_state"] = final
    report["checks"]["server_state"] = {"pass": bool(final) and all(s["ok"] for s in final), "items": len(final),
                                        "wrong": [s["line"] for s in final if not s["ok"]]}

    heals = []
    for log in server_logs:
        medkits = {field(x, "revision", int) for x in log
                   if " use-committed " in x and (field(x, "section") or "").startswith("medkit")}
        for x in log:
            if " use-followup " in x and field(x, "revision", int) in medkits:
                heals.append({"revision": field(x, "revision", int), "at_use": field(x, "health_at_use", float),
                              "after_5s": field(x, "health_now", float)})
    wounded = [h for h in heals if h["at_use"] < 0.99]  # a medkit used at full health cannot raise it
    report["checks"]["healing"] = {"pass": bool(wounded) and all(h["after_5s"] > h["at_use"] for h in wounded),
                                   "uses": heals, "wounded_uses": len(wounded)}
    report["checks"]["layouts"] = {
        "pass": all(layout_points.get(c) and all(p["match"] for p in layout_points[c]) for c in clients),
        "points": {c: {"count": len(v), "mismatches": [p for p in v if not p["match"]],
                       "max_wait_ms": max((p["waited_ms"] or 0 for p in v), default=None)}
                   for c, v in layout_points.items()}}
    report["checks"]["item_graph"] = {"pass": all(graph_points.get(c) and all(graph_points[c]) for c in clients),
                                      "points": {c: {"count": len(v), "mismatches": v.count(False)}
                                                 for c, v in graph_points.items()}}
    report["client_summaries"] = summaries
    report["checks"]["local_blocked"] = {
        "pass": all(int(summaries.get(c, {}).get("local_eats_blocked") or 0) > 0 for c in clients),
        "blocked": {c: summaries.get(c, {}).get("local_eats_blocked") for c in clients}}
    report["checks"]["presented"] = {
        "pass": all(int(summaries.get(c, {}).get("presented") or 0) > 0 for c in clients),
        "presented": {c: summaries.get(c, {}).get("presented") for c in clients}}
    adapter = [x.split(" ", 1)[1] for x in every if " item-adapter " in x]
    report["checks"]["adapter"] = {"pass": bool(adapter) and all("fx_wrapped=1" in x and "booster_wrapped=1" in x
                                                                 for x in adapter), "lines": adapter}
    report["checks"]["no_pending"] = {"pass": all(v == 0 for v in pending.values()), "pending": pending}
    r9 = results.get((final_era, 9, 0))
    report["checks"]["disconnect"] = {"pass": sum(disconnect.values()) >= 1 and bool(r9) and r9["outcome"] == 1,
                                      "disconnects": disconnect, "result": r9}

    if a.expect_restart:
        second = server_logs[-1] if len(server_logs) > 1 else []
        rec = [x.split(" ", 1)[1] for x in second if " recovery-" in x]
        replay = [x.split(" ", 1)[1] for x in second
                  if re.search(r" (use|equip)-replay\b", x) or " use-release " in x or "-replay-noop " in x]
        recovered = state(second, "recovered")
        report["restart"] = {"replay": replay, "recovered_state": recovered,
                             "answered_from_index": sum(1 for x in second if "-duplicate " in x)}
        report["checks"]["restart_recovered"] = {
            "pass": len(server_logs) > 1 and any("recovery-replayed" in x for x in rec) and
            not any("recovery-replay-failed" in x for x in rec) and all(s["ok"] for s in recovered),
            "replayed": len(replay), "recovered_items": len(recovered),
            "recovered_wrong": [s["line"] for s in recovered if not s["ok"]]}

    report["pass"] = all(v["pass"] for v in report["checks"].values())
    out = Path(a.out)
    out.write_text(json.dumps(report, indent=2) + "\n")
    for k, v in report["checks"].items():
        print(f"{'PASS' if v['pass'] else 'FAIL'} {k}")
    print("overall", "PASS" if report["pass"] else "FAIL", "->", out)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

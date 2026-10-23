#!/usr/bin/env python3
"""Check a co-op session running the S08-T01 pickup race (docs/persistence-contract.md §6 `pickup`).

Usage: python3 tools/compare-coop-pickup.py <session logs> [--out compare-pickup.json]
           [--expect-restart]

Inputs (builds/logs/<task>/run-<run>/): server/ (and server-first-run/ after a restart):
coop-server.log, coop-pickups.csv; c1/, c2/: coop-c*.log, coop-pickup-client.csv,
coop-world-final.csv.

Checks:
  one_winner          every race item (fixture round kind 0) has exactly one Accepted claim over
                      both clients, and the other client's claim was refused ItemTaken (9)
  one_commit_per_item no item entity was committed by two pickup records (any incarnation)
  duplicates_same     every repeat of a claim with the same id returned the original outcome and
                      commit
  new_id_refused      every repeat with a new id after losing was refused ItemTaken
  refusal_rounds      the far item was refused OutOfReach (10), the other actor NotTakeable (11),
                      the nonexistent target ItemGone (8), on both clients
  acknowledged_held   every claim a client got Accepted names an item the server's last state
                      shows in the committed owner's inventory
  server_state        the server's last pickup-state (final, else recovered) is all ok
  clients_agree       each client's final replica shows every pickup item with the server's owner
  confirmed           every applied pickup was confirmed by the server's level actor (none reverted)
  no_pending          no client ended with an unanswered pickup request
With --expect-restart also:
  restart_recovered   the restarted server recovered and its recovered pickup-state is all ok
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
        return list(csv.DictReader(f))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="compare-pickup.json")
    ap.add_argument("--expect-restart", action="store_true")
    a = ap.parse_args()
    logs = Path(a.logs)
    incarnations = [d for d in (logs / "server-first-run", logs / "server") if (d / "coop-server.log").exists()]
    server_logs = [lines(d / "coop-server.log") for d in incarnations]
    every = [x for log in server_logs for x in log]
    report = {"incarnations": [str(d.name) for d in incarnations], "checks": {}}

    # The fixture's rounds (target entity -> kind).
    rounds = {}
    for x in every:
        if " fixture-pickup-round " in x and " target=" in x:
            target = int(field(x, "target").split("/")[0])
            rounds[target] = {"round": field(x, "round", int), "kind": field(x, "kind", int),
                              "claim_tick": field(x, "claim_tick", int)}
    report["rounds"] = len(rounds)

    # Server commits.
    commits = []
    for log, d in zip(server_logs, incarnations, strict=True):
        for x in log:
            if " pickup-committed " in x:
                commits.append({"incarnation": d.name, "revision": field(x, "revision", int),
                                "slot": field(x, "slot", int), "operation": field(x, "operation", int),
                                "item_entity": field(x, "item_entity", int),
                                "owner_entity": field(x, "owner_entity", int), "section": field(x, "section"),
                                "distance": field(x, "distance", float)})
    by_item = {}
    for c in commits:
        by_item.setdefault(c["item_entity"], []).append(c)
    doubles = {k: v for k, v in by_item.items() if len({c["revision"] for c in v}) > 1}
    report["commits"] = len(commits)
    report["checks"]["one_commit_per_item"] = {"pass": not doubles, "double": doubles}
    owner_of_revision = {c["revision"]: c for c in commits}

    # Arrival ticks of the two claims per race item (server csv: commit and refused rows).
    ticks = {}
    for d in incarnations:
        for r in rows(d / "coop-pickups.csv"):
            if r["phase"] in ("commit", "refused"):
                ticks.setdefault(int(r["item_entity"]), []).append((int(r["tick"]), r["phase"], int(r["reason"])))

    # Client results.
    clients = sorted(p.name for p in logs.iterdir() if p.is_dir() and re.fullmatch(r"c\d+", p.name))
    results = {}  # (client, target) -> first result row
    dup_bad, new_bad, refusal = [], [], {}
    pending_end = {}
    for c in clients:
        crows = rows(logs / c / "coop-pickup-client.csv")
        first = {}
        for r in crows:
            target = int(r["target"])
            if r["event"] == "result":
                first.setdefault(int(r["operation"]), r)
                results.setdefault((c, target), r)
        for r in crows:
            if r["event"] == "dup-result":
                o = first.get(int(r["operation"]))
                if not o or (o["outcome"], o["commit"], o["reason"]) != (r["outcome"], r["commit"], r["reason"]):
                    dup_bad.append({"client": c, "dup": r, "original": o})
            if r["event"] == "retry-new-result" and not (r["outcome"] == "2" and r["reason"] == "9"):
                new_bad.append({"client": c, "row": r})
        summary = [x for x in lines(logs / c / f"coop-{c}.log") if " pickup-summary " in x]
        pending_end[c] = field(summary[-1], "pending", int) if summary else None
    dups = sum(1 for c in clients for r in rows(logs / c / "coop-pickup-client.csv") if r["event"] == "dup-result")
    news = sum(1 for c in clients for r in rows(logs / c / "coop-pickup-client.csv")
               if r["event"] == "retry-new-result")

    race = {}
    for target, info in rounds.items():
        if info["kind"] != 0:
            continue
        got = {c: results.get((c, target)) for c in clients}
        accepted = [c for c, r in got.items() if r and r["outcome"] == "1"]
        refused9 = [c for c, r in got.items() if r and r["outcome"] == "2" and r["reason"] == "9"]
        t = sorted(ticks.get(target, []))
        race[target] = {"round": info["round"], "accepted": accepted, "refused_taken": refused9,
                        "missing": [c for c, r in got.items() if not r],
                        "arrival_ticks": t, "same_tick": len(t) == 2 and t[0][0] == t[1][0],
                        "tick_gap": (t[1][0] - t[0][0]) if len(t) == 2 else None,
                        "ok": len(accepted) == 1 and len(refused9) == len(clients) - 1}
    report["race"] = {str(k): v for k, v in race.items()}
    report["checks"]["one_winner"] = {"pass": bool(race) and all(v["ok"] for v in race.values()),
                                      "rounds": len(race),
                                      "same_tick": sum(1 for v in race.values() if v["same_tick"]),
                                      "within_2_ticks": sum(1 for v in race.values()
                                                            if v["tick_gap"] is not None and v["tick_gap"] <= 2),
                                      "failed": [str(k) for k, v in race.items() if not v["ok"]]}
    report["checks"]["duplicates_same"] = {"pass": dups > 0 and not dup_bad, "count": dups, "bad": dup_bad}
    report["checks"]["new_id_refused"] = {"pass": news > 0 and not new_bad, "count": news, "bad": new_bad}

    expected_reason = {1: "10", 2: "11", 3: "8"}
    for target, info in rounds.items():
        if info["kind"] in expected_reason:
            got = {c: results.get((c, target)) for c in clients}
            refusal[info["kind"]] = {c: (r["outcome"], r["reason"]) if r else None for c, r in got.items()}
    refusal_ok = len(refusal) == 3 and all(
        all(v == ("2", expected_reason[k]) for v in got.values()) for k, got in refusal.items())
    report["checks"]["refusal_rounds"] = {"pass": refusal_ok, "results": {str(k): v for k, v in refusal.items()}}

    # The server's view.
    def state(log, tag):
        entries = [x for x in log if " pickup-state " in x and f" tag={tag} " in x]
        return {field(x, "item_entity", int): {"parent_entity": field(x, "parent_entity", int),
                                               "expected": field(x, "expected_owner", int),
                                               "ok": field(x, "ok") == "1"} for x in entries}
    last_log = server_logs[-1] if server_logs else []
    final = state(last_log, "final") or state(last_log, "recovered")
    report["server_state"] = {str(k): v for k, v in final.items()}
    report["checks"]["server_state"] = {"pass": bool(final) and all(v["ok"] for v in final.values()),
                                        "items": len(final), "wrong": [str(k) for k, v in final.items() if not v["ok"]]}

    acked = []
    for (c, target), r in results.items():
        if r["outcome"] == "1":
            commit = int(r["commit"])
            owner = owner_of_revision.get(commit, {}).get("owner_entity")
            held = final.get(target, {}).get("parent_entity")
            acked.append({"client": c, "target": target, "commit": commit, "owner": owner, "held_by": held,
                          "ok": owner is not None and held == owner})
    report["checks"]["acknowledged_held"] = {"pass": bool(acked) and all(x["ok"] for x in acked),
                                             "acknowledged": len(acked), "bad": [x for x in acked if not x["ok"]]}

    agree = {}
    for c in clients:
        world = {int(r["entity"]): r for r in rows(logs / c / "coop-world-final.csv")}
        bad = []
        seen = 0
        for item, s in final.items():
            w = world.get(item)
            if not w:
                continue
            seen += 1
            parents = (int(w.get("parent_entity") or 0), int(w.get("level_parent_entity") or 0))
            if parents != (s["expected"], s["expected"]):
                bad.append({"item": item, "client_parent": w.get("parent_entity"),
                            "client_level_parent": w.get("level_parent_entity"), "server_owner": s["expected"]})
        reparents = [x for x in lines(logs / c / f"coop-{c}.log") if " reparent-" in x]
        agree[c] = {"items_seen": seen, "bad": bad,
                    "reparent_applied": sum(1 for x in reparents if " reparent-applied " in x and " ok=1" in x),
                    "reparent_failed": sum(1 for x in reparents if " reparent-applied " in x and " ok=0" in x)
                    + sum(1 for x in reparents if " reparent-offline " in x or " reparent-no-parent " in x)}
    report["checks"]["clients_agree"] = {"pass": all(v["items_seen"] > 0 and not v["bad"] for v in agree.values())
                                         and bool(agree), "clients": agree}

    confirmed = sum(1 for x in every if " pickup-confirmed " in x)
    reverted = [x.split(" ", 1)[1] for x in every if " pickup-reverted " in x or " pickup-unconfirmed " in x]
    applied = sum(1 for x in every if (" pickup-apply " in x or " pickup-replay " in x) and " ok=1" in x) + \
        sum(1 for x in every if "-recreated " in x and " ok=1" in x)
    report["checks"]["confirmed"] = {"pass": confirmed >= applied and not reverted, "applied": applied,
                                     "confirmed": confirmed, "reverted": reverted}
    report["checks"]["no_pending"] = {"pass": all(v == 0 for v in pending_end.values()), "pending": pending_end}

    if a.expect_restart:
        second = server_logs[-1] if len(server_logs) > 1 else []
        recovered = state(second, "recovered")
        rec = [x.split(" ", 1)[1] for x in second if " recovery-" in x][:6]
        replay = [x.split(" ", 1)[1] for x in second if " pickup-replay" in x]
        index = [x.split(" ", 1)[1] for x in second if " pickup-duplicate " in x]
        report["restart"] = {"recovery": rec, "replay": replay, "answered_from_index": index}
        report["checks"]["restart_recovered"] = {
            "pass": len(server_logs) > 1 and any("recovery-replayed" in x for x in rec) and
            all(v["ok"] for v in recovered.values()),
            "recovered_items": len(recovered), "replayed": len(replay), "answered_from_index": len(index)}

    report["pass"] = all(v["pass"] for v in report["checks"].values())
    out = Path(a.out)
    out.write_text(json.dumps(report, indent=2) + "\n")
    for k, v in report["checks"].items():
        print(f"{'PASS' if v['pass'] else 'FAIL'} {k}")
    print("overall", "PASS" if report["pass"] else "FAIL", "->", out)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

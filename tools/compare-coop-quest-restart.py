#!/usr/bin/env python3
"""Check a party-quest session in which the server terminated and restarted (S08-T06).

Usage: python3 tools/compare-coop-quest-restart.py <session logs> [--out compare-quest-restart.json]

Inputs: server-first-run/ and server/ coop-server.log; c*/coop-transfer-client.csv.

The rule (docs/persistence-contract.md §8a): a party quest's accept and completion are answered only after
a generation that holds them is published, so whatever the termination window:

  one_completion      the clients were told of at most one completion (one Accepted turn-in commit)
  money_once          each party actor's final money on the server equals its money before the quest plus
                      the payment of the completion the clients were told of (nothing if none): an effect
                      applied but never published was rolled back and not paid twice
  told_is_durable     every Accepted quest answer a client got names an outcome the restarted server holds:
                      the completion's revision is the one the restarted server answered from its index,
                      or the restarted server completed it itself
  accept_consistent   the task is active after the restart only if an accept was answered (published) and
                      no completion was
  recovered           the restarted server recovered a generation and rebuilt or did not need the party task
  retry_completes     progress, not only safety: a turn-in the restarted server took (its phrases ran,
                      `quest-turnin … ok=1`) completed there (`quest-completed`), none timed out
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="compare-quest-restart.json")
    args = ap.parse_args()
    logs = Path(args.logs)
    first = lines(logs / "server-first-run" / "coop-server.log")
    second = lines(logs / "server" / "coop-server.log")
    report = {"checks": {}}
    clients = sorted(p.name for p in logs.iterdir() if p.is_dir() and re.fullmatch(r"c\d+", p.name))

    # what the clients were told: Accepted quest results (action 8) and their commits, by round
    told = []
    for c in clients:
        for r in rows(logs / c / "coop-transfer-client.csv"):
            if r["event"] in ("result", "dup-result") and r["action"] == "8" and r["outcome"] == "1":
                told.append({"client": c, "round": int(r["round"]), "commit": int(r["commit"]),
                             "event": r["event"]})
    # a completion is an Accepted quest answer whose commit is a completion's revision (any round: after a
    # restart the fixture's rounds start again and the turn-in can come early)
    completions = {field(x, "revision", int) for x in first + second if " quest-completed " in x}
    turnin_commits = sorted({t["commit"] for t in told} & completions)
    accept_told = any(t["round"] == 1 for t in told)
    report["told"] = told
    report["checks"]["one_completion"] = {"pass": len(turnin_commits) <= 1, "turn_in_commits": turnin_commits}

    # money before the quest (the first payment line's "before"), per actor entity
    def payments(log):
        return [{"entity": field(x, "entity"), "amount": field(x, "amount", int), "revision": field(x, "revision", int),
                 "before": int(field(x, "money").split("->")[0])} for x in log if " quest-paid " in x]
    p1, p2 = payments(first), payments(second)
    before = {}
    for p in p1 + p2:
        before.setdefault(p["entity"], p["before"])
    final = {field(x, "entity"): field(x, "money", int) for x in second if " quest-final " in x}
    if not before:
        before = dict(final)
    paid_told = {}
    for p in p1 + p2:
        if p["revision"] in turnin_commits:
            paid_told[p["entity"]] = p["amount"]
    expected = {e: before.get(e, 0) + paid_told.get(e, 0) for e in final}
    report["checks"]["money_once"] = {"pass": bool(final) and all(final[e] == expected[e] for e in final),
                                      "final": final, "expected": expected, "before": before,
                                      "paid_first_run": [(p["entity"], p["amount"], p["revision"]) for p in p1],
                                      "paid_after_restart": [(p["entity"], p["amount"], p["revision"]) for p in p2]}

    answered_from_index = {field(x, "revision", int) for x in second if " quest-duplicate " in x}
    completed_second = {field(x, "revision", int) for x in second if " quest-completed " in x}
    report["checks"]["told_is_durable"] = {
        "pass": all(c in answered_from_index or c in completed_second for c in turnin_commits),
        "answered_from_index": sorted(answered_from_index), "completed_after_restart": sorted(completed_second)}

    rebuilt = any(" quest-rebuilt " in x for x in second)
    active_end = rebuilt and not completed_second
    report["checks"]["accept_consistent"] = {
        "pass": (not active_end) or (accept_told and not (turnin_commits and not completed_second)),
        "rebuilt": rebuilt, "accept_told": accept_told, "active_at_end": active_end}
    report["checks"]["recovered"] = {"pass": any(" recovery-load " in x for x in second),
                                     "recovery": [x.split(" ", 1)[1] for x in second if " recovery-load " in x]}

    took = [x for x in second if " quest-turnin " in x and " ok=1" in x]
    timeouts = [x.split(" ", 1)[1] for x in second if " quest-turnin-timeout " in x]
    report["checks"]["retry_completes"] = {"pass": not timeouts and (not took or bool(completed_second)),
                                           "turn_ins_taken": len(took), "timeouts": timeouts}

    report["pass"] = all(v["pass"] for v in report["checks"].values())
    out = Path(args.out)
    out.write_text(json.dumps(report, indent=2) + "\n")
    for k, v in report["checks"].items():
        print(f"{'PASS' if v['pass'] else 'FAIL'} {k}")
    print("overall", "PASS" if report["pass"] else "FAIL", "->", out)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

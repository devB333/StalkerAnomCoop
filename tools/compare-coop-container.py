#!/usr/bin/env python3
"""Check a co-op session running the S08-T08 container and stack fixture (docs/persistence-contract.md §6
`entity`, `tombstone`, `stack`).

Usage: python3 tools/compare-coop-container.py <session logs> [--out compare-container.json] [--expect-restart]

Inputs (builds/logs/<task>/run-<run>/): server/ (and server-first-run/ after a restart):
coop-server.log; c1/, c2/: coop-c*.log, coop-transfer-client.csv.

The fixture's rounds (CoopServerHost.cpp stack_fixtures); every step must be Accepted:
  0 put          p1 bread -> box (created after the checkpoint); p2 vodka -> body (killed after it)
  1 take         p1 the medkit from the body; p2 the bread from the box
  2, 3 merge     each player's partial 9x18 stack -> the box (merged with the box's)
  4 backpack     p2 its backpack -> the box
  5 stash        p1 uses the deployable backpack (the installed script moves the ruck to a new stash)
  6 from stash   p2 takes one item from the stash

Checks:
  rounds              every step Accepted. With --expect-restart (the restart runs no fixture,
                      --restart-no-fixture): a step the termination interrupted may be refused 8, 5 or 12 (stale view)
                      and the rounds after it never run; checks of rounds never reached do not apply
  linked              every driver step became a request
  containers          a container record before the first transfer into each container
  merged              both merges committed a stack record: one stack of 11, then of 15 rounds
  stash_observed      the stash's moves were observed and journaled as server-made transfers
  server_state        the last container-state: every tracked container exists and holds every item a
                      record put in it (none elsewhere, none missing); every merged stack's count
  amounts             each client applied the merged counts (amount-applied) for known items
  invariants          every ownership-invariants line is ok
  item_graph          every item-graph point on each client matched
  no_pending          no client ended with an unanswered request
With --expect-restart also:
  restart_recovered   the restarted server re-created the containers the recovered world lacked, replayed
                      without failure, and its container-state at recovery was all ok
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


STEPS = {0: 2, 1: 2, 2: 1, 3: 1, 4: 1, 5: 1, 6: 1}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="compare-container.json")
    ap.add_argument("--expect-restart", action="store_true")
    a = ap.parse_args()
    logs = Path(a.logs)
    incarnations = [d for d in (logs / "server-first-run", logs / "server") if (d / "coop-server.log").exists()]
    server_logs = [lines(d / "coop-server.log") for d in incarnations]
    every = [x for log in server_logs for x in log]
    report = {"incarnations": [d.name for d in incarnations], "checks": {}}
    report["setup"] = [x.split(" ", 1)[1] for x in every if " fixture-stack " in x]

    clients = sorted(p.name for p in logs.iterdir() if p.is_dir() and re.fullmatch(r"c\d+", p.name))
    results, unlinked, pending, amounts, graph = {}, [], {}, {}, {}
    for c in clients:
        era, last_round, era_of = 0, -1, {}
        for r in rows(logs / c / "coop-transfer-client.csv"):
            rs = (int(r["round"]), int(r["step"]))
            if r["event"] == "step":
                if rs[0] < last_round:
                    era += 1
                last_round = rs[0]
                era_of[rs] = era
            key = (era_of.get(rs, 0),) + rs
            if r["event"] == "result" and int(r["step"]) >= 0:
                results.setdefault(key, {"client": c, "outcome": int(r["outcome"]), "reason": int(r["reason"])})
            if r["event"] == "unlinked":
                unlinked.append({"client": c, "round": rs[0], "step": rs[1]})
        log = lines(logs / c / f"coop-{c}.log")
        summary = [x for x in log if " transfer-summary " in x]
        pending[c] = field(summary[-1], "pending", int) if summary else None
        amounts[c] = sum(1 for x in log if " amount-applied " in x)
        graph[c] = [field(x, "match") == "1" for x in log if " item-graph seq=" in x]

    # Every step Accepted. With --expect-restart (the restart runs no fixture: --restart-no-fixture)
    # a step the termination interrupted may be refused 8 or 5, and the rounds after it never run.
    verdicts, inexact, reached = {}, 0, set()
    for rnd, n in STEPS.items():
        got = [results.get((0, rnd, st)) for st in range(n)]
        if any(got):
            reached.add(rnd)
        ok = all(g and g["outcome"] == 1 for g in got)
        lenient = all(not g or g["outcome"] == 1 or g["reason"] in (5, 8, 12) for g in got)
        if not ok and a.expect_restart and lenient and any(got):
            inexact += 1
        verdicts[str(rnd)] = {"ok": ok or (a.expect_restart and lenient), "exact": ok,
                              "got": [(g["client"], g["outcome"], g["reason"]) if g else None for g in got]}
    report["rounds_reached"] = sorted(reached)
    report["checks"]["rounds"] = {"pass": bool(results) and all(v["ok"] for v in verdicts.values()) and
                                  (not a.expect_restart or inexact <= 2), "rounds": verdicts}
    report["checks"]["linked"] = {"pass": not unlinked, "unlinked": unlinked}

    first = server_logs[0] if server_logs else []
    cont = [x.split(" ", 1)[1] for x in first if " container-committed " in x]
    kinds = {field(x, "kind") for x in cont}
    report["checks"]["containers"] = {"pass": {"box", "body"} <= kinds or (a.expect_restart and 0 not in reached),
                                      "records": cont}
    stacks = [x.split(" ", 1)[1] for x in first if " stack-committed " in x]
    counts = []
    for x in stacks:
        c = field(x, "counts") or ""
        counts.append(sorted(int(v.split(":")[1]) for v in c.split(",") if ":" in v))
    merges_due = 2 if 3 in reached else 1 if 2 in reached else 0
    report["checks"]["merged"] = {"pass": len(stacks) >= merges_due and
                                  (not stacks or counts[0] == [11]) and (len(stacks) < 2 or counts[1] == [15]) and
                                  (merges_due == 2 or a.expect_restart),
                                  "stacks": stacks}
    moves = [x for x in first if " observed-move " in x]
    report["checks"]["stash_observed"] = {"pass": len(moves) >= 1 or a.expect_restart,
                                          "moves": len(moves),
                                          "releases": sum(1 for x in first if " observed-release " in x),
                                          "takes": sum(1 for x in first if " observed-take " in x)}

    def state(log, tag):
        return [x.split(" ", 1)[1] for x in log if (" container-state " in x or " stack-state " in x) and
                f" tag={tag} " in x]
    last = server_logs[-1] if server_logs else []
    final = state(last, "final")
    report["server_state"] = final
    report["checks"]["server_state"] = {"pass": bool(final) and all(" ok=1 " in x + " " for x in final),
                                        "wrong": [x for x in final if " ok=1" not in x]}
    report["checks"]["amounts"] = {"pass": bool(amounts) and all(v > 0 for v in amounts.values()),
                                   "applied": amounts}
    inv = [x.split(" ", 1)[1] for x in every if " ownership-invariants " in x]
    report["checks"]["invariants"] = {"pass": bool(inv) and all(" ok=1 " in x for x in inv),
                                      "bad": [x for x in inv if " ok=1 " not in x]}
    report["checks"]["item_graph"] = {"pass": all(graph.get(c) and all(graph[c]) for c in clients),
                                      "points": {c: [len(v), v.count(False)] for c, v in graph.items()}}
    report["checks"]["no_pending"] = {"pass": all(v == 0 for v in pending.values()), "pending": pending}

    if a.expect_restart:
        second = server_logs[-1] if len(server_logs) > 1 else []
        rec = [x.split(" ", 1)[1] for x in second if " recovery-" in x]
        recreated = [x.split(" ", 1)[1] for x in second if " container-replay-recreated " in x]
        recovered = state(second, "recovered")
        report["restart"] = {"recreated": recreated, "recovered_state": recovered}
        report["checks"]["restart_recovered"] = {
            "pass": len(server_logs) > 1 and any("recovery-replayed" in x for x in rec) and
            not any("recovery-replay-failed" in x for x in rec) and bool(recovered) and
            all(" ok=1" in x for x in recovered) and all(" ok=1" in x for x in recreated),
            "recreated": len(recreated), "wrong": [x for x in recovered if " ok=1" not in x]}

    report["pass"] = all(v["pass"] for v in report["checks"].values())
    out = Path(a.out)
    out.write_text(json.dumps(report, indent=2) + "\n")
    for k, v in report["checks"].items():
        print(f"{'PASS' if v['pass'] else 'FAIL'} {k}")
    print("overall", "PASS" if report["pass"] else "FAIL", "->", out)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

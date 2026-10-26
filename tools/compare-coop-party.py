#!/usr/bin/env python3
"""Check the S08-T09 party quest presentation and the anchor's death (plan.md S08-T09, L41, L42).

Usage: python3 tools/compare-coop-party.py <session logs> --variant 0|1|2 [--out compare-party.json]
           [--expect-restart]

Inputs (builds/logs/<task>/run-<run>/): server/ (and server-first-run/): coop-server.log; c1/, c2/:
coop-c*.log. The session runs the S08-T04 quest fixture (--quest c1,c2) with --quest-variant:
  0  both turn in at once                     (presentation on both clients)
  1  the contributor drops its connection before the turn-in and comes back (the news it missed)
  2  participant 1 (the party's anchor) is killed after the collection and respawns (auto_respawn);
     participant 2 turns in; then a party stash is made (its map spot to both)

Checks:
  presented       both clients list the party task (party-task given) and show the server's latest
                  pooled count before the completion
  finished        every client in the world at the completion shows it completed (variant 1: the
                  client that stayed)
  completed       the server completed the task once (quest-completed)
  news_backlog    (variant 1) the server replayed the news the away player missed (present-backlog) and
                  that client showed the completion's news after its rejoin
  anchor          (variant 2) participant 1 died and respawned, the server kept the task
                  (quest-anchor-respawned), the turn-in after it was Accepted
  map_spot        (variant 2) both clients put the party stash's spot on their map
  continuity      (variant 2) the respawned actor received the old body's info portions
"""

import argparse
import json
import re
import sys
from pathlib import Path

TASK = "esc_m_trader_task_3"


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def field(line, name, cast=str):
    m = re.search(r"\b" + re.escape(name) + r"=(\S+)", line)
    return cast(m.group(1)) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--variant", type=int, required=True)
    ap.add_argument("--out", default="compare-party.json")
    ap.add_argument("--expect-restart", action="store_true")
    a = ap.parse_args()
    logs = Path(a.logs)
    dirs = [d for d in (logs / "server-first-run", logs / "server") if (d / "coop-server.log").exists()]
    every = [x for d in dirs for x in lines(d / "coop-server.log")]
    clients = sorted(p.name for p in logs.iterdir() if p.is_dir() and re.fullmatch(r"c\d+", p.name))
    report = {"variant": a.variant, "checks": {}}

    progress = [field(x, "count", int) for x in every if " quest-progress " in x and f"task={TASK}" in x]
    completed = [x for x in every if " quest-completed " in x and f"task={TASK}" in x]
    completed_ms = int(completed[0].split(" ", 1)[0]) if completed else None
    last_count = progress[-1] if progress else None
    report["server"] = {"progress": progress, "completed": len(completed)}
    report["checks"]["completed"] = {"pass": len(completed) == 1, "lines": len(completed)}

    shown, finished, news_after_rejoin = {}, {}, {}
    for c in clients:
        log = lines(logs / c / f"coop-{c}.log")
        given = [x for x in log if " party-task " in x and f"id={TASK}" in x]
        counts = [field(x, "count", int) for x in given if field(x, "state") == "1"]
        shown[c] = {"given": any(" party-task given " in x for x in given), "counts": counts}
        finished[c] = any(" party-task finished " in x and " state=2 " in x for x in given)
        rejoins = [int(x.split(" ", 1)[0]) for x in log if " world-live " in x]
        last_live = rejoins[-1] if rejoins else 0
        news_after_rejoin[c] = [x.split(" ", 1)[1] for x in log
                                if " news-shown " in x and "complete" in x and int(x.split(" ", 1)[0]) >= last_live]
    report["clients"] = {"shown": shown, "finished": finished}
    report["checks"]["presented"] = {
        "pass": all(v["given"] and v["counts"] and (last_count is None or last_count in v["counts"])
                    for v in shown.values()) and bool(shown),
        "server_last_count": last_count}
    if a.variant == 1:
        stayed = [c for c in clients if not any(" transfer-step-disconnect " in x
                                                for x in lines(logs / c / f"coop-{c}.log"))]
        report["checks"]["finished"] = {"pass": bool(stayed) and all(finished[c] for c in stayed), "stayed": stayed}
        backlog = [x.split(" ", 1)[1] for x in every if " present-backlog " in x]
        away = [c for c in clients if c not in stayed]
        report["checks"]["news_backlog"] = {
            "pass": bool(backlog) and bool(away) and all(news_after_rejoin[c] for c in away),
            "server": backlog, "client_news": {c: news_after_rejoin[c] for c in away}}
    else:
        report["checks"]["finished"] = {"pass": all(finished.values()) and bool(finished)}
    if a.variant == 2:
        killed = [x for x in every if " fixture-quest anchor-killed " in x]
        respawned = [x for x in every if " fixture-quest anchor-respawned=1 " in x]
        kept = [x.split(" ", 1)[1] for x in every if " quest-anchor-respawned " in x]
        turnin_ok = completed_ms is not None and bool(respawned) and int(respawned[0].split(" ", 1)[0]) < completed_ms
        report["checks"]["anchor"] = {"pass": bool(killed) and bool(respawned) and bool(kept) and turnin_ok,
                                      "kept": kept}
        # the stash's spot: the first treasure spot the server presented after the stash was made
        stash = [x for x in every if " fixture-quest party-stash made=1" in x]
        made_ms = int(stash[0].split(" ", 1)[0]) if stash else None
        objects = {field(x, "object") for x in every if made_ms and " kind=map-spot-add " in x and
                   " detail=treasure" in x and int(x.split(" ", 1)[0]) >= made_ms}
        spots = {c: sorted({field(x, "object") for x in lines(logs / c / f"coop-{c}.log")
                            if " map-spot kind=map-spot-add " in x and " spot=treasure" in x} & objects)
                 for c in clients}
        # (a restart run's fixture restarts its rounds and may complete the rebuilt task before the stash round)
        report["checks"]["map_spot"] = {"pass": (bool(stash) and bool(objects) and all(spots.values()) and bool(spots))
                                        or (a.expect_restart and not stash),
                                        "server_objects": sorted(objects), "clients": spots}
        if a.expect_restart:
            # the restart after the anchor's death: the rebuilt task's store is the original body's
            rebuilt = [x.split(" ", 1)[1] for x in every if " quest-rebuilt " in x]
            report["checks"]["rebuilt_store"] = {
                "pass": bool(rebuilt) and all(field(x, "holder") != field(x, "anchor") for x in rebuilt),
                "lines": rebuilt}
        cont = [x.split(" ", 1)[1] for x in every if " respawn-continuity " in x]
        report["checks"]["continuity"] = {"pass": bool(cont) and all((field(x, "infos", int) or 0) > 0 for x in cont),
                                          "lines": cont}

    report["pass"] = all(v["pass"] for v in report["checks"].values())
    out = Path(a.out)
    out.write_text(json.dumps(report, indent=2) + "\n")
    for k, v in report["checks"].items():
        print(f"{'PASS' if v['pass'] else 'FAIL'} {k}")
    print("overall", "PASS" if report["pass"] else "FAIL", "->", out)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Check the S10-T05c story fixture (plan.md S10-T05c; docs/validation/S10-T05c.md).
Usage: python3 tools/compare-coop-story.py <session logs> [--out compare-story.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log (the restarted run) and server-first-run/coop-server.log
(Run-CoopSession -ServerKill), each with "emission story-…" lines from the compat layer and fixture-story lines;
server/xray-campaign-server_*.log; c1/, c2/: xray_*.log; result.json.
The session: prepare-coop-session.py --story c1,c2 and Run-CoopSession -ServerKill <s> -ServerRestartAfter 10.
Checks:
  wrap      the compat layer kept the DoctorX variables the party's (story wrap: drx)
  drx       player 2's step chose a honcho and gave its meet task; player 1's meeting completed it (state 2, or
            65535 once the task manager has dropped the completed task: its on_fail would have lowered the number) and
            player 1's step chose the next one: the task number went up by one each step
  shared    after the steps, after the travel to Garbage and after the restart, both players' contexts and the
            host read the same DoctorX state, the current meet task in progress, the Living Legend chain moved to
            lttz_ll_visit_agro_ug (find_doctor complete) with its info for both (story-verify pass=1, three tags)
  durable   each change of a story variable was logged and published (story-checkpoint published=1)
  clients   no FATAL in a client's log; exit codes reported (a long run's teardown crash is B5)
"""
import argparse
import json
import re
import sys
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def kv(line):
    return {k: v.strip('"') for k, v in re.findall(r"(\w+)=(\"[^\"]*\"|[^\s\"]+)", line)}


def grep(log, marker):
    return [kv(ln) for ln in log if marker in ln]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    first = lines(root / "server-first-run" / "coop-server.log")
    second = lines(root / "server" / "coop-server.log")
    log = first + second if first else second
    xlog = "".join(p.read_text(errors="replace") for d in ("server", "server-first-run")
                   for p in (root / d).glob("xray-campaign-server_*.log"))
    checks = {}
    wrap = re.findall(r"S10-T05c compatibility layer story wrap: (\S+)", xlog)
    checks["wrap"] = {"pass": bool(wrap) and all(w == "drx" for w in wrap), "wrap": sorted(set(wrap))}
    steps = {s.get("label"): s for s in grep(log, " emission story-step ")}
    meet = grep(log, " emission story-meet-done ")
    p2, p1 = steps.get("drx-p2"), steps.get("drx-p1")

    def number(s):
        try:
            return int(s.get("number", "-1"))
        except (TypeError, ValueError):
            return -1

    checks["drx"] = {"pass": bool(p2) and bool(p1) and p2.get("ok") == "true" and p1.get("ok") == "true"
                     and p2.get("task_state") == "1" and p1.get("task_state") == "1" and bool(meet)
                     and meet[0].get("state") in ("2", "65535") and meet[0].get("task") == p2.get("task")
                     and number(p1) == number(p2) + 1 and p1.get("task") != p2.get("task"),
                     "p2_step": p2, "meet": meet[:1], "p1_step": p1}
    verify = {v.get("tag"): v for v in grep(log, " emission story-verify ")}
    want = f"{p1.get('honcho')}/{p1.get('number')}/{p1.get('task')}" if p1 else None
    tags = ("steps", "travel", "restart")
    checks["shared"] = {"pass": all(t in verify and verify[t].get("pass") == "1" and verify[t].get("drx0") == want
                                    for t in tags)
                        and verify.get("travel", {}).get("level") not in (None, "l01_escape"),
                        "expected": want, "verify": {t: verify.get(t) for t in tags}}
    changes = grep(log, " story-var ")
    published = grep(log, " story-checkpoint ")
    checks["durable"] = {"pass": bool(changes) and bool(published)
                         and all(p.get("published") == "1" for p in published),
                         "changes": len(changes), "checkpoints": published[:4]}
    exits = {}
    result = root / "result.json"
    if result.exists():
        exits = {k: v.get("exit_code") for k, v in json.loads(result.read_text(encoding="utf-8-sig"))
                 .get("clients", {}).items()}
    fatal = [str(p.relative_to(root)) for p in root.glob("c*/xray_*.log")
             if "FATAL ERROR" in p.read_text(errors="replace")]
    checks["clients"] = {"pass": not fatal, "exits": exits, "fatal": fatal}
    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

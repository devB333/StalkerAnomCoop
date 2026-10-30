#!/usr/bin/env python3
"""Check the S10-T02a NPC logic fixture (plan.md S10-T02a; docs/validation/S10-T02a.md).
Usage: python3 tools/compare-coop-npc.py <session logs> [--out compare-npc.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log (no restart in this session).
The fixture (--npc c1,c2, profile npc_party; CoopServerHost.cpp npc_fixtures): the alive stalker nearest
player 2 is the subject (player 1 moved away when nearer); a Lua chunk reads db.actor's id, AC_ID and a
distance condlist inside the NPC's evaluation scope and outside it.
Checks:
  subject   fixture-npc eval with d_p2 < d_p1 (player 2 is the nearer player)
  in_scope  in_actor and in_ac_id are player 2's id and in_pick=near
  outside   out_actor and out_ac_id are player 1's id (the host's actor) and out_pick=far when d_p1 > threshold
  scoped    the done line has npc_scope_enters > 0 (NPC binder updates ran with the rule)
  failures  fixture-npc done failures=0
"""
import argparse
import json
import re
import sys
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def kv(line):
    return dict(re.findall(r"(\w+)=([^\s\"]+|\"[^\"]*\")", line))


def first_kv(log, marker):
    for ln in log:
        if marker in ln:
            return kv(ln)
    return {}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    log = lines(root / "server" / "coop-server.log")
    checks = {}
    ev = first_kv(log, " fixture-npc eval ")
    done = first_kv(log, " fixture-npc done ")

    def f(key):
        try:
            return float(ev.get(key))
        except (TypeError, ValueError):
            return None

    d1, d2, thr = f("d_p1"), f("d_p2"), f("threshold")
    checks["subject"] = {"pass": d1 is not None and d2 is not None and d2 < d1, "line": ev}
    checks["in_scope"] = {"pass": ev.get("in_actor") == ev.get("p2") and ev.get("in_ac_id") == ev.get("p2")
                          and ev.get("in_pick") == "near", "line": ev}
    outside_far = ev.get("out_pick") == "far" if (d1 is not None and thr is not None and d1 > thr) else True
    checks["outside"] = {"pass": ev.get("out_actor") == ev.get("p1") and ev.get("out_ac_id") == ev.get("p1")
                         and outside_far, "line": ev}
    checks["scoped"] = {"pass": done.get("npc_scope_enters") not in (None, "0"), "done": done}
    checks["failures"] = {"pass": done.get("failures") == "0", "done": done}
    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

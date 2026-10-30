#!/usr/bin/env python3
"""Check the S10-T03b squad-target fixture (plan.md S10-T03b; docs/validation/S10-T03b.md).
Usage: python3 tools/compare-coop-squad.py <session logs> [--out compare-squad.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log and xray-campaign-server_*.log; each client's
xray_*.log; result.json.
The fixture (--squad c1,c2, profile squad_party; CoopServerHost.cpp squad_fixtures): p1 moves 300+ m away; a loner
squad created 120-250 m from p2 is sent at "the actor" (scripted_target "actor", rush_to_target, as bounty hunters
are); then p1 is put 10 m from it.
Checks:
  wrap       the squad-target wrap is installed (S10-T02f companion wrap includes +target; get_script_target)
  target_p2  the squad's script target is p2's actor id while p2 is the nearest player (the installed rule: 0, p1)
  reach      the squad's commander came within 30 m of p2 (fixture-squad reach reached=1)
  target_p1  with p1 put nearer, the target became p1's actor (0)
  lua        no fixture Lua error
  clients    every client exits 0, no FATAL in their engine logs
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


def grep(log, marker):
    return [kv(ln) for ln in log if marker in ln]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    log = lines(root / "server" / "coop-server.log")
    xlog = "".join(p.read_text(errors="replace") for p in (root / "server").glob("xray-campaign-server_*.log"))
    checks = {}
    wrap = re.findall(r"S10-T02f compatibility layer companion wrap: (\S+)", xlog)
    checks["wrap"] = {"pass": bool(wrap) and "+target" in wrap[-1], "wrap": wrap[-1:]}
    t2 = grep(log, " fixture-squad target phase=p2 ")
    checks["target_p2"] = {"pass": bool(t2) and t2[0].get("pass") == "1" and t2[0].get("target") == t2[0].get("p2")
                           and t2[0].get("target") != "0", "target": t2[:1],
                           "select": grep(log, " fixture-squad select ")[:1],
                           "created": grep(log, " fixture-squad created ")[:1]}
    reach = grep(log, " fixture-squad reach ")
    checks["reach"] = {"pass": bool(reach) and reach[0].get("reached") == "1", "reach": reach[:1],
                       "approach": grep(log, " fixture-squad approach ")}
    t1 = grep(log, " fixture-squad target phase=p1 ")
    checks["target_p1"] = {"pass": bool(t1) and t1[0].get("pass") == "1" and t1[0].get("target") == "0",
                           "target": t1[:1], "done": grep(log, " fixture-squad done ")[:1]}
    errors = [ln for ln in log if "lua-error" in ln or "lua-load-error" in ln]
    checks["lua"] = {"pass": not errors, "errors": errors[:5]}
    exits = {}
    result = root / "result.json"
    if result.exists():
        exits = {k: v.get("exit_code") for k, v in json.loads(result.read_text(encoding="utf-8-sig"))
                 .get("clients", {}).items()}
    fatal = [str(p.relative_to(root)) for p in root.glob("c*/xray_*.log")
             if "FATAL ERROR" in p.read_text(errors="replace")]
    checks["clients"] = {"pass": bool(exits) and all(v == 0 for v in exits.values()) and not fatal, "exits": exits,
                         "fatal": fatal}
    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

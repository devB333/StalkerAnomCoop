#!/usr/bin/env python3
"""Check the S10-T02f companion fixture (plan.md S10-T02f; docs/validation/S10-T02f.md).
Usage: python3 tools/compare-coop-companion.py <session logs> [--out compare-companion.json]
Inputs (builds/logs/<task>/run-<run>/): server-first-run/ (the run before -ServerKill, when there was one) and
server/ (the restarted run, or the only one): coop-server.log and xray-campaign-server_*.log; result.json.
The fixture (--companion c1,c2, profile companion_party; CoopServerHost.cpp companion_fixtures): p2 recruits the
nearest friendly stalker in p2's context (or, after a restart, the restored companion is the subject), then checks.
Checks:
  wrap       the server's compat layer wrapped the companion functions (S10-T02f companion wrap: squad+target+dialog)
  recruited  the first run's fixture-companion recruited line has slot = p2's slot (1), and a companion-changed line
             for that NPC asked for a checkpoint (published=1)
  checks     every fixture-companion checks line has bad=0 (recruiter slot p2's; db.actor in the companion's
             evaluation is p2 with p1 4 m away; the command dialogue refused to p1, offered to p2; squad target p2)
  follow     every fixture-companion follow line has reached=1 (the companion walked to within 8 m of p2)
  restored   with a server restart: the restarted run's fixture found the same companion (restored npc= saved_slot=1)
             and its checks passed; informational without a restart
  clients    both clients exit 0 and no FATAL in their engine logs
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
    first_dir = root / "server-first-run" if (root / "server-first-run").exists() else root / "server"
    restarted = (root / "server-first-run").exists()
    first = lines(first_dir / "coop-server.log")
    second = lines(root / "server" / "coop-server.log") if restarted else []
    xfirst = "".join(p.read_text(errors="replace") for p in first_dir.glob("xray-campaign-server_*.log"))
    checks = {}
    wrap = re.findall(r"S10-T02f compatibility layer companion wrap: (\S+)", xfirst)
    checks["wrap"] = {"pass": bool(wrap) and wrap[-1] == "squad+target+dialog", "wrap": wrap[-1:]}
    rec = grep(first, " fixture-companion recruited ")
    npc = rec[0].get("npc") if rec else None
    changed = [c for c in grep(first, " companion-changed ") if c.get("npc") == npc]
    asked = any(c.get("slot") == "1" and c.get("published") == "1" for c in changed)
    checks["recruited"] = {"pass": bool(rec) and rec[0].get("slot") == "1" and asked, "recruited": rec[:1],
                           "changed": changed[:2]}
    all_checks = grep(first, " fixture-companion checks ") + grep(second, " fixture-companion checks ")
    checks["checks"] = {"pass": bool(all_checks) and all(c.get("bad") == "0" for c in all_checks), "lines": all_checks}
    follows = grep(first, " fixture-companion follow ") + grep(second, " fixture-companion follow ")
    checks["follow"] = {"pass": bool(follows) and all(f.get("reached") == "1" for f in follows), "lines": follows}
    if restarted:
        restored = grep(second, " fixture-companion restored ")
        r_checks = grep(second, " fixture-companion checks ")
        checks["restored"] = {"pass": bool(restored) and restored[0].get("npc") == npc and restored[0].get(
            "saved_slot") == "1" and bool(r_checks) and all(c.get("bad") == "0" for c in r_checks),
            "restored": restored[:1], "checks": r_checks[:1]}
    else:
        checks["restored"] = {"pass": True, "informational": True}
    exits = {}
    result = root / "result.json"
    if result.exists():
        exits = {k: v.get("exit_code") for k, v in json.loads(result.read_text()).get("clients", {}).items()}
    fatal = [str(p.relative_to(root)) for c in ("c1", "c2") for p in (root / c).glob("xray_*.log")
             if "FATAL ERROR" in p.read_text(errors="replace")]
    checks["clients"] = {"pass": bool(exits) and all(v == 0 for v in exits.values()) and not fatal, "exits": exits,
                         "fatal": fatal}
    report = {"checks": checks, "restarted": restarted, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

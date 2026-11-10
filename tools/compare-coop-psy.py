#!/usr/bin/env python3
"""Check the S10-T02c psy fixture (plan.md S10-T02c; docs/validation/S10-T02c.md).
Usage: python3 tools/compare-coop-psy.py <session logs> [--out compare-psy.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log; c1/, c2/: coop-c*.log and xray_*.log
(no restart in this session).
The fixture (--psy c1,c2, profile psy_party; CoopServerHost.cpp psy_fixtures): p1 moved about 120 m from
p2, a controller spawned 15 m in front of p2; its psy attack awaited for up to 90 s; then the controller is
killed.
Checks:
  spawned   fixture-psy spawned with a controller id and p1 about 100 m or more from p2
  target    at least one psy-hit line with phase 3 (the tube hit) or 7 (the control hit), every hit's slot is
            p2's (1), and the done line has hits_p2 > 0 and hits_p1 = 0
  c2        c2 applied the attack's presentation (psy-hit-applied with the controller id)
  c1        c1 applied none (the attack is the hit player's alone)
  aura      c2's engine log has an aura-pp-add for the controller (the aura by its own distance); c1's has none
  failures  fixture-psy done failures=0
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


def engine(root, name):
    out = []
    for p in (root / name).glob("xray_*.log"):
        out += lines(p)
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    server = lines(root / "server" / "coop-server.log")
    c1, c2 = lines(root / "c1" / "coop-c1.log"), lines(root / "c2" / "coop-c2.log")
    e1, e2 = engine(root, "c1"), engine(root, "c2")
    checks = {}

    sp = first_kv(server, " fixture-psy spawned ")
    ctrl = sp.get("controller")
    try:
        far = float(sp.get("p1_distance", "0")) >= 100.0
    except ValueError:
        far = False
    checks["spawned"] = {"pass": ctrl not in (None, "65535") and far, "line": sp}

    hits = [kv(ln) for ln in server if " psy-hit controller=" in ln and kv(ln).get("phase") in ("3", "7")]
    done = first_kv(server, " fixture-psy done ")
    checks["target"] = {"pass": bool(hits) and all(h.get("slot") == "1" for h in hits)
                        and done.get("hits_p2") not in (None, "0") and done.get("hits_p1") == "0",
                        "hits": hits[:4], "done": done}

    def applied(log):
        return [kv(ln) for ln in log if " psy-hit-applied " in ln and kv(ln).get("controller") == ctrl]

    a2, a1 = applied(c2), applied(c1)
    checks["c2"] = {"pass": bool(a2), "lines": a2[:6]}
    checks["c1"] = {"pass": not a1, "lines": a1[:3]}

    def aura(log):
        return [ln for ln in log if "aura-pp-add" in ln and f"monster={ctrl} " in ln]

    checks["aura"] = {"pass": bool(aura(e2)) and not aura(e1), "c2": aura(e2)[:2], "c1": aura(e1)[:2]}
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

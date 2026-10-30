#!/usr/bin/env python3
"""Check the S10-T02e fauna fixture (plan.md S10-T02e; docs/validation/S10-T02e.md).
Usage: python3 tools/compare-coop-fauna.py <session logs> [--out compare-fauna.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log; c1/, c2/: coop-c*.log (no restart).
The fixture (--fauna c1,c2, profile fauna_party; CoopServerHost.cpp fauna_fixtures): p1 moved about 120 m from
p2; each generic mutant class spawned in turn 4 m in front of p2 for up to 40 s.
Checks:
  p1_away   fixture-fauna p1-away distance >= 100
  classes   every stage-done line has enemy_p2=1 and enemy_p1=0 (each class targets the player in front of it,
            never the host's actor 120 m away), and either hit_p2=1 (it attacked) or it fled from p2 (distance >= 30 m
            at the stage's end: a lone flesh, tushkano or rat panics by its own morale, as in single player)
  threaten  any pseudogigant threaten (monster-ability threaten) landed on slot 1 and client 2 applied the
            kick (monster-effect-applied phase 4); informational when the gigant never threatened
  phantoms  the psy dog's stage saw its phantoms (phantoms >= 1) and client 2 shows no refused or failed spawn
            (S10-T02e custom-monster spawn refused, Failed to spawn) and no crash
  lighting  fixture-fauna lighting: actor_tracks and view_picks at least 1.9 x frames (both players lit, one object in
            each player's view picked every frame, S03-T06; L13)
  failures  fixture-fauna done: every stage the fixture failed is one that fled (the fixture counts a stage without
            a hit as failed)
  clients   both clients exit 0 (result.json) and no FATAL in their engine logs
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
    server = lines(root / "server" / "coop-server.log")
    c2 = lines(root / "c2" / "coop-c2.log")
    checks = {}
    away = first_kv(server, " fixture-fauna p1-away ")
    try:
        far = float(away.get("distance", "0")) >= 100.0
    except ValueError:
        far = False
    checks["p1_away"] = {"pass": far, "line": away}
    stages = [kv(ln) for ln in server if " fixture-fauna stage-done " in ln]
    def fled(s):
        try:
            return float(s.get("distance", "0")) >= 30.0
        except ValueError:
            return False

    bad = [s for s in stages if not (s.get("enemy_p2") == "1" and s.get("enemy_p1") == "0"
                                     and (s.get("hit_p2") == "1" or fled(s)))]
    checks["classes"] = {"pass": bool(stages) and not bad, "stages": [
        {k: s.get(k) for k in ("section", "enemy_p2", "hit_p2", "enemy_p1", "seconds", "distance")} for s in stages],
        "attacked": [s.get("section") for s in stages if s.get("hit_p2") == "1"],
        "fled": [s.get("section") for s in stages if s.get("hit_p2") != "1" and fled(s)],
        "failing": [s.get("section") for s in bad]}
    threaten = [kv(ln) for ln in server if " monster-ability " in ln and "ability=threaten" in ln]
    kicks = [ln for ln in c2 if " monster-effect-applied " in ln and "phase=4" in ln]
    checks["threaten"] = {"pass": (not threaten) or (all(t.get("slot") == "1" for t in threaten) and bool(kicks)),
                          "server": threaten[:3], "c2": kicks[:2], "informational": not threaten}
    dog = next((s for s in stages if s.get("section") == "psy_dog"), {})
    xlog = "".join(p.read_text(errors="replace") for p in (root / "c2").glob("xray_*.log"))
    refused = [ln.strip() for ln in xlog.splitlines() if "S10-T02e custom-monster spawn refused" in ln
               or "Failed to spawn" in ln]
    try:
        seen = int(dog.get("phantoms", "0")) >= 1
    except ValueError:
        seen = False
    checks["phantoms"] = {"pass": seen and not refused, "psy_dog": dog.get("phantoms"), "refused": refused[:4]}
    light = first_kv(server, " fixture-fauna lighting ")
    try:
        frames = int(light.get("frames", "0"))
        lit = frames > 0 and int(light.get("actor_tracks", "0")) >= 1.9 * frames and int(
            light.get("view_picks", "0")) >= 1.9 * frames
    except ValueError:
        lit = False
    checks["lighting"] = {"pass": lit, "line": light}
    done = first_kv(server, " fixture-fauna done ")
    try:
        unexplained = int(done.get("failures", "-1")) - len(checks["classes"]["fled"])
    except ValueError:
        unexplained = -1
    checks["failures"] = {"pass": bool(done) and unexplained == 0, "done": done, "unexplained": unexplained}
    exits = {}
    result = root / "result.json"
    if result.exists():
        exits = {k: v.get("exit_code") for k, v in json.loads(result.read_text()).get("clients", {}).items()}
    fatal = [str(p.relative_to(root)) for c in ("c1", "c2") for p in (root / c).glob("xray_*.log")
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

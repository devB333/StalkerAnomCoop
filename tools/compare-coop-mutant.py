#!/usr/bin/env python3
"""Check the S10-T02d mutant fixture (plan.md S10-T02d; docs/validation/S10-T02d.md).
Usage: python3 tools/compare-coop-mutant.py <session logs> [--out compare-mutant.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log; c1/, c2/: coop-c*.log (no restart).
The fixture (--mutant c1,c2, profile mutant_party; CoopServerHost.cpp mutant_fixtures): p1 moved about 120 m
from p2; a bloodsucker, a burer and a poltergeist spawned in turn near p2, each up to 60 s with its ability
forced (the vampire wish, the anti-aim, the detection).
Checks:
  p1_away   fixture-mutant p1-away distance >= 100
  vampire   a monster-ability vampire-grab on slot 1, none on slot 0; c2 applied monster-effect phases 1 and 3
            (the grab and the release), c1 none
  anti_aim  a monster-ability anti-aim on slot 1, none on slot 0; c2 applied a camera-effector presentation
            from the burer's stage; c1 none at that time
  stamina   a monster-ability stamina on slot 1 (the burer's hit lands on its target)
  tele      a monster-ability tele on slot 1, none on slot 0 (the poltergeist throws at its target)
  failures  fixture-mutant done failures=0 and three stage-done lines
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
    c1, c2 = lines(root / "c1" / "coop-c1.log"), lines(root / "c2" / "coop-c2.log")
    checks = {}

    away = first_kv(server, " fixture-mutant p1-away ")
    try:
        far = float(away.get("distance", "0")) >= 100.0
    except ValueError:
        far = False
    checks["p1_away"] = {"pass": far, "line": away}

    abilities = [kv(ln) for ln in server if " monster-ability " in ln]

    def on(ability, slot):
        return [x for x in abilities if x.get("ability") == ability and x.get("slot") == slot]

    def effect(log, phase):
        return [kv(ln) for ln in log if " monster-effect-applied " in ln and kv(ln).get("phase") == phase]

    checks["vampire"] = {"pass": bool(on("vampire-grab", "1")) and not on("vampire-grab", "0")
                         and bool(effect(c2, "1")) and bool(effect(c2, "3")) and not effect(c1, "1"),
                         "server": on("vampire-grab", "1")[:2], "c2": effect(c2, "1")[:1] + effect(c2, "3")[:1],
                         "c1": effect(c1, "1")[:1]}

    stage1 = [ln for ln in server if " fixture-mutant spawned stage=1 " in ln]
    stage1_done = [ln for ln in server if " fixture-mutant stage-done stage=1 " in ln]
    t0 = int(stage1[0].split(" ", 1)[0]) if stage1 else None
    t1 = int(stage1_done[0].split(" ", 1)[0]) + 5000 if stage1_done else None

    def cam(log):
        if t0 is None or t1 is None:
            return []
        return [ln for ln in log if " kind=camera-effector " in ln and t0 <= int(ln.split(" ", 1)[0]) <= t1]

    checks["anti_aim"] = {"pass": bool(on("anti-aim", "1")) and not on("anti-aim", "0") and bool(cam(c2))
                          and not cam(c1), "server": on("anti-aim", "1")[:2], "c2": cam(c2)[:2], "c1": cam(c1)[:2]}
    checks["stamina"] = {"pass": bool(on("stamina", "1")) and not on("stamina", "0"), "server": on("stamina", "1")[:2]}
    checks["tele"] = {"pass": bool(on("tele", "1")) and not on("tele", "0"), "server": on("tele", "1")[:2]}
    done = first_kv(server, " fixture-mutant done ")
    stages = [kv(ln) for ln in server if " fixture-mutant stage-done " in ln]
    checks["failures"] = {"pass": done.get("failures") == "0" and len(stages) == 3, "done": done, "stages": stages}

    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

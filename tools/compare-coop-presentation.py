#!/usr/bin/env python3
"""Measure the presentation records a session's clients got (plan.md S10-T05d; docs/validation/S10-T05d.md).
Usage: python3 tools/compare-coop-presentation.py <session logs> [--baseline <session logs>] [--out compare.json]
Inputs (builds/logs/<task>/run-<run>/): c1/ and c2/ coop-c<n>.log ("present … <status>" lines: every record a client
applied or could not, except the applied ones of the chatty kinds), server/coop-server.log (loops-joined,
presentation-stop) and server/xray-campaign-server_*.log (the script-context stop: presented, broadcast, dropped).
Checks (S10-T05d1):
  orphans   a client got almost no record for a sound or effector it does not have: "absent" records are under
            1 % of the run's records (before: a volume for every loop every update, a remove for effects never on)
  loops     every client entering the world got the looped sounds already playing (loops-joined loops > 0) and
            applied them (sound / sound-at-pos records applied after its join)
  dropped   the server's outboxes evicted fewer records than the baseline (when one is given)
The baseline's counts are printed beside the run's.
"""
import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def counts(root):
    out = {}
    for c in ("c1", "c2"):
        k = Counter()
        for ln in lines(root / c / f"coop-{c}.log"):
            if " present " not in ln:
                continue
            m = re.search(r"kind=(\S+)", ln)
            k[(m.group(1) if m else "?", ln.rsplit(" ", 1)[-1])] += 1
        out[c] = k
    xlog = "".join(p.read_text(errors="replace") for d in ("server", "server-first-run")
                   for p in (root / d).glob("xray-campaign-server_*.log"))
    stop = re.findall(r"presented=(\d+) present_no_context=\d+ present_broadcast=(\d+) dropped=(\d+)", xlog)
    server = {"presented": sum(int(s[0]) for s in stop), "broadcast": sum(int(s[1]) for s in stop),
              "dropped": sum(int(s[2]) for s in stop)}
    return out, server


def summary(k):
    total = sum(k.values())
    absent = sum(v for (kind, st), v in k.items() if st == "absent")
    top = {f"{kind}/{st}": v for (kind, st), v in k.most_common(6)}
    return {"records": total, "absent": absent, "top": top}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--baseline", default="")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    run, server = counts(root)
    base = counts(Path(a.baseline)) if a.baseline else None
    checks = {}
    sums = {c: summary(k) for c, k in run.items()}
    checks["orphans"] = {"pass": all(s["records"] and s["absent"] < 0.01 * s["records"] for s in sums.values()),
                         "run": sums, "baseline": {c: summary(k) for c, k in base[0].items()} if base else None}
    log = lines(root / "server" / "coop-server.log")
    joins = [dict(re.findall(r"(\w+)=(\S+)", ln)) for ln in log if " loops-joined " in ln]
    applied = {c: sum(v for (kind, st), v in run[c].items() if kind in ("sound", "sound-at-pos") and st == "applied")
               for c in run}
    checks["loops"] = {"pass": bool(joins) and all(int(j.get("loops", "0")) > 0 for j in joins)
                       and {j.get("to") for j in joins} >= {"0", "1"} and all(v > 0 for v in applied.values()),
                       "joins": joins, "sounds_applied": applied,
                       "stop": [ln.split(" ", 1)[1] for ln in log if " presentation-stop " in ln]}
    checks["dropped"] = {"pass": base is None or server["dropped"] < base[1]["dropped"], "run": server,
                         "baseline": base[1] if base else None}
    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

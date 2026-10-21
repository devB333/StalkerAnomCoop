#!/usr/bin/env python3
"""Measure own-actor prediction and reconciliation in a movement session (S06-T03).

Usage: python3 tools/compare-coop-prediction.py <session logs> [--out compare-prediction.json]

Inputs, per client: coop-prediction.csv (one row per complete snapshot: the
error between the server's position for the last processed input and where
the client had its actor when it sampled that input, and the correction
applied: kind 0 none, 1 smoothed, 2 snapped) and the client's co-op log
(script-leg windows, script-leg-first-move, final status).

Checks for each client that predicts (fixed before the final runs):
  normal_legs     p95 error <= 0.30 m in legs A, B, C, D, E, F, I-backoff, J
  sprint          p95 error <= 0.50 m in I-sprint-toward
  wrong_bounded   max error <= 0.60 m in G (a stance the server refuses) and
                  P (the server told to stand while the client walks)
  wrong_converged after G and P (0.3-1.0 s after they end, before the next
                  leg or the connection's end), every error <= 0.05 m
  no_snaps        no snapped correction
  responsive      median time from a moving leg's start to 5 cm of shown
                  movement <= 120 ms (rule change after pred-3: 60 ms was below
                  what the native controller needs to cover 5 cm from rest,
                  about 60 ms of acceleration plus a frame or two; pred-3
                  measured 83-100 ms predicting, 234-250 ms not)
A client with predict = 0 is only reported (its responsiveness is the
baseline).
"""

import argparse
import csv
import json
import re
import statistics
import sys
from pathlib import Path

NORMAL = ("A-away", "A-toward", "B-away", "B-toward", "C-away", "C-toward", "D-stale", "E-away", "F-away",
          "F-toward", "I-backoff", "J-teleport")
WRONG = ("G-invalid", "P-wrong-prediction")


def pct(v, p):
    v = sorted(v)
    return v[min(len(v) - 1, max(0, int(round(p / 100 * (len(v) - 1)))))] if v else None


def client(d):
    name = d.name
    log = d / f"coop-{name.split('-')[0]}.log"
    if not log.exists():
        logs = list(d.glob("coop-c*.log"))
        if not logs:
            return None
        log = logs[0]
    legs, first, final, predicting = [], {}, "", None
    for line in open(log, errors="replace"):
        m = re.search(r"^(\d+) script-leg name=(\S+) start_utc=(\d+) end_utc=(\d+)", line)
        if m:
            legs.append((m.group(2), int(m.group(3)), int(m.group(4))))
        m = re.search(r"script-leg-first-move name=(\S+) ms=(\d+) predicting=(\d)", line)
        if m:
            first[m.group(1)] = int(m.group(2))
        m = re.search(r" prediction (on|off)", line)
        if m:
            predicting = m.group(1) == "on"
        if " final " in line:
            final = line
    rows = []
    f = d / "coop-prediction.csv"
    if f.exists():
        rows = [(int(r["utc_ms"]), float(r["error"]), int(r["kind"])) for r in csv.DictReader(open(f, newline=""))]
    pattern = r" (pred_\w+|view_max_offset|correction_frames)=([0-9.]+)"
    stats = {k: (float(v) if "." in v else int(v)) for k, v in re.findall(pattern, final)}
    per_leg = {}
    for i, (leg, a, b) in enumerate(legs):
        e = [err for t, err, _ in rows if a <= t <= b + 600]
        nxt = legs[i + 1][1] if i + 1 < len(legs) else (rows[-1][0] + 1 if rows else b)
        after = [err for t, err, _ in rows if b + 300 <= t <= min(b + 1000, nxt)]
        per_leg[leg] = {"n": len(e), "median": round(statistics.median(e), 4) if e else None,
                        "p95": round(pct(e, 95), 4) if e else None, "max": round(max(e), 4) if e else None,
                        "after_n": len(after), "after_max": round(max(after), 4) if after else None,
                        "first_move_ms": first.get(leg)}
    return {"predicting": predicting, "rows": len(rows), "snaps": sum(1 for _, _, k in rows if k == 2),
            "smoothed": sum(1 for _, _, k in rows if k == 1), "stats": stats, "legs": per_leg,
            "first_move_ms": sorted(first.values())}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    logs = Path(a.logs)
    result = {"clients": {}, "checks": []}
    for d in sorted(p for p in logs.iterdir() if p.is_dir() and p.name.startswith("c")):
        c = client(d)
        if c:
            result["clients"][d.name] = c
    for name, c in result["clients"].items():
        if not c["predicting"]:
            continue
        legs = c["legs"]

        def worst(names, key, legs=legs):
            v = [legs[n][key] for n in names if n in legs and legs[n][key] is not None]
            return max(v) if v else None

        def add(check, ok, detail, name=name):
            result["checks"].append({"check": f"{name} {check}", "ok": bool(ok), "detail": detail})
        n95 = worst(NORMAL, "p95")
        add("normal_legs", n95 is not None and n95 <= 0.30, f"worst p95 {n95}")
        s95 = legs.get("I-sprint-toward", {}).get("p95")
        add("sprint", s95 is not None and s95 <= 0.50, f"p95 {s95}")
        wmax = worst(WRONG, "max")
        add("wrong_bounded", wmax is not None and wmax <= 0.60, f"max {wmax}")
        conv = [(n, legs[n]["after_n"], legs[n]["after_max"]) for n in WRONG if n in legs]
        add("wrong_converged", conv and all(k >= 2 and m is not None and m <= 0.05 for _, k, m in conv), f"{conv}")
        add("no_snaps", c["snaps"] == 0, f"snaps {c['snaps']}")
        fm = c["first_move_ms"]
        med = statistics.median(fm) if fm else None
        add("responsive", med is not None and med <= 120, f"median first move {med} ms over {len(fm)} legs")
    result["match"] = bool(result["checks"]) and all(x["ok"] for x in result["checks"])
    text = json.dumps(result, indent=2)
    if a.out:
        Path(a.out).write_text(text + "\n")
    print(text)
    return 0 if result["match"] else 1


if __name__ == "__main__":
    sys.exit(main())

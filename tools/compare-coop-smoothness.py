#!/usr/bin/env python3
"""Measure how smoothly each client shows both players' movement (B51).

Usage: python3 tools/compare-coop-smoothness.py <session log dir> [--out <json>]

The session log dir is builds/logs/<task>/run-<run> from Run-CoopSession.ps1 with clients running the
smoothness walk (prepare-coop-session.py --smooth c1,c2; every session profile has motion_log = 1):
  cN/coop-motion-client.csv  one row per frame (CoopClientLink motion_row): the frame time, the camera and the own
                             actor as the last frame left them, the corrections applied since, the view's remaining
                             smoothing offset; the other player's actor as shown, where it was put, its buffer
                             margin (newest state time - render time) and the server clock's offset
  cN/coop-cN.log             the walk's legs (script-leg-start / script-leg lines, UTC ms)

For each moving leg (the first 400 ms and the last 100 ms left out: starting and stopping are not jitter) and
each stream (cam: the own camera; own: the own actor; remote: the other player as shown here), horizontal only
(the camera's bob is vertical):
  jerk     |change of the per-frame velocity| / frame time (m/s^2) over consecutive frames: steady motion is
           near 0, a pop or a stall is a spike; p50 / p95 / p99 and the spikes over 40 m/s^2 per second
  speed    the per-frame speed's coefficient of variation (std / mean) and its frames under half the median
           (stalls: a frame that showed no movement)
and per client: corrections per second while moving (and their mean size), the view's smoothing offset, the
remote's frames holding (the buffer ran dry) and its margin's p5, the clock offset's steps (frames where it
moved, and the largest), the frame time's p50 / p99.
No pass rules yet: the baseline decides them (B51).
"""

import argparse
import csv
import json
import math
import re
import sys
from pathlib import Path

LEG_START = re.compile(r"script-leg-start name=(\S+) bits=([0-9a-f]+)")
LEG_END = re.compile(r"script-leg name=(\S+) start_utc=(\d+) end_utc=(\d+)")
SPIKE = 40.0  # m/s^2
# A leg's normal speed (m/s, the native controller on open ground): a stream under 75 % of it was blocked (a wall,
# a slope, the other player) and is reported apart; contact is where the two simulations differ most.
NOMINAL = {"run": 2.7, "back": 1.5, "strafe-left": 2.7, "strafe-right": 2.7, "walk": 1.14, "sprint": 5.6,
           "run-turning": 2.6, "return": 2.7, "walk-return": 1.14, "sprint-return": 5.6, "circle": 2.6}


def blocked(leg_name, st):
    kind = re.sub(r"^other:", "", leg_name).rsplit("-", 1)[0]
    nominal = NOMINAL.get(kind)
    return bool(st and nominal and st["speed_median"] < 0.75 * nominal)


def pct(values, q):
    if not values:
        return None
    s = sorted(values)
    return s[min(len(s) - 1, int(q * (len(s) - 1) + 0.5))]


def legs_of(log: Path):
    bits, legs = {}, []
    for line in log.read_text(errors="replace").splitlines():
        m = LEG_START.search(line)
        if m:
            bits[m.group(1)] = int(m.group(2), 16)
            continue
        m = LEG_END.search(line)
        if m:
            legs.append((m.group(1), int(m.group(2)), int(m.group(3)), bits.get(m.group(1), 0)))
    return legs


def stream_stats(rows, xk, zk):
    """rows: consecutive frames of one leg window."""
    jerk, speeds = [], []
    for i in range(2, len(rows)):
        a, b, c = rows[i - 2], rows[i - 1], rows[i]
        dt1, dt2 = b["dt"], c["dt"]
        if dt1 <= 0 or dt2 <= 0:
            continue
        v1 = ((b[xk] - a[xk]) / dt1, (b[zk] - a[zk]) / dt1)
        v2 = ((c[xk] - b[xk]) / dt2, (c[zk] - b[zk]) / dt2)
        jerk.append(math.hypot(v2[0] - v1[0], v2[1] - v1[1]) / dt2)
        speeds.append(math.hypot(*v2))
    if len(speeds) < 5:
        return None
    mean = sum(speeds) / len(speeds)
    med = pct(speeds, 0.5)
    std = math.sqrt(sum((s - mean) ** 2 for s in speeds) / len(speeds))
    seconds = sum(r["dt"] for r in rows)
    return {
        "frames": len(speeds),
        "speed_median": round(med, 3),
        "speed_cv": round(std / mean, 3) if mean > 0.05 else None,
        "stall_frames": sum(1 for s in speeds if s < 0.5 * med) if med > 0.3 else None,
        "jerk_p50": round(pct(jerk, 0.5), 1),
        "jerk_p95": round(pct(jerk, 0.95), 1),
        "jerk_p99": round(pct(jerk, 0.99), 1),
        "spikes_per_s": round(sum(1 for j in jerk if j > SPIKE) / seconds, 2) if seconds > 0 else None,
    }


def merge(per_leg):
    """Frame-weighted summary of one stream over all legs."""
    legs = [s for s in per_leg if s]
    if not legs:
        return None
    frames = sum(s["frames"] for s in legs)
    out = {"frames": frames}
    for k in ("jerk_p50", "jerk_p95", "jerk_p99", "spikes_per_s", "speed_cv"):
        vals = [(s[k], s["frames"]) for s in legs if s[k] is not None]
        if vals:
            out[k] = round(sum(v * f for v, f in vals) / sum(f for _, f in vals), 2)
    out["stall_frames"] = sum(s["stall_frames"] or 0 for s in legs)
    return out


def client(dir_: Path, other_legs):
    motion = dir_ / "coop-motion-client.csv"
    logs = sorted(dir_.glob("coop-c*.log"))
    if not motion.exists() or not logs:
        return None
    legs = legs_of(logs[0])
    rows = []
    with motion.open(newline="") as f:
        for r in csv.DictReader(f):
            try:
                rows.append({
                    "utc": int(r["utc_ms"]), "dt": float(r["dt_ms"]) / 1000.0,
                    "cam_x": float(r["cam_x"]), "cam_z": float(r["cam_z"]),
                    "own_x": float(r["own_x"]), "own_z": float(r["own_z"]),
                    "corr": float(r["corr"]), "view_off": float(r["view_off"]),
                    "remote": int(r["remote"]), "r_x": float(r["r_x"]), "r_z": float(r["r_z"]),
                    "t_x": float(r["t_x"]), "t_z": float(r["t_z"]),
                    "margin": float(r["margin_ms"]), "holding": int(r["holding"]),
                    "clock": float(r["clock_off"]),
                })
            except (KeyError, ValueError):
                continue
    if not rows:
        return None
    result = {"rows": len(rows), "legs": []}
    moving_rows = []
    streams = {"cam": [], "own": [], "remote": []}
    for name, start, end, bits in legs:
        if not bits or name.startswith("separate"):
            continue
        window = [r for r in rows if start + 400 <= r["utc"] <= end - 100]
        if len(window) < 10:
            continue
        moving_rows += window
        leg = {"name": name}
        for key, (xk, zk) in (("cam", ("cam_x", "cam_z")), ("own", ("own_x", "own_z"))):
            st = stream_stats(window, xk, zk)
            leg[key] = st
            if blocked(name, st):
                result.setdefault("blocked", []).append(f"{key}:{name}")
            else:
                streams[key].append(st)
        result["legs"].append(leg)
    # the other player as shown here, over the other player's own moving legs
    remote_rows = []
    for name, start, end, bits in other_legs:
        if not bits or name.startswith("separate"):
            continue
        window = [r for r in rows if start + 400 + 100 <= r["utc"] <= end - 100 + 100 and r["remote"] >= 0]
        if len(window) < 10:
            continue
        remote_rows += window
        st = stream_stats(window, "r_x", "r_z")
        result["legs"].append({"name": "other:" + name, "remote": st})
        if blocked(name, st):
            result.setdefault("blocked", []).append(f"remote:{name}")
        else:
            streams["remote"].append(st)
    result["summary"] = {k: merge(v) for k, v in streams.items()}
    if moving_rows:
        seconds = sum(r["dt"] for r in moving_rows)
        corr = [r["corr"] for r in moving_rows if r["corr"] > 0]
        remote = remote_rows
        steps = [abs(rows[i]["clock"] - rows[i - 1]["clock"]) for i in range(1, len(rows))]
        result["own"] = {
            "corrections_per_s": round(len(corr) / seconds, 2) if seconds else None,
            "correction_mean_m": round(sum(corr) / len(corr), 4) if corr else 0,
            "correction_p95_m": pct(corr, 0.95),
            "view_off_p95_m": pct([r["view_off"] for r in moving_rows], 0.95),
        }
        result["remote"] = {
            "frames": len(remote),
            "holding_frac": round(sum(r["holding"] for r in remote) / len(remote), 3) if remote else None,
            "margin_p5_ms": pct([r["margin"] for r in remote], 0.05),
            "shown_vs_target_max_m": round(max((math.hypot(r["r_x"] - r["t_x"], r["r_z"] - r["t_z"]) for r in remote),
                                               default=0), 4),
        }
        result["clock"] = {"steps": sum(1 for s in steps if s > 0.01), "largest_ms": round(max(steps, default=0), 2)}
        dts = [r["dt"] * 1000 for r in moving_rows]
        result["frame_ms"] = {"p50": round(pct(dts, 0.5), 1), "p99": round(pct(dts, 0.99), 1)}
    return result


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out")
    a = ap.parse_args()
    root = Path(a.logs)
    report = {}
    dirs = sorted(p for p in root.iterdir() if p.is_dir() and re.fullmatch(r"c\d+", p.name))
    legs = {}
    for d in dirs:
        found = sorted(d.glob("coop-c*.log"))
        legs[d.name] = legs_of(found[0]) if found else []
    for d in dirs:
        other = [leg for n, ls in legs.items() if n != d.name for leg in ls]
        r = client(d, other)
        if r:
            report[d.name] = r
    if not report:
        print("no client motion logs with legs", file=sys.stderr)
        return 1
    for name, r in report.items():
        print(f"== {name}: {r['rows']} frames, {len(r['legs'])} moving legs, frame ms {r.get('frame_ms')}")
        for k, s in r["summary"].items():
            print(f"  {k:6} {s}")
        print(f"  own    {r.get('own')}")
        print(f"  remote {r.get('remote')}")
        print(f"  clock  {r.get('clock')}")
        print(f"  blocked (left out above) {r.get('blocked', [])}")
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1))
    return 0


if __name__ == "__main__":
    sys.exit(main())

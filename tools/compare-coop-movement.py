"""Measure what the server did with each leg of the S06-T01 movement
validation program and apply the task's acceptance rules.

Usage: python3 tools/compare-coop-movement.py <session log dir> [--out <json>]
                                               [--lag-ms 150]

The session log dir is builds/logs/<task>/run-<run> from Run-CoopSession.ps1
with clients running the program (prepare-coop-session.py --movement):
  server/coop-actors.csv       the server's actor positions (every snapshot)
  server/coop-server.log       status lines with the per-player input counters
  cN/coop-cN.log               the program's legs (script-leg lines, UTC ms),
                               the teleport result and the forgery close
  cN/coop-world-final.csv      engine id -> entity id on client N
  cN/coop-actors-client.csv    what client N showed (local flag)

For every leg the own actor's displacement on the server between the leg's
start and its end plus the lag (input -> tick -> snapshot -> CSV), its peak
speed over 250 ms windows, the other actor's displacement and the closest
the two actors came.

Rules (all must hold for match = true). Legs start where the previous one
ended, so slopes and vegetation change how far a leg gets; the rules test
that a forged stream cannot advance the actor faster or further, not that
every leg covers the same ground (move-2: a baseline-speed leg was slowed by
a slope and settled 0.17 m afterwards).
  A       the baseline legs moved the actor (away >= 1.0 m, toward >= 0.8 m)
  B, C    duplicates (x4) and a shifted counter (+7/sample, one +100000 jump):
          peak speed <= 1.15 x the same-direction A leg's, displacement no more
          than that speed limit allows over the leg (1.15 x A's peak x (leg +
          lag)), and >= 0.5 x A's displacement (the stream still worked); the
          server refused >= 1 ahead input. (move-6: a slope shortened an A leg
          to 2.47 m, so a displacement ratio against A measured the terrain.)
  D       a replay of A's batches while standing: no movement input applied
          by the server in the leg (its input-apply lines) and <= 0.25 m
          (physics settling)
  F       crouched legs moved it, at most 70 % of A's distance
  G       the Prone stance: no movement input applied, <= 0.25 m, and the
          server counted invalid inputs
  I       the sprint at the other actor: the bodies never overlap (closest
          approach >= 0.4 m; the actor bodies are 0.30-0.35 m boxes), the
          other actor is not carried along (pushed <= 1.0 m), and the sprint
          is faster than A (peak > 1.15 x). The sprinter may glance off and
          slide past, as a player slides past an NPC. (Before patch 0031's
          touch fix the two actors pinned each other at about 1.5 m:
          move-1..5. A 0.5 m push limit, a guess with no single-player
          reference, failed at 0.61 m in move-7 with no overlap.)
  J       the teleport intent was Rejected, no movement input applied, <= 0.25 m
  K       the forged snapshot closed the connection (code 1003), no movement
          input applied, <= 0.25 m, and <= 0.25 m in the 5 s after
  presence both actors on every client, one local
"""

import argparse
import bisect
import csv
import json
import math
import re
import sys
from pathlib import Path


def load_server(path):
    tracks = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            tracks.setdefault(int(r["entity"]), []).append(
                (int(r["utc_ms"]), float(r["x"]), float(r["y"]), float(r["z"])))
    for t in tracks.values():
        t.sort()
    return tracks


def at(track, utc):
    times = [p[0] for p in track]
    i = bisect.bisect_left(times, utc)
    if i <= 0:
        return track[0][1:]
    if i >= len(track):
        return track[-1][1:]
    a, b = track[i - 1], track[i]
    k = (utc - a[0]) / max(1, b[0] - a[0])
    return tuple(a[j] + (b[j] - a[j]) * k for j in (1, 2, 3))


def dist(p, q):
    return math.dist(p, q)


def peak_speed(track, start, end, window=250):
    best = 0.0
    t = start
    while t + window <= end:
        best = max(best, dist(at(track, t), at(track, t + window)) / (window / 1000))
        t += 50
    return best


def closest(a, b, start, end):
    best = float("inf")
    t = start
    while t <= end:
        best = min(best, dist(at(a, t), at(b, t)))
        t += 50
    return best


def entity_map(world_csv):
    out = {}
    with open(world_csv) as f:
        f.readline()
        for r in csv.DictReader(f):
            out[int(r["id"])] = int(r["entity"])
    return out


def parse_client_log(path):
    legs, info = {}, {"slot": None, "teleport_outcome": None, "forged_close": None, "ahead_sent": 0,
                      "replay_batches": 0, "program_started": False}
    for line in open(path, errors="replace"):
        m = re.search(r"script-leg name=(\S+) start_utc=(\d+) end_utc=(\d+) ms=(\d+) mode=(\d+)", line)
        if m:
            legs[m.group(1)] = {"start_utc": int(m.group(2)), "end_utc": int(m.group(3)),
                                "ms": int(m.group(4)), "mode": int(m.group(5))}
        m = re.search(r"welcomed slot=(\d+)", line)
        if m:
            info["slot"] = int(m.group(1))
        m = re.search(r"script-teleport-result outcome=(\d+)", line)
        if m:
            info["teleport_outcome"] = int(m.group(1))
        m = re.search(r"script-forged-snapshot-closed code=(-?\d+)", line)
        if m:
            info["forged_close"] = int(m.group(1))
        if "script-ahead-sent" in line:
            info["ahead_sent"] += 1
        m = re.search(r"script-replay batches=(\d+)", line)
        if m:
            info["replay_batches"] = int(m.group(1))
        if "script2-program-start" in line:
            info["program_started"] = True
    return legs, info


def parse_server_log(path):
    """The last status line's counters: per slot inputs and the totals."""
    last = None
    for line in open(path, errors="replace"):
        if " status " in line or " final " in line:
            last = line
    out = {"slots": {}, "violations": None, "intents_refused": None, "rate_drops": None}
    if not last:
        return out
    for m in re.finditer(r"in(\d)=(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)/(\d+)", last):
        names = ("batches", "offered", "queued", "consumed", "held", "stale", "ahead", "invalid", "dropped_oldest",
                 "depth_peak")
        out["slots"][int(m.group(1))] = {name: int(m.group(i + 2)) for i, name in enumerate(names)}
    for m in re.finditer(r"mv(\d)=(\d+)/(\d+)", last):
        out["slots"].setdefault(int(m.group(1)), {})
        out["slots"][int(m.group(1))].update({"moving_ticks": int(m.group(2)), "refused": int(m.group(3))})
    for key, pattern in (("violations", r"violations=(\d+)"), ("intents_refused", r"intents_refused=(\d+)"),
                         ("rate_drops", r"rate_drops=(\d+)")):
        m = re.search(pattern, last)
        if m:
            out[key] = int(m.group(1))
    return out


def parse_applies(path):
    """Server input-apply lines (movement inputs applied, logged at most every
    500 ms per player while moving): slot -> sorted UTC ms."""
    out = {}
    for line in open(path, errors="replace"):
        m = re.match(r"(\d+) input-apply slot=(\d+)", line)
        if m:
            out.setdefault(int(m.group(2)), []).append(int(m.group(1)))
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("session")
    parser.add_argument("--lag-ms", type=int, default=150)
    parser.add_argument("--out")
    args = parser.parse_args()
    root = Path(args.session)
    server = load_server(root / "server/coop-actors.csv")
    counters = parse_server_log(root / "server/coop-server.log")
    applies = parse_applies(root / "server/coop-server.log")
    clients = sorted(p.name for p in root.iterdir() if p.is_dir() and re.fullmatch(r"c\d", p.name))
    result = {"lag_ms": args.lag_ms, "server": counters, "clients": {}, "checks": []}
    lag = args.lag_ms

    def check(name, ok, detail):
        result["checks"].append({"check": name, "ok": bool(ok), "detail": detail})

    for c in clients:
        ids = entity_map(root / c / "coop-world-final.csv")
        rows = list(csv.DictReader(open(root / c / "coop-actors-client.csv", newline="")))
        local = {int(r["id"]) for r in rows if r["local"] == "1"}
        remote = {int(r["id"]) for r in rows if r["local"] == "0"}
        legs, info = parse_client_log(root / c / f"coop-{c}.log")
        own = ids.get(next(iter(local))) if len(local) == 1 else None
        others = [e for e in server if e != own]
        other = others[0] if others else None
        measured = {}
        for name, leg in legs.items():
            if own not in server:
                continue
            start, end = leg["start_utc"], leg["end_utc"] + lag
            p0, p1 = at(server[own], start), at(server[own], end)
            m = {"displacement_m": round(dist(p0, p1), 3), "ms": leg["ms"], "mode": leg["mode"],
                 "peak_speed_mps": round(peak_speed(server[own], start, end), 3)}
            if other in server:
                m["other_moved_m"] = round(dist(at(server[other], start), at(server[other], end)), 3)
                m["start_distance_m"] = round(dist(p0, at(server[other], start)), 3)
                m["closest_m"] = round(closest(server[own], server[other], start, end), 3)
            slot_applies = applies.get(info["slot"], [])
            m["moving_inputs_applied_logged"] = sum(1 for t in slot_applies if start <= t <= end)
            measured[name] = m
        slot_counters = counters["slots"].get(info["slot"], {}) if info["slot"] is not None else {}
        result["clients"][c] = {"slot": info["slot"], "own_entity": own, "other_entity": other,
                                "presence_ok": len(local) == 1 and len(remote) >= 1,
                                "program_started": info["program_started"], "legs": measured,
                                "teleport_outcome": info["teleport_outcome"], "forged_close": info["forged_close"],
                                "ahead_sent": info["ahead_sent"], "replay_batches": info["replay_batches"],
                                "server_counters": slot_counters}
        check(f"{c} presence", len(local) == 1 and len(remote) >= 1, f"local={sorted(local)} remote={sorted(remote)}")
        if not info["program_started"]:
            check(f"{c} program", False, "the program never started (no other actor seen?)")
            continue

        def d(name, legs_measured=measured):
            return legs_measured.get(name, {}).get("displacement_m")

        def bounded(leg, base, legs_measured=measured):
            x, y = legs_measured.get(leg), legs_measured.get(base)
            if not x or not y or y["displacement_m"] <= 0:
                return False
            reach = 1.15 * y["peak_speed_mps"] * (x["ms"] + lag) / 1000
            return (x["peak_speed_mps"] <= 1.15 * y["peak_speed_mps"]
                    and 0.5 * y["displacement_m"] <= x["displacement_m"] <= reach)

        def still(leg, limit=0.25, legs_measured=measured):
            x = legs_measured.get(leg)
            return bool(x) and x["moving_inputs_applied_logged"] == 0 and x["displacement_m"] <= limit

        def show(leg, legs_measured=measured):
            x = legs_measured.get(leg, {})
            return (f"{leg}: {x.get('displacement_m')} m peak {x.get('peak_speed_mps')} m/s "
                    f"applied {x.get('moving_inputs_applied_logged')}")

        check(f"{c} A moved", d("A-away") is not None and d("A-away") >= 1.0 and d("A-toward") is not None
              and d("A-toward") >= 0.8, f"away={d('A-away')} toward={d('A-toward')}")
        check(f"{c} B duplicates no faster or further than A",
              bounded("B-away", "A-away") and bounded("B-toward", "A-toward"),
              "; ".join(show(x) for x in ("A-away", "B-away", "A-toward", "B-toward")))
        check(f"{c} C shifted counter no faster or further than A",
              bounded("C-away", "A-away") and bounded("C-toward", "A-toward"),
              "; ".join(show(x) for x in ("C-away", "C-toward")))
        check(f"{c} C jump refused", info["ahead_sent"] >= 1 and slot_counters.get("ahead", 0) >= 1,
              f"sent={info['ahead_sent']} server_ahead={slot_counters.get('ahead')}")
        check(f"{c} D replay applied no movement",
              info["replay_batches"] >= 20 and still("D-stale"),
              f"replayed={info['replay_batches']} {show('D-stale')}")
        check(f"{c} F crouch slower",
              d("F-away") is not None and d("A-away") and 0.2 <= d("F-away") <= 0.7 * d("A-away"),
              f"F away={d('F-away')} A away={d('A-away')}")
        check(f"{c} G invalid stance refused",
              still("G-invalid") and slot_counters.get("invalid", 0) >= 50,
              f"{show('G-invalid')} server_invalid={slot_counters.get('invalid')}")
        i = measured.get("I-sprint-toward", {})
        a = measured.get("A-away", {})
        check(f"{c} I sprint collides with the other actor",
              i and i.get("closest_m", 0) >= 0.4 and i["peak_speed_mps"] > 1.15 * a.get("peak_speed_mps", 1e9)
              and i.get("other_moved_m", 1e9) <= 1.0,
              f"moved={i.get('displacement_m')} start_distance={i.get('start_distance_m')} "
              f"closest={i.get('closest_m')} peak={i.get('peak_speed_mps')} A_peak={a.get('peak_speed_mps')} "
              f"other_moved={i.get('other_moved_m')}")
        check(f"{c} J teleport rejected",
              info["teleport_outcome"] == 2 and still("J-teleport"),
              f"outcome={info['teleport_outcome']} {show('J-teleport')}")
        k = legs.get("K-forged-snapshot")
        stayed = None
        if k and own in server:
            stayed = round(dist(at(server[own], k["end_utc"]), at(server[own], k["end_utc"] + 5000)), 3)
        check(f"{c} K forged snapshot closed the connection",
              info["forged_close"] == 1003 and still("K-forged-snapshot") and stayed is not None and stayed <= 0.25,
              f"close={info['forged_close']} {show('K-forged-snapshot')} moved_after_5s={stayed}")
    check("server refused the intents", (counters["intents_refused"] or 0) >= len(clients),
          f"intents_refused={counters['intents_refused']}")
    check("server counted the violations", (counters["violations"] or 0) >= len(clients),
          f"violations={counters['violations']}")
    result["match"] = all(ch["ok"] for ch in result["checks"])
    text = json.dumps(result, indent=2)
    if args.out:
        Path(args.out).write_text(text + "\n")
    print(text)
    return 0 if result["match"] else 1


if __name__ == "__main__":
    sys.exit(main())

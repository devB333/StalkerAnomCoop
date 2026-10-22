#!/usr/bin/env python3
"""Measure the S06-T08 storm and emission visuals: the server's environment,
thunderbolts and script presentation against what each client showed.

Usage: python3 tools/compare-coop-weather.py <session logs> [--out compare-weather.json]
           [--lag-ms 0]

Inputs:
  server/coop-weather.csv        every second: cycle, weather, weather effect
                                 (wfx) and its time left, the two blended
                                 descriptors (cycle@exec_time), blend weight,
                                 rain density, thunderbolt collection, bolt and
                                 presentation counters
  server/coop-server.log         fixture-storm / fixture-surge, environment
                                 changes, "bolt n=" lines, "present slot=" lines
  <client>/coop-weather-client.csv  the same every second as the client shows
                                 it, plus camera effector 2, post-process 1,
                                 playing sounds and particles
  <client>/coop-c*.log           environment wfx-start/stop, "bolt n=",
                                 "present serial=... applied|absent|missing",
                                 screenshots

Checks (each client; --lag-ms widens the delays by twice the declared lag):
  storm_forced       the server forced w_storm1 and the client's weather was
                     w_storm1 within 1,500 ms
  descriptors        outside weather effects, the client's cycle, weather and
                     both blended descriptors equal the server's nearest row
                     (within 600 ms) in at least 97 % of its rows after the
                     first 5 s, changes excepted (1.5 s around each)
  blend              on those rows |weight| differs by at most 0.02 and rain
                     density by at most 0.05 at the 95th percentile
  wfx_phase          during the server's weather effect the client plays the
                     same effect, started within 1,000 ms of the server, and
                     its time left (moved by the row gap at the time factor)
                     differs by at most 3.5 game seconds at the 95th percentile
                     (the clients' game clock may sit 3 s from the server's,
                     S05-T03);
                     it ends within 3,000 ms of the server's end
  bolts              every server bolt while the client was in the world
                     reached it with the same entry within 600 ms, none
                     without one, and there was at least one bolt
  presentation       every record the server sent this client arrived within
                     1,000 ms, and at least 95 % of the visual ones applied (a
                     factor, removal, volume or move for an effector, sound or
                     particles the client has not got counts as applied: the
                     server has none either; news, dialogs and game over are
                     not this task's)
  effectors          the client's camera effector 2 and post-process 1 state
                     follow the server's adds and removes on at least 95 % of
                     the seconds the emission ran
  sounds             the client applied at least one sound record, none was
                     missing, and a sound played at some point
  screenshots        at least 4 screenshots per client
"""

import argparse
import bisect
import csv
import json
import re
import sys
from pathlib import Path

RE_LINE = re.compile(r"^(\d+) (.*)$")


def read_log(path):
    lines = []
    if not path.exists():
        return lines
    with open(path, encoding="utf-8", errors="replace") as handle:
        for raw in handle:
            m = RE_LINE.match(raw.rstrip("\n"))
            if m:
                lines.append((int(m.group(1)), m.group(2)))
    return lines


def read_csv(path):
    if not path.exists():
        return []
    with open(path, newline="", encoding="utf-8", errors="replace") as handle:
        return list(csv.DictReader(handle))


def kv(text):
    out = {}
    for token in text.split(" "):
        if "=" in token:
            k, v = token.split("=", 1)
            out[k] = v
    return out


def fnum(value, default=-1.0):
    try:
        return float(value)
    except (TypeError, ValueError):
        return default


def percentile(values, p):
    if not values:
        return 0.0
    ordered = sorted(values)
    index = min(len(ordered) - 1, int(round(p * (len(ordered) - 1))))
    return ordered[index]


class Rows:
    def __init__(self, rows):
        self.rows = rows
        self.times = [int(r["utc_ms"]) for r in rows]

    def nearest(self, utc, within):
        if not self.rows:
            return None
        i = bisect.bisect_left(self.times, utc)
        best = None
        for j in (i - 1, i):
            if 0 <= j < len(self.rows):
                gap = abs(self.times[j] - utc)
                if gap <= within and (best is None or gap < best[0]):
                    best = (gap, self.rows[j])
        return best[1] if best else None


def check_client(name, logs, server_rows, server_log, lag_ms):
    slack = 2 * lag_ms
    client_dir = logs / name
    rows = read_csv(client_dir / "coop-weather-client.csv")
    log = read_log(client_dir / ("coop-" + name + ".log"))
    checks = {}
    if not rows:
        return {"error": "no coop-weather-client.csv"}
    live_from = int(rows[0]["utc_ms"]) + 5000
    server = Rows(server_rows)

    # storm_forced
    storm = [t for t, text in server_log if text.startswith("fixture-storm ")]
    client_storm = [int(r["utc_ms"]) for r in rows if r["weather"] == "w_storm1"]
    # from the forcing, or from the client's first live row if it joined later
    delay = (client_storm[0] - max(storm[0], int(rows[0]["utc_ms"]))) if storm and client_storm else None
    checks["storm_forced"] = {
        "ok": bool(storm) and delay is not None and delay <= 1500 + slack,
        "details": {"server_forced_utc": storm[0] if storm else None,
                    "client_first_utc": client_storm[0] if client_storm else None, "delay_ms": delay},
    }

    # server change times (descriptor or wfx changes) to except
    changes = [t for t, text in server_log if text.startswith("environment ")]

    def near_change(utc):
        i = bisect.bisect_left(changes, utc - 1500)
        return i < len(changes) and changes[i] <= utc + 1500

    # descriptors and blend
    compared = mismatched = 0
    weight_diffs, rain_diffs = [], []
    mismatch_samples = []
    for r in rows:
        utc = int(r["utc_ms"])
        if utc < live_from or near_change(utc):
            continue
        s = server.nearest(utc, 600)
        if not s or s["wfx"] == "1" or r["wfx"] == "1":
            continue
        compared += 1
        same = (r["cycle"] == s["cycle"] and r["weather"] == s["weather"] and r["c0"] == s["c0"]
                and r["c1"] == s["c1"] and abs(fnum(r["c0_exec"]) - fnum(s["c0_exec"])) < 0.05
                and abs(fnum(r["c1_exec"]) - fnum(s["c1_exec"])) < 0.05)
        if not same:
            mismatched += 1
            if len(mismatch_samples) < 5:
                keys = ("cycle", "weather", "c0", "c0_exec", "c1", "c1_exec")
                mismatch_samples.append({"utc": utc, "client": [r[k] for k in keys], "server": [s[k] for k in keys]})
            continue
        weight_diffs.append(abs(fnum(r["weight"]) - fnum(s["weight"])))
        rain_diffs.append(abs(fnum(r["rain"]) - fnum(s["rain"])))
    ratio = (compared - mismatched) / compared if compared else 0.0
    checks["descriptors"] = {"ok": compared >= 20 and ratio >= 0.97,
                             "details": {"compared": compared, "mismatched": mismatched, "ratio": round(ratio, 4),
                                         "samples": mismatch_samples}}
    w95 = percentile(weight_diffs, 0.95)
    r95 = percentile(rain_diffs, 0.95)
    checks["blend"] = {"ok": bool(weight_diffs) and w95 <= 0.02 and r95 <= 0.05,
                       "details": {"rows": len(weight_diffs), "weight_p95": round(w95, 4),
                                   "weight_max": round(max(weight_diffs), 4) if weight_diffs else None,
                                   "rain_p95": round(r95, 4)}}

    # wfx phase
    server_wfx_rows = [s for s in server_rows if s["wfx"] == "1"]
    client_wfx_rows = [r for r in rows if r["wfx"] == "1"]
    wfx_details = {"server_rows": len(server_wfx_rows), "client_rows": len(client_wfx_rows)}
    ok = True
    if server_wfx_rows:
        server_start = next((t for t, text in server_log if text.startswith("environment ") and "wfx=1" in text), None)
        client_start = next((t for t, text in log if text.startswith("environment wfx-start")), None)
        start_delay = (client_start - server_start) if server_start and client_start else None
        wfx_details["start_delay_ms"] = start_delay
        ok = ok and start_delay is not None and start_delay <= 1000 + slack
        server_names = {s["weather"] for s in server_wfx_rows}
        client_names = {r["weather"] for r in client_wfx_rows}
        wfx_details["server_names"] = sorted(server_names)
        wfx_details["client_names"] = sorted(client_names)
        ok = ok and server_names == client_names
        time_diffs = []
        for r in client_wfx_rows:
            utc = int(r["utc_ms"])
            s = server.nearest(utc, 600)
            if not s or s["wfx"] != "1" or s["weather"] != r["weather"]:
                continue
            factor = fnum(s["factor"], 1.0)
            expected = fnum(s["wfx_time"]) - (utc - int(s["utc_ms"])) / 1000.0 * factor
            time_diffs.append(abs(fnum(r["wfx_time"]) - expected))
        t95 = percentile(time_diffs, 0.95)
        wfx_details["time_rows"] = len(time_diffs)
        wfx_details["time_p95"] = round(t95, 3)
        ok = ok and len(time_diffs) >= 5 and t95 <= 3.5
        # end
        server_end = None
        seen = False
        for s in server_rows:
            if s["wfx"] == "1":
                seen = True
            elif seen:
                server_end = int(s["utc_ms"])
                break
        client_end = None
        seen = False
        for r in rows:
            if r["wfx"] == "1":
                seen = True
            elif seen:
                client_end = int(r["utc_ms"])
                break
        wfx_details["server_end_utc"] = server_end
        wfx_details["client_end_utc"] = client_end
        if server_end:
            ok = ok and client_end is not None and abs(client_end - server_end) <= 3000 + slack
    else:
        ok = False
        wfx_details["note"] = "the server played no weather effect"
    checks["wfx_phase"] = {"ok": ok, "details": wfx_details}

    # bolts, while the client was in the world (its weather rows)
    first_row, last_row = int(rows[0]["utc_ms"]), int(rows[-1]["utc_ms"])
    all_server_bolts = [(t, kv(text)) for t, text in server_log if text.startswith("bolt n=")]
    server_bolts = [(t, b) for t, b in all_server_bolts if first_row <= t <= last_row]
    client_bolts = [(t, kv(text)) for t, text in log if text.startswith("bolt n=")]
    matched = 0
    worst = 0
    unmatched_client = len(client_bolts)
    used = set()
    # a client bolt is accounted for by any server bolt (one just outside the
    # client's rows can still arrive)
    for t, b in all_server_bolts:
        for i, (ct, cb) in enumerate(client_bolts):
            if i in used or cb.get("index") != b.get("index") or cb.get("id") != b.get("id"):
                continue
            if 0 <= ct - t <= 600 + slack:
                used.add(i)
                unmatched_client -= 1
                if first_row <= t <= last_row:
                    matched += 1
                    worst = max(worst, ct - t)
                break
    checks["bolts"] = {"ok": len(server_bolts) >= 1 and matched == len(server_bolts) and unmatched_client == 0,
                       "details": {"server": len(server_bolts), "matched": matched,
                                   "client_unmatched": unmatched_client, "worst_delay_ms": worst}}

    # presentation delivery
    slot = None
    for _t, text in log:
        if text.startswith("welcomed "):
            slot = kv(text).get("slot")
            break
    sent = [(t, kv(text)) for t, text in server_log
            if text.startswith("present slot=") and kv(text).get("slot") == slot]
    received = {}
    for utc, text in log:
        if text.startswith("present serial="):
            received[kv(text).get("serial")] = (utc, text.rsplit(" ", 1)[-1])
    # Kinds this task shows; news, dialogs, game over and tutorials stay L05.
    visual = {"camera-effector", "camera-effector-remove", "post-process", "post-process-remove",
              "post-process-factor", "sound", "sound-at-pos", "sound-stop", "sound-stop-deferred", "sound-volume",
              "sound-position", "particles", "particles-move", "particles-stop", "particles-stop-deferred"}
    # A factor or removal for an effector the client has not got is what the
    # server does too (the scripts set the psychic factor every second).
    absent_ok = {"post-process-factor", "post-process-remove", "camera-effector-remove", "sound-stop",
                 "sound-stop-deferred", "sound-volume", "sound-position", "particles-stop",
                 "particles-stop-deferred", "particles-move"}
    delivered = applied = judged = 0
    worst = 0
    outcomes = {}
    missing_samples = []
    for t, d in sent:
        got = received.get(d.get("serial"))
        if got and 0 <= got[0] - t <= 1000 + slack:
            delivered += 1
            worst = max(worst, got[0] - t)
            outcomes[got[1]] = outcomes.get(got[1], 0) + 1
            if d.get("kind") in visual:
                judged += 1
                if got[1] == "applied" or (got[1] == "absent" and d.get("kind") in absent_ok):
                    applied += 1
        elif len(missing_samples) < 5:
            missing_samples.append({"utc": t, "serial": d.get("serial"), "kind": d.get("kind"),
                                    "detail": d.get("detail")})
    checks["presentation"] = {
        "ok": len(sent) >= 1 and delivered == len(sent) and judged >= 1 and applied >= 0.95 * judged,
        "details": {"slot": slot, "sent": len(sent), "delivered": delivered, "visual": judged, "applied": applied,
                    "outcomes": outcomes, "worst_delay_ms": worst, "missing": missing_samples},
    }

    # effectors in step with the server's adds and removes during the emission
    def server_state(kind_add, kind_remove, object_id):
        events = []
        for t, d in sent:
            if d.get("object") == object_id and d.get("kind") == kind_add:
                events.append((t, True))
            elif d.get("object") == object_id and d.get("kind") == kind_remove:
                events.append((t, False))
        return events

    cam_events = server_state("camera-effector", "camera-effector-remove", "2")
    pp_events = server_state("post-process", "post-process-remove", "1")

    def expected(events, utc):
        state = False
        for t, on in events:
            if t <= utc - 1500:
                state = on
            elif t <= utc + 1500:
                return None  # a change this second: not judged
        return state

    judged = agree = 0
    first = min([t for t, _ in cam_events + pp_events], default=None)
    last = max([t for t, _ in cam_events + pp_events], default=None)
    for r in rows:
        utc = int(r["utc_ms"])
        if first is None or utc < first or utc > last + 5000:
            continue
        for events, column in ((cam_events, "cam2"), (pp_events, "pp1")):
            want = expected(events, utc)
            if want is None:
                continue
            have = r[column] == "1" if column == "cam2" else fnum(r[column]) >= 0.0
            judged += 1
            agree += 1 if have == want else 0
    checks["effectors"] = {"ok": judged >= 20 and agree >= 0.95 * judged,
                           "details": {"judged": judged, "agree": agree, "cam_events": len(cam_events),
                                       "pp_events": len(pp_events)}}

    # sounds
    presents = [text for _t, text in log if text.startswith("present serial=")]
    sound_applied = sum(1 for text in presents if " kind=sound " in text and text.endswith(" applied"))
    sound_at_pos = sum(1 for text in presents if " kind=sound-at-pos " in text and text.endswith(" applied"))
    sound_missing = sum(1 for text in presents if "kind=sound" in text and text.endswith(" missing"))
    playing = max((int(r["sounds"]) for r in rows), default=0)
    checks["sounds"] = {"ok": (sound_applied + sound_at_pos) >= 1 and sound_missing == 0 and playing >= 1,
                        "details": {"applied": sound_applied, "applied_at_pos": sound_at_pos, "missing": sound_missing,
                                    "max_playing": playing}}

    shots = sum(1 for t, text in log if text.startswith("screenshot script5-shot"))
    checks["screenshots"] = {"ok": shots >= 4, "details": {"count": shots}}
    return checks


def main():
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("logs")
    parser.add_argument("--out", default="")
    parser.add_argument("--lag-ms", type=int, default=0)
    args = parser.parse_args()
    logs = Path(args.logs)
    server_rows = read_csv(logs / "server" / "coop-weather.csv")
    server_log = read_log(logs / "server" / "coop-server.log")
    if not server_rows:
        raise SystemExit(f"No server/coop-weather.csv under {logs}")
    clients = sorted(p.name for p in logs.iterdir() if p.is_dir() and p.name.startswith("c"))
    result = {"logs": str(logs), "lag_ms": args.lag_ms, "clients": {}, "server": {
        "rows": len(server_rows), "bolts": sum(1 for t, text in server_log if text.startswith("bolt n=")),
        "presented": sum(1 for t, text in server_log if text.startswith("present slot=")),
        "weathers": sorted({r["weather"] for r in server_rows}),
        "factor_max": max(fnum(r["factor"]) for r in server_rows),
    }}
    match = bool(clients)
    for name in clients:
        checks = check_client(name, logs, server_rows, server_log, args.lag_ms)
        result["clients"][name] = checks
        if "error" in checks or not all(c["ok"] for c in checks.values()):
            match = False
    result["match"] = match
    text = json.dumps(result, indent=2)
    if args.out:
        Path(args.out).write_text(text + "\n", encoding="utf-8")
    print(text)
    for name, checks in result["clients"].items():
        if "error" in checks:
            print(f"{name}: {checks['error']}", file=sys.stderr)
            continue
        failed = [k for k, c in checks.items() if not c["ok"]]
        summary = f"{name}: {len(checks) - len(failed)}/{len(checks)} checks pass"
        print(summary + (f"; failed: {', '.join(failed)}" if failed else ""), file=sys.stderr)
    return 0 if match else 1


if __name__ == "__main__":
    sys.exit(main())

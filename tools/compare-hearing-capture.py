"""Compare two S02-T05c hearing captures (campaign server and normal client).

Usage: python3 tools/compare-hearing-capture.py <server.csv> <client.csv>

Row kinds are documented in xrGame/CampaignHearingCapture.h. Rules:

* Both sides ran on the same level with the same stalker and the same range
  gates; the sound core is reported (server cpu, client openal) and the
  server must have no listener while the client has one.
* Both sides: at least one emitter started, at least one AI sound event
  reached the level and a hearer, at least one feel_sound_new call, the
  walked stalker produced at least one event while walking, and the end row
  carries ok=1.
* The server dropped no 3D event: its events_2d_skipped are reported, and its
  registered count equals its events count (every event it handed to the level
  reached the spatial query).
* Per-side totals (started, events, registered, delegates, dispatched,
  feel_calls, step events, the walked stalker's events) and the distinct sound
  files and game types behind the events are reported for both sides; the set
  of game types the walked stalker produced must overlap.

Prints a JSON summary and exits 1 on any asserted mismatch.
"""

import csv
import json
import sys

META_FIELDS = ["frame", "time_ms", "level", "core", "listener", "policy", "anchors", "step_gate_m", "impact_gate_m",
               "som", "snd_env", "npc", "npc_name", "px", "py", "pz", "hearers_50m", "started", "events",
               "events_2d_skipped", "registered", "delegates", "dispatched", "feel_calls"]
TICK_FIELDS = ["n", "phase", "frame", "time_ms", "emitters", "rendered", "started", "events", "events_2d_skipped",
               "registered", "delegates", "dispatched", "feel_calls", "npc_feel", "npc_sounds"]
EVENT_FIELDS = ["n", "phase", "frame", "file", "game_type", "source_id", "source_name", "px", "py", "pz", "range",
                "max_ai", "volume", "hearers", "max_power", "nearest_id", "nearest_distance"]
END_FIELDS = ["ticks", "sim_ms", "wall_ms", "started", "events", "events_2d_skipped", "registered", "delegates",
              "dispatched", "feel_calls", "npc_events", "npc_walk_events", "step_events", "npc_feel", "ok"]


def load(path):
    capture = {"ticks": [], "events": []}
    with open(path, newline="") as handle:
        for row in csv.reader(handle):
            if not row:
                continue
            kind, values = row[0], row[1:]
            if kind == "meta":
                capture["meta"] = dict(zip(META_FIELDS, values, strict=True))
            elif kind == "tick":
                capture["ticks"].append(dict(zip(TICK_FIELDS, values, strict=True)))
            elif kind == "event":
                capture["events"].append(dict(zip(EVENT_FIELDS, values, strict=True)))
            elif kind == "end":
                capture["end"] = dict(zip(END_FIELDS, values, strict=True))
    for key in ("meta", "end"):
        if key not in capture:
            raise SystemExit(f"{path}: no {key} row")
    return capture


def summarize(capture):
    meta, end, events = capture["meta"], capture["end"], capture["events"]
    npc = meta["npc"]
    npc_events = [e for e in events if e["source_id"] == npc]
    by_type = {}
    for e in events:
        by_type[e["game_type"]] = by_type.get(e["game_type"], 0) + 1
    return {
        "core": meta["core"],
        "listener": meta["listener"],
        "policy": meta["policy"],
        "hearers_50m": meta["hearers_50m"],
        "ticks": int(end["ticks"]),
        "sim_ms": int(end["sim_ms"]),
        "started": int(end["started"]),
        "events": int(end["events"]),
        "events_2d_skipped": int(end["events_2d_skipped"]),
        "registered": int(end["registered"]),
        "delegates": int(end["delegates"]),
        "dispatched": int(end["dispatched"]),
        "feel_calls": int(end["feel_calls"]),
        "step_events": int(end["step_events"]),
        "npc_events": int(end["npc_events"]),
        "npc_walk_events": int(end["npc_walk_events"]),
        "npc_feel": int(end["npc_feel"]),
        "events_with_hearers": sum(1 for e in events if int(e["hearers"]) > 0),
        "distinct_files": len({e["file"] for e in events}),
        "game_types": by_type,
        "npc_files": sorted({e["file"] for e in npc_events}),
        "npc_game_types": sorted({e["game_type"] for e in npc_events}),
        "max_hearers": max((int(e["hearers"]) for e in events), default=0),
        "emitters_peak": max((int(t["emitters"]) for t in capture["ticks"]), default=0),
        "rendered_peak": max((int(t["rendered"]) for t in capture["ticks"]), default=0),
    }


def side_rules(name, capture, failures):
    end = capture["end"]
    if capture["meta"]["core"] == "none":
        failures.append(f"{name}: no sound core")
    for field in ("started", "registered", "delegates", "dispatched", "feel_calls", "npc_walk_events"):
        if int(end[field]) < 1:
            failures.append(f"{name}: {field} is {end[field]}")
    if end["ok"] != "1":
        failures.append(f"{name}: end row ok={end['ok']}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    server, client = load(sys.argv[1]), load(sys.argv[2])
    failures = []
    summary = {"server": sys.argv[1], "client": sys.argv[2]}

    sm, cm = server["meta"], client["meta"]
    for field in ("level", "npc", "npc_name", "step_gate_m", "impact_gate_m", "som", "snd_env"):
        if sm[field] != cm[field]:
            failures.append(f"meta {field}: {sm[field]} vs {cm[field]}")
    if sm["listener"] != "0":
        failures.append(f"server has a listener ({sm['core']})")
    if cm["listener"] != "1":
        failures.append(f"client has no listener ({cm['core']})")
    if server["end"]["registered"] != server["end"]["events"]:
        failures.append(f"server registered {server['end']['registered']} of {server['end']['events']} events")

    side_rules("server", server, failures)
    side_rules("client", client, failures)
    summary["server_side"] = summarize(server)
    summary["client_side"] = summarize(client)
    overlap = set(summary["server_side"]["npc_game_types"]) & set(summary["client_side"]["npc_game_types"])
    if not overlap:
        failures.append("the walked stalker's event game types do not overlap")
    summary["npc_game_type_overlap"] = sorted(overlap)
    summary["failures"] = failures
    summary["match"] = not failures
    print(json.dumps(summary, indent=2))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()

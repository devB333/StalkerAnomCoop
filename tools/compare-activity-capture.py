"""Compare two S02-T04c activity captures (campaign server and normal client).

Usage: python3 tools/compare-activity-capture.py <server.csv> <client.csv>

Row kinds are documented in xrGame/CampaignActivityCapture.h. Rules:

* Both sides ran on the same level with the same stalker (ID, name, section,
  schedule bounds) and the same ALife parameters (switch, online and offline
  distances, ALife schedule bounds, objects per update, time factor); the
  activity policy flag is reported (server 1, client 0), not asserted.
* The level's ALife object, online and scheduled counts at the begin agree
  within 5 (scripts spawn a few extra objects per run).
* The stalker's start positions agree within 5 m (the client's AI runs in real
  time for a second before the capture); each side's destination is at least
  its requested distance from its own start, and both destinations are
  reported.
* Both sides: at least 10 scheduled updates of the stalker while observing, at
  least one planner action, at least one ALife switch and one offline scheduled
  advance, the walk commanded, the stalker arrived within 1 m, the walked path
  at least 0.8 of the distance from the walk command to the destination (the
  stalker may move under its own AI while observed), and the end row carries
  ok=1.
* ALife game time advanced at the configured factor on both sides (within 10 %
  of sim_ms * factor). Per-tick averages of object updates, crows and
  scheduler dispatches are reported for both sides.

Prints a JSON summary and exits 1 on any asserted mismatch.
"""

import csv
import json
import sys

COUNT_TOL = 5
START_TOL = 5.0
ARRIVAL_TOL = 1.0
MIN_OBSERVE_UPDATES = 10
WALK_FRACTION = 0.8
GAME_TIME_TOL = 0.1

META_FIELDS = ["frame", "time_ms", "level", "policy", "crow_radius", "anchors", "switch_distance",
               "online_distance", "offline_distance", "alife_t_min", "alife_t_max", "objects_per_update",
               "time_factor", "game_time", "level_alife_objects", "online", "scheduled", "observe_ms", "walk_ms",
               "distance"]
NPCMETA_FIELDS = ["id", "name", "section", "t_min", "t_max", "alive", "level_vertex", "px", "py", "pz",
                  "anchor_distance", "dest_vertex", "dx", "dy", "dz", "dest_distance"]
TICK_FIELDS = ["n", "phase", "frame", "time_ms", "game_time", "objects", "active", "crows", "updated",
               "scheduled_updates", "physics_steps", "online", "switched_online", "switched_offline",
               "scheduled_objects", "scheduled_advanced"]
SWITCH_FIELDS = ["n", "id", "name", "state", "actor_distance"]
NPC_FIELDS = ["n", "phase", "updates", "brain_action", "handler_action", "path_type", "dest_vertex", "movement",
              "body", "mental", "px", "py", "pz", "speed", "dest_distance", "queued_actions", "script_control"]
END_FIELDS = ["ticks", "sim_ms", "wall_ms", "game_time_advance_ms", "npc_updates_observe", "npc_updates_walk",
              "brain_actions", "switched_online", "switched_offline", "scheduled_advanced", "walk_start", "arrival",
              "travelled", "final_dest_distance", "ok", "walk_distance"]


def load(path):
    capture = {"ticks": [], "switches": [], "npc": []}
    with open(path, newline="") as handle:
        for row in csv.reader(handle):
            if not row:
                continue
            kind, values = row[0], row[1:]
            if kind == "meta":
                capture["meta"] = dict(zip(META_FIELDS, values, strict=True))
            elif kind == "npcmeta":
                capture["npcmeta"] = dict(zip(NPCMETA_FIELDS, values, strict=True))
            elif kind == "tick":
                capture["ticks"].append(dict(zip(TICK_FIELDS, values, strict=True)))
            elif kind == "switch":
                capture["switches"].append(dict(zip(SWITCH_FIELDS, values, strict=True)))
            elif kind == "npc":
                capture["npc"].append(dict(zip(NPC_FIELDS, values, strict=True)))
            elif kind == "end":
                # Captures before S02-T05c have no walk_distance; the walk then
                # started from the begin position.
                if len(values) == len(END_FIELDS) - 1:
                    values = values + [capture["npcmeta"]["dest_distance"]]
                capture["end"] = dict(zip(END_FIELDS, values, strict=True))
    for key in ("meta", "npcmeta", "end"):
        if key not in capture:
            raise SystemExit(f"{path}: no {key} row")
    return capture


def distance(a, b):
    return sum((float(x) - float(y)) ** 2 for x, y in zip(a, b, strict=True)) ** 0.5


def averages(capture):
    ticks = capture["ticks"]
    if not ticks:
        return {}
    count = len(ticks)
    return {
        "ticks": count,
        "objects_mean": sum(int(t["objects"]) for t in ticks) / count,
        "active_mean": sum(int(t["active"]) for t in ticks) / count,
        "crows_mean": sum(int(t["crows"]) for t in ticks) / count,
        "updated_mean": sum(int(t["updated"]) for t in ticks) / count,
        "scheduled_updates_mean": sum(int(t["scheduled_updates"]) for t in ticks) / count,
        "online_begin": int(ticks[0]["online"]),
        "online_end": int(ticks[-1]["online"]),
        "scheduled_objects_end": int(ticks[-1]["scheduled_objects"]),
        "physics_steps": int(ticks[-1]["physics_steps"]) - int(ticks[0]["physics_steps"]),
        "switches": [(s["n"], s["id"], s["name"], s["state"], s["actor_distance"]) for s in capture["switches"]],
        "brain_actions_seen": sorted({int(r["brain_action"]) for r in capture["npc"] if r["phase"] == "0"}),
        "dest_vertex_changes_in_walk": len({r["dest_vertex"] for r in capture["npc"] if r["phase"] == "1"}),
    }


def side_rules(name, capture, failures):
    meta, npc, end = capture["meta"], capture["npcmeta"], capture["end"]
    start = (npc["px"], npc["py"], npc["pz"])
    dest = (npc["dx"], npc["dy"], npc["dz"])
    if distance(start, dest) < float(meta["distance"]):
        failures.append(f"{name}: destination closer than {meta['distance']} m to the start")
    if int(end["npc_updates_observe"]) < MIN_OBSERVE_UPDATES:
        failures.append(f"{name}: {end['npc_updates_observe']} scheduled updates while observing")
    if int(end["brain_actions"]) < 1:
        failures.append(f"{name}: no planner action")
    if int(end["switched_online"]) + int(end["switched_offline"]) < 1:
        failures.append(f"{name}: no ALife switch")
    if int(end["scheduled_advanced"]) < 1:
        failures.append(f"{name}: no offline scheduled advance")
    if int(end["walk_start"]) < 0:
        failures.append(f"{name}: walk never commanded")
    if int(end["arrival"]) < 0 or float(end["final_dest_distance"]) >= ARRIVAL_TOL:
        failures.append(f"{name}: no arrival ({end['final_dest_distance']} m from the destination)")
    if float(end["travelled"]) < WALK_FRACTION * float(end["walk_distance"]):
        failures.append(f"{name}: walked {end['travelled']} m of {end['walk_distance']} m")
    expected = float(end["sim_ms"]) * float(meta["time_factor"])
    if expected > 0 and abs(float(end["game_time_advance_ms"]) - expected) / expected > GAME_TIME_TOL:
        failures.append(f"{name}: game time advanced {end['game_time_advance_ms']} ms, expected {expected:.0f}")
    if end["ok"] != "1":
        failures.append(f"{name}: end row ok={end['ok']}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    server, client = load(sys.argv[1]), load(sys.argv[2])
    failures = []
    summary = {"server": sys.argv[1], "client": sys.argv[2]}

    sm, cm = server["meta"], client["meta"]
    for field in ("level", "switch_distance", "online_distance", "offline_distance", "alife_t_min", "alife_t_max",
                  "objects_per_update", "time_factor", "observe_ms", "walk_ms", "distance"):
        if sm[field] != cm[field]:
            failures.append(f"meta {field}: {sm[field]} vs {cm[field]}")
    for field in ("level_alife_objects", "online", "scheduled"):
        if abs(int(sm[field]) - int(cm[field])) > COUNT_TOL:
            failures.append(f"meta {field}: {sm[field]} vs {cm[field]}")
    summary["policy"] = {"server": sm["policy"], "client": cm["policy"], "server_anchors": sm["anchors"],
                         "server_crow_radius": sm["crow_radius"]}
    summary["alife"] = {k: sm[k] for k in ("switch_distance", "online_distance", "offline_distance", "alife_t_min",
                                            "alife_t_max", "objects_per_update", "time_factor")}
    summary["counts_begin"] = {k: (sm[k], cm[k]) for k in ("level_alife_objects", "online", "scheduled")}

    sn, cn = server["npcmeta"], client["npcmeta"]
    for field in ("id", "name", "section", "t_min", "t_max"):
        if sn[field] != cn[field]:
            failures.append(f"npc {field}: {sn[field]} vs {cn[field]}")
    start_gap = distance((sn["px"], sn["py"], sn["pz"]), (cn["px"], cn["py"], cn["pz"]))
    if start_gap > START_TOL:
        failures.append(f"npc start positions {start_gap:.2f} m apart")
    summary["npc"] = {
        "id": sn["id"], "name": sn["name"], "section": sn["section"], "t_min": sn["t_min"], "t_max": sn["t_max"],
        "start_gap_m": round(start_gap, 3),
        "server_anchor_distance": sn["anchor_distance"],
        "dest_vertex": (sn["dest_vertex"], cn["dest_vertex"]),
        "dest_gap_m": round(distance((sn["dx"], sn["dy"], sn["dz"]), (cn["dx"], cn["dy"], cn["dz"])), 3),
        "dest_distance": (sn["dest_distance"], cn["dest_distance"]),
    }

    side_rules("server", server, failures)
    side_rules("client", client, failures)
    summary["end"] = {"server": server["end"], "client": client["end"]}
    summary["server_ticks"] = averages(server)
    summary["client_frames"] = averages(client)
    summary["failures"] = failures
    summary["match"] = not failures
    print(json.dumps(summary, indent=2))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()

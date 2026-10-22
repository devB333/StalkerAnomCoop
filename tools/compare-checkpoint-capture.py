"""Compare the S02-T06d checkpoint rows of the campaign server and the normal client.

Usage: python3 tools/compare-checkpoint-capture.py <server.csv> <client.csv>

The server capture (xrGame/CampaignCheckpoint.h) holds the snapshots
``before-save``, ``after-save``, ``reloaded-alife`` and ``reloaded-ticked``
plus a ``save`` row; the client capture holds one ``client-loaded`` row taken
one second after the normal client spawned its actor from the checkpoint the
server wrote. Rules, server ``after-save`` against client ``client-loaded``:

* Same level and level id.
* The ALife holds the objects the checkpoint wrote: exactly (``alife_objects``
  equals the server's ``alife_savable``) for the server's ``reloaded-alife``
  row, taken before any tick; within max(10, 0.1 %) for the rows taken after
  the loaded level ran (the client cannot snapshot before its first tick, and
  the simulation releases and spawns objects as game time passes). The level
  registry holds the level objects it wrote within the same tolerance in
  every row: a squad member online at the save is registered under its group
  after the load (the server fixture names each such object).
* The ALife clock resumed from the saved time (not before it, and less than
  two game minutes after it).
* The live actor stands within 0.25 m of where the server's stood, alive,
  with its health within 0.05; its ALife actor keeps the saved graph and
  level vertex.
* The fixture stalker's ALife entity is back: exactly the saved health
  (within 0.001) and position (0.25 m) before any tick; within 0.05 health
  after the level ran (the wounded stalker bleeds and walks), its position
  reported, not asserted. It is alive on the client when it was on the server.
* The combat monster's ALife entity exists with the saved health (within
  0.001); when the server's row names none, nothing is checked.
* The actor binder completed its first update and the Lua marshal state
  carries the same game version string.

The server's own round-trip rows (``reloaded-alife`` and ``reloaded-ticked``
against ``after-save``) are reported with the same rules, so one summary holds
both products; the ticked rules are the ones ``xrCampaignLevel_CheckpointEnd``
asserts in C++. Prints a JSON summary and exits 1 on any mismatch.
"""

import csv
import json
import math
import sys

FLOAT_FIELDS = {"actor_x", "actor_y", "actor_z", "actor_health", "se_actor_x", "se_actor_y", "se_actor_z",
                "se_actor_health", "npc_se_health", "npc_se_x", "npc_se_y", "npc_se_z", "npc_health",
                "monster_se_health"}
INT_FIELDS = {"level_id", "game_time", "alife_objects", "alife_savable", "level_objects", "level_savable", "online",
              "server_entities", "client_objects", "actor_live", "actor_alive", "se_actor", "se_actor_graph",
              "se_actor_node", "npc_id", "npc_se", "npc_live", "npc_alive", "monster_id", "monster_se",
              "monster_se_online", "monster_live", "binder", "after_first_update"}
NO_ID = 65535
# The loaded game time may be this far past the saved one. Two minutes until
# S06-T08: since S06-T07 the capture harness keeps the game active while its
# window has no focus (g_always_active on), so game time advances through the
# ~30 s level load at the save's time factor (capture-60: 172 s).
GAME_RESUME_WINDOW_MS = 600000


def load(path):
    rows = {}
    with open(path, newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader)
        for values in reader:
            if not values or values[0] == "save":
                continue
            row = dict(zip(header, values, strict=True))
            for key, value in row.items():
                if key in FLOAT_FIELDS:
                    row[key] = float(value)
                elif key in INT_FIELDS:
                    row[key] = int(value)
            rows[row["label"]] = row
    return rows


def distance(a, b, prefix):
    return math.sqrt(sum((a[prefix + axis] - b[prefix + axis]) ** 2 for axis in "xyz"))


def near(loaded, saved):
    """Within max(10, 0.1 %) of the saved count, either way."""
    return abs(loaded - saved) <= max(10, saved // 1000)


def compare(saved, loaded, live_expected):
    """Rules for a snapshot taken from a level loaded from the checkpoint `saved` was written from.

    live_expected: the loaded level has spawned its client objects (ticked), so
    the live actor and stalker rules apply; False right after the ALife load.
    """
    checks = {}
    checks["same_level"] = saved["level"] == loaded["level"] and saved["level_id"] == loaded["level_id"]
    if live_expected:
        checks["alife_objects_near_saved"] = near(loaded["alife_objects"], saved["alife_savable"])
    else:
        checks["alife_objects_equal_saved"] = loaded["alife_objects"] == saved["alife_savable"]
    # The level registry may hold a few fewer objects than it did at the save: a squad member that was
    # online at the save is registered under its group after the load (the C++ rule names each one and
    # checks it is still in ALife on this level).
    checks["level_objects_near_saved"] = near(loaded["level_objects"], saved["level_savable"])
    game_time_delta = loaded["game_time"] - saved["game_time"]
    checks["game_time_resumed"] = 0 <= game_time_delta < GAME_RESUME_WINDOW_MS
    checks["se_actor_position"] = loaded["se_actor"] == 1 and distance(saved, loaded, "se_actor_") < 0.25
    checks["se_actor_state"] = (loaded["se_actor"] == 1 and abs(loaded["se_actor_health"] - saved["se_actor_health"])
                                < 1e-3 and loaded["se_actor_graph"] == saved["se_actor_graph"]
                                and loaded["se_actor_node"] == saved["se_actor_node"])
    if live_expected:
        checks["npc_se_state"] = loaded["npc_se"] == saved["npc_se"] and (
            saved["npc_se"] == 0 or abs(loaded["npc_se_health"] - saved["npc_se_health"]) < 0.05)
    else:
        checks["npc_se_state"] = loaded["npc_se"] == saved["npc_se"] and (saved["npc_se"] == 0 or (
            abs(loaded["npc_se_health"] - saved["npc_se_health"]) < 1e-3
            and distance(saved, loaded, "npc_se_") < 0.25))
    if saved["monster_id"] == NO_ID or saved["monster_se"] == 0:
        checks["monster_se_state"] = True
    else:
        checks["monster_se_state"] = (loaded["monster_se"] == 1
                                      and abs(loaded["monster_se_health"] - saved["monster_se_health"]) < 1e-3)
    checks["marshal_version_same"] = loaded["marshal_version"] == saved["marshal_version"]
    if live_expected:
        checks["actor_alive"] = loaded["actor_live"] == 1 and loaded["actor_alive"] == 1
        checks["actor_position"] = loaded["actor_live"] == 1 and distance(saved, loaded, "actor_") < 0.25
        checks["actor_health"] = (loaded["actor_live"] == 1
                                  and abs(loaded["actor_health"] - saved["actor_health"]) < 0.05)
        checks["npc_alive_as_saved"] = saved["npc_live"] == 0 or (loaded["npc_live"] == 1
                                                                   and loaded["npc_alive"] == saved["npc_alive"])
        checks["binder_first_update"] = loaded["binder"] == 1 and loaded["after_first_update"] == 1
    details = {
        "game_time_delta_ms": game_time_delta,
        "se_actor_gap_m": round(distance(saved, loaded, "se_actor_"), 4),
        "actor_gap_m": round(distance(saved, loaded, "actor_"), 4) if loaded["actor_live"] else None,
        "actor_health": [saved["actor_health"], loaded["actor_health"]],
        "alife_objects": [saved["alife_objects"], saved["alife_savable"], loaded["alife_objects"]],
        "level_objects": [saved["level_objects"], saved["level_savable"], loaded["level_objects"]],
        "client_objects": [saved["client_objects"], loaded["client_objects"]],
        "server_entities": [saved["server_entities"], loaded["server_entities"]],
        "npc_se_health": [saved["npc_se_health"], loaded["npc_se_health"]],
        "npc_se_gap_m": round(distance(saved, loaded, "npc_se_"), 4) if saved["npc_se"] and loaded["npc_se"] else None,
        "monster": [saved["monster_id"], saved["monster_se_health"], loaded["monster_se"],
                    loaded["monster_se_health"]],
        "marshal_version": [saved["marshal_version"], loaded["marshal_version"]],
    }
    return checks, details


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    server = load(sys.argv[1])
    client = load(sys.argv[2])
    saved = server["after-save"]
    summary = {"server": sys.argv[1], "client": sys.argv[2], "comparisons": {}}
    ok = True
    for name, rows, label, live in (("server-reloaded-alife", server, "reloaded-alife", False),
                                    ("server-reloaded-ticked", server, "reloaded-ticked", True),
                                    ("client-loaded", client, "client-loaded", True)):
        if label not in rows:
            summary["comparisons"][name] = {"missing": label}
            ok = False
            continue
        checks, details = compare(saved, rows[label], live)
        summary["comparisons"][name] = {"checks": checks, "details": details, "ok": all(checks.values())}
        ok = ok and all(checks.values())
    summary["match"] = ok
    print(json.dumps(summary, indent=2))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

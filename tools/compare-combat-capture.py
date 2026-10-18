"""Compare two S02-T06c combat captures (campaign server and normal client).

Usage: python3 tools/compare-combat-capture.py <server.csv> <client.csv>

Row kinds are documented in xrGame/CampaignCombatCapture.h. Rules:

* Both sides ran on the same level with the same stalker, spawned the same
  monster section at a similar distance (within 3 m), and set the actor's god
  mode for the fixture.
* Both sides: the monster came online, the stalker selected it as its enemy,
  the monster selected an enemy, the stalker fired, at least one bullet
  impacted the monster, the monster received a hit and lost health, one of
  the two died, and the end row carries ok=1.
* Both sides: every hit on the monster from a bullet came after an impact on
  it (the first hit tick is not before the first impact tick), and the
  stalker's first shot is not before it selected the monster as its enemy.
* Per-side totals (bullets by sender, impacts on the monster, hits on each
  participant, deaths, the first-milestone ticks and their device times) are
  reported for both sides; who died is reported, not asserted, since the
  fight's outcome is the engine's own.

Prints a JSON summary and exits 1 on any asserted mismatch.
"""

import csv
import json
import sys

META_FIELDS = ["frame", "time_ms", "level", "policy", "anchors", "npc", "npc_name", "npx", "npy", "npz",
               "npc_health", "section", "monster", "sx", "sy", "sz", "spawn_vertex", "spawn_distance", "budget_ms",
               "online_wait_ms", "godmode", "bullets", "impacts_dynamic", "impacts_static", "hits", "deaths"]
TICK_FIELDS = ["n", "phase", "frame", "time_ms", "monster_online", "npc_alive", "npc_health", "monster_alive",
               "monster_health", "distance", "npc_enemy", "npc_sees", "monster_enemy", "npc_action", "bullets",
               "impacts_dynamic", "impacts_static", "hits", "deaths"]
BULLET_FIELDS = ["n", "phase", "frame", "sender", "weapon", "px", "py", "pz", "dx", "dy", "dz", "speed", "hit_type"]
IMPACT_FIELDS = ["n", "phase", "frame", "sender", "target", "dynamic", "material", "px", "py", "pz"]
HIT_FIELDS = ["n", "phase", "frame", "victim", "who", "power", "hit_type", "bone", "health_after"]
DEATH_FIELDS = ["n", "phase", "frame", "victim", "killer"]
END_FIELDS = ["ticks", "sim_ms", "wall_ms", "online_tick", "enemy_tick", "seen_tick", "monster_enemy_tick",
              "shot_tick", "impact_tick", "hit_tick", "death_tick", "npc_bullets", "monster_impacts", "monster_hits",
              "monster_fire_hits", "npc_hits", "npc_health", "monster_health", "deaths", "dead", "killer", "ok"]
MILESTONES = ["online_tick", "enemy_tick", "seen_tick", "monster_enemy_tick", "shot_tick", "impact_tick", "hit_tick",
              "death_tick"]


def load(path):
    capture = {"ticks": [], "bullets": [], "impacts": [], "hits": [], "deaths": []}
    kinds = {"tick": ("ticks", TICK_FIELDS), "bullet": ("bullets", BULLET_FIELDS),
             "impact": ("impacts", IMPACT_FIELDS), "hit": ("hits", HIT_FIELDS), "death": ("deaths", DEATH_FIELDS)}
    with open(path, newline="") as handle:
        for row in csv.reader(handle):
            if not row:
                continue
            kind, values = row[0], row[1:]
            if kind == "meta":
                capture["meta"] = dict(zip(META_FIELDS, values, strict=True))
            elif kind == "end":
                capture["end"] = dict(zip(END_FIELDS, values, strict=True))
            elif kind in kinds:
                key, fields = kinds[kind]
                capture[key].append(dict(zip(fields, values, strict=True)))
    for key in ("meta", "end"):
        if key not in capture:
            raise SystemExit(f"{path}: no {key} row")
    return capture


def tick_time(capture, tick):
    """Device time of a fixture tick relative to the begin, or None."""
    if tick < 1:
        return None
    begin = int(capture["meta"]["time_ms"])
    for row in capture["ticks"]:
        if int(row["n"]) == tick:
            return int(row["time_ms"]) - begin
    return None


def summarize(capture):
    meta, end = capture["meta"], capture["end"]
    npc, monster = meta["npc"], meta["monster"]
    by_sender = {}
    for b in capture["bullets"]:
        by_sender[b["sender"]] = by_sender.get(b["sender"], 0) + 1
    hits_by_victim = {}
    for h in capture["hits"]:
        hits_by_victim[h["victim"]] = hits_by_victim.get(h["victim"], 0) + 1
    monster_hits = [h for h in capture["hits"] if h["victim"] == monster]
    dead = end["dead"]
    return {
        "section": meta["section"],
        "spawn_distance": float(meta["spawn_distance"]),
        "ticks": int(end["ticks"]),
        "sim_ms": int(end["sim_ms"]),
        "milestone_ticks": {m: int(end[m]) for m in MILESTONES},
        "milestone_ms": {m: tick_time(capture, int(end[m])) for m in MILESTONES},
        "npc_bullets": int(end["npc_bullets"]),
        "bullets_by_sender": by_sender,
        "monster_impacts": int(end["monster_impacts"]),
        "impacts_total": {"dynamic": sum(1 for i in capture["impacts"] if i["dynamic"] == "1"),
                          "static": sum(1 for i in capture["impacts"] if i["dynamic"] == "0")},
        "monster_hits": int(end["monster_hits"]),
        "monster_fire_hits": int(end["monster_fire_hits"]),
        "monster_hits_from_npc": sum(1 for h in monster_hits if h["who"] == npc),
        "monster_hit_types": sorted({h["hit_type"] for h in monster_hits}),
        "npc_hits": int(end["npc_hits"]),
        "hits_by_victim": hits_by_victim,
        "deaths_level": len(capture["deaths"]),
        "deaths_participants": int(end["deaths"]),
        "dead": "monster" if dead == monster else "stalker" if dead == npc else dead,
        "killer": "stalker" if end["killer"] == npc else "monster" if end["killer"] == monster else end["killer"],
        "npc_health_end": float(end["npc_health"]),
        "monster_health_end": float(end["monster_health"]),
        "ticks_seen": sum(1 for t in capture["ticks"] if t["npc_sees"] == "1"),
        "ticks_enemy_selected": sum(1 for t in capture["ticks"] if t["npc_enemy"] == monster),
        "distance_min": min((float(t["distance"]) for t in capture["ticks"] if float(t["distance"]) >= 0),
                            default=None),
    }


def side_rules(name, capture, failures):
    end = capture["end"]
    if capture["meta"]["godmode"] != "1":
        failures.append(f"{name}: actor god mode was not set for the fixture")
    for field in ("online_tick", "enemy_tick", "monster_enemy_tick", "shot_tick", "impact_tick", "hit_tick",
                  "death_tick"):
        if int(end[field]) < 0:
            failures.append(f"{name}: {field} is {end[field]}")
    for field in ("npc_bullets", "monster_impacts", "monster_hits", "monster_fire_hits", "deaths"):
        if int(end[field]) < 1:
            failures.append(f"{name}: {field} is {end[field]}")
    # The first *bullet* hit (a fire wound from the stalker) must follow the
    # first impact on the monster; other hits (a close-range wound, contact
    # damage) are the engine's own and may come earlier.
    meta = capture["meta"]
    fire_hit_ticks = [int(h["n"]) for h in capture["hits"]
                      if h["victim"] == meta["monster"] and h["who"] == meta["npc"] and h["hit_type"] == "6"]
    if fire_hit_ticks and int(end["impact_tick"]) >= 0 and min(fire_hit_ticks) < int(end["impact_tick"]):
        failures.append(f"{name}: first fire-wound hit (tick {min(fire_hit_ticks)}) before first impact "
                        f"(tick {end['impact_tick']})")
    if int(end["shot_tick"]) >= 0 and int(end["enemy_tick"]) >= 0 and int(end["shot_tick"]) < int(end["enemy_tick"]):
        failures.append(f"{name}: first shot (tick {end['shot_tick']}) before the enemy was selected "
                        f"(tick {end['enemy_tick']})")
    if end["ok"] != "1":
        failures.append(f"{name}: end row ok={end['ok']}")


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    server, client = load(sys.argv[1]), load(sys.argv[2])
    failures = []
    summary = {"server": sys.argv[1], "client": sys.argv[2]}

    sm, cm = server["meta"], client["meta"]
    for field in ("level", "npc", "npc_name", "section"):
        if sm[field] != cm[field]:
            failures.append(f"meta {field}: {sm[field]} vs {cm[field]}")
    if abs(float(sm["spawn_distance"]) - float(cm["spawn_distance"])) > 3.0:
        failures.append(f"spawn distance: {sm['spawn_distance']} vs {cm['spawn_distance']}")

    side_rules("server", server, failures)
    side_rules("client", client, failures)
    summary["server_side"] = summarize(server)
    summary["client_side"] = summarize(client)
    summary["failures"] = failures
    summary["match"] = not failures
    print(json.dumps(summary, indent=2))
    sys.exit(1 if failures else 0)


if __name__ == "__main__":
    main()

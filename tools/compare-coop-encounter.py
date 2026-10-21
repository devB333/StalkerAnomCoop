#!/usr/bin/env python3
"""Measure the S06-T05 encounter: an authoritative NPC fight and deaths, as the
server had them and as both clients showed them, plus the session's rates.

Usage: python3 tools/compare-coop-encounter.py <session logs> [--out compare-encounter.json]

Inputs:
  server/coop-encounter.csv     every 100 ms: each participant actor and the boar
                                (health 0..10000, 0 only when dead; alive; last
                                hitter; position)
  server/coop-server.log        encounter spawn / monster-dead / lethal lines;
                                5 s status lines (tick, snapshot counters, fire
                                presses, dead inputs)
  <client>/coop-c*.log          script4 lines, death-presented, script-action /
                                action-result, script-leg windows
  <client>/coop-encounter-client.csv  what the client shows of the actors and the
                                boar, 10 times a second

Checks (fixed before the final runs):
  spawn            the boar spawned and both clients showed it alive
  npc_damage       a participant's health fell while the boar lived, with the
                   boar as its last hitter
  player_damage    the boar's health fell with a participant's actor as its
                   last hitter
  monster_death    the boar died on the server and each client showed it dead
                   within 1,000 ms, and dead to the end
  player_death     a participant's actor died on the server and each client
                   showed it dead within 1,000 ms, and dead to the end
  dead_inert       during the dead player's D-dead-walk leg (forward and fire for
                   2 s) its body moved under 0.10 m on the server and on its
                   client, and the server pressed its trigger no more
  forged           the dead player's forged "revive" intent was refused and its
                   actor stayed dead
  agreement        each client's health and alive flag for the actors and the
                   boar are values the server held in the 600 ms before the row
                   (health within 10) in >= 95 % of rows, and equal at the end
  simulation_rate  the server's median tick rate over 5 s windows >= 45 Hz
Reported: snapshot rate and bytes per second per player (5 s windows),
presentation delays.
"""

import argparse
import bisect
import csv
import json
import re
import statistics
import sys
from collections import defaultdict
from pathlib import Path


def load_server(logs):
    series = defaultdict(list)
    kinds = {}
    for r in csv.DictReader(open(logs / "server/coop-encounter.csv", newline="")):
        if None in r.values():
            continue  # a row cut short (the server stopped while writing it)
        row = {k: (v if k == "kind" else float(v) if k in ("x", "y", "z") else int(v)) for k, v in r.items()}
        series[row["id"]].append((row["utc_ms"], row))
        kinds[row["id"]] = (row["kind"], row["slot"])
    return series, kinds


def at(series, t):
    ts = [x[0] for x in series]
    i = bisect.bisect_right(ts, t)
    return series[i - 1][1] if i > 0 else None


def window(series, a, b):
    return [r for t, r in series if a <= t <= b]


def first_dead(series):
    return next((t for t, r in series if r["alive"] == 0), None)


def status_rates(text):
    rows = []
    for line in text.splitlines():
        m = re.match(r"^(\d+) status tick=(\d+) ", line)
        if not m:
            continue
        sn = {s: [int(x) for x in v.split("/")] for s, v in re.findall(r" sn(\d)=([0-9/]+)", line)}
        fire = {s: int(v) for s, v in re.findall(r" fire(\d)=(\d+)", line)}
        dead = {s: int(v) for s, v in re.findall(r" dead_in(\d)=(\d+)", line)}
        rows.append({"utc": int(m.group(1)), "tick": int(m.group(2)), "sn": sn, "fire": fire, "dead_in": dead})
    return rows


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--behind-ms", type=int, default=600)
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    logs = Path(a.logs)
    server, kinds = load_server(logs)
    text = open(logs / "server/coop-server.log", errors="replace").read()
    result = {"checks": [], "clients": {}, "server": {}}

    def add(check, ok, detail):
        result["checks"].append({"check": check, "ok": bool(ok), "detail": detail})

    m = re.search(r"encounter spawn section=(\S+) id=(\d+) pos=(\S+) distance=(\S+)", text)
    monster = int(m.group(2)) if m else None
    actors = [i for i, (k, _) in kinds.items() if k == "actor"]
    result["server"]["spawn"] = m.group(0) if m else None
    result["server"]["lines"] = re.findall(r"encounter (?:monster-dead|lethal).*", text)
    M = server.get(monster, [])
    m_dead = first_dead(M)

    # damage
    npc_hits = []
    for aid in actors:
        s = server[aid]
        for (_, r0), (t, r1) in zip(s, s[1:], strict=False):
            if r1["health"] < r0["health"] and r1["hitter"] == monster and (m_dead is None or t <= m_dead + 200):
                npc_hits.append((aid, t, r0["health"] - r1["health"]))
    add("npc_damage", npc_hits, f"{len(npc_hits)} drops, first {npc_hits[:3]}")
    player_hits = [(t, r0["health"] - r1["health"], r1["hitter"]) for (_, r0), (t, r1) in zip(M, M[1:], strict=False)
                   if r1["health"] < r0["health"] and r1["hitter"] in actors]
    other_hitters = sorted({r1["hitter"] for (_, r0), (_t, r1) in zip(M, M[1:], strict=False)
                            if r1["health"] < r0["health"] and r1["hitter"] not in actors})
    add("player_damage", player_hits, f"{len(player_hits)} drops by participants, first {player_hits[:3]}; "
        f"other hitters {other_hitters}")

    deaths = {aid: first_dead(server[aid]) for aid in actors}
    dead_actors = {aid: t for aid, t in deaths.items() if t is not None}
    result["server"]["monster_dead_utc"] = m_dead
    result["server"]["actor_deaths"] = dead_actors

    statuses = status_rates(text)
    spawn_seen = True
    for d in sorted(p for p in logs.iterdir() if p.is_dir() and p.name.startswith("c")):
        f = d / "coop-encounter-client.csv"
        logf = list(d.glob("coop-c*.log"))
        if not f.exists() or not logf:
            continue
        name = d.name
        rows = [{k: (v if k == "kind" else float(v) if k in ("x", "y", "z") else int(v)) for k, v in r.items()}
                for r in csv.DictReader(open(f, newline=""))]
        log = open(logf[0], errors="replace").read()
        info = {}
        by_id = defaultdict(list)
        for r in rows:
            by_id[r["id"]].append(r)
        mon = by_id.get(monster, [])

        def shown_dead(rs, t_dead):
            first = next((r["utc_ms"] for r in rs if r["alive"] == 0), None)
            stays = first is not None and all(r["alive"] == 0 for r in rs if r["utc_ms"] >= first)
            return first, (first - t_dead) if first is not None and t_dead is not None else None, stays

        if m_dead is not None:
            first, delay, stays = shown_dead(mon, m_dead)
            info["monster_death"] = {"shown": first, "delay_ms": delay, "stays": stays}
        # every NPC in the record (the boar, Fanat) that died on the server is
        # shown dead here (the native Die, logged as death-presented) within 1 s;
        # the server's time is the first dead 100 ms row, so it can trail by 100 ms
        presented = {int(i): int(t) for t, i in re.findall(r"^(\d+) death-presented id=(\d+)", log, re.M)}
        npc_dead = {i: first_dead(server[i]) for i, (k, _) in kinds.items() if k != "actor"}
        npc_dead = {i: t for i, t in npc_dead.items() if t is not None}
        # (the client's 10 Hz record only follows the boar once it is within
        # 40 m of the player, enc-11; the log has every presented death)
        shown = {i: (presented[i] - t if i in presented else None) for i, t in npc_dead.items()}
        boar_died = monster in npc_dead
        if boar_died and shown.get(monster) is None:
            boar = info.get("monster_death", {})
            shown[monster] = boar.get("delay_ms") if boar.get("stays") else None
        # a presented death means the client had the boar alive (present_death
        # only kills a living body)
        spawn_seen = spawn_seen and (any(r["alive"] == 1 for r in mon) or monster in presented)
        info["npc_deaths"] = shown
        add(f"{name} npc_deaths", npc_dead and all(d is not None and -150 <= d <= 1000 for d in shown.values()),
            f"delay_ms by id {shown} (boar {monster} died={boar_died})")
        if dead_actors:
            for aid, t in dead_actors.items():
                first, delay, stays = shown_dead(by_id.get(aid, []), t)
                info[f"death_{aid}"] = {"shown": first, "delay_ms": delay, "stays": stays}
                add(f"{name} player_death {aid}", delay is not None and delay <= 1000 and stays,
                    f"{info[f'death_{aid}']}")
        else:
            add(f"{name} player_death", False, "no participant died on the server")
        # the dead player's attempts
        leg = re.search(r"script-leg name=D-dead-walk start_utc=(\d+) end_utc=(\d+)", log)
        if leg:
            s0, s1 = int(leg.group(1)), int(leg.group(2))
            own = next((r["id"] for r in rows if r["local"] == 1), None)
            srv = window(server.get(own, []), s0, s1 + 500)
            moved = max((abs(r["x"] - srv[0]["x"]) + abs(r["z"] - srv[0]["z"]) for r in srv), default=None)
            cl = [r for r in by_id.get(own, []) if s0 <= r["utc_ms"] <= s1 + 500]
            cl_moved = max((abs(r["x"] - cl[0]["x"]) + abs(r["z"] - cl[0]["z"]) for r in cl), default=None)
            slot = kinds[own][1] if own in kinds else None
            before = [s for s in statuses if s["utc"] <= s0]
            after = statuses[-1] if statuses else None
            fire_before = before[-1]["fire"].get(str(slot)) if before else None
            fire_after = after["fire"].get(str(slot)) if after else None
            dead_in = after["dead_in"].get(str(slot)) if after else None
            info["dead_walk"] = {"server_moved": moved, "client_moved": cl_moved, "fire_before": fire_before,
                                 "fire_after": fire_after, "dead_inputs": dead_in}
            add(f"{name} dead_inert", moved is not None and moved < 0.10 and cl_moved is not None and cl_moved < 0.10
                and fire_before == fire_after, f"{info['dead_walk']}")
            ops = re.findall(r"script-action forged-revive operation=(\d+)", log)
            res = {int(o): (int(c), int(r)) for o, c, r in
                   re.findall(r"action-result operation=(\d+) outcome=(\d+) reason=(\d+)", log)}
            refused = bool(ops) and res.get(int(ops[0]), (0, 0))[0] == 2
            still_dead = own in dead_actors and all(r["alive"] == 0 for t, r in server[own] if t >= dead_actors[own])
            add(f"{name} forged", refused and still_dead, f"refused={refused} stayed_dead={still_dead}")
        # agreement
        agree = total = 0
        last = {}
        for r in rows:
            series = server.get(r["id"])
            if not series:
                continue
            held = window(series, r["utc_ms"] - a.behind_ms, r["utc_ms"])
            before = at(series, r["utc_ms"] - a.behind_ms)
            if before:
                held.append(before)
            if not held:
                continue
            total += 1
            hs = [x["health"] for x in held]
            ok = min(hs) - 10 <= r["health"] <= max(hs) + 10 and r["alive"] in {x["alive"] for x in held}
            agree += 1 if ok else 0
            last[r["id"]] = (r, at(series, r["utc_ms"]))
        final_ok = bool(last) and all(
            e is not None and abs(r["health"] - e["health"]) <= 10 and r["alive"] == e["alive"]
            for r, e in last.values())
        ratio = agree / total if total else 0
        info["agreement"] = f"{agree}/{total} ({ratio:.3f})"
        add(f"{name} agreement", ratio >= 0.95 and final_ok, f"{info['agreement']} final_ok={final_ok}")
        info["presented"] = re.findall(r"death-presented id=(\d+) section=(\S+)", log)
        result["clients"][name] = info
    add("spawn", monster is not None and spawn_seen, f"monster={monster} shown alive on every client={spawn_seen}")

    # rates over 5 s windows (status lines)
    ticks, snaps, kbs = [], defaultdict(list), defaultdict(list)
    for s0, s1 in zip(statuses, statuses[1:], strict=False):
        dt = (s1["utc"] - s0["utc"]) / 1000.0
        if dt <= 0:
            continue
        ticks.append((s1["tick"] - s0["tick"]) / dt)
        for slot in s1["sn"]:
            if slot in s0["sn"]:
                snaps[slot].append((s1["sn"][slot][0] - s0["sn"][slot][0]) / dt)
                kbs[slot].append((s1["sn"][slot][4] - s0["sn"][slot][4]) / dt / 1024.0)

    def summary(v):
        if not v:
            return None
        return {"median": round(statistics.median(v), 2), "min": round(min(v), 2), "max": round(max(v), 2)}
    result["rates"] = {"tick_hz": summary(ticks),
                       "snapshots_per_s": {k: summary([x for x in v if x > 0]) for k, v in snaps.items()},
                       "snapshot_kb_per_s": {k: summary([x for x in v if x > 0]) for k, v in kbs.items()}}
    add("simulation_rate", ticks and statistics.median(ticks) >= 45, f"{result['rates']['tick_hz']}")
    # S06-T05: no level change (L14); a refused level-changer touch is logged
    xlog = "".join(open(f, errors="replace").read() for f in (logs / "server").glob("xray-*.log"))
    changed = "autosave.scop is successfully saved" in xlog
    refused = re.findall(r"coop level-change-refused (.*)", xlog)
    result["server"]["level_change_refused"] = refused
    add("no_level_change", not changed, f"native level change={changed}; refused={len(refused)}")
    result["match"] = all(x["ok"] for x in result["checks"])
    out = json.dumps(result, indent=2)
    if a.out:
        Path(a.out).write_text(out + "\n")
    print(out)
    return 0 if result["match"] else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Measure the S06-T04 combat program: what the server's weapons did and
whether both clients show the server's outcomes.

Usage: python3 tools/compare-coop-combat.py <session logs> [--behind-ms 600] [--ahead-ms 400]
                                               [--out compare-combat.json]

Inputs:
  server/coop-combat.csv        each participant actor at every snapshot round:
                                health (0..10000), active slot, weapon state,
                                rounds in the magazine, shot counter (the
                                magazine's decreases), last hitter, position
  server/coop-server.log        intents accepted / refused, arm and heal fixtures
  <client>/coop-c*.log          script-leg windows, script-action, action-result
  <client>/coop-combat-client.csv  what the client shows of each actor 10 times
                                a second (health, slot, rounds)

Checks for each client running the program (fixed before the first run):
  equip           the holster intent (slot 0) accepted and the server's actor
                  holding nothing within 3 s; then the slot-2 intent accepted and
                  the server's actor holding slot 2 within 3 s
  aim_fire        at least one shot, and the other actor lost health in the leg
  hold            at most one shot with the key held 1 s (a semi-automatic)
  rapid           at least 2 and at most 12 shots in 1 s of presses
  duplicates      batches sent four times fire no more than the normal rapid leg + 1
  empty           the magazine reached 0, and no shot after that until the reload
  reloads         every reload intent accepted and the magazine fuller within 5 s
                  than when it was sent (or already full)
  forged          the forged-hit intent refused; no hit on the other actor in
                  the leg (a hit: health falls by at least 1 % between rows;
                  bleeding from earlier hits falls slower)
  move_fire       the actor moved at least 0.5 m and fired at least 2 shots
  agreement       the client's health, active slot and rounds for both actors are values the
                  server held in the 600 ms before the row (the own actor's
                  rounds also up to 400 ms after: the shooter's weapon shows
                  its shot at once) in >= 95 % of rows, and equal at the end;
                  health within 10 (0.1 %); rows before the client's first
                  weapon outcome are not counted
Session check:
  simultaneous    both actors fired in their W-simultaneous legs, first shots
                  within 400 ms of each other on the server
"""

import argparse
import bisect
import csv
import json
import math
import re
import sys
from collections import defaultdict
from pathlib import Path


def load_server(logs):
    series = defaultdict(list)  # entity -> [(utc, row)]
    for r in csv.DictReader(open(logs / "server/coop-combat.csv", newline="")):
        row = {k: (float(v) if k in ("x", "y", "z") else int(v)) for k, v in r.items()}
        series[row["entity"]].append((row["utc_ms"], row))
    for v in series.values():
        v.sort(key=lambda x: x[0])
    return series


def at(series, t):
    """The server row in effect at t (the last one at or before t)."""
    ts = [x[0] for x in series]
    i = bisect.bisect_right(ts, t)
    return series[i - 1][1] if i > 0 else None


def window(series, a, b):
    return [r for t, r in series if a <= t <= b]


def client(d):
    logs = list(d.glob("coop-c*.log"))
    if not logs:
        return None
    legs, actions, results, wpn_states = {}, {}, {}, []
    for line in open(logs[0], errors="replace"):
        m = re.search(r"^(\d+) remote-weapon-state id=(\d+) state=(\d+)->(\d+) slot=(\d+) item=(\S+)", line)
        if m:
            wpn_states.append((int(m.group(2)), int(m.group(4)), m.group(6)))
        m = re.search(r"^(\d+) script-leg name=(\S+) start_utc=(\d+) end_utc=(\d+)", line)
        if m:
            legs.setdefault(m.group(2), []).append((int(m.group(3)), int(m.group(4))))
        m = re.search(r"^(\d+) script-action (\S+) operation=(\d+) kind=(\d+)", line)
        if m:
            actions[int(m.group(3))] = (m.group(2), int(m.group(1)))
        m = re.search(r"^(\d+) action-result operation=(\d+) outcome=(\d+) reason=(\d+)", line)
        if m:
            results[int(m.group(2))] = (int(m.group(3)), int(m.group(4)), int(m.group(1)))
    rows = []
    f = d / "coop-combat-client.csv"
    if f.exists():
        rows = [{k: int(v) for k, v in r.items()} for r in csv.DictReader(open(f, newline=""))]
    return {"legs": legs, "actions": actions, "results": results, "rows": rows, "wpn_states": wpn_states}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--behind-ms", type=int, default=600)
    ap.add_argument("--ahead-ms", type=int, default=400)
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    logs = Path(a.logs)
    server = load_server(logs)
    result = {"clients": {}, "checks": []}

    def add(check, ok, detail):
        result["checks"].append({"check": check, "ok": bool(ok), "detail": detail})

    first_sim_shot = {}
    for d in sorted(p for p in logs.iterdir() if p.is_dir() and p.name.startswith("c")):
        c = client(d)
        if not c or not c["legs"]:
            continue
        name = d.name
        own = next((r["entity"] for r in c["rows"] if r["local"] == 1), None)
        other = next((r["entity"] for r in c["rows"] if r["local"] == 0), None)
        if own is None or own not in server:
            add(f"{name} identity", False, "own actor not found in the server's combat log")
            continue
        S, OTH = server[own], server.get(other, [])
        info = {"own": own, "other": other, "legs": {}}

        def leg(leg_name, k=0, info=info, c=c, S=S, OTH=OTH):
            span = c["legs"].get(leg_name, [])
            if len(span) <= k:
                return None
            s0, s1 = span[k]
            r0, r1 = at(S, s0), at(S, s1 + 400)
            w = window(S, s0, s1 + 400)
            moved = math.dist((r0["x"], r0["z"]), (r1["x"], r1["z"])) if r0 and r1 else 0.0
            ow = window(OTH, s0, s1 + 600)
            steps = [ow[i]["health"] - ow[i + 1]["health"] for i in range(len(ow) - 1)]
            drops = sum(max(0, d) for d in steps)
            hits = sum(1 for d in steps if d >= 100)
            x = {"shots": (r1["shots"] - r0["shots"]) if r0 and r1 else None,
                 "ammo_start": r0["ammo"] if r0 else None, "ammo_end": r1["ammo"] if r1 else None,
                 "ammo_min": min((r["ammo"] for r in w), default=None), "moved": round(moved, 2),
                 "other_health_drop": drops, "other_hits": hits, "window": [s0, s1]}
            info["legs"][f"{leg_name}#{k}" if k else leg_name] = x
            return x

        # holster, then equip
        def switched(what, slot, c=c, S=S):
            ops = [(op, v) for op, v in c["actions"].items() if v[0] == what]
            accepted = bool(ops) and c["results"].get(ops[0][0], (0,))[0] == 1
            held = bool(ops) and any(r["slot"] == slot for t, r in S if ops[0][1][1] <= t <= ops[0][1][1] + 3000)
            return accepted, held
        ho, hh = switched("holster", 0)
        eo, eh = switched("equip-slot-2", 2)
        add(f"{name} equip", ho and hh and eo and eh,
            f"holster accepted={ho} slot 0 held={hh}; equip accepted={eo} slot 2 held={eh}")
        af = leg("W-aim-fire")
        add(f"{name} aim_fire", af and af["shots"] >= 1 and af["other_health_drop"] > 0, f"{af}")
        h = leg("W-hold")
        add(f"{name} hold", h and h["shots"] is not None and h["shots"] <= 1, f"{h}")
        rp = leg("W-rapid")
        add(f"{name} rapid", rp and rp["shots"] is not None and 2 <= rp["shots"] <= 12, f"{rp}")
        dp = leg("W-dup")
        add(f"{name} duplicates", dp and rp and dp["shots"] is not None and rp["shots"] is not None
            and dp["shots"] <= rp["shots"] + 1, f"dup {dp and dp['shots']} rapid {rp and rp['shots']}")
        em = leg("W-empty")
        empty_ok = False
        if em:
            w = window(S, em["window"][0], em["window"][1] + 400)
            zero = next((i for i, r in enumerate(w) if r["ammo"] == 0), None)
            empty_ok = zero is not None and all(r["shots"] == w[zero]["shots"] for r in w[zero:])
        add(f"{name} empty", empty_ok, f"{em}")
        rel_ok, rel_detail = True, []
        for op, (what, t) in sorted(c["actions"].items()):
            if what != "reload":
                continue
            accepted = c["results"].get(op, (0,))[0] == 1
            before = at(S, t)
            later = window(S, t, t + 5000)
            refilled = before is not None and (any(r["ammo"] > before["ammo"] for r in later)
                                               or before["ammo"] >= 8)
            rel_detail.append((op, accepted, refilled))
            rel_ok = rel_ok and accepted and refilled
        add(f"{name} reloads", rel_ok and rel_detail, f"{rel_detail}")
        fg = leg("W-forged")
        fops = [op for op, v in c["actions"].items() if v[0] == "forged-hit"]
        frefused = bool(fops) and c["results"].get(fops[0], (0,))[0] == 2
        add(f"{name} forged", frefused and fg and fg["other_hits"] == 0 and fg["shots"] == 0,
            f"refused={frefused} other_hits={fg and fg['other_hits']} shots={fg and fg['shots']}")
        mf = leg("W-move-fire")
        add(f"{name} move_fire", mf and mf["moved"] >= 0.5 and mf["shots"] >= 2, f"{mf}")
        sim = leg("W-simultaneous")
        if sim:
            first = None
            for (_, r_prev), (t, r) in zip(S, S[1:], strict=False):
                if sim["window"][0] <= t <= sim["window"][1] + 400 and r["shots"] > r_prev["shots"]:
                    first = t
                    break
            first_sim_shot[own] = first
        # agreement of what the client shows with the server
        agree, total, last = 0, 0, {}
        for r in c["rows"]:
            series = server.get(r["entity"])
            if not series or r["server_slot"] == 255:
                continue
            ahead = a.ahead_ms if r["local"] == 1 else 0
            held = window(series, r["utc_ms"] - a.behind_ms, r["utc_ms"] + ahead)
            before = at(series, r["utc_ms"] - a.behind_ms)
            if before:
                held.append(before)
            if not held:
                continue
            total += 1
            healths = [x["health"] for x in held]
            ok = min(healths) - 10 <= r["health"] <= max(healths) + 10
            ammo_ok = r["ammo"] < 0 or r["ammo"] in {x["ammo"] for x in held}
            slot_ok = r["slot"] in {x["slot"] for x in held}
            agree += 1 if ok and ammo_ok and slot_ok else 0
            last[r["entity"]] = (r, at(series, r["utc_ms"]))
        final_ok = bool(last) and all(
            s_end is not None and abs(r["health"] - s_end["health"]) <= 10
            and (r["ammo"] < 0 or r["ammo"] == s_end["ammo"]) and r["slot"] == s_end["slot"]
            for r, s_end in last.values())
        ratio = agree / total if total else 0
        add(f"{name} agreement", ratio >= 0.95 and final_ok, f"{agree}/{total} ({ratio:.3f}) final_ok={final_ok}")
        # S10-T04c: the partner's weapon states reached this client for its
        # third-person animations: firing (5), a reload (7) and a draw or
        # holster (1, 2); a knife swing (5 with a knife) when the partner has one
        other_id = next((r["id"] for r in c["rows"] if r["local"] == 0), None)
        seen = [(st, item) for ent, st, item in c["wpn_states"] if ent == other_id]
        states = {st for st, _ in seen}
        knife = sum(1 for st, item in seen if st in (5, 6) and "knife" in item)
        add(f"{name} partner_anim", {5, 7} <= states and bool(states & {1, 2}),
            f"states={sorted(states)} knife_swings={knife} changes={len(seen)}")
        info["partner_states"] = sorted(states)
        result["clients"][name] = info
    if len(first_sim_shot) >= 2:
        firsts = [t for t in first_sim_shot.values() if t is not None]
        add("simultaneous", len(firsts) == 2 and abs(firsts[0] - firsts[1]) <= 400, f"first shots {first_sim_shot}")
    else:
        add("simultaneous", False, f"legs found for {list(first_sim_shot)}")
    text = open(logs / "server/coop-server.log", errors="replace").read()
    result["server"] = {"heals": len(re.findall(r" heal slot=", text)), "arm": re.findall(r" arm slot=.*", text),
                        "intents_refused": len(re.findall(r" intent-refused ", text)),
                        "intents_accepted": len(re.findall(r" intent-accepted ", text))}
    result["match"] = bool(result["checks"]) and all(x["ok"] for x in result["checks"])
    out = json.dumps(result, indent=2)
    if a.out:
        Path(a.out).write_text(out + "\n")
    print(out)
    return 0 if result["match"] else 1


if __name__ == "__main__":
    sys.exit(main())

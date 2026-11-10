#!/usr/bin/env python3
"""Check the S10-T03f new game on the campaign server (plan.md S10-T03f; docs/validation/S10-T03f.md).
Usage: python3 tools/compare-coop-newgame.py <session logs> --faction stalker --level zaton --pos 111.96,-3.00,176.75
                                             [--money 2000] [--out compare-newgame.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log; c1/ and c2/: coop-c<n>.log and xray_*.log; result.json.
The session (prepare-coop-session.py --new-game <faction>:<start location>) starts the server on a new game: the
spawn data instead of the save, the creator's choices in the server's axr_options.ltx [character_creation].
Checks:
  start     the server started a new game (new-game-server), the new-game script's ChangeLevel to the start
            location was taken as the start move (new-game-start-asked, -move), the party travel's executor reloaded
            the server on that level (travel-reloaded ok=1, new-game-started ok=1) before or while the players joined
  creator   player 1's actor: the faction's community (actor_<faction>), the creator's name and money, a loadout
            (items > 0), on the start level within 5 m of the start point
  partner   player 2's actor (T01-3): the same community, its own name and money, its own loadout or the faction's
            kit given once (new-game-loadout / respawn-kit slot=1), on the start level near player 1
  clients   both clients' worlds went live on the start level; clients exit 0, no FATAL
"""
import argparse
import json
import math
import re
import sys
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def kv(line):
    return {k: v.strip('"') for k, v in re.findall(r"(\w+)=(\"[^\"]*\"|[^\s\"]+)", line)}


def grep(log, marker):
    return [kv(ln) for ln in log if marker in ln]


def pos(text):
    return [float(x) for x in text.split(",")]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--faction", required=True)
    ap.add_argument("--level", required=True)
    ap.add_argument("--pos", required=True)
    ap.add_argument("--money", type=int, default=2000)
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    log = lines(root / "server" / "coop-server.log")
    start = pos(a.pos)
    community = f"actor_{a.faction}"
    checks = {}
    server = grep(log, " new-game-server ")
    asked = grep(log, " new-game-start-asked ")
    move = grep(log, " new-game-start-move ")
    started = grep(log, " new-game-started ")
    reloaded = [r for r in grep(log, " travel-reloaded ") if r.get("level") == a.level]
    checks["start"] = {"pass": bool(server) and server[0].get("faction") == a.faction and len(asked) == 1
                       and asked[0].get("level") == a.level and len(move) == 1 and bool(started)
                       and started[0].get("ok") == "1" and started[0].get("level") == a.level and bool(reloaded)
                       and reloaded[0].get("ok") == "1",
                       "server": server[:1], "asked": asked, "move": move, "started": started, "reloaded": reloaded[:1]}
    actors = {r.get("slot"): r for r in grep(log, " new-game-actor ")}
    p1, p2 = actors.get("0"), actors.get("1")

    def near(r, limit):
        return r is not None and math.dist(pos(r["pos"]), start) <= limit

    checks["creator"] = {"pass": p1 is not None and p1.get("community") == community and p1.get("name") == "Player 1"
                         and int(p1.get("money", "-1")) == a.money and int(p1.get("items", "0")) > 0
                         and p1.get("level") == a.level and near(p1, 5.0), "actor": p1}
    kit = [r for r in grep(log, " new-game-loadout ") + grep(log, " respawn-kit ") if r.get("slot") == "1"]
    checks["partner"] = {"pass": p2 is not None and p2.get("community") == community and p2.get("name") == "Player 2"
                         and int(p2.get("money", "-1")) == a.money and p2.get("level") == a.level and near(p2, 15.0)
                         and len(kit) == 1, "actor": p2, "kit": kit}
    clients = {}
    for c in ("c1", "c2"):
        cl = lines(root / c / f"coop-{c}.log")
        clients[c] = [r.get("level") for r in grep(cl, " world-level ")]
    exits = {}
    result = root / "result.json"
    if result.exists():
        exits = {k: v.get("exit_code") for k, v in json.loads(result.read_text(encoding="utf-8-sig"))
                 .get("clients", {}).items()}
    fatal = [str(p.relative_to(root)) for p in root.glob("c*/xray_*.log")
             if "FATAL ERROR" in p.read_text(errors="replace")]
    checks["clients"] = {"pass": all(v and v[-1] == a.level for v in clients.values()) and bool(exits)
                         and all(v == 0 for v in exits.values()) and not fatal,
                         "levels": clients, "exits": exits, "fatal": fatal}
    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

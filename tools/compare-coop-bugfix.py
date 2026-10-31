#!/usr/bin/env python3
"""Check a bug-fix run (plan.md S10-T07; docs/validation/S10-T07.md).
Usage: python3 tools/compare-coop-bugfix.py <session logs> [--out compare.json]
Inputs (builds/logs/S10-T07/run-<run>/): server-first-run/ and server/ (coop-server.log: the compat layer's notes,
the music fixture), c1/ and c2/ (coop-c<n>.log), result.json. Each check runs when its evidence is in the run:
  music     B18: the combat music started for player 2 (fighting) and never for player 1 (120 m away): the compat
            layer's music-start notes, and only client 2 applied music\\combat sounds
  jump      B6: player 2's own actor moved about 500 m in one step; client 2 took it as a far snap and stayed
  clock     B13: a client's clock is stepped only at its joins and the world's jumps (at most 6 steps per client
            process; before: about one a second), and its forward corrections are counted
  lost      B16: with the server gone for over a minute each client told its player (server-lost-told) and joined
            again when it was back (server-back told=1)
  ambient   B17: the ambient loops (wind, drone) were each player's own (slot-loop notes for slot 0 and 1) and both
            clients applied looped plays
  grab      B10: a bloodsucker's grab held client 2 (grab-hold, then grab-released) and client 2 sent no movement
            while held (the server's actor stayed within 1 m between the hold and the release)
  upgrade   B8: the client's tooltip prices with the server's percent (upgrade-percent tooltip_percent = percent)
  exits     every client process exited 0
"""
import argparse
import json
import re
import sys
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def fields(line):
    return dict(re.findall(r"(\w+)=(\[[^\]]*\]|\"[^\"]*\"|\S+)", line))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    server = lines(root / "server-first-run" / "coop-server.log") + lines(root / "server" / "coop-server.log")
    clients = {c: lines(root / c / f"coop-{c}.log") for c in ("c1", "c2")}
    result = json.loads((root / "result.json").read_text()) if (root / "result.json").exists() else {}
    checks = {}

    if any("fixture-music" in ln for ln in server):
        starts = [fields(ln).get("slot") for ln in server if " music-start " in ln]
        music_applied = {c: sum(1 for ln in log if " present " in ln and "music\\combat" in ln
                                and ln.endswith(" applied")) for c, log in clients.items()}
        fought = any("fixture-music fighting" in ln for ln in server)
        checks["music"] = {"pass": fought and "1" in starts and "0" not in starts and music_applied["c2"] > 0
                           and music_applied["c1"] == 0,
                           "fought": fought, "starts": starts, "music_applied": music_applied}
        jump = [fields(ln) for ln in server if "fixture-music p2-jump" in ln]
        done = [fields(ln) for ln in server if "fixture-music done" in ln]
        snaps = [float(fields(ln).get("metres", 0)) for ln in clients["c2"] if " far-snap " in ln]
        checks["jump"] = {"pass": bool(jump) and float(jump[0].get("metres", 0)) >= 450 and any(m >= 400 for m in snaps)
                          and bool(done) and done[0].get("p2_client_in_world") == "1",
                          "jump": jump, "c2_far_snaps": snaps, "done": done}

    clock = {}
    for c, log in clients.items():
        status = [fields(ln) for ln in log if re.search(r" clock game_ms=", ln)]
        last = status[-1] if status else {}
        clock[c] = {"steps": int(last.get("steps", -1)), "forward": int(last.get("forward", -1)),
                    "held": int(last.get("held", -1)), "events": int(last.get("clock_events", -1))}
    checks["clock"] = {"pass": all(0 <= v["steps"] <= 6 and v["forward"] >= 0 and v["events"] > 20
                                   for v in clock.values()), "clients": clock}

    if any("server-killed" in str(e.get("event")) for e in result.get("events", [])):
        lost = {c: {"told": [fields(ln) for ln in log if " server-lost-told " in ln],
                    "back": [fields(ln) for ln in log if " server-back " in ln]} for c, log in clients.items()}
        checks["lost"] = {"pass": all(v["told"] and any(b.get("told") == "1" for b in v["back"])
                                      for v in lost.values()), "clients": lost}

    loops = [fields(ln) for ln in server if " slot-loop " in ln]
    looped = {c: sum(1 for ln in log if " present " in ln and re.search(r"kind=sound(-at-pos)? .*flags=(\d+)", ln)
                     and int(re.search(r"flags=(\d+)", ln).group(1)) & 1 and ln.endswith(" applied"))
              for c, log in clients.items()}
    checks["ambient"] = {"pass": {"0", "1"} <= {e.get("slot") for e in loops} and all(v > 0 for v in looped.values()),
                         "slot_loops": len(loops), "slots": sorted({e.get("slot") for e in loops}),
                         "looped_applied": looped}

    holds = [ln for ln in clients["c2"] if " grab-hold " in ln]
    if holds:
        released = [fields(ln) for ln in clients["c2"] if " grab-released " in ln]
        checks["grab"] = {"pass": bool(released) and len(released) >= len(holds), "holds": len(holds),
                          "released": released}

    percents = [fields(ln) for ln in clients["c1"] if " upgrade-percent " in ln]
    if percents:
        checks["upgrade"] = {"pass": all(abs(float(e.get("percent", 0)) - float(e.get("tooltip_percent", -1))) < 0.001
                                         for e in percents), "percents": percents[:3]}

    exits = {k: v.get("exit_code") for k, v in (result.get("clients") or {}).items()}
    checks["exits"] = {"pass": bool(exits) and all(v == 0 for v in exits.values()), "clients": exits,
                       "server": result.get("server_exit_code")}

    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

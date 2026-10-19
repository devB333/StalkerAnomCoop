#!/usr/bin/env python3
"""Compare the server's and the client's actor lighting from the same checkpoint (S03-T06).

Usage: compare-lighting-capture.py <server-lighting-snapshot.csv> <client-lighting.csv>

The server row is participant 1's actor (the saved actor) after the second
checkpoint restore (label server-restored); the client row is the normal
client's view actor one second after it spawned from the same checkpoint
(label client-restored). Prints a JSON summary; exit 0 when every rule holds.
"""

import csv
import json
import math
import sys

# The two processes reach different game times: the client's own load runs
# the clock (x6 on k00_marsh); capture-18/19 were 255 s and 71 s of game time
# after the server's row. The environment values below are what matter.
MAX_GAME_TIME_S = 600.0
# Where each actor stands: the client's actor settles on its own physics during
# the 10 s wait (capture-22: 1.03 m, 0.86 m of it up); the server's stands still.
# A guard that both are in the same place, not the measured quantity.
MAX_POSITION_M = 1.5
MAX_SUN_DIR = 0.02  # per component, unit vector
MAX_COLOUR = 0.02  # per component
# The luminosity is the brightest channel of ambient + sky x sky openness + sun
# x sun visibility + static lights, each smoothed; the same code on both sides.
MAX_LUMINOCITY_ABS = 0.03
MAX_LUMINOCITY_REL = 0.10


def load(path, label):
    with open(path, newline="") as handle:
        rows = [row for row in csv.DictReader(handle) if row.get("kind") == "light" and row.get("label") == label]
    if not rows:
        raise SystemExit(f"{path}: no light row labelled {label}")
    return rows


def vec(row, *names):
    return [float(row[name]) for name in names]


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    server_rows = load(sys.argv[1], "server-restored")
    client = load(sys.argv[2], "client-restored")[0]
    # participant 1's actor is the saved actor; the client's view actor has the same id
    server = next((row for row in server_rows if row["actor"] == client["actor"]), server_rows[0])

    rules = []

    def rule(name, ok, server_value, client_value):
        rules.append({"rule": name, "ok": bool(ok), "server": server_value, "client": client_value})

    rule("same actor", server["actor"] == client["actor"], server["actor"], client["actor"])
    rule("same weather", server["weather"] == client["weather"], server["weather"], client["weather"])
    dt = abs(float(server["game_time_s"]) - float(client["game_time_s"]))
    rule(f"game time within {MAX_GAME_TIME_S:.0f} s", dt <= MAX_GAME_TIME_S,
         server["game_time_s"], client["game_time_s"])
    dp = math.dist(vec(server, "x", "y", "z"), vec(client, "x", "y", "z"))
    rule(f"position within {MAX_POSITION_M} m", dp <= MAX_POSITION_M,
         vec(server, "x", "y", "z"), vec(client, "x", "y", "z"))
    for group, names, limit in (
        ("sun direction", ("sun_dir_x", "sun_dir_y", "sun_dir_z"), MAX_SUN_DIR),
        ("sun colour", ("sun_r", "sun_g", "sun_b"), MAX_COLOUR),
        ("ambient", ("ambient_r", "ambient_g", "ambient_b"), MAX_COLOUR),
        ("sky colour", ("hemi_r", "hemi_g", "hemi_b"), MAX_COLOUR),
    ):
        a, b = vec(server, *names), vec(client, *names)
        rule(f"{group} within {limit}", max(abs(x - y) for x, y in zip(a, b, strict=True)) <= limit, a, b)
    ls, lc = float(server["luminocity"]), float(client["luminocity"])
    lum_ok = abs(ls - lc) <= max(MAX_LUMINOCITY_ABS, MAX_LUMINOCITY_REL * max(ls, lc))
    rule(f"luminocity within {MAX_LUMINOCITY_ABS} or {MAX_LUMINOCITY_REL:.0%}", lum_ok, ls, lc)
    rule("server luminocity above the darkness floor", ls > 0.001, ls, lc)
    if "sample_x" in server and "sample_x" in client:
        ds = math.dist(vec(server, "sample_x", "sample_y", "sample_z"), vec(client, "sample_x", "sample_y", "sample_z"))
        sample = ("sample_x", "sample_y", "sample_z", "sphere_r")
        rule(f"light sample point within {MAX_POSITION_M} m", ds <= MAX_POSITION_M, vec(server, *sample),
             vec(client, *sample))
    hs, hc = float(server["hemi"]), float(client["hemi"])
    rule("sky term within 0.05", abs(hs - hc) <= 0.05, hs, hc)

    summary = {"match": all(r["ok"] for r in rules), "rules": rules, "server": server, "client": client}
    print(json.dumps(summary, indent=1))
    sys.exit(0 if summary["match"] else 1)


if __name__ == "__main__":
    main()

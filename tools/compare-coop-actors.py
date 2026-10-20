"""Check that each player moves only its own actor and that both clients show
both actors where the server has them (S05-T03).

Usage: python3 tools/compare-coop-actors.py <session log dir> [--out <json>]

The session log dir is builds/logs/<task>/run-<run> from Run-CoopSession.ps1
with clients running the S05-T03 script (profile script = 1):
  server/coop-actors.csv          the server's actor positions (UTC ms, tick, entity)
  cN/coop-actors-client.csv       what client N showed (UTC ms, engine id, local)
  cN/coop-world-final.csv         engine id -> entity id on client N
  cN/coop-cN.log                  the script's walk start ("script-walk", UTC ms)

Checks:
  ownership     in each client's walk window (start .. start + 2.5 s) its own
                actor moves more than 0.5 m on the server and the other actor
                less than 0.1 m
  presentation  for every client sample, the distance between the position
                the client showed and the server's position for that actor,
                interpolated at the sample's time minus the lag (default
                150 ms: snapshot interval + easing); reported as median, p95
                and max, while the actor is still and overall
  presence      both actors appear on both clients, exactly one as local
"""

import argparse
import bisect
import csv
import json
import math
import statistics
import sys
from pathlib import Path


def load_server(path):
    tracks = {}
    with open(path, newline="") as f:
        for r in csv.DictReader(f):
            tracks.setdefault(int(r["entity"]), []).append(
                (int(r["utc_ms"]), float(r["x"]), float(r["y"]), float(r["z"])))
    for t in tracks.values():
        t.sort()
    return tracks


def at(track, utc):
    times = [p[0] for p in track]
    i = bisect.bisect_left(times, utc)
    if i <= 0:
        return track[0][1:]
    if i >= len(track):
        return track[-1][1:]
    a, b = track[i - 1], track[i]
    k = (utc - a[0]) / max(1, b[0] - a[0])
    return tuple(a[j] + (b[j] - a[j]) * k for j in (1, 2, 3))


def dist(p, q):
    return math.dist(p, q)


def entity_map(world_csv):
    out = {}
    with open(world_csv) as f:
        f.readline()
        for r in csv.DictReader(f):
            out[int(r["id"])] = int(r["entity"])
    return out


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("session")
    parser.add_argument("--lag-ms", type=int, default=150)
    parser.add_argument("--out")
    args = parser.parse_args()
    root = Path(args.session)
    server = load_server(root / "server/coop-actors.csv")
    clients = sorted(p.name for p in root.iterdir() if p.is_dir() and p.name.startswith("c") and "-" not in p.name)
    result = {"clients": {}, "ownership": [], "lag_ms": args.lag_ms}
    own_entity = {}
    walks = {}
    for c in clients:
        ids = entity_map(root / c / "coop-world-final.csv")
        rows = list(csv.DictReader(open(root / c / "coop-actors-client.csv", newline="")))
        local = {int(r["id"]) for r in rows if r["local"] == "1"}
        remote = {int(r["id"]) for r in rows if r["local"] == "0"}
        if len(local) == 1:
            own_entity[c] = ids.get(next(iter(local)))
        for line in open(root / c / f"coop-{c}.log"):
            if "script-walk" in line:
                walks[c] = int(line.split()[0])
        errors_still, errors_all = [], []
        for r in rows:
            entity = ids.get(int(r["id"]))
            if entity not in server:
                continue
            utc = int(r["utc_ms"])
            shown = (float(r["x"]), float(r["y"]), float(r["z"]))
            truth = at(server[entity], utc - args.lag_ms)
            e = dist(shown, truth)
            errors_all.append(e)
            before = at(server[entity], utc - args.lag_ms - 500)
            if dist(before, truth) < 0.01:
                errors_still.append(e)

        def summary(v):
            if not v:
                return None
            v = sorted(v)
            return {"n": len(v), "median_m": round(statistics.median(v), 4),
                    "p95_m": round(v[int(0.95 * (len(v) - 1))], 4), "max_m": round(v[-1], 4)}

        result["clients"][c] = {
            "local_ids": sorted(local), "remote_ids": sorted(remote),
            "presence_ok": len(local) == 1 and len(remote) >= 1,
            "error_still": summary(errors_still), "error_all": summary(errors_all),
        }
    for c, start in sorted(walks.items()):
        mine = own_entity.get(c)
        moved = {}
        for entity, track in server.items():
            moved[entity] = round(dist(at(track, start), at(track, start + 2500)), 3)
        others = [m for e, m in moved.items() if e != mine]
        ok = mine in moved and moved[mine] > 0.5 and all(m < 0.1 for m in others)
        result["ownership"].append({"client": c, "walk_utc_ms": start, "own_entity": mine,
                                    "moved_m": {str(k): v for k, v in moved.items()}, "ok": ok})
    result["match"] = (all(v["presence_ok"] for v in result["clients"].values()) and
                       bool(result["ownership"]) and all(o["ok"] for o in result["ownership"]))
    text = json.dumps(result, indent=2)
    if args.out:
        Path(args.out).write_text(text + "\n")
    print(text)
    return 0 if result["match"] else 1


if __name__ == "__main__":
    sys.exit(main())

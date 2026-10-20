"""Compare a co-op client's replica with the server's world (S05-T02).

Usage: python3 tools/compare-coop-world.py <server coop-lifecycle.csv> <client coop-world-*.csv> [--out <json>]

The server logs every lifecycle event it published (commit, spawn/despawn,
engine id, entity id, generation, section). Replaying that log up to the
commit the client last applied gives the server's online set at that point.
The client's summary lists what its replica holds: each engine id with the
server entity it maps to, whether the server said it is online (in_set) and
whether the client's level has the object (online_object).

Checks:
  identity  every entity the server had online at the commit is in the
            client's set under the same engine id, entity id, generation
            and section, and the client's set has nothing else
  presence  every entity in the client's set exists on the client's level
"""

import argparse
import csv
import json
import sys


def server_set(path, commit):
    online = {}
    with open(path, newline="") as f:
        for row in csv.DictReader(f):
            if int(row["commit"]) > commit:
                break
            key = int(row["id"])
            if row["kind"] == "spawn":
                online[key] = (int(row["entity"]), int(row["generation"]), row["section"])
            elif online.get(key, (None, None))[:2] == (int(row["entity"]), int(row["generation"])):
                del online[key]
    return online


def client_set(path):
    with open(path) as f:
        header = f.readline().strip().lstrip("#").split()
        meta = dict(item.split("=") for item in header)
        rows = list(csv.DictReader(f))
    in_set = {int(r["id"]): (int(r["entity"]), int(r["generation"]), r["section"])
              for r in rows if r["in_set"] == "1"}
    absent = sorted(int(r["id"]) for r in rows if r["in_set"] == "1" and r["online_object"] != "1")
    return int(meta["applied_commit"]), in_set, absent, rows


def main():
    parser = argparse.ArgumentParser()
    parser.add_argument("server")
    parser.add_argument("client")
    parser.add_argument("--out")
    args = parser.parse_args()
    commit, client, absent, _ = client_set(args.client)
    server = server_set(args.server, commit)
    missing = sorted(set(server) - set(client))
    extra = sorted(set(client) - set(server))
    differ = sorted(k for k in set(server) & set(client) if server[k] != client[k])
    result = {
        "client_applied_commit": commit,
        "server_online_at_commit": len(server),
        "client_set": len(client),
        "identity": {"missing_on_client": missing[:20], "extra_on_client": extra[:20], "differ": differ[:20],
                     "counts": [len(missing), len(extra), len(differ)],
                     "ok": not missing and not extra and not differ},
        "presence": {"absent_from_client_level": absent[:20], "count": len(absent), "ok": not absent},
    }
    result["match"] = result["identity"]["ok"] and result["presence"]["ok"]
    text = json.dumps(result, indent=2)
    if args.out:
        with open(args.out, "w") as f:
            f.write(text + "\n")
    print(text)
    return 0 if result["match"] else 1


if __name__ == "__main__":
    sys.exit(main())

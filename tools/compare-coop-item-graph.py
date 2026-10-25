#!/usr/bin/env python3
"""Check the S08-T03 canonical item graph of a co-op session: the server's and each client's
owner of every item, compared at the same point of the lifecycle stream.

Usage: python3 tools/compare-coop-item-graph.py <session logs> [--out compare-item-graph.json]

Inputs (builds/logs/<task>/run-<run>/): server/ (and server-first-run/): coop-server.log
(`item-graph tag=… seq= items= hash=`), coop-item-graph.csv (seq, item, owner); c*/:
coop-c*.log (`item-graph seq= … match= … missing= extra=`), coop-item-graph-client.csv (the
client's items at a mismatching point).

The server hashes its published items' owners every 10 s and sends the hash as a lifecycle
event; each client compares its replica when it reaches that event (CoopClientLink.cpp
on_item_graph). A sequence number restarts with each server incarnation.

Checks:
  compared        every client compared at least 3 points
  graphs_match    every point a client compared matched the server's hash and item count
  own_inventory   at every point, each client's own actor's inventory held exactly the items its
                  replica names as the actor's children
For a mismatch the report lists the items whose owner differs (from the two csv files).
"""

import argparse
import csv
import json
import re
import sys
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def field(line, name, cast=str):
    m = re.search(r"\b" + re.escape(name) + r"=(\S+)", line)
    return cast(m.group(1)) if m else None


def graph_rows(path):
    out = {}
    if path.exists():
        with open(path, newline="") as f:
            for r in csv.DictReader(f):
                out.setdefault(int(r["seq"]), {})[int(r["item_entity"])] = int(r["owner_entity"])
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="compare-item-graph.json")
    a = ap.parse_args()
    logs = Path(a.logs)
    incarnations = [d for d in (logs / "server-first-run", logs / "server") if (d / "coop-server.log").exists()]
    server_points = sum(1 for d in incarnations for x in lines(d / "coop-server.log") if " item-graph tag=" in x)
    # the last incarnation's items by seq (a client compares against the server it is connected to)
    server_items = graph_rows(incarnations[-1] / "coop-item-graph.csv") if incarnations else {}
    report = {"server_points": server_points, "checks": {}, "clients": {}}
    all_compared, all_match, all_own = True, True, True
    for c in sorted(p.name for p in logs.iterdir() if p.is_dir() and re.fullmatch(r"c\d+", p.name)):
        points = [x for x in lines(logs / c / f"coop-{c}.log") if " item-graph seq=" in x]
        mismatches = [x.split(" ", 1)[1] for x in points if " match=0 " in x]
        own_bad = [x.split(" ", 1)[1] for x in points if field(x, "missing", int) or field(x, "extra", int)]
        client_items = graph_rows(logs / c / "coop-item-graph-client.csv")
        diffs = {}
        for seq, items in client_items.items():
            srv = server_items.get(seq, {})
            only_client = sorted(set(items) - set(srv))[:20]
            only_server = sorted(set(srv) - set(items))[:20]
            owner = sorted(i for i in set(items) & set(srv) if items[i] != srv[i])[:20]
            diffs[str(seq)] = {"only_client": only_client, "only_server": only_server,
                               "owner_differs": [(i, items[i], srv[i]) for i in owner]}
        report["clients"][c] = {"compared": len(points), "matched": len(points) - len(mismatches),
                                "mismatches": mismatches[:10], "own_inventory_bad": own_bad[:10],
                                "diffs": diffs}
        all_compared = all_compared and len(points) >= 3
        all_match = all_match and not mismatches
        all_own = all_own and not own_bad
    report["checks"]["compared"] = {"pass": bool(report["clients"]) and all_compared}
    report["checks"]["graphs_match"] = {"pass": bool(report["clients"]) and all_match}
    report["checks"]["own_inventory"] = {"pass": bool(report["clients"]) and all_own}
    report["pass"] = all(v["pass"] for v in report["checks"].values())
    out = Path(a.out)
    out.write_text(json.dumps(report, indent=2) + "\n")
    for k, v in report["checks"].items():
        print(f"{'PASS' if v['pass'] else 'FAIL'} {k}")
    for c, v in report["clients"].items():
        print(f"{c}: compared={v['compared']} matched={v['matched']} own_inventory_bad={len(v['own_inventory_bad'])}")
    print("overall", "PASS" if report["pass"] else "FAIL", "->", out)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

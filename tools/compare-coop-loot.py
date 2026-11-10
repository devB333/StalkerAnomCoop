#!/usr/bin/env python3
"""Check the S10-T02b NPC loot fixture (plan.md S10-T02b; docs/validation/S10-T02b.md).
Usage: python3 tools/compare-coop-loot.py <session logs> [--out compare-loot.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log, server/xray-campaign-server_*.log,
c1/, c2/: coop-c*.log (no restart in this session).
The fixture (--loot c1,c2, profile loot_party; CoopServerHost.cpp loot_fixtures): two stalkers spawned
beside p2; p2 kills one (the installed death_manager fills the body in p2's context); the other loots the
body through xr_corpse_detection.get_all_from_corpse inside its NPC evaluation scope; the body's release is
asked for as the release manager would, with p2 standing at it.
Checks:
  killed    fixture-loot killed; fixture-loot body dead=1 body_items > 0 (loot generated once, on the server)
  looted    fixture-loot looted body_items=a->b with b < a and looter_items=c->d with d > c
  graphs    both clients' last item-graph point after the loot has match=1 (the moves reached both)
  kept      fixture-loot release-asked still_present=1 and the engine log has body-release-kept for the body
  failures  fixture-loot done failures=0
"""
import argparse
import json
import re
import sys
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def kv(line):
    return dict(re.findall(r"(\w+)=([^\s\"]+|\"[^\"]*\")", line))


def first_kv(log, marker):
    for ln in log:
        if marker in ln:
            return kv(ln)
    return {}


def arrow(value):
    m = re.match(r"(\d+)->(\d+)", value or "")
    return (int(m.group(1)), int(m.group(2))) if m else (None, None)


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    server = lines(root / "server" / "coop-server.log")
    engine = []
    for p in (root / "server").glob("xray-campaign-server*.log"):
        engine += lines(p)
    clients = [lines(root / "c1" / "coop-c1.log"), lines(root / "c2" / "coop-c2.log")]
    checks = {}

    killed = first_kv(server, " fixture-loot killed ")
    body = first_kv(server, " fixture-loot body ")
    checks["killed"] = {"pass": bool(killed) and body.get("dead") == "1" and int(body.get("body_items", 0)) > 0,
                        "killed": killed, "body": body}

    looted = first_kv(server, " fixture-loot looted ")
    ba, bb = arrow(looted.get("body_items"))
    la, lb = arrow(looted.get("looter_items"))
    checks["looted"] = {"pass": ba is not None and bb < ba and la is not None and lb > la, "line": looted}

    loot_ts = None
    for ln in server:
        if " fixture-loot looted " in ln:
            loot_ts = int(ln.split(" ", 1)[0])
            break
    graphs = []
    for log in clients:
        points = [ln for ln in log if " item-graph seq=" in ln and loot_ts and int(ln.split(" ", 1)[0]) > loot_ts]
        graphs.append(kv(points[-1]).get("match") if points else None)
    checks["graphs"] = {"pass": all(g == "1" for g in graphs) and len(graphs) == 2, "last_points": graphs}

    asked = first_kv(server, " fixture-loot release-asked ")
    kept = [ln for ln in engine if "body-release-kept" in ln and f"id={asked.get('body')}" in ln]
    checks["kept"] = {"pass": asked.get("still_present") == "1" and bool(kept), "asked": asked, "kept": kept[:2]}

    done = first_kv(server, " fixture-loot done ")
    checks["failures"] = {"pass": done.get("failures") == "0", "done": done}

    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

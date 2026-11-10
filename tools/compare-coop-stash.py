#!/usr/bin/env python3
"""Check the S10-T04e stash fixture (plan.md S10-T04e; docs/validation/S10-T04e.md).
Usage: python3 tools/compare-coop-stash.py <session logs> [--out compare-stash.json]
Inputs (builds/logs/<task>/run-<run>/): server-first-run/ and server/: coop-server.log (the session is run
with an external server kill and restart: -ServerKill <s> -ServerRestartAfter 10; the stash's spawn
publishes a checkpoint, the take is journaled); c1/, c2/: coop-c*.log.
The fixture (--stash c1,c2, profile stash_party; CoopServerHost.cpp stash_fixtures):
  created   the nearest open inventory box becomes a stash (two items) in p1's context: a "treasure" spot
  opened    p1 uses the box (kind 17): the treasure manager spawns the contents, a checkpoint follows
  taken     p1 takes one item (kind 7): the box's take callback fires, the spot goes
  recovered the restarted server reads the box, p1's items and the spots back
Checks:
  created   fixture-stash created ... created=<box id> (the treasure manager accepted the box)
  spot      both clients logged map-spot kind=map-spot-add ... spot=treasure for the box
  opened    fixture-stash opened reason=0; stash-opened box=... items=a->b published=1
  taken     fixture-stash spawned items=a->b taken=<id> reason=0; box-take-callback box=<box> item=<id>; done
            phase=stashed treasure_spots=0 p1_has=1 box_items=b-1
  spot_gone both clients logged map-spot kind=map-spot-remove ... spot=treasure for the box
  recovered fixture-stash recovered box_items as the first run's done line, treasure_spots=0 and p1 holds the taken
            section
  failures  both fixture-stash done lines have failures=0
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    first = lines(root / "server-first-run" / "coop-server.log")
    second = lines(root / "server" / "coop-server.log")
    if not first:
        first, second = second, []
    clients = [lines(root / "c1" / "coop-c1.log"), lines(root / "c2" / "coop-c2.log")]
    checks = {}

    created = first_kv(first, " fixture-stash created ")
    box = created.get("box")
    checks["created"] = {"pass": box is not None and created.get("created") == box, "line": created}

    def spot_lines(log, kind):
        return [kv(ln) for ln in log if f" map-spot kind={kind} " in ln and "spot=treasure" in ln
                and kv(ln).get("object") == box]

    adds = [spot_lines(c, "map-spot-add") for c in clients]
    checks["spot"] = {"pass": all(adds), "c1": adds[0][:2], "c2": adds[1][:2]}

    opened = first_kv(first, " fixture-stash opened ")
    stash_open = first_kv(first, " stash-opened ")
    checks["opened"] = {"pass": opened.get("reason") == "0" and stash_open.get("box") == box
                        and stash_open.get("published") == "1", "opened": opened, "stash_opened": stash_open}

    spawned = first_kv(first, " fixture-stash spawned ")
    taken = spawned.get("taken")
    callback = [kv(ln) for ln in first if " box-take-callback " in ln]
    done1 = first_kv(first, " fixture-stash done phase=stashed ")
    m = re.match(r"(\d+)->(\d+)", spawned.get("items", ""))
    remaining = str(int(m.group(2)) - 1) if m and int(m.group(2)) > int(m.group(1)) else None
    checks["taken"] = {"pass": spawned.get("reason") == "0" and taken not in (None, "65535")
                       and any(c.get("box") == box and c.get("item") == taken for c in callback)
                       and done1.get("treasure_spots") == "0" and done1.get("p1_has") == "1"
                       and remaining is not None and done1.get("box_items") == remaining,
                       "spawned": spawned, "callbacks": callback[:2], "done": done1}

    removes = [spot_lines(c, "map-spot-remove") for c in clients]
    checks["spot_gone"] = {"pass": all(removes), "c1": removes[0][:2], "c2": removes[1][:2]}

    recovered = first_kv(second, " fixture-stash recovered ")
    section = spawned.get("section", "")
    held = recovered.get(f"p1_{section}s") if section else None
    checks["recovered"] = {"pass": recovered.get("box") == box and recovered.get("box_items") == done1.get("box_items")
                           and recovered.get("treasure_spots") == "0" and held not in (None, "0"),
                           "line": recovered, "section": section}

    done2 = first_kv(second, " fixture-stash done phase=recovered ")
    checks["failures"] = {"pass": done1.get("failures") == "0" and done2.get("failures") == "0", "first": done1,
                          "restart": done2}

    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

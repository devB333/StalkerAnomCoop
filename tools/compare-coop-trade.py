#!/usr/bin/env python3
"""Check the S10-T04c door and trade fixture (plan.md S10-T04c; docs/validation/S10-T04c.md).

Usage: python3 tools/compare-coop-trade.py <session logs> [--out compare-trade.json]

Inputs (builds/logs/<task>/run-<run>/): server-first-run/ and server/: coop-server.log (the session is
run with an external server kill and restart: -ServerKill <s> -ServerRestartAfter 10; no publication is
requested after the trades, so the restart recovers the join checkpoint and replays the journal);
c1/, c2/: coop-c*.log.

The fixture (--trade c1,c2, profile trade_party; CoopServerHost.cpp trade_fixtures):
  door       p2 placed at Sidorovich's bunker door uses it (take_use_object): the installed ph_door logic
             opens it on the server and the joint drive is presented to every client
  trade      p1 opens a talk and the trade (the price table); p2's talk is refused busy (33) and p2's trade
             open not-talking (34); p1 buys the cheapest affordable item and sells one of its own; p1 closes
  recovered  the restarted server replayed the two trade records (trade-replayed); p1's money is the after

Checks:
  door       fixture-trade door-used reason=0, a use-object ... ok=1 line, joint-drive presentations on the
             server and applied on both clients
  opened     fixture-trade opened talk_p1=0 open_p1=0 talk_p2=33 open_p2=34; a trade-prices line with entries
  traded     fixture-trade bought and sold with reason=0; two trade-committed lines whose money arithmetic
             holds (money_after = money - pay + get); two trade-item lines
  client     c1 received the price table (trade-prices ... buy>0) and opened the window (trade-open)
  replayed   the restarted run has two trade-replayed lines and fixture-trade recovered money_p1 equal to
             the first run's final money
  failures   both fixture-trade done lines have failures=0
  condition  (repair slice) fixture-trade worn and repaired lines; c1 applied both condition bytes
             (layout-condition entity=... byte=a->b) to the same entity
  belt       (L48) fixture-trade belt-outfit and belt with reason=0, the artefact at place 2 on the server,
             c1 layout-belt ok=1 for the same entity
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
    c1 = lines(root / "c1" / "coop-c1.log")
    c2 = lines(root / "c2" / "coop-c2.log")
    checks = {}

    door = first_kv(first, " fixture-trade door-used ")
    used = [kv(ln) for ln in first if " use-object " in ln and "ok=" in ln]
    drives_server = sum(1 for ln in first if "kind=joint-drive" in ln)
    drives_c1 = sum(1 for ln in c1 if "kind=joint-drive" in ln and ln.rstrip().endswith("applied"))
    drives_c2 = sum(1 for ln in c2 if "kind=joint-drive" in ln and ln.rstrip().endswith("applied"))
    drives = {"server": drives_server, "c1": drives_c1, "c2": drives_c2}
    checks["door"] = {"pass": door.get("reason") == "0" and any(u.get("ok") == "1" for u in used)
                      and all(v > 0 for v in drives.values()),
                      "door_used": door, "use_object": used[:3], "joint_drives": drives}

    opened = first_kv(first, " fixture-trade opened ")
    prices = [kv(ln) for ln in first if " trade-prices " in ln]
    checks["opened"] = {"pass": opened.get("talk_p1") == "0" and opened.get("open_p1") == "0"
                        and opened.get("talk_p2") == "33" and opened.get("open_p2") == "34"
                        and any(int(p.get("entries", 0)) > 0 for p in prices),
                        "opened": opened, "prices": prices[:3]}

    bought = first_kv(first, " fixture-trade bought ")
    sold = first_kv(first, " fixture-trade sold ")
    committed = [kv(ln) for ln in first if " trade-committed " in ln]
    items = [kv(ln) for ln in first if " trade-item " in ln]
    arithmetic = True
    for c in committed:
        m = re.match(r"(\d+)->(\d+)", c.get("money", ""))
        if not m or int(m.group(2)) != int(m.group(1)) - int(c.get("pay", 0)) + int(c.get("get", 0)):
            arithmetic = False
    checks["traded"] = {"pass": bought.get("reason") == "0" and sold.get("reason") == "0" and len(committed) == 2
                        and len(items) == 2 and arithmetic,
                        "bought": bought, "sold": sold, "committed": committed, "items": items}

    c1_prices = [kv(ln) for ln in c1 if " trade-prices " in ln]
    c1_open = any(" trade-open npc=" in ln for ln in c1)
    checks["client"] = {"pass": any(int(p.get("buy", 0)) > 0 for p in c1_prices) and c1_open,
                        "prices": c1_prices[:2], "window_opened": c1_open}

    replayed = [kv(ln) for ln in second if " trade-replayed " in ln]
    done1 = first_kv(first, " fixture-trade done phase=traded ")
    recovered = first_kv(second, " fixture-trade recovered ")
    checks["replayed"] = {"pass": len(replayed) == 2 and recovered.get("money_p1") is not None
                          and recovered.get("money_p1") == done1.get("money_p1"),
                          "replayed": replayed, "recovered": recovered, "first_final": done1}

    # S10-T04c repair slice: the fixture wears p1's weapon and repairs it on the
    # server; the layout event (kind 9) carries the condition byte and c1 applies it
    worn = first_kv(first, " fixture-trade worn ")
    repaired = first_kv(first, " fixture-trade repaired ")
    c1_cond = [kv(ln) for ln in c1 if " layout-condition " in ln]
    applied = {(c.get("entity"), c.get("byte", "").split("->")[-1]) for c in c1_cond}
    checks["condition"] = {"pass": worn.get("byte") is not None and repaired.get("byte") is not None
                           and (worn.get("entity"), worn.get("byte")) in applied
                           and (repaired.get("entity"), repaired.get("byte")) in applied,
                           "worn": worn, "repaired": repaired, "c1_applied": c1_cond[:6]}

    # L48: an outfit with artefact slots worn, then an artefact on the belt (server reason 0,
    # c1 applied the belt placement to the same entity)
    belt_outfit = first_kv(first, " fixture-trade belt-outfit ")
    belt = first_kv(first, " fixture-trade belt ")
    c1_belt = [kv(ln) for ln in c1 if " layout-belt " in ln]
    checks["belt"] = {"pass": belt_outfit.get("reason") == "0" and belt.get("reason") == "0"
                      and belt.get("place") == "2"
                      and any(b.get("entity") == belt.get("entity") and b.get("ok") == "1" for b in c1_belt),
                      "outfit": belt_outfit, "belt": belt, "c1_belt": c1_belt[:4]}

    done2 = first_kv(second, " fixture-trade done phase=recovered ")
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

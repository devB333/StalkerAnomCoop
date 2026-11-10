#!/usr/bin/env python3
"""Check the S10-T04f upgrade and repair fixture (plan.md S10-T04f; docs/validation/S10-T04f.md).
Usage: python3 tools/compare-coop-upgrade.py <session logs> [--out compare-upgrade.json]
Inputs (builds/logs/<task>/run-<run>/): server-first-run/ and server/: coop-server.log (the session is
run with an external server kill and restart: -ServerKill <s> -ServerRestartAfter 10; no publication is
requested after the fixture, so the restart recovers the join checkpoint and replays the journal);
c1/, c2/: coop-c*.log.
The fixture (--upgrade c1,c2, profile upgrade_party; CoopServerHost.cpp upgrade_fixtures):
  mechanic   p1 placed at the nearest talking mechanic of its community; p1 gets 50000 RU and the mechanic's tier infos
  opened     p1 talks and opens the upgrade session (the table); p2, left where it stood, is refused busy (33:
             checked before the reach) and its upgrade-open not-talking (34)
  installed  p1 installs the first installable upgrade of the first item that has one (op 1)
  repaired   p1's first weapon is worn to 0.5 and repaired (op 2)
  recovered  the restarted server replayed both records (upgrade-replayed); p1's money is the after
Checks:
  mechanic   fixture-upgrade mechanic id=... present
  opened     fixture-upgrade opened talk_p1=0 open_p1=0 talk_p2=33 open_p2=34; a server upgrade-table line with
             items > 0
  installed  fixture-upgrade installed reason=0 has_upgrade=1 and the purse decreased; upgrade-committed op=1
  repaired   fixture-upgrade repaired reason=0 condition=1.00 and the purse decreased; upgrade-committed op=2
  client     c1 received the table (upgrade-table items > 0) and opened the window (upgrade-open); c1 installed
             the same upgrade on its copy (upgrade-installed op=1 installed=1 for the committed item entity);
             c1 applied the repaired condition (layout-condition for the repaired entity ending at 255)
  replayed   the restarted run has two upgrade-replayed lines with done=1 and fixture-upgrade recovered with
             upgrades=[...] non-empty and money_p1 equal to the first run's final money
  failures   both fixture-upgrade done lines have failures=0
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


def money_fell(entry):
    m = re.match(r"(\d+)->(\d+)", entry.get("money", ""))
    return bool(m) and int(m.group(2)) < int(m.group(1))


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
    checks = {}

    mechanic = first_kv(first, " fixture-upgrade mechanic ")
    checks["mechanic"] = {"pass": mechanic.get("id") is not None, "mechanic": mechanic}

    opened = first_kv(first, " fixture-upgrade opened ")
    tables = [kv(ln) for ln in first if " upgrade-table " in ln]
    checks["opened"] = {"pass": opened.get("talk_p1") == "0" and opened.get("open_p1") == "0"
                        and opened.get("talk_p2") == "33" and opened.get("open_p2") == "34"
                        and any(int(t.get("items", 0)) > 0 for t in tables),
                        "opened": opened, "tables": tables[:3]}

    installed = first_kv(first, " fixture-upgrade installed ")
    committed = [kv(ln) for ln in first if " upgrade-committed " in ln]
    install_commit = next((c for c in committed if c.get("op") == "1"), {})
    checks["installed"] = {"pass": installed.get("reason") == "0" and installed.get("has_upgrade") == "1"
                           and money_fell(installed) and bool(install_commit),
                           "installed": installed, "committed": install_commit}

    repaired = first_kv(first, " fixture-upgrade repaired ")
    repair_commit = next((c for c in committed if c.get("op") == "2"), {})
    checks["repaired"] = {"pass": repaired.get("reason") == "0" and repaired.get("condition") == "1.00"
                          and money_fell(repaired) and bool(repair_commit),
                          "repaired": repaired, "committed": repair_commit}

    c1_tables = [kv(ln) for ln in c1 if " upgrade-table " in ln]
    c1_open = any(" upgrade-open npc=" in ln for ln in c1)
    c1_installed = [kv(ln) for ln in c1 if " upgrade-installed " in ln]
    c1_cond = [kv(ln) for ln in c1 if " layout-condition " in ln]
    same_install = any(x.get("op") == "1" and x.get("installed") == "1"
                       and x.get("item_entity") == install_commit.get("item_entity") for x in c1_installed)
    same_repair = any(x.get("entity") == repair_commit.get("item_entity") and x.get("byte", "").endswith("->255")
                      for x in c1_cond)
    checks["client"] = {"pass": any(int(t.get("items", 0)) > 0 for t in c1_tables) and c1_open and same_install
                        and same_repair,
                        "tables": c1_tables[:2], "window_opened": c1_open, "installed": c1_installed[:4],
                        "condition": c1_cond[:4]}

    replayed = [kv(ln) for ln in second if " upgrade-replayed " in ln]
    done1 = first_kv(first, " fixture-upgrade done phase=upgraded ")
    recovered = first_kv(second, " fixture-upgrade recovered ")
    checks["replayed"] = {"pass": len(replayed) == 2 and all(r.get("done") == "1" for r in replayed)
                          and recovered.get("money_p1") is not None
                          and recovered.get("money_p1") == done1.get("money_p1")
                          and recovered.get("upgrades", "[]") not in ("[]", ""),
                          "replayed": replayed, "recovered": recovered, "first_final": done1}

    done2 = first_kv(second, " fixture-upgrade done phase=recovered ")
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

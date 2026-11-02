#!/usr/bin/env python3
"""Check the S10-T05b general party-task fixture (plan.md S10-T05b; docs/validation/S10-T05b.md).

Usage: python3 tools/compare-coop-party-task.py <session logs> [--out compare-party-task.json]

Inputs (builds/logs/<task>/run-<run>/): server-first-run/ and server/: coop-server.log (the session is
run with an external server kill and restart: -ServerKill <s> -ServerRestartAfter 10; the same profile
runs the fixture's recovered half).

The fixture (--party-task c1,c2, profile task_party; CoopServerHost.cpp task_fixtures):
  accepted     (first run) p2 accepts Sidorovich's bounty esc_m_trader_task_2 through party_accept_task;
               the installed accept phrases pick a target (bounties_by_id); a checkpoint is requested
  recovered    (restarted server) the task was rebuilt as the party's; p2 kills the target online (an
               offline target is first brought beside p1 by the ALife teleport); p2 turns the task in
               through party_turn_in_task; quest_tick applies the resolved reward
  item-probe   xr_effects.reward_item('bandage') under a probe turn-in in p1's context: given to both
  death-probe  a stalker beside p1 killed by p2: npc_on_death_callback saw AC_ID == who == p2

Checks:
  accepted     the first run logged fixture-task accepted with reason=0, party=1 and a target >= 0, then a
               requested publication (fixture-task done phase=accepted ... publication=1, failures=0)
  rebuilt      the restarted run rebuilt the task (quest-rebuilt task=esc_m_trader_task_2; fixture-task
               rebuilt=1) with the same target as the first run
  completed    the target was killed online by p2 and read dead once the task staged (fixture-task target-dead=1),
               the turn-in was accepted and the restarted run logged quest-completed task=esc_m_trader_task_2
  paid         exactly two quest-paid lines for the task, to two different actors, the same amount > 0
  goodwill     two quest-goodwill lines for the task (one per participant actor), community stalker
  item_probe   two quest-item-given lines for task=probe, section bandage, ok=1, to two different actors; G10:
               the bread created on the anchor (quest-item-shared created_on=<anchor>) given once to the other actor
  g10_probes   G10: fixture-task purse-probe pass=1 (the clicker paid the fee, both got the payment) and
               fail-probe pass=1 (a fail path's goodwill loss on both actors)
  death_probe  fixture-task death-probe ok=1 (npc == the spawned stalker, ac == who == p2's actor)
  failures     the restarted run's fixture-task done phase=recovered has failures=0
"""

import argparse
import json
import re
import sys
from pathlib import Path

TASK = "esc_m_trader_task_2"


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
    checks = {}
    detail = {}

    acc = first_kv(first, " fixture-task accepted ")
    done1 = first_kv(first, " fixture-task done phase=accepted ")
    target1 = int(acc.get("target", -1))
    checks["accepted"] = {"pass": acc.get("reason") == "0" and acc.get("party") == "1" and target1 >= 0
                          and done1.get("publication") == "1" and done1.get("failures") == "0",
                          "accepted": acc, "done": done1}

    rebuilt_lines = [kv(ln) for ln in second if " quest-rebuilt " in ln and f"task={TASK}" in ln]
    reb = first_kv(second, " fixture-task rebuilt=")
    target2 = int(reb.get("target", -1))
    checks["rebuilt"] = {"pass": bool(rebuilt_lines) and reb.get("rebuilt") == "1" and target2 >= 0
                         and target2 == target1,
                         "quest_rebuilt": rebuilt_lines, "fixture": reb}

    killed = first_kv(second, " fixture-task target-killed ")
    dead = first_kv(second, " fixture-task target-dead=")
    turned = first_kv(second, " fixture-task turned-in ")
    completed = [kv(ln) for ln in second if " quest-completed " in ln and f"task={TASK}" in ln]
    comp = first_kv(second, " fixture-task completed=")
    checks["completed"] = {"pass": killed.get("online") == "1" and dead.get("dead") == "1"
                           and turned.get("reason") == "0" and len(completed) == 1 and comp.get("completed") == "1",
                           "target_killed": killed, "target_dead": dead, "turned_in": turned,
                           "quest_completed": completed}

    paid = [kv(ln) for ln in second if " quest-paid " in ln and f"task={TASK}" in ln]
    amounts = {int(p.get("amount", 0)) for p in paid}
    actors = {p.get("actor") for p in paid}
    checks["paid"] = {"pass": len(paid) == 2 and len(actors) == 2 and len(amounts) == 1 and min(amounts, default=0) > 0,
                      "payments": paid}

    gw = [kv(ln) for ln in second if " quest-goodwill " in ln and f"task={TASK}" in ln]
    gw_actors = {g.get("actor") for g in gw}
    checks["goodwill"] = {"pass": len(gw) == 2 and len(gw_actors) == 2
                          and all(g.get("community") == "stalker" for g in gw),
                          "goodwill": gw}

    probe_given = [kv(ln) for ln in second if " quest-item-given " in ln and "task=probe" in ln]
    given = [g for g in probe_given if g.get("section") == "bandage"]
    bread = [g for g in probe_given if g.get("section") == "bread"]
    shared = first_kv(second, " quest-item-shared ")
    given_actors = {g.get("actor") for g in given}
    probe = first_kv(second, " fixture-task item-probe ")
    checks["item_probe"] = {"pass": len(given) == 2 and len(given_actors) == 2
                            and all(g.get("ok") == "1" for g in given) and len(probe_given) == 3
                            and len(bread) == 1 and bread[0].get("ok") == "1"
                            and bread[0].get("actor") != shared.get("created_on")
                            and probe.get("applied") == "1",
                            "given": probe_given, "shared": shared, "fixture": probe}
    purse = first_kv(second, " fixture-task purse-probe ")
    fail = first_kv(second, " fixture-task fail-probe ")
    checks["g10_probes"] = {"pass": purse.get("pass") == "1" and fail.get("pass") == "1",
                            "purse": purse, "fail": fail}

    death = first_kv(second, " fixture-task death-probe ")
    checks["death_probe"] = {"pass": death.get("ok") == "1", "probe": death}

    done2 = first_kv(second, " fixture-task done phase=recovered ")
    checks["failures"] = {"pass": done2.get("failures") == "0", "done": done2}

    detail["effects"] = [kv(ln) for ln in second if " quest-effects " in ln]
    detail["news"] = [ln.split(" quest-news ", 1)[1] for ln in first + second if " quest-news " in ln]
    report = {"checks": checks, "detail": detail, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

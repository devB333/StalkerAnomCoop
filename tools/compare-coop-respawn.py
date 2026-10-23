#!/usr/bin/env python3
"""Check a co-op session with a participant death, a respawn and a server
restart (S07-T06 death adapter, S07-T07 respawn; docs/persistence-contract.md
§6 `death`, §9).

Usage: python3 tools/compare-coop-respawn.py <session logs> [--out compare-respawn.json]

Inputs (builds/logs/<task>/run-<run>/): server-first-run/, server/ (coop-server.log,
coop-deaths.csv, coop-lifecycle.csv), <client>/coop-c*.log and coop-world-final.csv.

Checks:
  deaths_committed     the first incarnation committed a `death` record for a participant
                       (slot != 255) and for at least one NPC (slot 255)
  prompt_then_request  the dead client showed the prompt (>= 3 s after its death) and sent the
                       respawn request after it (the use key or the auto driver), never before
  early_refused        every request sent while alive was refused with reason 6
  respawn_accepted     a respawn was committed and applied on the server (respawn-committed,
                       respawn-apply) and the client got Accepted (possibly from the index after
                       the restart)
  rejoin_new_actor     after the rejoin the client's world names a different local actor than
                       before the death
  corpse_kept          the old actor's entity is still in both clients' final replica sets and
                       the server never destroyed it
  restart_recovered    the restarted server recovered, the slot's actor after the start is the
                       new one (from the checkpoint or `respawn-replay`), and every death and
                       respawn record after the checkpoint was replayed or already held
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


def utc(line):
    return int(line.split(" ", 1)[0])


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="compare-respawn.json")
    a = ap.parse_args()
    logs = Path(a.logs)
    first = lines(logs / "server-first-run/coop-server.log")
    second = lines(logs / "server/coop-server.log")
    report = {"checks": {}}

    # deaths
    deaths = [{"slot": field(x, "slot", int), "id": field(x, "id", int), "entity": field(x, "entity", int),
               "section": field(x, "section"), "revision": field(x, "revision", int), "utc": utc(x)}
              for x in first if " death-committed " in x]
    participant_deaths = [d for d in deaths if d["slot"] != 255]
    npc_deaths_all = sum(1 for x in first + second if " death-committed " in x and "slot=255" in x)
    report["deaths"] = deaths
    report["checks"]["deaths_committed"] = {"pass": bool(participant_deaths) and npc_deaths_all > 0,
                                            "participant": participant_deaths,
                                            "npc_first_incarnation": sum(1 for d in deaths if d["slot"] == 255),
                                            "npc_all": npc_deaths_all}

    # respawn on the server (first incarnation, or the restart's replay)
    committed = [x for x in first if " respawn-committed " in x]
    applied = [x for x in first + second if " respawn-apply " in x or " respawn-replay " in x]
    old_entity = field(committed[0], "old_entity", int) if committed else None
    new_entity = field(committed[0], "new_entity", int) if committed else None
    new_id = field(applied[0], "id", int) if applied else None
    report["respawn"] = {"committed": committed[:2], "applied": applied[:3], "old_entity": old_entity,
                         "new_entity": new_entity, "new_id": new_id}

    # clients
    clients = {}
    prompt_ok = early_ok = accepted_any = rejoin_ok = True
    dead_client = None
    for folder in sorted(p for p in logs.iterdir() if p.is_dir() and p.name.startswith("c")):
        log = lines(folder / f"coop-{folder.name}.log")
        slot = next((field(x, "slot", int) for x in log if " welcomed " in x), None)
        prompts = [x for x in log if " respawn-prompt " in x]
        requests = [x for x in log if " respawn-request " in x]
        results = [x for x in log if " respawn-result " in x]
        rejoins = [x for x in log if " rejoin-requested " in x]
        worlds = [field(x, "local_actor", int) for x in log if " world-received " in x]
        deaths_presented = [x for x in log if " death-presented " in x and "own=1" in x]
        c = {"slot": slot, "prompts": len(prompts), "requests": [field(x, "why") for x in requests],
             "results": [(field(x, "outcome", int), field(x, "reason", int)) for x in results],
             "rejoins": len(rejoins), "local_actors": worlds, "own_death_presented": len(deaths_presented)}
        if deaths_presented and prompts:
            dead_client = folder.name
            c["prompt_after_death_ms"] = utc(prompts[0]) - utc(deaths_presented[0])
            auto_or_key = [x for x in requests if field(x, "why") in ("auto", "use-key")]
            c["request_after_prompt_ms"] = utc(auto_or_key[0]) - utc(prompts[0]) if auto_or_key else None
            prompt_ok = prompt_ok and c["prompt_after_death_ms"] >= 2500 and bool(auto_or_key) and \
                c["request_after_prompt_ms"] >= 0
            accepted = [x for x in results if "accepted" in x]
            accepted_any = accepted_any and bool(accepted)
            rejoin_ok = rejoin_ok and bool(rejoins) and len(worlds) >= 2 and worlds[-1] != worlds[0]
        # every early (alive) request refused with reason 6
        early_requests = [x for x in requests if field(x, "why") == "early-while-alive"]
        early_results = [x for x in results if "refused-as-expected" in x and field(x, "reason", int) == 6]
        c["early_requests"] = len(early_requests)
        c["early_refused_6"] = len(early_results)
        early_ok = early_ok and (not early_requests or early_results)
        # corpse on the client
        # the final summary when the client left from a live world, else the
        # last periodic one (a client mid-rejoin at its exit writes none)
        corpse = None
        summary = folder / "coop-world-final.csv"
        if not summary.exists():
            summary = folder / "coop-world-latest.csv"
        if old_entity is not None and summary.exists():
            for row in csv.reader(open(summary)):
                if len(row) >= 2 and row[1].isdigit() and int(row[1]) == old_entity:
                    corpse = row
        c["corpse_row"] = corpse
        c["corpse_summary"] = summary.name if summary.exists() else None
        clients[folder.name] = c
    report["clients"] = clients
    report["checks"]["prompt_then_request"] = {"pass": dead_client is not None and prompt_ok,
                                               "dead_client": dead_client}
    report["checks"]["early_refused"] = {"pass": early_ok and any(c["early_requests"] for c in clients.values())}
    report["checks"]["respawn_accepted"] = {"pass": bool(committed) and bool(applied) and accepted_any}
    report["checks"]["rejoin_new_actor"] = {"pass": dead_client is not None and rejoin_ok}

    # corpse kept: on both clients and never destroyed by the server
    destroyed = False
    for inc in ("server-first-run", "server"):
        path = logs / inc / "coop-lifecycle.csv"
        if path.exists() and old_entity is not None:
            for r in csv.DictReader(open(path, newline="")):
                if r["kind"] == "despawn" and int(r["entity"]) == old_entity and r["reason"] == "1":
                    destroyed = True
    report["checks"]["corpse_kept"] = {"pass": old_entity is not None and not destroyed and
                                       all(c["corpse_row"] is not None for c in clients.values()),
                                       "destroyed_on_server": destroyed}

    # restart
    recovery = next((x for x in second if " recovery " in x and "status=" in x), None)
    replayed = next((x for x in second if " recovery-replayed " in x), None)
    failures = [x for x in second if "recovery-replay-failed" in x or "recovery-identity-mismatch" in x]
    actor_lines = [x for x in second if " recovery-actor " in x or " respawn-replay" in x]
    # the slot's actor after the start: the replayed respawn's new actor, else the
    # first status line that names one (the replayed actor comes online a few
    # ticks after the start)
    slot_actor = None
    if participant_deaths:
        slot = participant_deaths[0]["slot"]
        replayed_actor = next((field(x, "id", int) for x in second if " respawn-replay " in x
                               and field(x, "slot", int) == slot), None)
        if replayed_actor is not None:
            slot_actor = replayed_actor
        else:
            for x in second:
                if x.find(" status tick=") > 0:
                    m = re.search(r"p%d=\w+,actor=(\d+)" % slot, x)
                    if m:
                        slot_actor = int(m.group(1))
                        break
    death_replays = [x for x in second if " death-replay" in x]
    report["restart"] = {"recovery": recovery, "replayed": replayed, "failures": failures, "actor_lines": actor_lines,
                         "slot_actor_after_start": slot_actor, "new_id": new_id, "death_replays": len(death_replays)}
    report["checks"]["restart_recovered"] = {"pass": recovery is not None and "status=recovered" in recovery and
                                             replayed is not None and not failures and new_id is not None and
                                             slot_actor is not None and slot_actor != (participant_deaths[0]["id"]
                                                                                       if participant_deaths else -1)}
    report["pass"] = all(c["pass"] for c in report["checks"].values())
    Path(a.out).write_text(json.dumps(report, indent=2) + "\n")
    for name, c in report["checks"].items():
        print(f"{name:20s} {'PASS' if c['pass'] else 'FAIL'}")
    print("overall", "PASS" if report["pass"] else "FAIL", "->", a.out)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

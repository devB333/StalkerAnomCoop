#!/usr/bin/env python3
"""B57: write the quest sweep fixture's task list from the installed task configs.

Reads configs/misc/task/tm_*.ltx from the installed configs.db0 (read only, via
xray-db-extract.py) and writes, for the chosen task files:

  <out>.txt   one line per task: "<task id> <kind> <giver>"
              kind:  fetch (actor_has_fetch_item), bounty (bounty_task), scripted (its reward reads a
                     scripted event's state: skipped) or forced
              giver: npc:<section or object name>  the NPC whose prefix gives it
                     sim:<faction>                  a random stalker of that faction
                     none                           given directly (story tasks)
  <out>.json  per task: its file, status functor, giver, kind and the rewards its
              on_complete names (money / items / goodwill / stash), read by
              compare-coop-quest-sweep.py

Usage:
  python3 tools/make-quest-sweep-list.py builds/quest-sweep/cordon --files tm_escape.ltx,tm_dynamic.ltx
  python3 tools/make-quest-sweep-list.py builds/quest-sweep/marsh --files tm_marsh.ltx
"""
import argparse
import json
import re
import subprocess
import sys
import tempfile
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
CONFIGS_DB = Path("/mnt/c/Users/djb20/StalkerAnom/db/configs/configs.db0")

# rewards only: drx_sl_money_task_payment is a fee the turning-in player pays (G10-1), not a payment to him
MONEY = {"reward_random_money", "reward_money", "reward_random_money_by_dist", "fetch_reward_and_remove"}
ITEMS = {"reward_item", "reward_random_item", "give_letter"}
GOODWILL = {"complete_task_inc_goodwill", "inc_goodwill_by_tasker_comm", "inc_goodwill_by_tasker_id",
            "inc_faction_goodwill_to_actor"}


def read_tasks(path: Path) -> dict:
    tasks, cur = {}, None
    for raw in path.read_text(encoding="latin-1").splitlines():
        line = raw.split(";")[0].rstrip()
        m = re.match(r"^\[([^\]]+)\]", line)
        if m:
            cur = m.group(1)
            tasks[cur] = {}
            continue
        if cur and "=" in line:
            k, v = line.split("=", 1)
            tasks[cur][k.strip()] = v.strip()
    return {k: v for k, v in tasks.items() if "title" in v or "status_functor" in v}


def giver_of(task_id: str, fields: dict) -> str:
    if task_id.startswith("simulation_task_"):
        communities = [c.strip() for c in fields.get("sim_communities", "").split(",") if c.strip()]
        return f"sim:{communities[0]}" if communities else "none"
    m = re.match(r"^drx_sl_(.+?)_(?:default_task|cf_task_\w+|task_\w+)$", task_id)
    if m:
        return f"npc:{m.group(1)}"
    m = re.match(r"^(.+)_task_\w+$", task_id)
    if m and not task_id.startswith("drx_sl_"):
        return f"npc:{m.group(1)}"
    return "none"


# on_complete effects that read the state of a scripted event the task plays out (a defense battle):
# forcing such a task's goal skips the event and the effect faults (bugfix-79), so the sweep skips it
SCRIPTED = {"pump_station_defense_reward", "pump_station_defense_task_cleanup"}


def kind_of(fields: dict) -> str:
    effects = set(re.findall(r"=([a-z_0-9]+)\b", fields.get("on_complete", "")))
    if effects & SCRIPTED:
        return "scripted"
    functor = fields.get("status_functor", "")
    return {"actor_has_fetch_item": "fetch", "bounty_task": "bounty"}.get(functor, "forced")


def main() -> int:
    parser = argparse.ArgumentParser(description=__doc__, formatter_class=argparse.RawDescriptionHelpFormatter)
    parser.add_argument("out", help="output path without extension (writes .txt and .json)")
    parser.add_argument("--files", required=True, help="comma-separated tm_*.ltx names to include")
    parser.add_argument("--configs-db", default=str(CONFIGS_DB))
    args = parser.parse_args()
    files = [f.strip() for f in args.files.split(",") if f.strip()]
    if "tm_warfare.ltx" in files:
        print("tm_warfare.ltx is not part of co-op (story mode only)", file=sys.stderr)
        return 2
    with tempfile.TemporaryDirectory() as scratch:
        cmd = [sys.executable, str(ROOT / "tools" / "xray-db-extract.py"), "-q", "extract", "-m", "configs/misc/task/",
               "-o", scratch, args.configs_db]
        subprocess.run(cmd, check=True)
        task_dir = next(Path(scratch).rglob("tm_escape.ltx")).parent
        entries = []
        for name in files:
            path = task_dir / name
            if not path.exists():
                print(f"no such task file: {name}", file=sys.stderr)
                return 2
            for task_id, fields in read_tasks(path).items():
                effects = re.findall(r"=([a-z_0-9]+)\b", fields.get("on_complete", ""))
                entries.append({
                    "id": task_id, "file": name, "status_functor": fields.get("status_functor", ""),
                    "kind": kind_of(fields), "giver": giver_of(task_id, fields),
                    "stage_complete": fields.get("stage_complete", ""),
                    "money": any(e in MONEY for e in effects), "items": any(e in ITEMS for e in effects),
                    "goodwill": any(e in GOODWILL for e in effects), "stash": "reward_stash" in effects,
                    "effects": effects,
                })
    out = Path(args.out)
    out.parent.mkdir(parents=True, exist_ok=True)
    lines = ["# B57 quest sweep list: <task id> <kind> <giver> (tools/make-quest-sweep-list.py)",
             f"# files: {','.join(files)}"]
    lines += [f"{e['id']} {e['kind']} {e['giver']}" for e in entries]
    out.with_suffix(".txt").write_text("\n".join(lines) + "\n", encoding="ascii")
    out.with_suffix(".json").write_text(json.dumps({"files": files, "tasks": entries}, indent=1) + "\n")
    kinds = {}
    for e in entries:
        kinds[e["kind"]] = kinds.get(e["kind"], 0) + 1
    by_kind = ", ".join(f"{k} {n}" for k, n in sorted(kinds.items()))
    print(f"{len(entries)} tasks -> {out.with_suffix('.txt')} ({by_kind})")
    return 0


if __name__ == "__main__":
    sys.exit(main())

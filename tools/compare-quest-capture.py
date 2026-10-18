"""Compare the S02-T06e quest rows of the campaign server and the normal client.

Usage: python3 tools/compare-quest-capture.py <server.csv> <client-restored-completed.csv> <client-persisted.csv>

The server capture (xrGame/CampaignQuestCapture.h) holds the rows
``before-give``, ``given``, ``ticked``, ``restored``, ``completed`` and
``persisted``. The first client capture, taken by a normal client started
from the server's first checkpoint (task given), holds ``client-restored``
and the client's completion row, written by the shared completion code under
the label ``completed``; the second, from the server's second checkpoint
(task completed), holds ``client-persisted``.

Rules, the server's own rows first (the same rules the fixture asserts in
C++, so one summary holds both products):

* ``given``: the native task exists in progress and is active; the Lua task
  is ``selected`` at stage 0 with the giver recorded; the fetch item is in
  the portable store; no reward yet; the dialogue is closed.
* ``restored`` against ``given``: same giver id, native task in progress with
  the same receive time, Lua task loaded from the marshal and ``selected`` at
  stage 0 with the same giver and fetch item, no reward, no item.
* ``completed`` against ``given``: native task completed, Lua entry retired,
  info portion set, money and goodwill higher, no item left, dialogue closed.
* ``persisted`` against ``completed``: native task completed with the given
  receive time, Lua entry retired, info portion, money and goodwill equal,
  no item.

Then the client rows: ``client-restored`` against the server's ``given``
with the restored rules, ``client-completed`` against ``client-restored``
with the completed rules, and ``client-persisted`` against the server's
``completed`` with the persisted rules (the client loaded the server's
second checkpoint, so its money, goodwill and info portion must be the
server's). Prints a JSON summary and exits 1 on any mismatch.
"""

import csv
import json
import sys

INT_FIELDS = {"giver_id", "giver_se", "giver_alive", "giver_online", "giver_live", "native_task", "native_state",
              "receive_time", "native_total", "native_inprogress", "native_completed", "active_is_task", "tm_updates",
              "state_updates", "lua_task", "lua_stage", "lua_loaded", "lua_has_t", "lua_checked", "lua_giver_id",
              "fetch_count", "items_in_inventory", "money", "goodwill_csky", "info_tier1", "actor_talking",
              "npc_talking", "used_npc_event", "available_count", "spawned_items"}
IN_PROGRESS = 1
COMPLETED = 2


def load(path):
    rows = {}
    with open(path, newline="") as handle:
        reader = csv.reader(handle)
        header = next(reader)
        for values in reader:
            if not values:
                continue
            row = dict(zip(header, values, strict=True))
            for key in INT_FIELDS:
                row[key] = int(row[key])
            rows[row["label"]] = row
    return rows


def given_rules(g):
    return {
        "giver_present": (g["giver_se"] == 1 and g["giver_alive"] == 1 and g["giver_online"] == 1
                          and g["giver_live"] == 1),
        "native_in_progress": g["native_task"] == 1 and g["native_state"] == IN_PROGRESS,
        "native_active": g["active_is_task"] == 1,
        "lua_selected_stage_0": g["lua_task"] == 1 and g["lua_status"] == "selected" and g["lua_stage"] == 0,
        "lua_giver_recorded": g["lua_giver_id"] == g["giver_id"],
        "fetch_item_stored": g["fetch_section"] != "-" and g["fetch_count"] >= 1,
        "no_item_yet": g["items_in_inventory"] == 0,
        "no_reward_yet": g["info_tier1"] == 0,
        "dialogue_closed": g["actor_talking"] == 0 and g["npc_talking"] == 0 and g["used_npc_event"] == 0,
    }


def restored_rules(given, r):
    return {
        "same_task": r["task_id"] == given["task_id"],
        "same_giver": r["giver_se"] == 1 and r["giver_alive"] == 1 and r["giver_id"] == given["giver_id"],
        "native_in_progress": r["native_task"] == 1 and r["native_state"] == IN_PROGRESS,
        "receive_time_kept": r["receive_time"] == given["receive_time"],
        "native_active": r["active_is_task"] == 1,
        "lua_loaded_selected_stage_0": (r["lua_task"] == 1 and r["lua_loaded"] == 1 and r["lua_status"] == "selected"
                                        and r["lua_stage"] == 0),
        "lua_giver_kept": r["lua_giver_id"] == given["lua_giver_id"],
        "fetch_item_kept": r["fetch_section"] == given["fetch_section"] and r["fetch_count"] == given["fetch_count"],
        "lua_refound_native": r["lua_has_t"] == 1 and r["lua_checked"] == 1,
        "no_reward_crossed": (r["money"] == given["money"] and r["goodwill_csky"] == given["goodwill_csky"]
                              and r["info_tier1"] == 0),
        "no_item": r["items_in_inventory"] == 0,
    }


def completed_rules(before, c):
    return {
        "item_spawned": c["spawned_items"] >= 1,
        "native_completed": c["native_task"] == 1 and c["native_state"] == COMPLETED,
        "native_counts_moved": (c["native_completed"] > before["native_completed"]
                                and c["native_inprogress"] + 1 == before["native_inprogress"]),
        "lua_retired": c["lua_task"] == 0,
        "info_portion_set": c["info_tier1"] == 1,
        "reward_paid": c["money"] > before["money"],
        "goodwill_raised": c["goodwill_csky"] > before["goodwill_csky"],
        "item_removed": c["items_in_inventory"] == 0,
        "dialogue_closed": c["actor_talking"] == 0 and c["npc_talking"] == 0 and c["used_npc_event"] == 0,
    }


def persisted_rules(completed, given, p):
    return {
        "native_completed": p["native_task"] == 1 and p["native_state"] == COMPLETED,
        "receive_time_kept": p["receive_time"] == given["receive_time"],
        "native_counts_kept": (p["native_completed"] == completed["native_completed"]
                               and p["native_inprogress"] == completed["native_inprogress"]),
        "lua_retired": p["lua_task"] == 0,
        "info_portion_kept": p["info_tier1"] == 1,
        "money_kept": p["money"] == completed["money"],
        "goodwill_kept": p["goodwill_csky"] == completed["goodwill_csky"],
        "no_item": p["items_in_inventory"] == 0,
    }


def details(row):
    return {key: row[key] for key in ("task_id", "giver_id", "native_state", "receive_time", "native_inprogress",
                                      "native_completed", "lua_status", "lua_stage", "lua_loaded", "fetch_section",
                                      "fetch_count", "items_in_inventory", "money", "goodwill_csky", "info_tier1",
                                      "tm_updates", "state_updates")}


def main():
    if len(sys.argv) != 4:
        raise SystemExit(__doc__)
    server = load(sys.argv[1])
    client = load(sys.argv[2])
    client_persisted = load(sys.argv[3])
    summary = {"server": sys.argv[1], "client": sys.argv[2], "client_persisted": sys.argv[3], "comparisons": {}}
    ok = True

    def record(name, rows, label, checks_for):
        nonlocal ok
        if label not in rows:
            summary["comparisons"][name] = {"missing": label}
            ok = False
            return
        checks = checks_for(rows[label])
        summary["comparisons"][name] = {"checks": checks, "details": details(rows[label]), "ok": all(checks.values())}
        ok = ok and all(checks.values())

    missing = [label for label in ("given", "completed") if label not in server]
    if missing:
        summary["comparisons"]["server"] = {"missing": missing}
        print(json.dumps(summary, indent=2))
        return 1
    given, completed = server["given"], server["completed"]
    record("server-given", server, "given", given_rules)
    record("server-restored", server, "restored", lambda r: restored_rules(given, r))
    record("server-completed", server, "completed", lambda c: completed_rules(given, c))
    record("server-persisted", server, "persisted", lambda p: persisted_rules(completed, given, p))
    record("client-restored", client, "client-restored", lambda r: restored_rules(given, r))
    if "client-restored" in client:
        record("client-completed", client, "completed", lambda c: completed_rules(client["client-restored"], c))
    else:
        summary["comparisons"]["client-completed"] = {"missing": "client-restored"}
        ok = False
    record("client-persisted", client_persisted, "client-persisted", lambda p: persisted_rules(completed, given, p))
    summary["match"] = ok
    print(json.dumps(summary, indent=2))
    return 0 if ok else 1


if __name__ == "__main__":
    sys.exit(main())

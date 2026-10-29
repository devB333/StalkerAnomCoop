# Session handoff — S10-T05b started (every quest is a party quest); paused mid-edit

## State (2026-10-29, end of session)

- **Completed this session, all committed:** S10-T04a the talk window
  ([S10-T04a](../../validation/S10-T04a.md), patch 0058, talk-7/8/9) and
  S10-T04b party info portions and actor-2 script state
  ([S10-T04b](../../validation/S10-T04b.md), patch 0059, info-1). Builds
  **campaign-313 / client-210**, `proto-build-49` (v1.6; kinds 14–16,
  reasons 33–36, CampaignEvent 14).
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, and the
  two taken with the user on 2026-10-29: **T05b-1** every task is the
  party's (one adapter per progress family, no allow list) and **T05b-2**
  the reward goes to both players in full, the same amount each (the
  Stage 8 fetch rule, money per own items, stays for fetch tasks).
- **The user reordered the work:** S10-T05b (the general quest system)
  runs **before** S10-T04c (trade). The user still owes: the hands-on
  talk at Sidorovich (two windows: the window's look, the busy HUD hint,
  the voice lines); the trade-UI choice (native drag-and-drop window with
  server prices, my recommendation, or a simpler list); whether upgrades
  stay in S10-T04c or get their own child. The S10-T04c design is in the
  snapshot [before S10-T05b](../done/snapshot-before-S10-T05b.md).
- **S10-T05b In Progress, paused mid-edit.** Engine-work holds unbuilt
  edits, exported as the **work-in-progress patch 0060
  `party-tasks-general`** (check 68; `patches/engine/README.md` says it is
  replaced in place when the task completes). Done so far:
  - `CampaignScriptContext.{h,cpp}`: `party_inventory_view()` is true
    inside a task scope on the server (`ScriptContextScope(const CGameTask*)`
    sets `m_task`, a depth counter).
  - `script_game_object3.cpp` `GetObjectByName` and
    `script_game_object_inventory_owner.cpp` `IterateInventory`: on a
    participant actor with the view on, the other participant actors'
    inventories are searched or iterated too (a fetch-style check counts
    the party's items; includes added).
  - `entity_alive.cpp` `Die` and `hit_memory_manager.cpp`: the installed
    death and hit callbacks run in the participant attacker's context
    (`ScriptContextScope(actor)` when `who` is an actor; the scope refuses
    off the server), so `AC_ID` and `db.actor` name the killer (T05b-3, a
    decision row still to write).
  **Nothing built, nothing run.** `CoopServerHost.cpp` is untouched for
  S10-T05b.

## What the reading established (reuse, do not re-read)

611 task sections in `tm_*.ltx`; status functors by count:
`actor_has_fetch_item` 206 (items in `db.actor`'s inventory: the party
view covers it, the S08 pooled wrapper stays), `assault_task_status_functor`
80 (world: squads at smarts, `pstor` on the owner), `bounty_task` 42
(`se_obj:alive()`; `axr_task_manager.npc_on_death_callback` completes the
task whoever killed; `tagged_by_actor` is set by the hit callback with
`AC_ID`), `special_steps_for_lostzone_tasks` 34 (`has_alife_info` shared by
T04b-1, items by the party view), the DoctorX `drx_sl_*` 41 (items: the
view; money: `db.actor:money()` on the owner, the anchor's purse: a limit),
hostage/spy/measure/delivery/fate/… (world + owner `pstor`, untested).
Rewards run in `task_manager.task_callback` (the task owner's context, the
anchor) through `on_complete` condlists and `CGeneralTask:give_reward`:
`reward_money` / `reward_random_money` / `_by_dist` →
`dialogs.relocate_money(db.actor, n, "in")`; `reward_item` /
`reward_random_item` → `alife_create_item(section, db.actor)`;
`give_reward` → `dialogs.relocate_money` and `dialogs.relocate_item_section`;
`complete_task_inc_goodwill` (both already, the S08 wrapper);
`reward_stash` (party map spots, S08-T09). `has_tasks_by_npc` and the
giver's task list are world state (talk-7 round 2: the party's task is
not offered twice). Sidorovich's sections: `esc_m_trader_task_1` assault,
`_task_2` bounty (`reward_money(7000)`), `_task_3` fetch (the S08 task),
`_task_4` lostzone steps, plus DoctorX `drx_sl_esc_m_trader_*`.

## Next (the implementation plan for S10-T05b)

1. **`CoopServerHost.cpp`** (patch 0060 continues):
   - `party_task_supported(id)` → true for any task section
     (`task_manager.task_ini:section_exist(id)` via a Lua helper); drop
     `kPartyTasks`; `rebuild_party_tasks` iterates every in-process native
     task (`GetGameTasks()`, `HasGameTask(id, true)`) instead of the list.
   - `party_source` wrappers, active while `P.turnin` is set:
     `dialogs.relocate_money` (type `in`: one `money` entry per participant
     actor, the same amount, `count = 0`), `dialogs.relocate_item_section`
     and `xr_effects.reward_item` (one `give` entry `{actor, section, amount}`
     per actor), `task_objects.CGeneralTask.give_reward` (call the original,
     then `P.effects.done = true`: the universal completion marker). Helpers
     `campaign_party_task_exists(id)`, `campaign_party_title(id)`,
     `campaign_party_bounty_target(id)` (`axr_task_manager.bounties_by_id`),
     and a `npc_on_death_callback` probe recording `AC_ID` and `who:id()`.
   - `apply_party_effects`: apply `give` entries with `give_to(alife,
     section, actor)` (declared after it: add a forward declaration), news
     per actor; the money news generic when `count == 0`.
   - Factor `party_accept_task(link, npc, task, source)` and
     `party_turn_in_task(link, npc, task, source)` out of `talk_quest_accept`
     / `talk_quest_turn_in` (the talk hooks only resolve the task id first);
     the accept news uses the task title.
   - Fixture `task_party` (config `task_party`, prepare `--party-task c1,c2`):
     first run: p2 accepts `esc_m_trader_task_2` (Sidorovich's bounty)
     through `party_accept_task`, read the target id, request a checkpoint;
     the harness kills and restarts (`-ServerKill 130 -ServerRestartAfter 10`,
     the same profile); the recovered run: the task rebuilt, kill the target
     with p2 as the killer (`KillEntity(p2)` if online, else
     `alife().kill_entity`), `party_turn_in_task` by p2, expect
     `quest-completed`, two `quest-paid` of 7,000 each, two `quest-goodwill`;
     then an item-reward probe (`campaign_party_begin_turnin('probe')`,
     `xr_effects.reward_item(db.actor, npc, {'bandage'})` in p1's context,
     `apply_party_effects` with a dummy pending turn-in, `end_turnin`:
     two `quest-item-given`), then a kill-attribution probe (spawn
     `sim_default_stalker_0` beside p1 as the quest fixture does, kill it
     with p2 online, read the death probe: `AC_ID == who == p2`).
   - `compare-coop-party-task.py` for those lines; `CoopConfig` `task_party`.
2. Decision rows to write: T05b-3 (callbacks in the attacker's context),
   T05b-4 (money tasks pay from the anchor's purse: a limit until a
   per-player rule), T05b-5 (the party inventory view during task scopes).
3. Build both products (`campaign-314`, `client-211`), run `party-1`,
   report `docs/validation/S10-T05b.md`, re-export 0060 in place (remove
   its line from `series` and the file first, check 69), L10 narrowed,
   then end-session.
4. Then S10-T04c (trade) with the user's answers above.

## Run names

- Used: `proto-build-49`, `st-24`, `st-25`, `campaign-306…313`,
  `client-203…210`, `talk-1…9`, `info-1`, patch checks 65–68.
- Next: `campaign-314`, `client-211`, `proto-build-50`, `st-26`, patch
  check 69; session names `party-1…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T05b](../done/snapshot-before-S10-T05b.md).

# Stage 8 closing work — gameplay decisions for approval

Started 2026-10-26 with S08-T07. The user accepted G08 on 2026-10-25 on the
condition that its limits (L37–L43) are solved; S08-T07…T09 own L37–L42.
Every gameplay decision taken while solving them is listed here, with the
task that took it and where it is implemented, so the user can approve or
change each one when Stage 8 closes. None of them is final until approved.

**Status: approved by the user on 2026-10-27** ("they all work You can close the gate"), with T07-5 and T07-8 as the user revised them
(patches 0052, 0053). See the [G08 condition review](validation/G08.md#condition-review--ready-for-the-users-confirmation-2026-10-26).

## S08-T07 — equip, use and consumption

**User's answers (2026-10-27):** T07-1, T07-2, T07-3, T07-4, T07-6, T07-7,
T07-9 **approved**. T07-5 **revised**: the animation may play only on the
user's client, but the user must not be able to fire while it plays
(implemented: below). T07-8 **revised**: thirst, sleep and the like must be
separate for each player (implemented: below).

| # | Decision | Why | Alternative |
|---|---|---|---|
| T07-1 | A player's slot, belt and ruck placement is **durable** (journaled), not only checkpoint state: after a restart every item is where the player last put it | equip state was listed under L36/L37; a restart that silently unequips a player's gear would feel like data loss | keep placement as checkpoint state only (rolls back to the last checkpoint) |
| T07-2 | An item may go only into the slots the **installed inventory UI** allows (pistol: 1, 2 or 3; rifle: 2 or 3; anything else: its own slot); the engine itself would accept any free slot | a client could otherwise put e.g. a bandage into the rifle slot by sending a crafted request | trust the engine's free-slot check only |
| T07-3 | A use runs the **installed scripts on the server** inside the user's context (`actor_on_item_before_use`, `CInventory::Eat`, the `actor_on_item_use` handlers): their refusals apply in co-op exactly as in single player (e.g. no second medkit while the first one's 33 s booster runs) | single-player rules unchanged | skip the scripts' checks |
| T07-4 | The "a stronger booster is still active" memory is kept **per player**: one player's medkit does not stop the other from using one | the installed script keeps one table for "the actor"; with two actors it blocked the partner | shared memory (one medkit at a time for the whole party) |
| T07-5 (revised by the user) | The use's first-person animation, sound and camera effect play **only on the user's own client**; the partner does not see it. **The server ignores the user's fire key until the animation raises the weapon again** (the window read from the installed timeline, `configs/plugins/actor_effects.ltx`: the `use_weapon(1)` key, e.g. 9.68 s for food, 4.15 s for a bandage); with the animations turned off in the options (as in single player) there is no window | the installed animation timeline runs from participant 1's update on the server and would have lowered participant 1's weapon for anyone's use | run the timeline on the server per actor (needs the actor-2 script split, L32) |
| T07-6 | A use is applied first and **committed before the player is answered**; a crash in between loses that unacknowledged use entirely (the item and its effect come back); the client's retry then uses it again | only the engine and scripts know a use's effects; nothing acknowledged is ever lost | — |
| T07-7 | After a restart the user's **health, stamina, radiation, psy health and satiety** are set to their values right after the last durable use since the checkpoint, and that item's **boosters restart with their full duration** | otherwise a used medkit would stay used but its healing would be undone | restore item state only (effects roll back to the checkpoint) |
| T07-8 (revised by the user) | **Thirst, sleep and psy health are separate for each player**: the server keeps one copy of those scripts' state per player, swaps it in whenever it runs a player's scripts, runs each player's survival update itself, saves every copy in the checkpoint and shows each player its own values (HUD indicators, psy bar); the installed pre-hit handlers (armour, psy damage) now run for the actor that is hit. **Hunger** was already separate (the engine keeps it per actor); at the user's request (2026-10-27) each client's HUD now shows the server's **hunger, stamina and radiation** for its player instead of its own local copy | the user: "they need to be separate for each player" | — |
| T07-9 | After a restart the "a stronger booster is still active" memory starts empty (a second medkit is allowed at once), as after loading a save in single player | the installed script keeps it in a local table it never saves | persist it per player in the checkpoint |

## S08-T08 — durable containers and stack semantics

**User's answer (2026-10-27):** all approved ("they all work You can close the gate").

| # | Decision | Why | Alternative |
|---|---|---|---|
| T08-1 | A box or a body that a player's committed transfer touches is **re-created after a restart** if the recovered world lacks it (a stash a script made, the body of an NPC spawned after the checkpoint); a re-created body is a dead body at the same place | committed contents must be in their container, not on the ground (L38) | keep them on the ground at the container's place (the S08-T02 rule) |
| T08-2 | Whatever the **server's own gameplay** does to a player's inventory (scripts' deferred work such as the backpack stash, an NPC taking from a player, a script creating or releasing an item) is **durable** like a player's own action | otherwise the backpack stash would come back empty after a restart, its items back in the ruck | only player actions durable |
| T08-3 | **Combat consumables** (ammunition boxes, grenades, bolts) used up by the fight (reloads, throws) are **not** durable: after a restart the player has the ammunition and grenades of the last checkpoint, consistent with the weapon's magazine, which also rolls back | journaling every reload would keep the boxes gone while the magazines roll back: rounds would vanish | journal them and the magazines too |
| T08-4 | Ammunition moved into a box is **merged on the server** with the box's stacks of the same kind exactly as the installed scripts do in single player; clients no longer merge locally and receive the counts | the client's own merge changed counts only locally and tried to delete items it does not own | no merging in co-op (partial stacks pile up in boxes) |
| T08-5 | **No stack splitting** is added: the installed content has no way to split a stack; moving a stack moves it whole | nothing to request it | add a split action |
| T08-6 | A backpack (slot-13 item) moves into a box as **one item** (in this engine items hold no items); a deployed backpack stash is an ordinary box | engine semantics | — |
| T08-7 | The contents of a body that **no player touched** are not made durable: after a restart the body of an NPC that died after the checkpoint may hold different loot (the installed death scripts roll it again) | only what a player committed is promised | snapshot every body's contents at death |
| T08-8 | The deployable backpack (`itm_actor_backpack`) is not deleted by the engine after use (it cannot be deleted); the installed stash script then moves it into the stash with the rest of the ruck — kept as in single player | installed behaviour | — |

## S08-T09 — party quest presentation and robustness

**User's answer (2026-10-27):** all approved.

| # | Decision | Why | Alternative |
|---|---|---|---|
| T09-1 | Both players' PDAs list the party task as a **copy presented by the server**, with the pooled count in its title ("Mobbing the Mob (4/10)"), its map location and its final state; the usual "new task" / "task complete" news line plays on both | the partner saw only news lines (L41) | a separate party panel |
| T09-2 | A co-op client **evaluates no task logic** at all (no local completion, rewards or stage changes); the server completes and fails tasks | a client's checkpoint copy could otherwise complete a task locally and run its rewards' scripts | — |
| T09-3 | Every **map spot the installed scripts make** on the server (stashes, task targets, a deployed backpack) appears on **both** players' maps, including for a player who joins later | a shared map suits a party; the quest's bonus stash is one stash for both (S08-T04) | spots only for the player whose script made them |
| T09-4 | **News missed while away** (the newest 20 addressed to that player) are shown when the player rejoins; camera, sound and particle effects of that time are not replayed | the completion news was lost for a disconnected contributor (L41) | none |
| T09-5 | When **participant 1 (the party's anchor) dies**, the party task stays active; it **cannot be turned in until participant 1 has respawned**, then participant 1's new body carries it on (it runs the turn-in, it owns the native task); the task's script data stays in the original body's saved store | L42; the original body's store is the one the checkpoints save (the respawned body has no Lua binder, L32) | let participant 2 take the task over |
| T09-6 | A **respawned player keeps the story knowledge** (info portions) of their previous body | continuity of the player; the info portions live in the ALife registry and are saved with every checkpoint | the new body starts with none |
| T09-7 | A finished party task is told to both players even if one was loading or away at that moment (the newest 16 finished tasks are kept for that) | pq-1: the anchor rejoining after its respawn kept the task "in progress" | — |

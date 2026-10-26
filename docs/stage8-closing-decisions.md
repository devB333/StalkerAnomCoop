# Stage 8 closing work — gameplay decisions for approval

Started 2026-10-26 with S08-T07. The user accepted G08 on 2026-10-25 on the
condition that its limits (L37–L43) are solved; S08-T07…T09 own L37–L42.
Every gameplay decision taken while solving them is listed here, with the
task that took it and where it is implemented, so the user can approve or
change each one when Stage 8 closes. None of them is final until approved.

## S08-T07 — equip, use and consumption

| # | Decision | Why | Alternative |
|---|---|---|---|
| T07-1 | A player's slot, belt and ruck placement is **durable** (journaled), not only checkpoint state: after a restart every item is where the player last put it | equip state was listed under L36/L37; a restart that silently unequips a player's gear would feel like data loss | keep placement as checkpoint state only (rolls back to the last checkpoint) |
| T07-2 | An item may go only into the slots the **installed inventory UI** allows (pistol: 1, 2 or 3; rifle: 2 or 3; anything else: its own slot); the engine itself would accept any free slot | a client could otherwise put e.g. a bandage into the rifle slot by sending a crafted request | trust the engine's free-slot check only |
| T07-3 | A use runs the **installed scripts on the server** inside the user's context (`actor_on_item_before_use`, `CInventory::Eat`, the `actor_on_item_use` handlers): their refusals apply in co-op exactly as in single player (e.g. no second medkit while the first one's 33 s booster runs) | single-player rules unchanged | skip the scripts' checks |
| T07-4 | The "a stronger booster is still active" memory is kept **per player**: one player's medkit does not stop the other from using one | the installed script keeps one table for "the actor"; with two actors it blocked the partner | shared memory (one medkit at a time for the whole party) |
| T07-5 | The use's first-person animation, sound and camera effect play **only on the user's own client**; the server does not lower the user's weapon for the animation's duration (the user can still fire while it plays), and the partner does not see the animation | the installed animation timeline runs from participant 1's update on the server and would have lowered participant 1's weapon for anyone's use | run the timeline on the server per actor (needs the actor-2 script split, L32) |
| T07-6 | A use is applied first and **committed before the player is answered**; a crash in between loses that unacknowledged use entirely (the item and its effect come back); the client's retry then uses it again | only the engine and scripts know a use's effects; nothing acknowledged is ever lost | — |
| T07-7 | After a restart the user's **health, stamina, radiation, psy health and satiety** are set to their values right after the last durable use since the checkpoint, and that item's **boosters restart with their full duration** | otherwise a used medkit would stay used but its healing would be undone | restore item state only (effects roll back to the checkpoint) |
| T07-8 | Thirst, sleep and other per-actor values kept in installed **script globals** stay one value for the whole server (unchanged here; it is L32, owned by S10-T05) | needs the actor-2 script split | — |
| T07-9 | After a restart the "a stronger booster is still active" memory starts empty (a second medkit is allowed at once), as after loading a save in single player | the installed script keeps it in a local table it never saves | persist it per player in the checkpoint |

## S08-T08 — durable containers and stack semantics

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

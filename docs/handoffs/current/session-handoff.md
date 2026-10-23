# Session handoff — G07 accepted; next S08-T01 (durable item pickup)

## State (2026-10-23)

- **G07 accepted** by the user ([G07](../../validation/G07.md)); every Stage 7
  task Completed under engine patch 0042 (patch check 46), protocol
  `proto-build-37`, final builds `campaign-238` / `client-138`. Stage 7 is
  Completed in plan.md.
- User decisions at acceptance: play-test the remaining checkpoint pause
  (the world serialization, 45–65 ms, L34) on the current build; keep a
  **host-triggered save** as the optional **S07-T09** (Not Started; a
  quicksave-style `checkpoint` action from the host player, no timer).
  Activate it only if the play test shows a hitch or the user asks.
- Carried limits: L15 closed for Stage 7's parts; L32–L36 open (L36:
  inventory, quests and rewards are not durable until Stage 8 gives them
  records and adapters).
- Everything is committed; the engine tree matches the exported series.

## First thing next session: S08-T01 — durable authoritative item pickup

Spec: plan.md S08-T01 and the [persistence contract](../../persistence-contract.md)
(§4 phases, §6 record kinds: add `pickup`; `entity`/`tombstone` for items
created or destroyed after a checkpoint). Design sketch:
1. **Action** `ActionIntent` kind 5 `pickup` (target = the item's `EntityRef`;
   params none). Validate on the server: attached session, own actor alive,
   the item exists online, is on the ground (no parent), within reach
   (actor-to-item distance ≤ the engine's pickup range), inventory capacity.
2. **Resolve** the transfer as absolute post-state: item entity → owner
   entity, revision. **Commit** a `pickup` record (slot, player, operation,
   item entity, item u16 at commit, owner entity, section) before any
   change; then **apply** through one adapter that performs the native
   ownership change the server's own pickup path uses (`CInventory::Take`
   through the server event, not the client's `GE_OWNERSHIP_TAKE` import,
   which the message policy refuses); **acknowledge**; the lifecycle stream
   carries the item's parent change (Spawn with parent / Despawn) so both
   clients show it.
3. **Recovery**: replay `pickup` records after the checkpoint (the item
   exists in the checkpoint without an owner → give it; already owned by
   the recorded owner → no-op; owned by someone else → `recovery-replay-failed`).
4. **Concurrent claims**: both clients request the same item in the same
   tick → exactly one accepted, the other refused (`ItemTaken`); retries
   with the same id return the stored outcome.
5. **Client**: the use key on a looked-at item sends the intent (today the
   engine's local pickup is refused by the replica); a driver `--pickup`
   program for two clients racing for one dropped item; `compare-coop-pickup.py`.
6. Crash test with `run-recovery-matrix.py` points `pickup:after-flush`,
   `pickup:after-apply`, `pickup:after-ack` (add the failpoints), plus a
   restart; then the report `docs/validation/S08-T01.md`.
Builds: `campaign-239` / `client-139`; protocol `proto-build-38` if the
store changes; tests `st-5`.

## Known issues (not to chase now)

[known issues](../../known-issues.md): K1–K4 (K4, items kept on clients
after an NPC consumed them, is Stage 8's ownership class).

## Run names

- Used: see the previous snapshot (through campaign-238 / client-138,
  proto-build-37, st-4, rec-4, mx3-1…5, death-4, patch check 46).
- Next: `proto-build-38`, `st-5`, `campaign-239`, `client-139`, `pt-31`,
  `smoke-171`, `capture-62`, patch check 47.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S08-T01](../done/snapshot-before-S08-T01.md).

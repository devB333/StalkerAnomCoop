## Complete

# Session handoff — Stage 7 started: S07-T01 completed, S07-T02/T03 in progress

## State (2026-10-22)

- **G06 accepted** 2026-10-21; every Stage 6 task Completed. The user said
  they are "ready to accept gate 6" (already accepted) and to **work through
  to the next gate (G07)**, stopping only for a major blocker.
- **S07-T01 Completed** 2026-10-22 ([report](../../validation/S07-T01.md)):
  the persistence contract is [docs/persistence-contract.md](../../persistence-contract.md)
  (guarantee matrix, revision/generation/epoch/operation ids, the six
  transaction phases, checkpoint publication in 8 steps, journal format,
  recovery sequence, client resync, respawn, the synthetic ledger fixture,
  17 crash boundaries on paper). It is the spec for S07-T02…T07.
- **S07-T02 + S07-T03 In Progress** (no engine change yet, nothing built):
  - Project-owned store library written: `protocol/include/coop/store/{file,journal,checkpoint}.h`,
    `protocol/src/store/*.cpp` (durable file primitives with failpoints,
    `JournalWriter`/`read_journal`, `CheckpointStore::publish/recover`,
    supplemental record and manifest codecs), `coop_store` in
    `protocol/CMakeLists.txt`; tests `tests/store/test_store.cpp` (journal
    round trip, torn tail, corruption, fail-closed append, publish/recover/
    retention/fallback, publication failpoints, a **termination matrix** that
    runs this exe as a child with `--filter store/child` and
    `TerminateProcess` at 8 failpoints) added to `protocol-tests`.
  - `proto-build-33` **failed to compile** (one error: assigning a
    `JournalWriter`, `checkpoint.cpp:747`). Fixed in source
    (`JournalWriter::reset()`, movable `AppendFile`) but **not rebuilt**.
- Engine: patches 0001–0041 unchanged; engine-work has no uncommitted
  changes of this session. Final Stage 6 build `campaign-222` / `client-124`,
  protocol `proto-build-32` (v1.5).

## First thing next session

1. Rebuild the protocol: `Build-Protocol.ps1 -Run proto-build-34 -Task S07-T02`,
   then `python3 tools/run-protocol-tests.py st-1 --build proto-build-34 --task S07-T02 --exe protocol-tests`
   (expect 98 old tests + 8 store tests; the termination matrix spawns
   children). Fix what fails; record in `docs/validation/S07-T02.md` and
   `S07-T03.md` (not yet written).
2. Engine integration (design in the contract §5–§7, §10; plan sketched this
   session): new `xrGame/CoopPersistence.{h,cpp}`; `xrCoopServer_Prepare`
   runs `CheckpointStore::recover` and chooses the save name
   (`coop-gen-<G>` or the profile's base save), allocates the epoch
   (journal `epoch` record, flushed) before listening; `start_resync` and a
   periodic `checkpoint_seconds` profile key publish generations (native
   save under `coop-tmp-gen-<G>`, then `publish`); the supplemental record
   needs `CampaignIdentityRegistry` additions (export u16→EntityId, restore
   map consulted in `on_engine_id_assigned`, `next_entity`), participants
   assigned by saved u16 after the ALife load and before the client stages
   (new hook in `x_ray.cpp` between `campaign-server-ready` and the client
   stages); `ActionIntent` kind 3 `ledger` (`s32 delta` → `set_money`)
   committed through the journal before apply/ack; reason 5 `StoreFailed`
   fail-closed; `Welcome`/`ResyncBegin` epoch from the store; client program
   6 (ledger) and `tools/compare-coop-recovery.py`; `Run-CoopSession.ps1`
   needs a server kill/restart option and clients need reconnect (S07-T05).
3. Then S07-T04 (replay adapters), T05 (client fence/reconnect), T06
   (failpoint matrix, G07 report), T07 (respawn). Add L32 (actor 2 script
   state) to carried limits at G07.

## Known issues (not to chase now)

[known issues](../../known-issues.md): K1–K4 unchanged.

## Run names

- Used this session: `proto-build-33` (failed compile, S07-T02 log folder).
- Next: `proto-build-34`, `st-1` (store tests), `campaign-223`, `client-125`,
  `pt-31`, `smoke-171`, `capture-62`, patch check 45.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S07-T02](./snapshot-before-S07-T02.md).

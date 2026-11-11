# Session handoff — the quest sweep found B57-B64; B64 (the clients' event channel stalls) is next

## State (2026-11-10, paused by the user)

- **S10-T07 part 27** ([report](../../validation/S10-T07.md), bugs in
  [known issues](../../known-issues.md) B56-B64). engine-work equals the
  series of **117** patches (`0117-quest-sweep`, patch check 131). Latest
  builds **campaign-498 / client-381** (0117 plus nothing else),
  proto-build-66.
- **B57** (the user's Wolf turn-in froze the server, released player 1's ALife
  actor): luabind's shared nil slot was written; fixed in luabind
  (`ref.hpp`), `apply_party_effects` reads lists with the plain Lua API, a
  per-tick `lua-refnil-cleared` guard. Confirmed by the sweep (Wolf's task
  turns in, both paid). **B56** (a third player's key took no slot and the
  join hung) is open; the user's roster was fixed by hand (backup
  `builds/stage5/S11-T00/kit-host-17/roster.before-slot-fix.txt`).
- **The quest sweep** (the user's request: "almost every quest end to end";
  the user chose phased coverage and "real where cheap, forced rest"):
  fixture `quest_sweep`, lists in `builds/quest-sweep/` (cordon 104, marsh 37,
  smoke 8; `tools/make-quest-sweep-list.py`), checker
  `tools/compare-coop-quest-sweep.py`. Results: Cordon 0-34 **32 pass**
  (bugfix-76), Marsh **36 pass + 1 skip** (bugfix-82); random stalker tasks
  pass until B64 stops givers reaching the clients (8-10 per session).
- **Fixed in code by the sweep:** B58, B59 (random stalker accept and
  turn-in), B60 (fetch pay per holder, G08), B62 (self-completing tasks pay
  both), B63 (finishing stage other than 1). **Open:** B61 (the storyline's
  own give/finish still bypass the party, though its rewards reach both via
  B62), **B64**.
- **B64:** in every sweep session the lifecycle outbox to both clients stops
  draining (`status ... w<slot>=world/sent/outbox`: sent frozen, outbox
  grows; the clients' `world-summary applied=` frozen; no spawns, talk
  windows, party updates; snapshots and deaths still flow). Non-sweep runs
  keep up. 0117 logs `outbox-stall why=backlog-query|limit|send queued=
  unacked=` and `outbox-flowing`. If real play can trigger it, players stop
  seeing new NPCs and items.
- The user is updated on Discord (chat `1566639386420973638`).
- Old logs: `builds/stage2/test-install/S11-T00/kit-host-17-server/appdata/logs/coop-server.log`
  is 132 GB (B57's loop): ask the user before deleting it.

## Next

1. The B64 diagnostic (bugfix-83 was stopped at the user's pause; the build
   is ready): `python3 tools/prepare-coop-session.py bugfix-84 --task S10-T07
   --protocol-build proto-build-66 --fixture cordon --quest-sweep c1,c2
   --quest-sweep-list builds/quest-sweep/cordon.txt --quest-sweep-first 35
   --quest-sweep-count 20 --checkpoint-seconds 120 --client-exit-after 1800`,
   then `Run-CoopSession.ps1 -Run bugfix-84 -Task S10-T07 -ServerAloneSeconds
   10 -ClientStarts '0,15' -ClientSeconds '840,825' -AfterSeconds 15`; read
   `grep outbox- builds/logs/S10-T07/run-bugfix-84/server/coop-server.log`.
2. Fix B64 at its cause; rerun the random stalker batches (cordon 35+35,
   70+34) and the whole Cordon list on one build.
3. B61 (route `drx_sl_give_sl_task` / `drx_sl_set_finished_task_complete`
   through the party), B56 (refuse a full party with words; no roster write).
4. Phase 2: the other levels through the passage sweep's travel.
5. New kits for the user once the sweep is clean (memory: only then).

## Picking this up

- Read `CLAUDE.md`, `plan.md` §I, this file, S10-T07 part 27, known issues
  B56-B64.
- **One cycle:** edit engine-work → `Build-Stage2.ps1` Campaign and Client
  `-ProtocolBuild proto-build-66` → a sweep run → `python3
  tools/export-engine-patch.py <NNNN-name>.patch <check>` → manifest, README,
  report, known issues, handoff, commit.
- A sweep session: 35 tasks take ~8 min plus ~3 min loading; the session runs
  its whole `-ClientSeconds`. Game windows open: the user should not type.
- `luabind::functor` calls return proxies: never mix them in `?:` (two
  compile errors this session); wrap each in `luabind::object(...)`.

## Run names

- Used: `campaign-334…498`, `client-226…381`, `proto-build-53…66`,
  `st-28…41`, `bugfix-1…83` (S10-T07), `demo-1…22`, `menu-1…10`,
  `kit-1…16`, `kit-19…23`, `two-pc-1`, patch checks 77–131, kits host-1…17,
  friend-1…3, client-4…11, server-1…12.
- Next: `campaign-499`, `client-382`, `proto-build-67`, `st-42`,
  `bugfix-84`, `demo-23`, `menu-11`, `kit-24`, patch check 132, kits host-18,
  client-12, server-13.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the B64 diagnostic](../done/snapshot-before-B64-diagnostic.md).

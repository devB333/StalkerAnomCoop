# Session handoff — G08 accepted; next S08-T07 (equip, use, consumption)

## State (2026-10-25)

- **G08 accepted** ("I accept as long as the limits will be solved",
  [G08](../../validation/G08.md)). The condition is plan work: the new limits
  are **L37–L43** in [carried limits](../../carried-limits.md), each owned
  by a task that solves it. Added to Stage 8: **S08-T07** (equip, use/eat,
  durable consumption: L37, L39), **S08-T08** (durable post-checkpoint
  containers, stacks/nesting: L38, L40), **S08-T09** (party quest
  presentation, anchor death: L41, L42); **S10-T04**'s first child routes the
  talk window (L43). Stage 8 stays In Progress until S08-T07…T09 are done.
- Engine patches 0001–0048, protocol v1.6 `proto-build-42`, final builds
  campaign-267 / client-167. Stage 8 reports: S08-T01…T06 in
  `docs/validation/`.
- Open from earlier: the user's K1/K5 hands-on retest.

## Next

S08-T07 (plan.md): start from the S08-T02 interception point
(`xrServer::Process_event` → `campaign::coop_client_ownership_event`) for
`GEG_PLAYER_ITEM2SLOT/2BELT/2RUCK` and `GEG_PLAYER_ITEM_EAT`; read
`CInventory::Slot/Belt/Ruck`, `CEatableItem::UseBy`, and the installed
`actor_on_item_use` callbacks first.

## Run names

- Next: `campaign-268`, `client-168`, `proto-build-43`, `st-12`, patch
  check 55; session names `eq-1…`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S08-T07](../done/snapshot-before-S08-T07.md).

## Complete

# Session handoff — S10-T04c in progress (attack animations run, repair condition and belt built but unrun)

## State (2026-10-29, session cut off by the usage limit; nothing committed this session)

- **Uncommitted work in the tree** (engine-work sources, `tools/`, `docs/`):
  not yet exported as a patch, not in a report. Export it as **patch 0066
  `remote-weapon-anim-repair-belt`** (or split: 0066 animations, 0067
  repair condition + belt) with the export-patches skill before anything else.
- **The partner's attack animations (K1 follow-up 2) — built and run, pass.**
  Design (differs from the previous handoff's sketch, on purpose): the
  server's sampled `EntityState::weapon_state` is stored on the replica
  actor (`CActor::replica_set_weapon_state`, `Actor.h` / `Actor_Movement.cpp`)
  and read **only** by the third-person animation picker
  (`ActorAnimation.cpp`: `replica_anim_weapon_state(W->GetState())` for
  weapons and missiles). The item's own state stays K1's instant
  shown/hidden: setting eFire on the weapon object would make
  `CWeaponMagazined::UpdateCL` fire bullets on the client. An attack state
  (eFire/eFire2) is held at least 350 ms (`ReplicaWeaponAnim::kHoldMs`) and
  a counted shot between samples also kicks eFire (`replica_present_attack`).
  Client log `remote-weapon-state id=… state=a->b slot=… item=…`, engine log
  `S10-T04c remote-torso id=… state=… motion=slot/idx standing=…`, stats
  counter `weapon_states_presented`. The combat program got a knife leg
  (`equip-knife`, `face-sky`, `W-knife`) and `compare-coop-combat.py` a
  `partner_anim` check (fire 5, reload 7, draw/holster 1/2 seen for the
  partner; knife swings counted). **Run anim-1** (campaign-330 / client-222,
  `prepare-coop-session.py anim-1 --task S10-T04c --protocol-build
  proto-build-51 --combat c1,c2`; `Run-CoopSession.ps1 -Run anim-1 -Task
  S10-T04c -ServerAloneSeconds 10 -ClientStarts '0,10' -ClientSeconds
  '250,240' -AfterSeconds 10`): **23/23 pass**
  (`builds/logs/S10-T04c/run-anim-1/compare-combat.json`). c2 saw c1's
  knife swings (state 5 with `wpn_knife`, torso motion 0/365); c1 saw none
  of c2's because the fixture's armed actor has no knife (equip refused,
  reason 4) — a fixture inventory fact, not a bug. Not yet seen by hand.
  Not presented: the partner's zoom/aim pose (IsZoomed is false on a
  replica) and the aim yaw/pitch (separate).
- **Repair condition — built, unrun.** CampaignEvent 9 entries are now 13
  bytes: `u8 condition` (condition × 255) after the u16 amount; both layout
  hashes include it (`layout_hash` server, `replica_layout_hash` client).
  The client applies it (`apply_layout`), logging `layout-condition
  actor= entity= item= byte=a->b` when the step is > 2. The trade fixture
  gained phase 4 (p1's first weapon worn to 0.35, then repaired to 1.0 two
  seconds later: `fixture-trade worn|repaired … byte=`), and
  `compare-coop-trade.py` a `condition` check. `docs/network-protocol.md`
  kind 9 updated.
- **L48 belt — built, unrun.** Trade fixture phases 5–6: the first installed
  outfit section with `artefact_count > 0` (class containing `STLK`, name
  containing `outfit`) and an `af_medusa` are given to p1; the outfit is
  worn through `take_equip` (OUTFIT_SLOT), then the artefact goes to the
  belt (`fixture-trade belt-given|belt-outfit|belt … reason=`); the client
  logs `layout-belt … ok=`. No compare check for the belt yet (add `belt`:
  server reason 0 and c1 `layout-belt ok=1` for the artefact entity).
- **Builds:** campaign-329/330, client-222 (animations only; anim-1 ran on
  330/222). client-223 / campaign-331 **failed** (ini `Sect` by value,
  fixed). client-224 / campaign-332 **failed** too (`pSettings` is const, so
  `Sect::line_exist` is unusable; fixed in the source with
  `pSettings->line_exist(section.Name, …)`, **unbuilt**). Build next as
  client-225 / campaign-333 (both products).
- **Decisions this session (technical, for the user's list):** (1) the
  attack-animation design above; (2) the fixture door stays the nearest
  `ph_door`-scheme object (same code path as the bunker door the user opened
  by hand in demo-1) — no change; (3) a repair's payment and condition are
  durable through the checkpoint only (the mechanic's dialogue is a script
  action, not a journal record) — to be written as a limit. **Gameplay
  directions reaffirmed by the user 2026-10-29:** trade is NPC-to-one-player
  (T01-7), the native drag-and-drop window (T04c-1), quest rewards to both
  players in full (T05b-2). No new gameplay decision was taken.

## Next

1. Confirm/redo the builds; run **trade-4** (`prepare-coop-session.py trade-4
   --task S10-T04c --protocol-build proto-build-51 --trade c1,c2`, the
   trade-3 `Run-CoopSession` arguments with `-ServerKill 150
   -ServerRestartAfter 10`); `compare-coop-trade.py` must pass 7 checks
   (add the `belt` check first). Watch the restart: the fixture's given
   outfit/artefact are not journaled, so their equip replays must be
   tolerated.
2. Write `docs/validation/S10-T04c.md` (anim-1, trade-4, the three
   paragraphs above, limits: zoom pose, hold heuristic, repair durability,
   fixture actor 2 has no knife), close **L48** in `docs/carried-limits.md`
   with trade-4 evidence, narrow **L01** (repair condition), update
   `docs/known-issues.md` K1 (follow-up 2), add rows T04c-6…T04c-8
   (technical) to `docs/stage10-decisions.md`, plan.md §I row, export the
   patch(es), update `docs/source-manifest.json`, commit.
3. The user's hands-on: `bash tools/start-coop-demo.sh S10-T04c 0 20 0` —
   trade window, partner's weapon switch, partner's knife swing / reload /
   draw animations, Sidorovich's voice, task map spot.
4. Then S10-T04f (upgrades), S10-T04d, S10-T04e, and the rest of Stage 10
   (S10-T02a–f, S10-T03a–f, S10-T05a/c/d, S10-T06); stop at G10 with the
   decisions list.

## Run names

- Used: `campaign-329…332`, `client-222…224`, `anim-1`, plus all in the
  snapshot.
- Next: `campaign-333`, `client-225` (the fixed source, unbuilt), `proto-build-52`, `st-28`, patch
  check 76; sessions `trade-4`, `anim-2`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T04c animations](./snapshot-before-S10-T04c-animations.md).

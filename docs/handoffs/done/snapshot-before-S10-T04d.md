## Complete

# Session handoff — S10-T04c complete (patch 0066); next S10-T04f upgrades

## State (2026-10-29, end of session)

- **Completed this session, committed:** **S10-T04c** trade, repair and the
  use of world objects ([S10-T04c](../../validation/S10-T04c.md)), finished
  by **patch 0066 `remote-weapon-anim-repair-belt`** (patch check 76):
  (1) **the partner's attack, reload, draw and holster animations** on a
  replica: the server's sampled `weapon_state` is stored on the replica
  actor (`CActor::replica_set_weapon_state`, `Actor_Movement.cpp`) and read
  by the third-person animation picker alone (`ActorAnimation.cpp`,
  `replica_anim_weapon_state`); the weapon object stays in K1's idle
  (eFire on it would fire on the client); attack states held ≥ 350 ms; the
  combat program got a knife leg and `compare-coop-combat.py` a
  `partner_anim` check; **anim-1 pass 23/23** (campaign-330 / client-222):
  client 2 saw client 1's knife swings; (2) **a repaired item's condition**
  in the layout event (kind 9: a `condition` byte per item, both hashes);
  (3) **L48 closed**: an outfit with artefact slots worn and an artefact on
  the belt by the trade fixture; **trade-4 pass 8/8** (campaign-333 /
  client-225, the final builds). Decision rows **T04c-6…T04c-8**
  (technical). L01 narrowed again. K1 has a "follow-up 2" entry.
- **Hands-on by the user, pending** (list for G10): the trade window (buy and
  sell, the partner refused busy), the partner's weapon switch (0065), the
  partner's knife swing / reload / draw / holster (0066), the task's map
  spot and Sidorovich's voice (0062). Command: `bash
  tools/start-coop-demo.sh S10-T04c 0 20 0`.
- **The user's standing directions (2026-10-29):** trade is NPC-to-one-player
  (T01-7), the native drag-and-drop window (T04c-1), quest rewards to both
  players in full (T05b-2). Keep a running list of decisions for the gate.
- **Decisions awaiting the user at G10** ([Stage 10
  decisions](../../stage10-decisions.md)): T04a-1…4, T04b-1…3, T05b-1…5,
  T04c-1…8.
- Open visible limits: the partner's zoom pose is not presented; NPCs slide
  (L57); the `sound-volume` presentation flood (since S06-T08).

## Next

1. **S10-T04f upgrades at mechanics** (T04c-3). Read first:
   `UITalkWnd::SwitchToUpgrade` (`m_remote` returns early, S10-T04a),
   `CUIGameSP::StartUpgrade`, `CUIInventoryUpgradeWnd::OnMesBoxYes`
   (`upgrade_install` + `inventory_upgrades.effect_upgrade_item`),
   `inventory_upgrade.cpp` (precondition / effect / prereq functors run
   the installed `inventory_upgrades.script`, which charges `db.actor`),
   `CInventoryUpgradeManager` (`inventory_upgrade_manager.h`). Model it on
   trade (T04c-1): an upgrade-open intent (the mechanic the player talks
   to; the server answers with the item's scheme state and the costs for
   the items the player holds), an upgrade-install intent (item ref +
   upgrade id; the server runs `upgrade_install` in the buyer's context,
   journals it as a record so a restart keeps it, pushes the purse and the
   layout), the client's upgrade window reading the server's table. New
   kinds 21–22 and a campaign event → protocol v1.8 (`proto-build-52`,
   `st-28`). Fixture: a mechanic on the Cordon fixture save (check which
   NPC has `upgrade_mechanic`; the installed scripts' preconditions need
   tools/parts), `compare-coop-upgrade.py`. Upgrades persist on the item
   (`m_upgrades`) and are saved with it.
2. Then S10-T04d (factions and reputation per player), S10-T04e (stashes),
   then S10-T02a–f, S10-T03a–f, S10-T05a/c/d, S10-T06; stop at G10 with the
   decisions list.

## Run names

- Used: `campaign-329…333`, `client-222…225`, `anim-1`, `trade-4`, patch
  check 76; all in the snapshots.
- Next: `campaign-334`, `client-226`, `proto-build-52`, `st-28`, patch
  check 77; sessions `upgrade-1…`, `anim-2`, `trade-5`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T04f](./snapshot-before-S10-T04f.md).

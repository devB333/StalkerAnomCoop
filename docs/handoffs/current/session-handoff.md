# Session handoff — B51 smooth movement in code, the user's hands-on next; G10 and S11-T00 wait on the user's word

## State (2026-11-04)

- **S10-T07** (bug pass before Stage 11) continues through the user's
  hands-on demos. engine-work equals the series of **114** patches (patch
  check 127). Builds **campaign-480 / client-363**, protocol
  **proto-build-66** (st-41 124/124). Report: [S10-T07](../../validation/S10-T07.md) parts
  9–22; bug table: [known issues](../../known-issues.md).
- **B46 (patch 0108):** travel matches vanilla (decision T07-11 in
  [Stage 10 decisions](../../stage10-decisions.md)): only fast travel and the
  backpack check combat. The user's demo-19: travel back and forth worked.
- **B47 (patch 0109), found in demo-19:** after player 1 walked into the
  Cordon ↔ Marsh passage the server updated no object for the rest of the
  session (shots from 0,0,0, NPCs standing). The passage's space restrictor
  ran the installed travel box (`ui_sr_teleport`) on the server, which
  paused the device (`set_device_paused_script`); a paused device runs no
  `UpdateCL`. Same in demo-16/17: **the root cause of B45**. Now the campaign
  server refuses every pause (`device-pause-refused`) and shows no window;
  `update-stall` logs any object near a player that stops updating; the
  travel checker's new `updates` check fails on it. Reproduced by
  **bugfix-38/39** (the fixture through the passage's approach:
  `--travel-changer to_marsh_1 --travel-approach=-278.6,-21.0,-268.7`),
  passing in **bugfix-40/41** (8/8) and **bugfix-42** (B46's round, 9/9).
- **demo-20** (the user): "almost everything is working"; no stall. Then
  **B48 (patch 0110):** the L key's torch goes through the server
  (ActionIntent 30, Presentation TorchState); the demo kit adds a headlamp;
  `demo_quest` starts the party with Sidorovich's "Mobbing the Mob" and its
  bandit patches, ready to turn in. **bugfix-44** pass (quest `stage=1`
  11/11 on both PDAs; both torches on and off, seen by both clients).
- **Not verified:** the quest's turn-in by hand, shooting an NPC after a travel and the refusal wording
  (hands-on; the fixture does not shoot); a fast travel in combat.
- **Still open from before:** the quest fixture's variant 2 is
  order-dependent; patch 0107's kick stays as a guard though B45's cause is
  now B47.
- **Decisions waiting on the user's word at G10:** G10-5 revised (B34), B35,
  T07-11 (the user's own choice).
- **Test kits (2026-11-03):** the user's first try (host-7) found **B49**:
  New game showed no co-op choice. Patch **0111** fixed it (kit-12/13).
- **B50 (patch 0112, 2026-11-04):** the friend's client-7 crashed at Join
  co-op: the installed `message_box_direct_ip` has no text window and
  `msg_box_ui` sets a text (null `SetText`). Also fixed: `GetHost` turned
  `host:27515` into `host/port=27515`; the captions now read *Host address* /
  *Join code* (the code shown as typed). The menu driver's `join` now goes
  through New game, the box's fields and its OK. Reproduced in **kit-14**,
  passing in **kit-15** (host-10 + client-8 from its zip) and **kit-16**
  (server-7 + two client-8), [S11-T00](../../validation/S11-T00.md).
- **B51 (patch 0113, 2026-11-04): movement smoothed.** The user: both
  players choppy, on one PC too. Measured with the new per-frame motion log
  (`motion_log`, `coop-motion-client.csv`), the smoothness walk (`--smooth
  c1,c2`, script 9) and `compare-coop-smoothness.py`: a correction loop
  (`SetPosition` lost the body's lead and restarted its interpolation) made
  7-9 corrections a second. Now corrections shift the body
  (`ShiftPosition`), small errors are bled off (30 %/snapshot), the clock
  slews 1 ms, render time is full resolution. bugfix-45 → 49: camera spikes
  17-18 → 0-0.4 a second; the other player's jerk p50 8-10 → 3. Left: a few
  corrections after a frame hitch and at player contact
  ([S10-T07](../../validation/S10-T07.md) part 23). **Kits not remade**
  (host-11 / client-8 are the B50 build): after the user's hands-on.
- **B52 (patch 0114):** the minimap showed whatever the option said (the
  option's first-update handler never runs on a client); applied at every
  world-live; bugfix-50 pass (off hidden, on shown, screenshots agree). The
  user's demo-22 of B51: "it feels a lot better".
- **First two-PC test passed (two-pc-1, 2026-11-04):** the user hosted with
  host-11, the friend joined with client-8 by code over Tailscale from
  100.106.37.43; about ten minutes of play, clean stop, no FATAL. The user:
  "Great we have a working build!" B49 and B50 confirmed hands-on.
- **Kits for the two-PC test:** **host-11**
  (`builds/kits/client1HostKit/host-11`), **server-8**
  (`builds/kits/ServerHostKit/server-8`), **client-8**
  (`builds/kits/clientKit/client-8.zip`). host-6…10 and server-3…7 hold test
  players; client-7 and older crash at Join. A choice in the box runs on a
  double-click.
- demo-19's launcher stopped before copying its logs into
  `builds/logs/S10-T07/run-demo-19/` (empty); they are under
  `builds/stage2/test-install/S10-T07/demo-19-*/appdata/logs/`.

## Next

1. The user's hands-on of B51 (demo-22: "it feels a lot better") and B52: `bash tools/start-coop-demo.sh S10-T07 0 30
   120 0 1` (demo-23: two windows on this PC). Its motion logs are in the
   run's c1/c2 (the checker needs the walk's legs: read them by hand).
2. If the user says it is smooth: remake the kits (host-12, client-9,
   server-9) and a second two-PC test; else the next lead (hitch recovery,
   player contact: no collision between the players' bodies on a client?).
3. Ask the user whether to close **S11-T00** and **G10**.

## Picking this up

- Read `CLAUDE.md` / `AGENTS.md`, `plan.md` §I, this file, then the S10-T07
  report parts 9–21 and the known issues B27–B47.
- **One cycle:** edit engine-work → `Build-Stage2.ps1` Campaign and Client
  with `-ProtocolBuild proto-build-64` → a fixture run → `python3
  tools/export-engine-patch.py <NNNN-name>.patch <check>` → manifest, README,
  report, known issues, handoff, commit.
- **Travel fixtures:** `--travel c1,c2 --travel-variant 2|5`, optionally
  `--travel-changer <name part> --travel-approach=x,y,z`; check with
  `compare-coop-travel.py --variant 2|5` (`updates` is the B47 check). Others:
  `--smooth c1,c2` (B51: check with `compare-coop-smoothness.py`), `--sleep`, `--pickup`, `--loot`, `--quest c1,c2 --quest-variant 2
  --auto-respawn 5`; arm with `arm = 1` in the server's `coop.ltx`. Don't
  type in the terminal while a fixture runs.
- Installed scripts: `python3 tools/xray-db-extract.py -q grep 'pattern'
  /mnt/c/Users/djb20/StalkerAnom/db/configs/scripts.db0`, or `extract -m
  <name> -o <scratch dir>`.

## Run names

- Used: `campaign-334…480`, `client-226…363`, `proto-build-53…66`,
  `st-28…41`, `bugfix-1…50`, `demo-1…22` (S10-T07), `menu-1…10`,
  (S11-T00), `kit-1…16`, `two-pc-1`, patch checks 77–127, kits host-1…11, friend-1…3,
  client-4…8, server-1…8.
- Next: `campaign-481`, `client-364`, `proto-build-67`, `st-42`,
  `bugfix-51`, `demo-23`, `menu-11`, `kit-17`, patch check 126, kits host-12,
  client-9, server-9.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the B51 hands-on](../done/snapshot-before-B51-hands-on.md).

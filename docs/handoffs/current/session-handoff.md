# Session handoff — G10 reviewed by the user; nine revisions to implement before the gate closes

## State (2026-11-02)

- **The user reviewed G10 item by item** (2026-11-02). Every Stage 10
  decision and new limit is answered: [G10](../../validation/G10.md) "The
  user's review", the outcome paragraph in
  [Stage 10 decisions](../../stage10-decisions.md), and new limits
  **L60-L72** in [carried limits](../../carried-limits.md).
- Bug pass: B1, B2, B4, B22, B23 fixed (hands-on); B5, B20 carried to
  S11-T02 (L59, L70); B9 waived (L66); B11 carried (L72); B21's rest carried
  to S11-T01 (L71). **B24** (a client crash animating the other player) is
  fixed in code (patch 0092, campaign-427 / client-310) but **not run yet**.
- Engine-work equals patch 0092 (commit 660d212).

## Next: the nine revisions (S10-T07 continues; then the gate)

1. **T05b-4**: a task's money cost is paid by the player who clicked (today
   player 1's purse: `db.actor:money()` / `transfer_money` in task logic).
2. **T03b-2**: bounty hit squads are triggered by either player, by the
   worse standing (`sim_squad_bounty.spawn_timer` reads player 1 today).
3. **T04f-2**: mechanic lending always on in co-op
   (`gameplay/general/mechanic_feature`, default off; the server's
   `axr_options.ltx` leaves it unset). Find how the installed scripts track a
   lent item (`axr_trade_manager.script` and the mechanic's dialogue). If it
   is not easy to follow, ask the user before closing.
4. **T03a-2**: an absent player's area stays live 5 minutes after the leave,
   then its character is parked (its area goes offline, it cannot be hurt,
   NPCs ignore it) and unparked on the rejoin. Today detached actors stay
   with no limit (`CoopServerHost.cpp` detach, around line 18418).
5. **T03c-2**: both players must be in shelter to sleep (a sleep zone,
   `ui_sleep_dialog.sleep_zones` / `current_safe_cover`, or within 1.5 m of
   their own tent, `item_tent.get_nearby_tent`). The partner's USE counts
   only there; otherwise it is told to find shelter.
6. **Item rewards to both**: `xr_effects.give_reward` / stored rewards and
   `alife_create_item` outside `reward_item` go to the anchor only today
   ([S10-T05b](../../validation/S10-T05b.md) limits).
7. **A failed or cancelled party task's goodwill penalty to both**
   (`fail_task_dec_goodwill`, not wrapped today).
8. **Story mode only**: remove Warfare, Azazel, Ironman and Survival from
   co-op (the creation screen and the server's create check, T06-3).
9. **The trade window's donate and the field workshop repair
   (`ui_workshop`) through the server**, like trade and the mechanic's
   repair.

Then: build, fixture runs where a fixture fits, and the user's hands-on
session, which also checks B7 (the partner's aim pose, the controller's
pull), B10 (the bloodsucker's grab) and B24 (no recurrence; `[B24] blend list
full` lines name the churning model). Update G10 and close it only on the
user's word.

## Network test (the user wants one; independent of G10)

Cloudflare cannot host the server (a Windows program with the game files).
It could carry the connection: Cloudflare Tunnel and the Cloudflare One
client, UDP, untested with GNS. The recommendation is Tailscale or forwarding
UDP 27515. The other PC needs Anomaly 1.5.3 (not installed yet), a client
kit and its own player key (`coop-keytool new-player`, DPAPI) whose roster
line the host adds ([session authentication](../../session-authentication.md)).
Waiting on the user's choice of route.

## Picking this up (for a new session or model)

- **Read first**: `CLAUDE.md` / `AGENTS.md` (rules), `plan.md` §I (status),
  this file, then [G10](../../validation/G10.md) "The user's review" for the
  exact wording of each revision.
- **The user** owns gameplay design and tests anything that affects how the
  game looks or feels **by hand**. Never call such a fix done before their
  session; give them `bash tools/start-coop-demo.sh S10-T07 0 30 120 0` and
  read `builds/stage2/test-install/S10-T07/demo-<n>-*/appdata/logs`
  afterwards. When they are away they reach the session through the Discord
  plugin (reply with the Discord reply tool to their chat). They are learning
  C++ and X-Ray: keep answers short, with a one-line "why" for engine
  concepts.
- **Engine source first**: `engine/xray-monolith/src` (pinned, read only).
  Edit only `builds/build_1/engine-work`. Game scripts and configs are in
  the read-only install's archives: extract them into a scratch folder with
  `python3 tools/xray-db-extract.py -q extract -m .script -o <dir>
  /mnt/c/Users/djb20/StalkerAnom/db/configs/scripts.db0` (configs:
  `configs.db0`; `grep -E -m <path part> <regex> <archive>` searches).
- **One cycle**: edit → `Build-Stage2.ps1` Campaign **and** Client with
  `-ProtocolBuild proto-build-59` (protocol unchanged since; ~3-5 min each,
  run in the background) → `python3 tools/export-engine-patch.py
  <NNNN-name>.patch <check-number>` (reconstructs and checks the whole
  series) → `docs/source-manifest.json` hash, `patches/engine/README.md`
  entry, the task report, known-issues, handoff (snapshot first), commit.
- **Before a build**, list running processes under `builds\`. A demo's
  campaign server often outlives the session (`AnomalyCampaignServer.exe`).
  **Ask the user before stopping it**: the permission check refuses a stop
  without their OK, and an earlier OK does not carry over.
- **Pitfalls hit this session**:
  - A failed build still uses up its run name.
  - The release build has no debug-only APIs (`LL_MotionDefName_dbg`
    exists only under `#ifdef DEBUG`).
  - `svector::push_back` checks its bound only in debug builds.
  - In `CoopClientLink.cpp`, an `extern` for a global must sit at global
    scope, outside `namespace campaign`; otherwise it fails at link time
    (LNK2001).
  - The bash working directory persists between calls: use absolute paths
    (two doc commits silently failed from `docs/handoffs`).
- **Useful one-offs**: a client checkpoint (`.scop`) is a 12-byte header
  plus LZO1X data, decodable with `lzo1x_decompress` from
  `tools/xray-db-extract.py`. Import it with `importlib` and register the
  module in `sys.modules` first, or its dataclasses fail. A Windows
  minidump's exception stream (stream type 6) gives the fault code, the
  address and the registers: that is how B24's null-blend read was confirmed.
- **New diagnostics in the logs**:
  - `replica-anim … legs=<idx>:<name> torso= yaw= drawn= travel= head=<cur>><target> body= danger= visual=`
    (NPC facing);
  - `trade-open … panel=` (the Lua trade panel);
  - `frame-trace` (frame time near fires);
  - `! [B24] blend list full` in the engine log (which model churns blends).
- **Game console settings** (client, co-op): `coop_campfire_shadow <m>`
  (default 0), `coop_campfire_light 0/1`.

## Run names

- Used: `campaign-334…427`, `client-226…310`, `proto-build-53…59`,
  `st-28…34`, `bugfix-1…16`, `demo-1…5` (S10-T07), patch checks 77–104.
- Next: `campaign-428`, `client-311`, `proto-build-60`, `st-35`,
  `bugfix-17`, patch check 105, `demo-6`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the G10 revisions](../done/snapshot-before-G10-revisions.md).

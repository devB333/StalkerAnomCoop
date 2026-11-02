## Complete

Snapshot of the handoff as it stood before S11-T00 (local hosting) and the two-PC test (2026-11-02). History only.

# Session handoff — the G10 revisions are in code; the user's hands-on session decides

## State (2026-11-02)

- **All nine G10 revisions are in code** (patch **0093** `g10-revisions`,
  engine-work equals the series of 93; builds **campaign-429 / client-311**;
  protocol **proto-build-60**, st-35 pass). Report:
  [S10-T07](../../validation/S10-T07.md) part 7. The choices made while
  implementing them are decision rows **G10-1…G10-9** in
  [Stage 10 decisions](../../stage10-decisions.md), for the user's word at
  the gate.
- Fixture runs pass: **bugfix-17** (sleep, with a new shelter round) and
  **bugfix-18** (party task, with the clicker's purse, the item and the fail
  penalty probes).
- Mechanic lending as built was OK'd by the user on Discord (2026-11-02:
  "Yeah go ahead with that").
- New bug row **B25** (Donate was paid as a sale) fixed in code.
- **The user was sent the demo command on Discord** and may be testing:
  `bash tools/start-coop-demo.sh S10-T07 0 30 120 0` (the next free run is
  `demo-6`). Read `builds/logs/S10-T07/run-demo-<n>` and
  `builds/stage2/test-install/S10-T07/demo-<n>-*/appdata/logs` afterwards.

## Next

1. The user's session: the checklist sent on Discord: the New game screen
   (no Ironman / Azazel / Survival / Warfare); sleep only in shelter (the
   partner told to find shelter); Donate gives for 0 RU; a repair kit's
   window and repair; mechanic lending (lend and ready news, own items only);
   one player away 5+ minutes then back (`parked` / `unparked` lines); and
   B7 (the partner's aim pose, the controller's pull), B10 (the
   bloodsucker's grab), B24 (no crash; `[B24] blend list full` lines name
   the model).
2. Record the result in S10-T07 (part 8) and the bug list; fix what fails.
3. Present G10-1…G10-9 and the part 7 limits (the workbench `ui_workshop`
   not routed; both players away keeps player 1's area online) for the
   user's word; close G10 only on the user's word.

## Network test (the user wants one; independent of G10)

Unchanged: waiting on the user's choice of route (Tailscale or forwarding
UDP 27515; Cloudflare cannot host the server). The other PC needs Anomaly
1.5.3, a client kit and its own player key
([session authentication](../../session-authentication.md)).

## Picking this up (for a new session or model)

- **Read first**: `CLAUDE.md` / `AGENTS.md`, `plan.md` §I, this file,
  [G10](../../validation/G10.md) "The user's review", S10-T07 part 7.
- **The user** owns gameplay design and tests look-and-feel by hand; never
  call such a fix done before their session. Away, they use Discord (chat
  `1566639386420973638`, reply with the Discord reply tool). They are
  learning C++ and X-Ray: short answers, a one-line "why".
- **Engine source first**: `engine/xray-monolith/src` (read only). Edit only
  `builds/build_1/engine-work`. Game scripts: extract with
  `python3 tools/xray-db-extract.py -q extract -m .script -o <dir>
  /mnt/c/Users/djb20/StalkerAnom/db/configs/scripts.db0` (files are CRLF,
  latin-1: use `grep -a`).
- **The G10 Lua adapter** (`g10_source` in `CoopServerHost.cpp`) is long:
  it was written as a plain Lua file and converted to C string lines; keep
  its `P.wrapped` count in mind (`party-adapter installed wrapped=14`).
- **One cycle**: edit → `Build-Stage2.ps1` Campaign and Client with
  `-ProtocolBuild proto-build-60` (background; do not edit engine-work while
  a build compiles) → `python3 tools/export-engine-patch.py
  <NNNN-name>.patch <check>` → `docs/source-manifest.json`,
  `patches/engine/README.md`, the report, known-issues, handoff (snapshot
  first), commit.
- **Before a build**, list running processes under `builds\`; ask the user
  before stopping a leftover campaign server.
- **Pitfalls**: a failed build still uses its run name; release builds lack
  debug-only APIs; `svector::push_back` checks its bound only in debug; an
  `extern` in `CoopClientLink.cpp` must sit at global scope; use absolute
  paths (the bash working directory persists).

## Run names

- Used: `campaign-334…429`, `client-226…311`, `proto-build-53…60`,
  `st-28…35`, `bugfix-1…18`, `demo-1…5` (S10-T07), patch checks 77–105.
- Next: `campaign-430`, `client-312`, `proto-build-61`, `st-36`,
  `bugfix-19`, patch check 106, `demo-6`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the G10 revisions' hands-on](snapshot-before-G10-revisions-hands-on.md).

# Session handoff — S10-T07: B1, B4 and B23 fixed hands-on; crash B24 fixed in code; next the G10 review and a two-PC network test

## State (2026-11-02)

- **demo-5** (campaign-426 / client-309): the user: "the bugs are fixed".
  B4 (NPC facing and hip twist) and B23 (fire lag, with the default of no
  campfire shadow) are **fixed**; B1 (trade panel) was fixed in demo-4.
  B22 (jump): no complaint since demo-3.
- **B24 (new)**: client 2 crashed animating the other player. A part's blend
  list (`svector`, 16) overflowed in release and the next fade walked off its
  end (minidump: access violation reading 0x12). Patch **0092**
  `blend-list-guard` retires the oldest blend and logs `[B24] blend list
  full`. Builds **campaign-427 / client-310**; **not run yet**. What churns
  the remote body's legs is not proven (lead: `g_cl_ValidateMState` in the
  remote branch of `CActor::UpdateCL`). Report:
  [S10-T07](../../validation/S10-T07.md), "Part 6".
- Engine-work equals patch 0092.

## What G10 still needs (the user asked; summary given 2026-11-02)

1. **Bug pass**: the user's word on B2 (a task marker at a far target, one
   icon after death and respawn), B7 (the partner's aim pose, the
   controller's pull) and B10 (the look of the bloodsucker's grab); decisions
   on B5 and B20 (crash-dump permission, or carry as limits), B9 (a
   "pending" mark, or waive) and B21's rest (fix, or carry); B11 carried; B24
   seen not to recur.
2. **Decision review**: the questions T05b-4 (who pays a task's money cost)
   and T03b-2 (what attracts bounty hit squads), then 103 decisions
   (T02a…T07-10) item by item, in short batches with a recommendation each.
3. New limits L60… and the [G10](../../validation/G10.md) report updated
   (it still describes part 2); close G10.

## Network test (the user wants one tonight; independent of G10)

- The user asked about hosting on Cloudflare. Cloudflare cannot run the
  server (a Windows program with the game files). It could carry the
  connection to this PC: Cloudflare Tunnel (`cloudflared`) plus the
  Cloudflare One (WARP) client on the other PC (UDP supported; untested with
  GNS). The recommendation was Tailscale or forwarding UDP 27515.
  **Waiting on the user's choice.**
- The other PC has no Anomaly 1.5.3 install yet (the user is not sure).
- The other PC needs: Anomaly 1.5.3; a client kit (the client build, the
  `gamedata` overlay, `fsgame.ltx` pointing at its install, a `coop.ltx`
  with `server = <host>:27515`); a player key made **on that PC**
  (`coop-keytool new-player`, encrypted with DPAPI), whose printed roster line
  goes into the host's roster ([session authentication](../../session-authentication.md)).
  The server already listens on `0.0.0.0:27515`.

## Run names

- Used: `campaign-334…427`, `client-226…310`, `proto-build-53…59`,
  `st-28…34`, `bugfix-1…16`, `demo-1…5` (S10-T07), patch checks 77–104.
- Next: `campaign-428`, `client-311`, `proto-build-60`, `st-35`,
  `bugfix-17`, patch check 105, `demo-6`.

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the G10 review and the network test](../done/snapshot-before-G10-review-and-network-test.md).

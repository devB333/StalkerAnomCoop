# Session handoff — S10-T01 Completed; Stage 10 decisions await approval

## State (2026-10-28)

- **S10-T01 Completed** (no engine or protocol change; engine `c57fe5b9` +
  patches 0001–0057, protocol `proto-build-48`, builds campaign-305 /
  client-202 unchanged): [S10-T01](../../validation/S10-T01.md). The
  [campaign coverage matrix](../../campaign-coverage-matrix.md) inventories
  the installed campaign (33 levels; 122 level changers forming 98 directed
  passages, listed by `tools/list-passages.py` in
  `builds/logs/S10-T01/passages.tsv`; 413 scripts; 610 task sections in 29
  files, 218 storyline; 17 mutant classes; 36 trade files), ranks the
  remaining global-player sites (`xr_logic:372` condlists and `xr_meet`
  first), gives 24 families a policy, fixture, evidence requirement and
  status, and defines **21 children S10-T02a…S10-T05d** (matrix §4; also
  listed under each parent in plan.md).
- **Awaiting the user:** the 16 scope decisions in
  [Stage 10 decisions](../../stage10-decisions.md) T01-1…T01-16 (every
  passage in scope; a new game; one party faction; party story info
  portions; nearest-participant NPC logic; per-player goodwill and money;
  consent to sleep; guides as party travel; companions per recruiter;
  one speaker per NPC; the exclusions: Warfare/Azazel/Ironman/survival,
  vehicles, debug/MP). None is implemented. Children whose scope depends
  on a decision (S10-T02f companions, S10-T03f new game, S10-T03e's
  passage set, S10-T05c's chains) wait for it; the others may start.
- Carried limits: L48, L50, L51, L52 still hold with routed owners
  ([carried limits](../../carried-limits.md)); no new rows (none are added
  before G10).
- Still the user's to run by hand: the two-window travel trial and the
  K1/K5 retest (from G09).

## Next

1. Review the decisions with the user item by item (the gate-review
   pattern); record approvals or revisions in `docs/stage10-decisions.md`
   and adjust the matrix's §5 scope.
2. Then **S10-T04a — the talk window** (L43): route a client's talk UI to
   the server's `PhraseDialog` with one speaker per NPC (T01-15), so a
   player reaches the S08 party quest's accept and turn-in by talking.
   First action: read `Actor_Communication.cpp` (`RunTalkDialog`, the
   S03-T05 `dialogue_open` presentation), `ui/UITalkWnd.cpp` and
   `PhraseDialog.cpp:85-129` for the phrase-selection path, then define
   the ActionIntent kinds for "say phrase" and "end talk" in the protocol.
   Followed by S10-T04b (party info portions and actor-2 script state).

## Run names

- Next: `campaign-306`, `client-203`, `proto-build-49`, `st-24`, patch
  check 65; session names `tv-27…` (tv-1…tv-26 used; tv-2, tv-9, tv-10
  prepared, not run; tv-24 invalid). S10 fixtures take new prefixes per
  child (for example `talk-1…` for S10-T04a).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before the Stage 10 decision review](../done/snapshot-before-S10-decision-review.md).

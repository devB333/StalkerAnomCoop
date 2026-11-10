## Complete

# Session handoff — S10-T04a (the talk window) started: design read, no code yet

## State (2026-10-28)

- **S10-T01 Completed** and committed (`c9d9c10`, `08023b7`, `4e72531`):
  [S10-T01](../../validation/S10-T01.md), the
  [campaign coverage matrix](../../campaign-coverage-matrix.md) (24 families,
  21 children S10-T02a…S10-T05d plus S10-T06), and
  [Stage 10 decisions](../../stage10-decisions.md) **T01-1…T01-19, all
  approved by the user on 2026-10-28** ("I approve the rest"). They bind:
  every passage in scope; a new game with one party faction; party story
  info portions; nearest-participant NPC logic; per-player goodwill and
  money; consent to sleep; guides as party travel; companions per
  recruiter; one speaker per NPC (T01-15); the exclusions; a dedicated
  server controlled from the clients (Create/Load/Join, the creator owns
  the session, the joiner inherits faction and type: T01-17…19, child
  S10-T06).
- **S10-T04a In Progress**: source reading only, **no engine, protocol or
  tool change, no build**. Engine `c57fe5b9` + patches 0001–0057,
  `proto-build-48` (v1.6), campaign-305 / client-202 unchanged. The
  working tree is clean apart from this handoff.
- The user's instruction for the session: keep going, stop only for major
  decisions; the user accepted all Stage 10 scope decisions.

## S10-T04a — what the reading established (reuse, do not re-read)

Goal (L43, L01 dialogue): a client's talk window drives the server's
dialogue so a player reaches the S08 party quest's accept and turn-in by
talking; one speaker per NPC (T01-15).

**Engine path (single player):** `CActor::TryToTalk` (`ActorInput.cpp:608`,
the USE key) → `CActor::RunTalkDialog` (`Actor_Communication.cpp:125-145`;
on the server it already calls `campaign::dialogue_open` and returns) →
`OfferTalk`/`StartTalk` (`InventoryOwner.cpp:263-285`; no "busy" check:
`OfferTalk` only tests `IsTalkEnabled` and both alive) →
`CUIGameSP::StartTalk` → `CUITalkWnd` (`ui/UITalkWnd.cpp`): `InitTalkDialog`
(both `CPhraseDialogManager`s, `InitOthersStartDialog`: the NPC's first
available dialog, phrase "0"), `UpdateQuestions` (topic mode: the actor's
`UpdateAvailableDialogs` → captions by dialog id; phrase mode: the current
dialog's `PhraseList`, `allIsDummy` auto-say), `AskQuestion` → `SayPhrase`
(`CPhraseDialogManager::SayPhrase` → `CPhraseDialog::SayPhrase`
`PhraseDialog.cpp:85-157`: the phrase's script `Action`, the next phrases'
`Precondition`s, the partner's `ReceivePhrase`), `GetPhraseText`
(`:168-224`, may run a Lua text function). `CInventoryOwner::UpdateInventoryOwner`
(`:229-241`) ends a talk when the partner stopped or died. The trade and
upgrade buttons (`SwitchToTrade/Upgrade`) are S10-T04c: disable them in
co-op for now.

**Server today:** `campaign::dialogue_open/close`
(`CampaignScriptContext.cpp:482-520`): context scope, `OfferTalk`,
`StartTalk`, `pda.actor_menu_mode(10/11)`, `DialogOpen/DialogClose`
presentation. The S08 kind-8 quest path (`CoopServerHost.cpp:6373-6625`,
`take_quest`, `quest_accept_phrases`, `quest_turnin_phrases`) calls the
installed `dialogs.generate_available_tasks / npc_has_task / npc_skip_task /
text_npc_has_task / npc_give_task` and `actor_has_finished_task /
text_npc_task_finish / npc_set_finished_task_complete` directly, **on the
anchor (slot 0's actor)** inside `dialogue_open`, then commits the party
task durably (S08-T05/T06). Movement input is applied by `apply_inputs()`
(`CoopServerHost.cpp:1629`) with no `IsTalking` check (add one: a talking
actor takes no movement, as `ActorInput.cpp:49,357` does in single player).

**Client today:** `coop_client_key_press` (`CoopClientLink.cpp:5402`)
handles only travel and respawn keys; a USE on an NPC runs the native
`RunTalkDialog` against the replica with local dialogs (nothing reaches the
server: L43). Intents go through `request_ownership(kind, target, params,
…)` (`:3584`); results dispatch by kind at `:4801-4960`; CampaignEvent
kind 4 presentations at `on_presentation` (`:2806`), where `DialogOpen` is
not yet handled. Protocol constants: `protocol/include/coop/proto/messages.h`
(`kActionTravelReady = 13` is the last kind; reasons end at 32
`kReasonNotLoaded`); the wire docs are `docs/network-protocol.md` rows
"ActionIntent" and "CampaignEvent".

**Design to implement (my plan; no user decision needed beyond T01-15):**

1. Protocol v1.7: ActionIntent kinds **14 talk start** (target the NPC, no
   params), **15 talk say** (params: u8 len + id of the clicked topic or
   phrase, ≤ 64), **16 talk end**; new reasons **33 busy** (the NPC talks
   to the other player), **34 not talking** (a say/end with no open talk),
   **35 wrong phrase** (the id is not in the current list). CampaignEvent
   **kind 14 dialogue state** to the talking player: u16 npc, u8 state
   (1 open, 2 closed), u8 flags (bit 0 no-break, bit 1 topic mode), the
   current dialog id, u8 count + the questions (id, text, u8 finalizer),
   u8 count + the new answers since the last event (u8 speaker 0 npc /
   1 us, text). Texts are what `GetPhraseText` returns (the client
   translates with `CStringTable`, as the window does).
2. Server: a `TalkSession` per player link (npc, `DIALOG_SHARED_PTR
   current`, answers pending). Start: checks (attached session, own actor
   alive, the NPC alive, an `CInventoryOwner`, not an actor, within the
   S08 giver reach `kGiverReach`, **`npc->IsTalking()` with another
   partner → 33**), `campaign::dialogue_open`, then the `CUITalkWnd` logic
   moved server-side (`InitOthersStartDialog`, `UpdateQuestions` with the
   dummy auto-say, `AskQuestion`/`SayPhrase`) inside the player's script
   context, sending kind 14 after every change. End: `dialogue_close`.
   Each tick: close the talk (kind 14 state 2) when the actor or NPC
   stopped talking or died or moved out of reach. **Quest phrases:** the
   installed task dialog's accept/turn-in actions (`dialogs.npc_give_task`,
   `dialogs.npc_set_finished_task_complete`) must not run through the
   native `Action` for the talking player; when the clicked phrase's
   `CDialogScriptHelper` actions contain one of them, route it through
   the kind-8 path (`take_quest` logic factored into a function taking
   the task id: anchor, durable commit, pending answer) and present the
   phrase texts; the task id comes from `axr_task_manager.available_tasks[npc]`
   as `quest_accept_phrases` reads it. Check `PhraseScript.h` for access
   to the action names (`m_sScriptActions`, private: add an accessor).
3. Client: on `talk start` accepted and kind 14 state 1, open `CUITalkWnd`
   in a remote mode (a flag set by `CoopClientLink`): `InitTalkDialog`
   skips the local dialog managers, `UpdateQuestions` fills from the last
   kind 14, `AskQuestion` sends kind 15 with the clicked id, closing the
   window sends kind 16, trade/upgrade buttons off. The USE key on an NPC
   (`CActor::RunTalkDialog` client branch) sends kind 14 instead of
   opening locally when the co-op link is active. A busy refusal shows the
   installed "busy" hint (`actor_menu.set_msg`-style HUD text through the
   existing presentation path) and nothing opens.
4. Fixture: `--talk c1,c2` driver in `prepare-coop-session.py` /
   `CoopClientLink.cpp` (as `--quest`): round 1 client 1 starts a talk with
   Sidorovich, picks the topic whose caption is the task offer, says the
   phrases to accept (the server logs `talk-say`, `quest-accept` as the
   kind-8 path does); round 2 client 2 starts a talk with the same NPC
   while client 1 talks → 33; round 3 client 2 talks after client 1 ended;
   round 4 the turn-in by talking after the fixture gives the patches;
   a crash window (`quest:before-publish` or the existing S08 failpoints)
   while a talk is open: the recovered server holds no talk, the client's
   window closes on rejoin. Compare tool `tools/compare-coop-talk.py` from
   `compare-coop-travel.py`'s shape. Hands-on: the user walks to a trader
   and talks in two windows.
5. Report `docs/validation/S10-T04a.md`; decisions row(s) in
   `docs/stage10-decisions.md` (e.g. the hint shown to the busy player,
   movement frozen while talking); L43 closed, L01 narrowed; patch 0058
   `talk-window`, `proto-build-49`, protocol tests `st-24`, builds
   campaign-306 / client-203; export patches, manifest hashes.

## Next

1. Implement S10-T04a as above (first file edits: `messages.h` constants
   and the decoder bounds in `protocol/src/proto`, then `CoopServerHost.cpp`
   talk session, then `CoopClientLink.cpp` + `UITalkWnd.cpp` remote mode).
2. Then S10-T04b (party info portions and actor-2 script state).

## Run names

- Next: `campaign-306`, `client-203`, `proto-build-49`, `st-24`, patch
  check 65; session names `talk-1…` for S10-T04a (`tv-1…tv-26` used; tv-2,
  tv-9, tv-10 prepared, not run; tv-24 invalid).

Preserve the original installation, the reference checkout, the accepted
source-client runtime and both saved-game fixtures.
Previous snapshot: [before S10-T04a](snapshot-before-S10-T04a.md).

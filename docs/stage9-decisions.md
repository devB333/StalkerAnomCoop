# Stage 9 — gameplay decisions for approval

Started 2026-10-27 with S09-T01. Every gameplay decision taken while
implementing coordinated travel is listed here with the task that took it
and where it is implemented, so the user can approve or change each one at
G09. **All approved as listed at G09 (2026-10-28).** Decision D5 (plan.md) and risk R10 bind:
both players confirm; no automatic or unconfirmed teleport; a changed
consent policy needs the user's approval.

## S09-T01 — request and two-player consent

| # | Decision | Why | Alternative |
|---|---|---|---|
| T01-1 | Walking into a level changer **asks the player first** (USE to request the travel, DROP to stay), on silent changers too; nothing is sent by the touch itself | in single player a non-silent changer asks too; in co-op a request involves the partner, so an accidental step into a changer should not raise a prompt on the partner's screen | the touch is the request (as a silent changer changes level at once in single player) |
| T01-2 | The partner has **30 s** to answer; then the request expires and the requester is told | long enough to read the prompt and finish what one is doing; short enough that a forgotten request does not hold the party | 15 s / 60 s / no expiry |
| T01-3 | **No partner, no travel:** a request while the partner is disconnected or dead is refused ("your partner is not here: you travel together") | D5 requires both confirmations; a lone traveller would move the one online level away from the absent player's actor | let one player travel alone when the other is away (needs the user's approval, R10) |
| T01-4 | A **disconnect** of either player while a request is up **aborts** it; after the rejoin the player asks again | the consent of an absent player cannot be assumed (R10); a rejoin takes long enough that the situation may have changed | keep the request live across the reconnect |
| T01-5 | **Revised 2026-11-03 by the user (B46, [Stage 10 T07-11](stage10-decisions.md)): only a fast travel or a backpack jump checks combat; a level changer does not.** Was: **Combat blocks travel:** a request or an accept is refused, and a live request aborted, while an alive NPC or monster has either player as its chosen enemy | a level change mid-fight would leave the fight unresolved and the party's state in flux; the native game lets one walk away, but there one player decides for one actor | no combat rule; or "no damage taken in the last N s" |
| T01-6 | Declining a changer's prompt (DROP) silences that changer for **20 s**; standing inside keeps re-prompting every 5 s otherwise (the engine's own invitation interval) | one should be able to stand at a passage without being nagged, yet reach the prompt again without leaving | silence until the player leaves the shape |
| T01-7 | **One request at a time** for the party; a second is refused ("a travel request is already up") until the first ends | one world, one destination; simpler to reason about and to present | queue requests |
| T01-8 | The requester's own accept counts for nothing (refused as "not yours to give"); only the partner's accept agrees | the request is the requester's consent; the partner's answer must come from the partner | — |
| T01-9 | After both agree, **S09-T01 does not change level**: both players see "agreed by both players (the level change itself is not in this build)" | the migration is S09-T02's work; this task owns the consent only | — |

## S09-T02 — the travel itself

| # | Decision | Why | Alternative |
|---|---|---|---|
| T02-1 | While an agreed travel is carried out (a few seconds: the barrier, the save, the publication), **nobody can move or act**; actions sent then are refused ("travel in progress") and a player's client re-sends the unanswered ones on the new level | one consistent world is saved and loaded; nothing may change between the take and the reload | keep playing until the reload (changes after the take would be lost) |
| T02-2 | The two players arrive **side by side** at the passage's arrival point (the second 0.8 m beside the first), facing the way the passage says, as a single player would arrive | the level changer defines one arrival point | spread them further / keep their relative positions |
| T02-3 | The arrival point becomes each player's **respawn point** on the new level (as the level's entry is in single player, G07 decision 2) | the previous respawn point is on the old level | the destination's own first spawn point |
| T02-4 | A **disconnect or death during the barrier** rolls the travel back (both are told); after the commit the travel goes on and an absent player rejoins on the new level | nothing durable has changed before the commit; after it the world is already there | wait for the player |
| T02-5 | The travel is **not undone by a crash after the commit**: a restarted server comes up on the destination level with both players there; a crash before the commit leaves everyone on the source level (the players ask again) | the published generation is the one recoverable world (the persistence contract) | — |
| T02-6 | A **vehicle** a player sits in is not carried over (the player arrives on foot) | the native change-level moves the holder too, but vehicles are outside the tested content (L-list) | move the vehicle's entity as well |
| T02-7 | The **same server incarnation continues** on the new level (same authority epoch, same transition ids, the clients reconnect to the same server) rather than restarting the process | a restart would take the same load time and add a recovery; the in-process path is the engine's own level change | restart the process from the committed generation |

## S09-T03 — the coordinated resume

| # | Decision | Why | Alternative |
|---|---|---|---|
| T03-1 | After a travel **nobody moves until both players have loaded** the new level (each client reports ready; the server discards input until then); the world's NPCs and clock run meanwhile | one player must not play the new level against a partner who is still loading (D5) | let each player go as soon as loaded (the faster one would play alone for ~10 s) |
| T03-2 | The wait lasts **60 s at most**: if a player has not reported ready by then (its client crashed, or it is away), the other resumes alone and the absent player's actor stands at the arrival point, vulnerable, until that player rejoins | a missing player must not freeze the game for ever; the detached-actor policy already covers an absent player | wait for ever; or pause the whole world |

## S09-T04 — failures and recovery

| # | Decision | Why | Alternative |
|---|---|---|---|
| T04-1 | A **disconnect or death during the barrier** rolls the travel back and tells the remaining player **who** left or died ("the partner left"), as a live request's abort does; the party asks again once both are back | nothing durable has changed; the player should know why the passage did not open | a bare "the travel failed" |
| T04-2 | Once the destination save is **taken** (the publication is running), a disconnect **no longer rolls back**: the travel commits and the absent player rejoins on the new level | the taken save already holds both actors at the arrival; a rollback here would race the commit point | roll back until the pointer swap |
| T04-3 | A **failed publication** (a disk problem before the commit point) rolls the travel back, records it (phase 3), tells both players "the travel failed; ask again", and the game goes on | the store did not fail closed: one publication failed; the next may succeed | stop the server |
| T04-4 | A **server crash during the reload or the hold** recovers the destination level; the recovered server **does not hold**: each player plays as soon as it has rejoined, as after any recovery | a recovery is not a travel: both actors are already on the destination; the hold protects against playing while the partner is still loading a *travel*, and the recovery's own join path already streams the world to each | hold again after a recovery |
| T04-5 | A player whose client **crashes or disconnects during the hold** and comes back before the deadline is told the hold and must report ready like the other; one who comes back **after the deadline** is told the partner went on and plays at once | the hold must not depend on the client remembering the travel; a late player must not wait for a resume that already happened | wait for ever; or let the late one in silently |
| T04-6 | The **60-s hold deadline** and the **10-s drain cap** stand as tested (tv-16, tv-21) and are not tuned | a client's reconnect, join and load take 13–20 s nominal; 60 s is three times that | 30 s / 120 s |

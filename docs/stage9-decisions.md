# Stage 9 — gameplay decisions for approval

Started 2026-10-27 with S09-T01. Every gameplay decision taken while
implementing coordinated travel is listed here with the task that took it
and where it is implemented, so the user can approve or change each one at
G09. None is final until approved. Decision D5 (plan.md) and risk R10 bind:
both players confirm; no automatic or unconfirmed teleport; a changed
consent policy needs the user's approval.

## S09-T01 — request and two-player consent

| # | Decision | Why | Alternative |
|---|---|---|---|
| T01-1 | Walking into a level changer **asks the player first** (USE to request the travel, DROP to stay), on silent changers too; nothing is sent by the touch itself | in single player a non-silent changer asks too; in co-op a request involves the partner, so an accidental step into a changer should not raise a prompt on the partner's screen | the touch is the request (as a silent changer changes level at once in single player) |
| T01-2 | The partner has **30 s** to answer; then the request expires and the requester is told | long enough to read the prompt and finish what one is doing; short enough that a forgotten request does not hold the party | 15 s / 60 s / no expiry |
| T01-3 | **No partner, no travel:** a request while the partner is disconnected or dead is refused ("your partner is not here: you travel together") | D5 requires both confirmations; a lone traveller would move the one online level away from the absent player's actor | let one player travel alone when the other is away (needs the user's approval, R10) |
| T01-4 | A **disconnect** of either player while a request is up **aborts** it; after the rejoin the player asks again | the consent of an absent player cannot be assumed (R10); a rejoin takes long enough that the situation may have changed | keep the request live across the reconnect |
| T01-5 | **Combat blocks travel:** a request or an accept is refused, and a live request aborted, while an alive NPC or monster has either player as its chosen enemy | a level change mid-fight would leave the fight unresolved and the party's state in flux; the native game lets one walk away, but there one player decides for one actor | no combat rule; or "no damage taken in the last N s" |
| T01-6 | Declining a changer's prompt (DROP) silences that changer for **20 s**; standing inside keeps re-prompting every 5 s otherwise (the engine's own invitation interval) | one should be able to stand at a passage without being nagged, yet reach the prompt again without leaving | silence until the player leaves the shape |
| T01-7 | **One request at a time** for the party; a second is refused ("a travel request is already up") until the first ends | one world, one destination; simpler to reason about and to present | queue requests |
| T01-8 | The requester's own accept counts for nothing (refused as "not yours to give"); only the partner's accept agrees | the request is the requester's consent; the partner's answer must come from the partner | — |
| T01-9 | After both agree, **S09-T01 does not change level**: both players see "agreed by both players (the level change itself is not in this build)" | the migration is S09-T02's work; this task owns the consent only | — |

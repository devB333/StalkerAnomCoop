#!/usr/bin/env python3
"""Check the S09-T01 party travel request and two-player consent (plan.md S09-T01, docs/travel-contract.md).

Usage: python3 tools/compare-coop-travel.py <session logs> [--variant 0|1|2|3|4|5] [--out compare-travel.json]
         [--expect-resume all-ready|timeout] [--expect-wrong-readies N] [--expect-late c2]
         [--expect-client-crash c2] [--expect-retry-after-crash]

Inputs (builds/logs/<task>/run-<run>/): server/ (and server-first-run/ for variant 1): coop-server.log;
c1/, c2/: coop-c*.log. The session runs the travel fixture (--travel c1,c2): both actors are placed in a
level changer and the rounds of docs/travel-contract.md §validation run through the step driver.

Checks:
  rounds        every step's result on the client that sent it has the expected outcome and reason
                (per round: both accept, one refuses, timeout, stale and replayed confirmations, a
                disconnect, a cancel, not at the changer, busy and wrong-party answers, combat)
  combat_ignored (variants 0 and 5, B46) round 8's request and accept were taken while the boar hunted
                participant 2; no combat refusal or abort at the changer
  ends          every round's transition ends in the expected state on the server, in order
  consent       no transition was agreed without the partner's accept of its own id (server
                travel-answer accept -> agreed count == agreed count); no load began (no change_level)
  duplicates    the step driver's same-id repeats got Duplicate with the stored transition, and a
                refused request's repeat was never turned into a proposal
  prompt        each client's own actor touching the changer raised the travel prompt
  crash         (variant 1) the server exited at travel:after-propose with a proposal live; the restarted
                server refused the partner's accept of it (no-transition) and ran a fresh agreed round
  execution     (variants 2 and 5, S09-T02) the agreed travel was carried out: the transition record prepared and
                committed, the destination generation published (reason travel), the level reloaded in
                process, both actors arrived within 3 m of the changer's destination point alive, both
                clients rejoined on the destination level (travel-arrived), the participants' item layout
                hash is unchanged across the travel, and no input or action was applied during the barrier
  resume        (variant 2, S09-T03) after the reload the server held the world (inputs consumed and
                discarded, no actor moved) until every client's ready named the pending transition and
                level (or the 60-s deadline), refused the driver's wrong readies (31), answered a
                ready's repeat from its stored outcome, and both clients saw the resume
  rollback      (variants 3 and 4, S09-T04) the first agreed travel was rolled back in process for the expected
                reason (3: a party member left during the barrier, nothing durable; 4: the publication failed,
                phase 3 recorded) and both players were told; the retry was carried out
  recovery      (variant 2 with a crash window) the restarted server recovered on the right level: the
                source level after travel:before-take (prepared only), the destination level after
                travel:after-commit (committed, rolled forward), and both clients rejoined there
"""

import argparse
import json
import re
import sys
from pathlib import Path

A, R, D = 1, 2, 3  # ActionOutcome


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def kv(line):
    return dict(re.findall(r"(\w+)=([^\s\"]+|\"[^\"]*\")", line))


# per round: (slot, kind, outcome, reason) per step in announcement order; None: no request that step
EXPECT_STEPS = {
    0: [(0, 11, A, 0), (1, 12, A, 0)],
    1: [(0, 11, A, 0), (1, 12, A, 0)],
    2: [(0, 11, A, 0)],
    3: [(0, 11, A, 0), (1, 12, R, 28), (1, 12, A, 0)],
    4: [(0, 11, A, 0), None, (0, 11, R, 26), (1, 12, R, 28), (0, 11, A, 0), (1, 12, A, 0)],
    5: [(0, 11, A, 0), (0, 12, A, 0), (1, 12, R, 28)],
    6: [(0, 11, R, 25)],
    7: [(0, 11, A, 0), (0, 11, R, 23), (1, 12, R, 29), (0, 12, R, 29), (1, 12, A, 0)],
    # B46 (T01-5 revised 2026-11-03): a level changer has no combat rule; with the boar hunting
    # participant 2 the request and the accept are taken and the party agrees
    8: [(0, 11, A, 0), (1, 12, A, 0)],
}
EXPECT_ENDS = {
    0: ["agreed"], 1: ["refused"], 2: ["expired"], 3: ["agreed"], 4: ["aborted:partner-left", "agreed"],
    5: ["cancelled"], 6: [], 7: ["agreed"], 8: ["agreed"],
}
# S09-T04 variants 3 and 4: two agreed travels; the first is rolled back by the executor (not a consent end)
EXPECT_STEPS_RETRY = {0: [(0, 11, A, 0), (1, 12, A, 0)], 1: [(0, 11, A, 0), (1, 12, A, 0)]}
EXPECT_ENDS_RETRY = {0: ["agreed"], 1: ["agreed"]}


def server_rounds(log):
    """The server log split by fixture round announcements: round -> its lines (before the first: -1)."""
    rounds = {-1: []}
    current = -1
    for line in log:
        m = re.search(r"fixture-travel-round round=(\d+)", line)
        if m:
            current = int(m.group(1))
            rounds.setdefault(current, [])
        rounds[current].append(line)
    return rounds


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--variant", type=int, default=0)
    ap.add_argument("--out", default="")
    ap.add_argument("--expect-resume", default="all-ready", choices=("all-ready", "timeout"),
                    help="variant 2 (S09-T03): how the hold on the destination level is expected to end")
    ap.add_argument("--expect-wrong-readies", type=int, default=0,
                    help="variant 2: readies the server must have refused as wrong (the driver's)")
    ap.add_argument("--expect-late", default="",
                    help="S09-T04: the client that rejoins after the hold timed out (travel_rejoin_delay): it gets no "
                         "resume, its ready is refused as no-hold and it plays on the destination level")
    ap.add_argument("--expect-client-crash", default="",
                    help="S09-T04: the client whose first run died at the destination load (travel_load_fail); its "
                         "restart is told the hold (state 10) and reports ready")
    ap.add_argument("--expect-retry-after-crash", action="store_true",
                    help="S09-T04: after travel:before-take the restarted server ran the fixture again and the "
                         "party travelled")
    a = ap.parse_args()
    root = Path(a.logs)
    server = lines(root / "server" / "coop-server.log")
    first = lines(root / "server-first-run" / "coop-server.log")
    # a client's first run (before a restart) comes first: the logs read chronologically; the restarted
    # process appends to the same log file, so when the final log already begins with the first run's
    # first line it holds both runs and the kept copy is not added again
    clients = {}
    for n in ("c1", "c2"):
        earlier = lines(root / f"{n}-first-run" / f"coop-{n}.log")
        later = lines(root / n / f"coop-{n}.log")
        clients[n] = later if earlier and later and earlier[0] == later[0] else earlier + later
    checks = {}
    detail = {}

    # client results by (round, step): list of (slot, kind, outcome, reason, transition)
    results = {}
    for n, log in clients.items():
        for line in log:
            if "travel-result" not in line:
                continue
            d = kv(line)
            key = (int(d["round"]), int(d["step"]))
            results.setdefault(key, []).append((int(n[1]) - 1, int(d["kind"]), int(d["outcome"]),
                                                int(d["reason"]), d["transition"]))
    detail["results"] = {f"{r}/{s}": v for (r, s), v in sorted(results.items())}

    both_runs = first + server  # a crash window splits one story over two server logs
    rounds = server_rounds(both_runs)
    ran = sorted(r for r in rounds if r >= 0)
    # variant 5 (B46): round 8 alone (the request and accept while the boar hunts), then carried out
    expected_rounds = [0, 1] if a.variant in (1, 3, 4) else [0] if a.variant == 2 else [8] if a.variant == 5 \
        else list(range(9))
    if a.variant == 2 and a.expect_retry_after_crash:
        expected_rounds = [0]  # round 0 in each server run; judged per run below
    expect_steps = EXPECT_STEPS_RETRY if a.variant in (3, 4) else EXPECT_STEPS
    expect_ends = EXPECT_ENDS_RETRY if a.variant in (3, 4) else EXPECT_ENDS
    detail["rounds_ran"] = ran
    combat_line = next((kv(ln) for ln in server if "fixture-travel combat=" in ln), None)
    detail["combat_exercised"] = combat_line.get("combat") == "1" if combat_line else None

    # rounds: each step's result
    failures = []
    no_combat = bool(combat_line) and combat_line.get("combat") != "1"
    detail["combat_not_exercised"] = no_combat
    for r in expected_rounds:
        steps = expect_steps[r]
        for s, exp in enumerate(steps):
            got = results.get((r, s), [])
            if exp is None:
                if got:
                    failures.append(f"round {r} step {s}: no request expected, got {got}")
                continue
            slot, kind, outcome, reason = exp
            want = [(slot, kind, outcome, reason)]
            have = sorted((g[0], g[1], g[2], g[3]) for g in got)
            if a.variant == 1 and r == 0:
                # the first run's round 0: the server exits after the proposal and before the
                # acknowledgement, so c1's request is re-sent to the restarted server and re-evaluated
                # fresh (refused: the recovered actors stand where the checkpoint had them), then the
                # restarted fixture's round 0 request is accepted; c2's accept of the dead proposal
                # (its id, or 0 when the proposal event never left the dying server) is refused 28,
                # then its accept of the new one is accepted
                if s == 0:
                    have = sorted((g[0], g[1], g[2], 0 if g[2] == R else g[3]) for g in got)
                    want = [(0, 11, A, 0), (0, 11, R, 0)]
                else:
                    want = [(1, 12, R, 28), (1, 12, A, 0)]
            if a.expect_retry_after_crash:
                # round 0 ran in both server runs (the failpoint at travel:before-take killed the first
                # after the accept, in the same tick: its result was not flushed, so c2 re-sends the
                # accept to the restarted server, which refuses the pre-restart id 28); the restarted
                # fixture's round 0 is accepted whole
                alternatives = [[(0, 11, A, 0), (0, 11, A, 0)]] if s == 0 else \
                    [[(1, 12, R, 28), (1, 12, A, 0)], [(1, 12, A, 0), (1, 12, A, 0)]]
                if have not in [sorted(w) for w in alternatives]:
                    failures.append(f"round {r} step {s}: expected one of {alternatives}, got {have}")
                continue
            if have != sorted(want):
                failures.append(f"round {r} step {s}: expected {sorted(want)}, got {have}")
    checks["rounds"] = {"pass": not failures, "failures": failures}

    # B46: round 8 ran while the boar hunted (combat=1), the server noted the request and the
    # accept as taken in combat, and refused or aborted nothing for combat at the changer
    if 8 in expected_rounds:
        r8 = rounds.get(8, [])
        ignored = sorted({kv(ln).get("kind") for ln in r8 if "travel-combat-ignored" in ln})
        refused = [ln for ln in both_runs if "travel-in-combat" in ln or "travel-combat-abort" in ln]
        exercised = detail["combat_exercised"] is True
        # not exercised (the boar picked no enemy in 25 s): only the absence of refusals is judged
        noted = ignored == ["11", "12"] if exercised else True
        checks["combat_ignored"] = {"pass": noted and not refused, "exercised": exercised,
                                    "ignored_kinds": ignored, "combat_refusals": len(refused)}

    # ends per round
    end_failures = []
    for r in expected_rounds:
        ends = []
        for line in rounds.get(r, []):
            m = re.search(r"travel-end id=(\w+) state=([\w-]+) abort=([\w-]+)", line)
            if m:
                ends.append(m.group(2) if m.group(2) != "aborted" else f"aborted:{m.group(3)}")
        want = list(expect_ends[r])
        if a.variant == 1 and r == 0:
            want = ["agreed"]  # the restarted server's round 0; the first run's proposal never ended there
        if a.expect_retry_after_crash:
            want = ["agreed", "agreed"]  # once per server run
        # transitions requested by hand (a player's USE key in a client window) after the round's
        # steps are not the driver's: recorded, not judged
        if ends[:len(want)] != want:
            end_failures.append(f"round {r}: expected ends {want}, got {ends}")
        elif len(ends) > len(want):
            detail.setdefault("extra_ends", {})[r] = ends[len(want):]
    checks["ends"] = {"pass": not end_failures, "failures": end_failures}

    # consent
    agreed = sum(1 for ln in both_runs if re.search(r"travel-end \S+ state=agreed", ln))
    accepts = sum(1 for ln in both_runs if "travel-answer" in ln and "answer=accept -> agreed" in ln)
    loads = sum(1 for ln in server + first if "change_level" in ln and "refused" not in ln)
    checks["consent"] = {"pass": agreed == accepts and agreed > 0 and loads == 0, "agreed": agreed,
                         "partner_accepts": accepts, "loads": loads}

    # duplicates: dup-result csv events on the clients with outcome Duplicate for accepted steps
    dup_total = sum(1 for ln in both_runs if "travel-duplicate" in ln)
    proposed_at = {}  # operation -> UTC ms of its proposal
    for ln in both_runs:
        m = re.search(r"^(\d+) travel-proposed .*operation=(\w+)", ln)
        if m:
            proposed_at.setdefault(m.group(2), int(m.group(1)))
    # a proposal made after the same operation had already been refused would be a refused repeat
    # turned proposal (a request refused in one run and re-evaluated after a restart is the reverse
    # order and is the contract's behaviour)
    refused_at = {}
    for log in clients.values():
        for ln in log:
            if "travel-result kind=11" in ln:
                d = kv(ln)
                if d["outcome"] == "2":
                    refused_at.setdefault(d["operation"], int(ln.split(" ", 1)[0]))
    repeat_proposals = sorted(op for op, t in refused_at.items() if op in proposed_at and proposed_at[op] > t)
    checks["duplicates"] = {"pass": (dup_total > 0 or a.variant in (3, 4)) and not repeat_proposals,
                            "server_duplicates": dup_total, "refused_then_proposed": repeat_proposals}

    # prompt
    prompts = {n: sum(1 for ln in log if "travel-prompt changer=" in ln) for n, log in clients.items()}
    detail["hands_on_key_requests"] = {n: sum(1 for ln in log if "why=use-key" in ln or "why=drop-key" in ln)
                                       for n, log in clients.items()}
    checks["prompt"] = {"pass": all(v > 0 for v in prompts.values()), "prompts": prompts}

    if a.variant >= 2:
        dest = next((kv(ln) for ln in server + first if "travel-proposed" in ln), None)
        level = dest["level"] if dest else None
        first_level = level
        if a.expect_retry_after_crash:
            # the restarted fixture may choose another changer (tv-23: Garbage after Darkscape): the
            # retry is judged against its own proposal
            retry = next((kv(ln) for ln in reversed(server) if "travel-proposed" in ln), None)
            level = retry["level"] if retry else level
        prepared = any("travel-record" in ln and "phase=1" in ln for ln in server + first)
        committed = any("travel-record" in ln and "phase=2" in ln for ln in server + first)
        published = any("checkpoint-published" in ln and "reason=travel" in ln for ln in server + first)
        reloaded = [kv(ln) for ln in server if "travel-reloaded" in ln]
        arrivals = [kv(ln) for ln in server if "travel-arrival slot=" in ln and "actor=none" not in ln]
        arrived_ok = len(arrivals) == 2 and all(float(x["distance"]) <= 3.0 and x["alive"] == "1"
                                                and x["level"] == level for x in arrivals)
        client_arrivals = {n: [kv(ln) for ln in log if "travel-arrived" in ln] for n, log in clients.items()}
        # a client that crashed at the load and restarted knows no arrival: its restart's world on the
        # destination level stands for it
        for n in (a.expect_client_crash,):
            if n in client_arrivals:
                client_arrivals[n] = [kv(ln) for ln in clients[n] if "world-level level=" in ln
                                      and kv(ln)["level"] == level]
        clients_ok = all(len(v) >= 1 and v[-1]["level"] == level for v in client_arrivals.values())
        # the participants' item layout hash: the last point before the (last) take, the first after the reload
        take_at = next((i for i, ln in reversed(list(enumerate(server))) if "travel-take id=" in ln), None)
        before = [kv(ln)["layout"] for ln in server[:take_at] if "item-graph tag=" in ln] if take_at else []
        after = [kv(ln)["layout"] for ln in server[take_at:] if "item-graph tag=" in ln] if take_at else []
        layout_same = bool(before) and bool(after) and before[-1] == after[0]
        barred = sum(1 for ln in both_runs if "travel-exec id=" in ln and "state=draining" in ln)
        crash = any("failpoint" in ln and "travel:" in ln for ln in first)
        before_take = any("failpoint" in ln and "travel:before-take" in ln for ln in first)
        # before the take: prepared only, nothing published; after the commit: all three phases; no
        # crash: the whole travel through the reload and the arrivals
        exec_ok = (prepared and not committed and not published) if before_take else \
            (prepared and committed and published and (crash or (bool(reloaded) and arrived_ok and clients_ok
                                                                and layout_same)))
        if before_take and a.expect_retry_after_crash:
            # the first run: prepared only, nothing published; the restarted server's run: the whole travel
            first_ok = any("travel-record" in ln and "phase=1" in ln for ln in first) and \
                not any("phase=2" in ln and "travel-record" in ln for ln in first) and \
                not any("checkpoint-published" in ln and "reason=travel" in ln for ln in first)
            exec_ok = first_ok and bool(reloaded) and arrived_ok and clients_ok and layout_same
        if a.variant in (3, 4):
            # the retry: the first execution failed (checked below), the second went through
            exec_ok = exec_ok and sum(1 for ln in server if "travel-committed" in ln) == 1 and len(reloaded) == 1
        checks["execution"] = {"pass": exec_ok, "destination": level,
                               "prepared": prepared, "committed": committed, "published": published,
                               "reloaded": [r.get("level") for r in reloaded], "arrivals": arrivals,
                               "client_arrivals": {n: [v["level"] for v in vs] for n, vs in client_arrivals.items()},
                               "layout_before_after": (before[-1] if before else None, after[0] if after else None),
                               "executions": barred, "crash_window": crash}
        if a.variant in (3, 4):
            failed = [kv(ln) for ln in server if "travel-exec-failed" in ln]
            phase3 = sum(1 for ln in server if "travel-record" in ln and "phase=3" in ln)
            pub_failed = [kv(ln) for ln in server if "checkpoint-failed" in ln]
            told = {n: [kv(ln) for ln in log if "travel-event" in ln and "state=6" in ln] for n, log in clients.items()}
            if a.variant == 3:
                # the partner left during the barrier: rolled back before anything durable, the
                # requester told who left; the partner is away and learns nothing (it asks again)
                want_why, want_abort, want_state = "party-member-left", "2", "draining"
                ok = (len(failed) == 1 and failed[0].get("why") == want_why and failed[0].get("abort") == want_abort
                      and failed[0].get("state") == want_state and phase3 == 0
                      and any(e.get("abort") == want_abort for e in told["c1"]))
            else:
                # the publication failed: phase 3 recorded, both told execution-failed (8)
                want_why, want_abort, want_state = "publication", "8", "publishing"
                ok = (len(failed) == 1 and failed[0].get("why") == want_why and failed[0].get("abort") == want_abort
                      and failed[0].get("state") == want_state and phase3 == 1
                      and len(pub_failed) == 1 and pub_failed[0].get("reason") == "travel"
                      and all(any(e.get("abort") == want_abort for e in v) for v in told.values()))
            checks["rollback"] = {"pass": ok, "exec_failed": [{k: f.get(k) for k in ("state", "why", "abort")}
                                  for f in failed], "phase3_records": phase3,
                                  "publication_failures": [{k: f.get(k) for k in ("generation", "reason", "detail")}
                                                           for f in pub_failed],
                                  "clients_told_abort": {n: [e.get("abort") for e in v] for n, v in told.items()}}
        if not crash:
            hold = next((kv(ln) for ln in server if "travel-hold id=" in ln), None)
            readies = [kv(ln) for ln in server if "travel-ready slot=" in ln]
            wrong = sum(1 for ln in server if "travel-ready-wrong" in ln)
            resumed = next((kv(ln) for ln in server if "travel-resumed id=" in ln), None)
            hold_at = next((i for i, ln in enumerate(server) if "travel-hold id=" in ln), None)
            resume_at = next((i for i, ln in enumerate(server) if "travel-resumed id=" in ln), None)
            moved_during_hold = sum(1 for ln in server[hold_at:resume_at] if "input-after-tick" in ln) \
                if hold_at is not None and resume_at is not None else -1
            ready_dups = sum(1 for ln in server if "travel-duplicate" in ln and "kind=13" in ln)
            client_resumes = {n: sum(1 for ln in log if "travel-resumed id=" in ln) for n, log in clients.items()}
            want_ready = 1 if a.expect_resume == "timeout" else 2
            late = a.expect_late
            # every present client is told the resume; a late one (back after the timeout) is not: its
            # ready is refused as no-hold and it knows the party went on (travel-hold-over), on the level
            resumes_ok = all(v >= 1 for n, v in client_resumes.items() if n != late)
            late_ok = True
            late_detail = None
            if late:
                no_hold = [kv(ln) for ln in server if "travel-ready-no-hold" in ln]
                resumed_at = int(next(ln for ln in server if "travel-resumed id=" in ln).split(" ", 1)[0]) \
                    if resumed else 0
                overs = [ln for ln in clients[late] if "travel-hold-over" in ln]
                late_levels = [(int(ln.split(" ", 1)[0]), kv(ln)["level"]) for ln in clients[late]
                               if "world-level level=" in ln]
                late_ok = (client_resumes.get(late, 0) == 0 and len(no_hold) == 1 and bool(overs)
                           and bool(late_levels) and late_levels[-1][1] == level and late_levels[-1][0] > resumed_at)
                late_detail = {"no_hold_readies": len(no_hold), "hold_overs": len(overs),
                               "levels_after_resume": [lv for ts, lv in late_levels if ts > resumed_at]}
            crashed = a.expect_client_crash
            crash_ok = True
            crash_detail = None
            if crashed:
                # the first run died at the load; the restart was told the hold (state 10) and reported ready
                died = sum(1 for ln in clients[crashed] if "travel-load-crash" in ln)
                notices = [kv(ln) for ln in server if "travel-held-notice" in ln]
                held = sum(1 for ln in clients[crashed] if "travel-held id=" in ln)
                crash_ok = died == 1 and len(notices) >= 1 and held >= 1 and a.expect_resume == "all-ready"
                crash_detail = {"load_crashes": died, "held_notices": [n.get("slot") for n in notices],
                                "client_held": held}
            ok = (hold is not None and resumed is not None and resumed.get("why") == a.expect_resume
                  and len({r["slot"] for r in readies}) == want_ready and wrong == a.expect_wrong_readies
                  and moved_during_hold == 0 and resumes_ok and late_ok and crash_ok)
            ready_slots = sorted({r["slot"] for r in readies})
            checks["resume"] = {"pass": ok, "hold": hold is not None, "ready_slots": ready_slots,
                                "ready_after_ms": [r["after_ms"] for r in readies], "wrong_readies": wrong,
                                "ready_duplicates": ready_dups, "resumed": resumed and {k: resumed[k] for k in
                                ("why", "after_ms", "inputs_ignored")}, "inputs_applied_during_hold": moved_during_hold,
                                "client_resumes": client_resumes, "late": late_detail, "client_crash": crash_detail}
        if crash:
            window = next((ln for ln in first if "failpoint" in ln and "travel:" in ln), "")
            rec = [kv(ln) for ln in server if "recovery-transition" in ln]
            # after travel:before-take the recovery must run on the source level (prepared only, rolled
            # back); after every later window (after-commit, reloading, after-ready) on the destination
            level_now = rec[-1]["level_now"] if rec else None
            levels_now = {n: [kv(ln)["level"] for ln in log if "world-level level=" in ln]
                          for n, log in clients.items()}
            recovered = next((kv(ln) for ln in server if "recovery status=" in ln), None)
            if rec is None or not rec:
                # no transition record in the recovered generation's journal: the crash came after the
                # reload, when later (join) generations had been published on the destination (tv-20);
                # the level is the one both clients' worlds went live on after the restart
                after = {v[-1] for v in levels_now.values() if v}
                level_now = after.pop() if len(after) == 1 else None
            if "before-take" in window:
                right = level_now is not None and level_now != first_level and rec and \
                    rec[-1]["verdict"] == "prepared-only-rolled-back"
            elif rec:
                right = level_now == level and rec[-1]["verdict"] == "committed-rolled-forward"
            else:
                right = level_now == level and recovered is not None and recovered.get("status") == "recovered"
            # a restarted server holds nothing: no hold in its run
            no_hold_after = not any("travel-hold id=" in ln for ln in server) or a.expect_retry_after_crash
            levels = {n: [kv(ln)["level"] for ln in log if "world-level level=" in ln] for n, log in clients.items()}
            if a.expect_retry_after_crash:
                # the restarted server ran the fixture again: the party travelled from the recovered source
                retried = bool(reloaded) and arrived_ok and clients_ok
                rejoined = retried and all(v and v[-1] == level for v in levels.values())
            else:
                rejoined = all(v and v[-1] == level_now for v in levels.values())
            voids = {n: sum(1 for ln in log if "travel-hold-void" in ln) for n, log in clients.items()}
            checks["recovery"] = {"pass": bool(right) and rejoined and no_hold_after,
                                  "window": window.split()[-1] if window else "",
                                  "recovery_transition": rec[-1] if rec else None, "level_now": level_now,
                                  "recovered_generation": recovered.get("generation") if recovered else None,
                                  "clients_levels": levels, "hold_voids": voids,
                                  "retried_after_recovery": a.expect_retry_after_crash and rejoined}
    if a.variant == 1:
        first_proposed = [kv(ln) for ln in first if "travel-proposed" in ln]
        exited = any("failpoint" in ln and "travel:after-propose" in ln for ln in first)
        first_id = first_proposed[0]["id"] if first_proposed else None
        # the partner's accept names the dead proposal's id, or 0 when the proposal event never
        # left the dying server (the failpoint is before the outbox flush)
        refused_old = [kv(ln)["id"] for ln in server if "travel-no-transition" in ln
                       and (f"id={first_id} " in ln or "id=0 " in ln)]
        fresh = any(re.search(r"travel-end \S+ state=agreed", ln) for ln in server)
        checks["crash"] = {"pass": bool(first_proposed) and exited and bool(refused_old) and fresh,
                           "first_run_proposed": first_id, "exited_at_failpoint": exited,
                           "restart_refused_ids": sorted(set(refused_old)), "fresh_agreed_after_restart": fresh}

    report = {"logs": str(root), "variant": a.variant, "checks": checks, "detail": detail,
              "pass": all(c["pass"] for c in checks.values())}
    text = json.dumps(report, indent=2)
    if a.out:
        Path(a.out).write_text(text + "\n")
    for name, c in checks.items():
        print(f"{name:12} {'PASS' if c['pass'] else 'FAIL'}  " +
              ", ".join(f"{k}={v}" for k, v in c.items() if k != "pass"))
    print("overall", "PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

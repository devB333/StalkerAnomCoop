#!/usr/bin/env python3
"""Check the S09-T01 party travel request and two-player consent (plan.md S09-T01, docs/travel-contract.md).

Usage: python3 tools/compare-coop-travel.py <session logs> [--variant 0|1] [--out compare-travel.json]

Inputs (builds/logs/<task>/run-<run>/): server/ (and server-first-run/ for variant 1): coop-server.log;
c1/, c2/: coop-c*.log. The session runs the travel fixture (--travel c1,c2): both actors are placed in a
level changer and the rounds of docs/travel-contract.md §validation run through the step driver.

Checks:
  rounds        every step's result on the client that sent it has the expected outcome and reason
                (per round: both accept, one refuses, timeout, stale and replayed confirmations, a
                disconnect, a cancel, not at the changer, busy and wrong-party answers, combat)
  ends          every round's transition ends in the expected state on the server, in order
  consent       no transition was agreed without the partner's accept of its own id (server
                travel-answer accept -> agreed count == agreed count); no load began (no change_level)
  duplicates    the step driver's same-id repeats got Duplicate with the stored transition, and a
                refused request's repeat was never turned into a proposal
  prompt        each client's own actor touching the changer raised the travel prompt
  crash         (variant 1) the server exited at travel:after-propose with a proposal live; the restarted
                server refused the partner's accept of it (no-transition) and ran a fresh agreed round
  execution     (variant 2, S09-T02) the agreed travel was carried out: the transition record prepared and
                committed, the destination generation published (reason travel), the level reloaded in
                process, both actors arrived within 3 m of the changer's destination point alive, both
                clients rejoined on the destination level (travel-arrived), the participants' item layout
                hash is unchanged across the travel, and no input or action was applied during the barrier
  resume        (variant 2, S09-T03) after the reload the server held the world (inputs consumed and
                discarded, no actor moved) until every client's ready named the pending transition and
                level (or the 60-s deadline), refused the driver's wrong readies (31), answered a
                ready's repeat from its stored outcome, and both clients saw the resume
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
    8: [(0, 11, R, 27), (0, 12, R, 28), (0, 11, A, 0), (1, 12, A, 0)],
}
# round 8 when the boar noticed nobody within 25 s (fixture-travel combat=0): the combat refusal not exercised
EXPECT_STEPS_NO_COMBAT_8 = [(0, 11, A, 0), (0, 12, A, 0), (0, 11, A, 0), (1, 12, A, 0)]
EXPECT_ENDS = {
    0: ["agreed"], 1: ["refused"], 2: ["expired"], 3: ["agreed"], 4: ["aborted:partner-left", "agreed"],
    5: ["cancelled"], 6: [], 7: ["agreed"], 8: ["agreed"],
}


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
    a = ap.parse_args()
    root = Path(a.logs)
    server = lines(root / "server" / "coop-server.log")
    first = lines(root / "server-first-run" / "coop-server.log")
    clients = {n: lines(root / n / f"coop-{n}.log") for n in ("c1", "c2")}
    for n in ("c1", "c2"):
        clients[n] += lines(root / f"{n}-first-run" / f"coop-{n}.log")
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
    expected_rounds = [0, 1] if a.variant == 1 else [0] if a.variant == 2 else list(range(9))
    detail["rounds_ran"] = ran
    combat_line = next((kv(ln) for ln in server if "fixture-travel combat=" in ln), None)
    detail["combat_exercised"] = combat_line.get("combat") == "1" if combat_line else None

    # rounds: each step's result
    failures = []
    no_combat = bool(combat_line) and combat_line.get("combat") != "1"
    detail["combat_not_exercised"] = no_combat
    for r in expected_rounds:
        steps = EXPECT_STEPS_NO_COMBAT_8 if r == 8 and no_combat else EXPECT_STEPS[r]
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
            if have != sorted(want):
                failures.append(f"round {r} step {s}: expected {sorted(want)}, got {have}")
    checks["rounds"] = {"pass": not failures, "failures": failures}

    # ends per round
    end_failures = []
    for r in expected_rounds:
        ends = []
        for line in rounds.get(r, []):
            m = re.search(r"travel-end id=(\w+) state=([\w-]+) abort=([\w-]+)", line)
            if m:
                ends.append(m.group(2) if m.group(2) != "aborted" else f"aborted:{m.group(3)}")
        want = list(EXPECT_ENDS[r])
        if a.variant == 1 and r == 0:
            want = ["agreed"]  # the restarted server's round 0; the first run's proposal never ended there
        if r == 8 and no_combat:
            want = ["cancelled", "agreed"]
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
    checks["duplicates"] = {"pass": dup_total > 0 and not repeat_proposals, "server_duplicates": dup_total,
                            "refused_then_proposed": repeat_proposals}

    # prompt
    prompts = {n: sum(1 for ln in log if "travel-prompt changer=" in ln) for n, log in clients.items()}
    detail["hands_on_key_requests"] = {n: sum(1 for ln in log if "why=use-key" in ln or "why=drop-key" in ln)
                                       for n, log in clients.items()}
    checks["prompt"] = {"pass": all(v > 0 for v in prompts.values()), "prompts": prompts}

    if a.variant == 2:
        dest = next((kv(ln) for ln in server + first if "travel-proposed" in ln), None)
        level = dest["level"] if dest else None
        prepared = any("travel-record" in ln and "phase=1" in ln for ln in server + first)
        committed = any("travel-record" in ln and "phase=2" in ln for ln in server + first)
        published = any("checkpoint-published" in ln and "reason=travel" in ln for ln in server + first)
        reloaded = [kv(ln) for ln in server if "travel-reloaded" in ln]
        arrivals = [kv(ln) for ln in server if "travel-arrival slot=" in ln and "actor=none" not in ln]
        arrived_ok = len(arrivals) == 2 and all(float(x["distance"]) <= 3.0 and x["alive"] == "1"
                                                and x["level"] == level for x in arrivals)
        client_arrivals = {n: [kv(ln) for ln in log if "travel-arrived" in ln] for n, log in clients.items()}
        clients_ok = all(len(v) >= 1 and v[-1]["level"] == level for v in client_arrivals.values())
        # the participants' item layout hash: the last point before the take, the first after the reload
        take_at = next((i for i, ln in enumerate(server) if "travel-take id=" in ln), None)
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
        checks["execution"] = {"pass": exec_ok, "destination": level,
                               "prepared": prepared, "committed": committed, "published": published,
                               "reloaded": [r.get("level") for r in reloaded], "arrivals": arrivals,
                               "client_arrivals": {n: [v["level"] for v in vs] for n, vs in client_arrivals.items()},
                               "layout_before_after": (before[-1] if before else None, after[0] if after else None),
                               "executions": barred, "crash_window": crash}
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
            ok = (hold is not None and resumed is not None and resumed.get("why") == a.expect_resume
                  and len({r["slot"] for r in readies}) == want_ready and wrong == a.expect_wrong_readies
                  and moved_during_hold == 0 and all(v >= 1 for v in client_resumes.values()))
            ready_slots = sorted({r["slot"] for r in readies})
            checks["resume"] = {"pass": ok, "hold": hold is not None, "ready_slots": ready_slots,
                                "ready_after_ms": [r["after_ms"] for r in readies], "wrong_readies": wrong,
                                "ready_duplicates": ready_dups, "resumed": resumed and {k: resumed[k] for k in
                                ("why", "after_ms", "inputs_ignored")}, "inputs_applied_during_hold": moved_during_hold,
                                "client_resumes": client_resumes}
        if crash:
            window = next((ln for ln in first if "failpoint" in ln and "travel:" in ln), "")
            rec = [kv(ln) for ln in server if "recovery-transition" in ln]
            # after travel:after-commit the recovery must run on the destination level; after
            # travel:before-take on any other (the source) level
            level_now = rec[-1]["level_now"] if rec else None
            if "after-commit" in window:
                right = level_now == level and rec and rec[-1]["verdict"] == "committed-rolled-forward"
            else:
                right = level_now is not None and level_now != level and rec and \
                    rec[-1]["verdict"] == "prepared-only-rolled-back"
            levels = {n: [kv(ln)["level"] for ln in log if "world-level level=" in ln] for n, log in clients.items()}
            rejoined = all(v and v[-1] == level_now for v in levels.values())
            checks["recovery"] = {"pass": bool(right) and rejoined, "window": window.split()[-1] if window else "",
                                  "recovery_transition": rec[-1] if rec else None, "level_now": level_now,
                                  "clients_levels": levels}
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

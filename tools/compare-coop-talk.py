#!/usr/bin/env python3
"""Check the S10-T04a talk-window fixture (plan.md S10-T04a; docs/validation/S10-T04a.md).

Usage: python3 tools/compare-coop-talk.py <session logs> [--out compare-talk.json] [--expect-restart]

Inputs (builds/logs/<task>/run-<run>/): server/ (and server-first-run/ with --expect-restart): coop-server.log;
c1/, c2/: coop-c*.log. The session runs the talk fixture (--talk c1,c2): both actors are placed beside
Sidorovich and the rounds run through the step driver (actions 14 talk start, 15 say-path, 16 talk end).

Rounds (CoopServerHost.cpp talk_fixtures):
  0  p1 starts, walks the accept path, ends          -> every say Accepted; the party task accepted by talking
  1  p1 starts; p2 starts while p1 talks; p1 ends    -> p2's start refused 33 (busy)
  2  p2 starts, walks the accept path, ends          -> the giver does not offer the party's task again (the task
                                                        manager's list is world state): p2 asks for the next task until
                                                        the giver has none and leaves; no second accept
  3  the patches appear beside each actor; each picks up its half
  4  p1 starts, walks the turn-in path, ends         -> the party task completed by talking
  5  p1 says with no talk open (34); p1 starts, says an unknown id (35); p2 starts (33); p1 ends; p2 ends
     with no talk (34)

Checks:
  rounds          every step's result on the client that sent it has the expected outcome and reason
  one_speaker     every refused start while the NPC talked to the other player is a 33; no talk was opened
                  for a player while the NPC's talk with the other was open (server talk-start/talk-close order)
  quest           the party task was accepted by round 0's talk (quest-accepted source=talk) and completed by
                  round 4's talk (quest-completed after talk-quest op=finish reason=0); nothing was given twice
                  (one quest-accepted, none by slot 1, one quest-completed)
  windows         each accepted start opened the window on its client (talk-open) and every open was closed
                  (talk-closed) with the server's why; each open talk got at least one list and one answer
  frozen          no input moved a talking actor (the server held inputs while talking: inputs_held > 0) and
                  no talk is open at the end (talk-summary open=0)
  restart         (--expect-restart) the first server run exited at the failpoint with a talk open; the restarted
                  server held no talk and the client's window closed on its rejoin (talk-reset); the fixture did
                  not run again (--restart-no-fixture)
"""

import argparse
import json
import re
import sys
from pathlib import Path

A, R = 1, 2  # ActionOutcome


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def kv(line):
    return dict(re.findall(r"(\w+)=([^\s\"]+|\"[^\"]*\")", line))


# per round: (slot, kind, outcome, reason) per step; "says": one or more say results, all accepted
EXPECT_STEPS = {
    0: [(0, 14, A, 0), "says0", (0, 16, A, 0)],
    1: [(0, 14, A, 0), (1, 14, R, 33), (0, 16, A, 0)],
    2: [(1, 14, A, 0), "says1", (1, 16, A, 0)],
    3: ["pickups"],
    4: [(0, 14, A, 0), "says0", (0, 16, A, 0)],
    5: [(0, 15, R, 34), (0, 14, A, 0), (0, 15, R, 35), (1, 14, R, 33), (0, 16, A, 0), (1, 16, R, 34)],
}


def server_rounds(log):
    rounds = {-1: []}
    current = -1
    for line in log:
        m = re.search(r"fixture-talk-round round=(\d+)", line)
        if m:
            current = int(m.group(1))
            rounds.setdefault(current, [])
        rounds[current].append(line)
    return rounds


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    ap.add_argument("--expect-restart", action="store_true")
    a = ap.parse_args()
    root = Path(a.logs)
    server = lines(root / "server" / "coop-server.log")
    first = lines(root / "server-first-run" / "coop-server.log")
    clients = {}
    for n in ("c1", "c2"):
        earlier = lines(root / f"{n}-first-run" / f"coop-{n}.log")
        later = lines(root / n / f"coop-{n}.log")
        clients[n] = later if earlier and later and earlier[0] == later[0] else earlier + later
    checks = {}
    detail = {}

    results = {}  # (round, step) -> [(slot, kind, outcome, reason)]
    for n, log in clients.items():
        for line in log:
            if "talk-result" in line or "pickup-result" in line:
                d = kv(line)
                if "step" not in d and "talk-result" not in line:
                    continue
                if "talk-result" in line:
                    key = (int(d["round"]), int(d["step"]))
                    if key[0] < 0 or key[1] < 0:
                        continue
                    results.setdefault(key, []).append((int(n[1]) - 1, int(d["kind"]), int(d["outcome"]),
                                                        int(d["reason"])))
    # pickups of round 3 come from the driver's csv-like pickup-result lines (round=3)
    pickups = {}
    for n, log in clients.items():
        for line in log:
            if "pickup-result" in line:
                d = kv(line)
                if d.get("round") == "3":
                    pickups.setdefault(n, []).append((int(d["outcome"]), int(d["reason"])))
    detail["results"] = {f"{r}/{s}": v for (r, s), v in sorted(results.items())}
    detail["pickups"] = pickups

    both_runs = first + server
    rounds = server_rounds(both_runs)
    ran = sorted(r for r in rounds if r >= 0)
    detail["rounds_ran"] = ran
    expected_rounds = [0] if a.expect_restart else list(range(6))

    failures = []
    for r in expected_rounds:
        for s, exp in enumerate(EXPECT_STEPS[r]):
            got = results.get((r, s), [])
            if exp == "pickups":
                for n in ("c1", "c2"):
                    got_p = pickups.get(n, [])
                    if not got_p or any(g != (A, 0) for g in got_p):
                        failures.append(f"round 3: {n} pickups {got_p}")
                continue
            if isinstance(exp, str) and exp.startswith("says"):
                slot = int(exp[4])
                if a.expect_restart and r == 0:
                    # the server dies inside the accept phrase: that say's result never comes
                    if not got or any(g[0] != slot or g[1] != 15 for g in got):
                        failures.append(f"round {r} step {s}: expected says by slot {slot}, got {got}")
                    continue
                if not got or any(g != (slot, 15, A, 0) for g in got) or len(got) < 2:
                    failures.append(f"round {r} step {s}: expected >= 2 accepted says by slot {slot}, got {got}")
                continue
            if a.expect_restart and r == 0 and s == 2:
                continue  # the end never ran: the server was gone
            if sorted(got) != [exp]:
                failures.append(f"round {r} step {s}: expected {[exp]}, got {sorted(got)}")
    checks["rounds"] = {"pass": not failures, "failures": failures}

    # one speaker per NPC: the server's talk-start / talk-close / talk-busy order
    open_slots = set()
    overlap = []
    busy = 0
    for line in both_runs:
        d = kv(line)
        if " talk-start " in line:
            slot = int(d["slot"])
            if open_slots and slot not in open_slots:
                overlap.append(line[:160])
            open_slots.add(slot)
        elif " talk-close " in line:
            open_slots.discard(int(d["slot"]))
        elif " talk-busy " in line:
            busy += 1
    # the step driver repeats each request once with the same id; a talk kind is not a stored outcome, so
    # the server evaluates the repeat afresh (a second talk-busy line per refused start)
    busy_results = sum(1 for log in clients.values() for ln in log
                       if "talk-result kind=14" in ln and "outcome=2 reason=33" in ln)
    expected_busy = 0 if a.expect_restart else 2
    checks["one_speaker"] = {"pass": not overlap and busy_results == expected_busy and busy >= expected_busy,
                             "overlaps": overlap, "busy_results": busy_results, "busy_lines": busy,
                             "expected_busy": expected_busy}

    # quest through the talk
    accepted = [ln for ln in both_runs if "quest-accepted " in ln and "source=talk" in ln]
    completed = [ln for ln in both_runs if " quest-completed " in ln]
    finish_ok = [ln for ln in both_runs if "talk-quest " in ln and "op=finish" in ln and "reason=0" in ln]
    second_accept = [ln for ln in both_runs if "quest-accept" in ln and "slot=1" in ln]
    all_accepted = [ln for ln in both_runs if "quest-accepted " in ln]
    if a.expect_restart:
        quest_pass = len(accepted) == 1 and len(all_accepted) == 1 and not completed
    else:
        quest_pass = (len(accepted) == 1 and len(all_accepted) == 1 and len(completed) == 1 and len(finish_ok) == 1
                      and not second_accept)
    checks["quest"] = {"pass": quest_pass, "accepted_by_talk": len(accepted), "accepted": len(all_accepted),
                       "completed": len(completed), "finish_routed": len(finish_ok),
                       "second_accept_by_slot_1": len(second_accept)}

    # windows
    window_failures = []
    for n, log in clients.items():
        opens = [ln for ln in log if " talk-open " in ln]
        closes = [ln for ln in log if " talk-closed " in ln]
        resets = [ln for ln in log if " talk-reset " in ln]
        lists = sum(1 for ln in log if " talk-list " in ln)
        answers = sum(1 for ln in log if " talk-answer " in ln)
        starts_ok = sum(1 for ln in log if "talk-result kind=14" in ln and "outcome=1" in ln)
        if len(opens) != starts_ok:
            window_failures.append(f"{n}: {len(opens)} opens for {starts_ok} accepted starts")
        # the driver's end step closes the window itself (no talk-closed line: the server's close finds no open talk)
        driver_ends = sum(1 for ln in log if "talk-end-request" in ln)
        if len(closes) + len(resets) + driver_ends < len(opens):
            window_failures.append(f"{n}: {len(opens)} opens, {len(closes)} closes, {len(resets)} resets, "
                                   f"{driver_ends} driver ends")
        if opens and (lists < len(opens) or answers < len(opens)):
            window_failures.append(f"{n}: {len(opens)} opens, {lists} lists, {answers} answers")
        detail[f"{n}_windows"] = {"opens": len(opens), "closes": len(closes), "resets": len(resets), "lists": lists,
                                  "answers": answers, "driver_ends": driver_ends}
    checks["windows"] = {"pass": not window_failures, "failures": window_failures}

    # frozen: inputs held while talking; nothing open at the end
    summary = next((kv(ln) for ln in reversed(server) if " talk-summary " in ln), None)
    held = int(summary["inputs_held"]) if summary else 0
    open_end = int(summary["open"]) if summary else -1
    # with a restart the talk ran in the first server run, which died without a summary: only the end state is judged
    checks["frozen"] = {"pass": (held > 0 or a.expect_restart) and open_end == 0, "inputs_held": held,
                        "open_at_end": open_end}

    if a.expect_restart:
        died = any("failpoint" in ln and "quest:after-accept" in ln for ln in first)
        first_open = [ln for ln in first if " talk-start " in ln]
        first_closed = [ln for ln in first if " talk-close " in ln]
        restart_summary = summary
        reran = any("fixture-talk-round" in ln for ln in server)
        c1 = clients["c1"]
        c1_reset = [ln for ln in c1 if " talk-reset " in ln]
        # the client's window: closed by the server's death (talk-reset on the rejoin) or already by the
        # driver's end step before the connection timed out; the retried say is refused by the restarted
        # server (34: it holds no talk) and the client's final summary shows no open talk
        rejoin_at = next((i for i, ln in enumerate(c1) if "epoch-changed" in ln), len(c1))
        refused_after = sum(1 for ln in c1[rejoin_at:] if "talk-result kind=15" in ln and "reason=34" in ln)
        c1_summary = next((kv(ln) for ln in reversed(c1) if " talk-summary " in ln), None)
        c1_open_end = int(c1_summary["open"]) if c1_summary else -1
        checks["restart"] = {"pass": died and len(first_open) == 1 and not first_closed and restart_summary is not None
                             and open_end == 0 and not reran and c1_open_end == 0 and refused_after >= 1,
                             "died_at_failpoint": died, "first_run_talks": len(first_open),
                             "first_run_closes": len(first_closed), "fixture_reran": reran,
                             "c1_resets": len(c1_reset), "c1_say_refused_after_rejoin": refused_after,
                             "c1_open_at_end": c1_open_end}

    report = {"checks": checks, "detail": detail, "pass": all(c["pass"] for c in checks.values())}
    text = json.dumps(report, indent=1)
    if a.out:
        Path(a.out).write_text(text + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

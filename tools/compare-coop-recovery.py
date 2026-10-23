#!/usr/bin/env python3
"""Check a co-op session with a server termination and restart against the
persistence contract (docs/persistence-contract.md §10, S07-T02/T03/T04).

Usage: python3 tools/compare-coop-recovery.py <session logs> [--out compare-recovery.json]

Inputs (builds/logs/<task>/run-<run>/):
  server-first-run/, server-run-N/, server/   one folder per server incarnation
      coop-server.log       recovery, restored, ledger-*, checkpoint-* lines
      coop-ledger.csv       every commit, apply and replay of the ledger effect
      coop-checkpoints.csv  every publication attempt
  <client>/coop-c*.log      the welcomed slot
  <client>/coop-ledger-client.csv   sent, retry, result, late rows

Checks:
  recovery          every incarnation after the first recovered a generation
                    (or a fresh journal) and replayed without a refusal
  restored_money    for each participant, the money the restarted server holds
                    after its replay equals the money_after of the last record
                    the previous incarnation committed (its last csv commit
                    row, or a later record the restart replayed: a record
                    flushed after the csv line was lost with the process)
  acked_not_lost    every operation a client saw Accepted has a revision no
                    higher than the restarted server's highest revision, and
                    when the client retried it after the restart the server
                    answered from its index with the same revision
  duplicates        every late result (a second answer to an operation) carries
                    the same outcome and commit as the first
  revisions_unique  no commit revision appears twice across incarnations
  deltas_chain      within a slot, each committed money_after is the previous
                    one plus the delta (clamped at 0), across the restart too
  checkpoints       every incarnation published at least one generation and no
                    publication failed
A transient rollback (positions, NPCs) is reported from the recovery line, not
failed: the contract allows it.
"""

import argparse
import csv
import json
import re
import sys
from collections import defaultdict
from pathlib import Path


def read_csv(path):
    if not path.exists():
        return []
    return list(csv.DictReader(open(path, newline="")))


def incarnations(logs):
    folders = []
    if (logs / "server-first-run").exists():
        folders.append(logs / "server-first-run")
    n = 1
    while (logs / f"server-run-{n}").exists():
        folders.append(logs / f"server-run-{n}")
        n += 1
    folders.append(logs / "server")
    return folders


def log_lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def field(line, name, cast=str):
    m = re.search(r"\b" + re.escape(name) + r"=(\S+)", line)
    return cast(m.group(1)) if m else None


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="compare-recovery.json")
    a = ap.parse_args()
    logs = Path(a.logs)
    checks = {}
    report = {"incarnations": [], "clients": {}}

    runs = []
    for folder in incarnations(logs):
        lines = log_lines(folder / "coop-server.log")
        ledger = read_csv(folder / "coop-ledger.csv")
        checkpoints = read_csv(folder / "coop-checkpoints.csv")
        rec = {"folder": folder.name, "ledger_rows": len(ledger), "checkpoint_rows": len(checkpoints)}
        for line in lines:
            if line.find(" recovery ") >= 0 and "status=" in line:
                rec["recovery"] = {k: field(line, k) for k in ("status", "generation", "fallback", "revision",
                                                              "epoch", "replay", "torn_tails", "save")}
            elif "recovery-refused" in line:
                rec["refused"] = line
            elif "recovery-replayed" in line:
                rec["replayed"] = {k: field(line, k, int) for k in ("records", "commit", "epoch")}
            elif "recovery-replay-failed" in line or "recovery-identity-mismatch" in line \
                    or "recovery-roster-mismatch" in line:
                rec.setdefault("failures", []).append(line)
            elif " ledger-state " in line:
                rec.setdefault("restored_money", {})[int(field(line, "slot"))] = int(field(line, "money"))
            elif " ledger-duplicate " in line:
                rec.setdefault("duplicates", []).append({"operation": int(field(line, "operation")),
                                                         "revision": int(field(line, "revision")),
                                                         "outcome": int(field(line, "outcome"))})
            elif " recovery-load " in line:
                rec["load"] = {k: field(line, k) for k in ("entities_saved", "entities_restored", "not_loaded")}
            elif " store-failed " in line:
                rec.setdefault("store_failed", []).append(line)
            elif " failpoint-armed " in line:
                rec["failpoint"] = field(line, "point")
        rec["commits"] = [{"revision": int(r["revision"]), "slot": int(r["slot"]), "operation": int(r["operation"]),
                           "delta": int(r["delta"]), "money_after": int(r["money_after"])}
                          for r in ledger if r["phase"] == "commit"]
        rec["replays"] = [{"revision": int(r["revision"]), "slot": int(r["slot"]), "money_after": int(r["money_after"])}
                          for r in ledger if r["phase"] == "replay"]
        rec["checkpoints"] = [{"generation": int(r["generation"]), "revision": int(r["revision"]),
                               "reason": r["reason"], "ok": r["ok"] == "1", "save_ms": int(r["save_ms"]),
                               "publish_ms": float(r["publish_ms"]), "detail": r["detail"]} for r in checkpoints]
        runs.append(rec)
        report["incarnations"].append(rec)

    # recovery
    later = runs[1:]
    checks["recovery"] = {
        "pass": bool(later) and all("recovery" in r and "refused" not in r and "failures" not in r
                                    and "replayed" in r for r in later),
        "detail": [{"folder": r["folder"], "recovery": r.get("recovery"), "refused": r.get("refused"),
                    "failures": r.get("failures"), "replayed": r.get("replayed"), "load": r.get("load")}
                   for r in later],
    }

    # restored money per slot across each boundary
    restored = []
    ok = bool(later)
    for before, after in zip(runs, runs[1:], strict=False):
        for slot, money in after.get("restored_money", {}).items():
            commits = [c for c in before["commits"] if c["slot"] == slot]
            last = commits[-1] if commits else None
            # the checkpoint the restart loaded may be older than the last commit; the replayed
            # records bring it forward; a record flushed after the csv line was written is
            # replayed too and is also accepted
            replayed = [r for r in after["replays"] if r["slot"] == slot]
            expected = last["money_after"] if last else None
            later_replay = [r for r in replayed if last and r["revision"] > last["revision"]]
            passed = (expected is None and True) or money == expected or \
                (bool(later_replay) and money == later_replay[-1]["money_after"])
            if expected is None:
                # no commit before: the restored money must equal the previous run's starting state;
                # unknown here, so report only
                passed = True
            ok = ok and passed
            restored.append({"boundary": f"{before['folder']}->{after['folder']}", "slot": slot, "restored": money,
                             "last_commit": last, "later_replay": later_replay, "pass": passed})
    checks["restored_money"] = {"pass": ok, "detail": restored}

    # clients
    client_acked = {}
    dup_ok, dup_detail = True, []
    for folder in sorted(p for p in logs.iterdir() if p.is_dir() and p.name.startswith("c")):
        slot = None
        for line in log_lines(folder / f"coop-{folder.name}.log"):
            if " welcomed " in line and slot is None:
                slot = field(line, "slot", int)
        rows = read_csv(folder / "coop-ledger-client.csv")
        acked, results, late = {}, {}, []
        sent = retries = rejected = unsent = 0
        for r in rows:
            op = int(r["operation"])
            if r["event"] == "sent":
                sent += 1
            elif r["event"] == "retry":
                retries += 1
            elif r["event"] == "unsent":
                unsent += 1
            elif r["event"] == "result":
                results[op] = {"outcome": int(r["outcome"]), "commit": int(r["commit"]), "delta": int(r["delta"])}
                if r["outcome"] == "1":
                    acked[op] = int(r["commit"])
                else:
                    rejected += 1
            elif r["event"] == "late":
                late.append({"operation": op, "outcome": int(r["outcome"]), "commit": int(r["commit"])})
        for item in late:
            if item["operation"] < (1 << 40):
                continue  # not a ledger operation (ledger ids are seeded from the UTC clock << 16)
            first = results.get(item["operation"])
            same = first is not None and first["outcome"] == item["outcome"] and first["commit"] == item["commit"]
            dup_ok = dup_ok and same
            dup_detail.append({"client": folder.name, **item, "first": first, "pass": same})
        client_acked[folder.name] = (slot, acked)
        report["clients"][folder.name] = {"slot": slot, "sent": sent, "retries": retries, "unsent": unsent,
                                          "acked": len(acked), "rejected": rejected, "late": len(late),
                                          "pending_at_end": int(rows[-1]["pending"]) if rows else None}
    checks["duplicates"] = {"pass": dup_ok, "detail": dup_detail, "late_results": len(dup_detail)}

    # acked not lost: every acked op revision <= the next incarnation's highest revision; retries answered
    # from the index with the same revision
    ok = bool(later)
    detail = []
    for before, after in zip(runs, runs[1:], strict=False):
        highest = after.get("replayed", {}).get("commit")
        answered = {d["operation"]: d for d in after.get("duplicates", [])}
        for client, (slot, acked) in client_acked.items():
            before_ops = {c["operation"]: c for c in before["commits"]}
            for op, commit in acked.items():
                if op not in before_ops:
                    continue  # acknowledged by another incarnation
                kept = highest is not None and commit <= highest
                dup = answered.get(op)
                same = dup is None or (dup["revision"] == commit and dup["outcome"] == 1)
                ok = ok and kept and same
                detail.append({"boundary": f"{before['folder']}->{after['folder']}", "client": client, "slot": slot,
                               "operation": op, "commit": commit, "kept": kept,
                               "retry_answered": dup is not None, "same_revision": same})
    # an operation a client saw Accepted must have been committed by some incarnation
    all_commits = {c["operation"] for r in runs for c in r["commits"]}
    acked_without_commit = [{"client": client, "operation": op, "commit": commit}
                            for client, (slot, acked) in client_acked.items() for op, commit in acked.items()
                            if op not in all_commits]
    ok = ok and not acked_without_commit
    checks["acked_not_lost"] = {"pass": ok, "count": len(detail),
                                "retries_answered": sum(1 for d in detail if d["retry_answered"]),
                                "failures": [d for d in detail if not (d["kept"] and d["same_revision"])],
                                "acked_without_commit": acked_without_commit}

    # revisions unique
    seen = defaultdict(list)
    for r in runs:
        for c in r["commits"]:
            seen[c["revision"]].append(r["folder"])
    dupes = {k: v for k, v in seen.items() if len(v) > 1}
    checks["revisions_unique"] = {"pass": not dupes, "duplicates": dupes,
                                  "commits": sum(len(r["commits"]) for r in runs)}

    # deltas chain
    ok, breaks = True, []
    last_money = {}
    for r in runs:
        for slot, money in r.get("restored_money", {}).items():
            last_money[slot] = money
        for c in r["commits"]:
            prev = last_money.get(c["slot"])
            if prev is not None:
                expected = max(0, prev + c["delta"])
                if c["money_after"] != expected:
                    ok = False
                    breaks.append({"folder": r["folder"], **c, "expected": expected})
            last_money[c["slot"]] = c["money_after"]
    checks["deltas_chain"] = {"pass": ok, "breaks": breaks}

    # checkpoints
    ok = all(any(c["ok"] for c in r["checkpoints"]) for r in runs) and \
        all(c["ok"] for r in runs for c in r["checkpoints"])
    per_incarnation = []
    for r in runs:
        cps = r["checkpoints"]
        per_incarnation.append({"folder": r["folder"], "published": sum(c["ok"] for c in cps),
                                "failed": sum(not c["ok"] for c in cps),
                                "save_ms_max": max([c["save_ms"] for c in cps] or [0]),
                                "publish_ms_max": max([c["publish_ms"] for c in cps] or [0]),
                                "generations": [c["generation"] for c in cps if c["ok"]]})
    checks["checkpoints"] = {"pass": ok, "per_incarnation": per_incarnation}

    report["checks"] = checks
    report["pass"] = all(c["pass"] for c in checks.values())
    Path(a.out).write_text(json.dumps(report, indent=2) + "\n")
    for name, c in checks.items():
        print(f"{name:18s} {'PASS' if c['pass'] else 'FAIL'}")
    print("overall", "PASS" if report["pass"] else "FAIL", "->", a.out)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

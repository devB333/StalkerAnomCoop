#!/usr/bin/env python3
"""Run the S07-T06 termination matrix: one co-op session per failpoint, the
server terminating itself at the armed boundary (or failing the store call
with :fail), restarted by the runner, the clients reconnecting, and
tools/compare-coop-recovery.py checking each run.

Usage: python3 tools/run-recovery-matrix.py <prefix> [--task S07-T06]
          [--points commit:before-append,commit:after-flush,...]
          [--count-commit 40] [--count-checkpoint 5] [--checkpoint-seconds 60]
          [--client-seconds 340] [--only n]

Each point gets run <prefix>-<n>: prepare-coop-session.py (ledger program on
both clients, periodic checkpoints, the failpoint in the first profile only),
Run-CoopSession.ps1 -ServerKill 0 -ServerRestartAfter 10, then the recovery
comparison. The per-point expectations (docs/persistence-contract.md §11):

  commit:before-append    the operation is not durable: evaluated afresh on retry
  commit:after-flush      durable, unapplied, unacknowledged: replayed; the
  commit:after-apply      client's retry gets the stored outcome (ledger-duplicate)
  commit:after-ack        durable and acknowledged
  checkpoint:after-temp   the pointer stays: recovery from the previous
  checkpoint:after-rename generation plus its journal tail
  checkpoint:after-manifest
  checkpoint:after-rotation
  checkpoint:after-pointer the new generation is current, nothing to replay
  commit:before-append:fail  the store fails closed: reason 5, no apply, clean
                          stop without a final generation; recovery intact

Writes builds/logs/<task>/matrix-<prefix>.json.
"""

import argparse
import json
import re
import subprocess
import sys
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parents[1]
DEFAULT_POINTS = ["commit:before-append", "commit:after-flush", "commit:after-apply", "commit:after-ack",
                  "checkpoint:after-temp", "checkpoint:after-rename", "checkpoint:after-manifest",
                  "checkpoint:after-rotation", "checkpoint:after-pointer", "commit:before-append:fail"]
TERMINATED = 0xDEAD


def winpath(path: Path) -> str:
    return subprocess.check_output(["wslpath", "-w", str(path)], text=True).strip()


def lines_of(path: Path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def field(line, name, cast=str):
    m = re.search(r"\b" + re.escape(name) + r"=(\S+)", line)
    return cast(m.group(1)) if m else None


def run_point(prefix, n, point, a):
    run = f"{prefix}-{n}"
    task = a.task
    fail_mode = point.endswith(":fail")
    base = point[:-5] if fail_mode else point
    count = a.count_checkpoint if base.startswith("checkpoint:") else a.count_commit
    spec = f"{base}:{count}" + (":fail" if fail_mode else "")
    subprocess.run([sys.executable, str(WORKSPACE / "tools/prepare-coop-session.py"), run, "--task", task,
                    "--ledger", "c1,c2", "--checkpoint-seconds", str(a.checkpoint_seconds), "--failpoint", spec],
                   check=True, stdout=subprocess.DEVNULL)
    script = winpath(WORKSPACE / "tools/Run-CoopSession.ps1")
    cmd = ["powershell.exe", "-NoLogo", "-NoProfile", "-NonInteractive", "-ExecutionPolicy", "Bypass", "-File",
           script, "-Run", run, "-Task", task, "-ServerAloneSeconds", "10", "-ClientStarts", "0,15",
           "-ClientSeconds", f"{a.client_seconds},{a.client_seconds - 15}", "-AfterSeconds", "15",
           "-ServerKill", "0", "-ServerRestartAfter", "10"]
    proc = subprocess.run(cmd, capture_output=True, text=True)
    logs = WORKSPACE / "builds/logs" / task / f"run-{run}"
    result = {"run": run, "point": point, "spec": spec, "runner_exit": proc.returncode}
    if proc.returncode != 0:
        result["runner_error"] = proc.stdout[-2000:] + proc.stderr[-2000:]
        return result
    runner = json.loads((logs / "result.json").read_text())
    result["events"] = [(e["t"], e["event"]) for e in runner["events"]]
    first_exit = runner.get("server_runs", [{}])[0].get("exit_code") if runner.get("server_runs") else None
    result["first_exit_code"] = first_exit
    compare = logs / "compare-recovery.json"
    cp = subprocess.run([sys.executable, str(WORKSPACE / "tools/compare-coop-recovery.py"), str(logs), "--out",
                         str(compare)], capture_output=True, text=True)
    result["compare_pass"] = cp.returncode == 0
    r = json.loads(compare.read_text()) if compare.exists() else {}
    incs = r.get("incarnations", [])
    first = incs[0] if incs else {}
    second = incs[1] if len(incs) > 1 else {}
    first_log = lines_of(logs / "server-first-run/coop-server.log")
    armed = next((line for line in first_log if " failpoint-armed " in line), "")
    result["armed"] = armed.split(" ", 1)[1] if armed else None
    result["first_commits"] = len(first.get("commits", []))
    result["first_generations"] = [c["generation"] for c in first.get("checkpoints", []) if c["ok"]]
    result["recovery"] = second.get("recovery")
    result["replayed"] = second.get("replayed")
    result["restored_money"] = second.get("restored_money")
    result["duplicates_answered"] = len(second.get("duplicates", []))
    result["store_failed"] = first.get("store_failed")
    result["clients"] = r.get("clients")
    checks = {k: v["pass"] for k, v in r.get("checks", {}).items()}
    # per-point expectations
    expect = {}
    if fail_mode:
        expect["clean_stop_after_store_failure"] = first_exit == 0 and bool(first.get("store_failed"))
        rejected = any(" reason=5" in line and "intent-refused" in line for line in first_log)
        expect["intent_refused_reason_5"] = rejected
        gens = first.get("checkpoints", [])
        failed_at = next((i for i, line in enumerate(first_log) if " store-failed " in line), None)
        last_checkpoint = next((i for i, line in reversed(list(enumerate(first_log)))
                                if "checkpoint-published" in line), -1)
        expect["no_generation_after_failure"] = failed_at is not None and last_checkpoint < failed_at and \
            all(c["ok"] for c in gens)
    else:
        expect["terminated_at_point"] = first_exit == TERMINATED
        if base.startswith("checkpoint:"):
            published = result["first_generations"]
            rec_gen = int(second.get("recovery", {}).get("generation", 0)) if second.get("recovery") else 0
            if base == "checkpoint:after-pointer":
                # the pointer moved before the termination: the new generation (not in the csv: the
                # csv line is written after retention) is current and holds everything
                expect["new_generation_current"] = rec_gen == (max(published) + 1 if published else 1) and \
                    (second.get("replayed") or {}).get("records") == 0
            else:
                expect["previous_generation_current"] = bool(published) and rec_gen == max(published)
        else:
            stored = second.get("duplicates", [])
            if base in ("commit:after-flush", "commit:after-apply"):
                expect["retry_answered_from_index"] = len(stored) >= 1
            if base == "commit:before-append":
                expect["retry_evaluated_afresh"] = len(stored) == 0
    result["checks"] = checks
    result["expect"] = expect
    result["pass"] = result["compare_pass"] and all(expect.values())
    return result


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("prefix")
    ap.add_argument("--task", default="S07-T06")
    ap.add_argument("--points", default=",".join(DEFAULT_POINTS))
    ap.add_argument("--count-commit", type=int, default=40)
    ap.add_argument("--count-checkpoint", type=int, default=5)
    ap.add_argument("--checkpoint-seconds", type=int, default=60)
    ap.add_argument("--client-seconds", type=int, default=340)
    ap.add_argument("--only", type=int, default=0, help="run only point number n (1-based)")
    a = ap.parse_args()
    if not re.fullmatch(r"[a-zA-Z0-9-]+", a.prefix):
        raise SystemExit("prefix must match ^[a-zA-Z0-9-]+$")
    points = [p for p in a.points.split(",") if p]
    out = WORKSPACE / "builds/logs" / a.task / f"matrix-{a.prefix}.json"
    results = json.loads(out.read_text()) if out.exists() else {"prefix": a.prefix, "runs": []}
    for n, point in enumerate(points, start=1):
        if a.only and n != a.only:
            continue
        print(f"== {a.prefix}-{n} {point}", flush=True)
        result = run_point(a.prefix, n, point, a)
        results["runs"] = [x for x in results["runs"] if x["run"] != result["run"]] + [result]
        out.write_text(json.dumps(results, indent=2) + "\n")
        print(f"   {'PASS' if result.get('pass') else 'FAIL'} exit={result.get('first_exit_code')} "
              f"recovery={result.get('recovery')} replayed={result.get('replayed')} expect={result.get('expect')}",
              flush=True)
    results["pass"] = all(x.get("pass") for x in results["runs"])
    out.write_text(json.dumps(results, indent=2) + "\n")
    print("matrix", "PASS" if results["pass"] else "FAIL", "->", out.relative_to(WORKSPACE))
    return 0 if results["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

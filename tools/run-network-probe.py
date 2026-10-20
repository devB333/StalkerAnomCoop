#!/usr/bin/env python3
"""S04-T01: run the GameNetworkingSockets probe scenarios and record the result.

Usage: python3 tools/run-network-probe.py <run> --build <build-run> [--only a,b] [--skip-selftest]

<build-run> is the Build-NetworkProbe.ps1 run whose gns-probe.exe is used.
Each scenario starts one listener and its peers as separate Windows
processes on 127.0.0.1. Logs go to builds/logs/S04-T01/run-<run>/.
"""

import argparse
import hashlib
import json
import re
import subprocess
import sys
import time
from pathlib import Path

WORKSPACE = Path(__file__).resolve().parent.parent
LOGS = WORKSPACE / "builds" / "logs" / "S04-T01"
BUILDS = WORKSPACE / "builds" / "network-probe" / "build"

# Impairments are applied in every process of a scenario, to the packets it
# sends (FakePacket*_Send), so each direction sees them once.
LOSSY = {"--loss": "10", "--reorder": "10", "--reorder-ms": "25", "--dup": "5", "--dup-ms": "20", "--lag": "20"}

SCENARIOS = [
    {
        "name": "basic",
        "purpose": "two peers connect, are welcomed and close cleanly; a third is refused while two are active",
        "fake": {},
        "server": ["--max-active", "2", "--sessions", "2", "--min-rejects", "1"],
        "clients": [
            {"name": "A", "scenario": "basic", "delay": 0.0, "args": []},
            {"name": "B", "scenario": "basic", "delay": 0.0, "args": []},
            {"name": "C", "scenario": "basic", "delay": 1.0, "args": ["--expect-reject"]},
        ],
    },
    {
        "name": "reliable",
        "purpose": "3000 reliable messages each way per peer (some 20 KB and 100 KB) under 10% loss, "
        "10% reorder, 5% duplication, 20 ms lag: complete, in order, intact",
        "fake": LOSSY,
        "server": ["--sessions", "2", "--rel-count", "3000", "--deadline-s", "180"],
        "clients": [
            {"name": "A", "scenario": "reliable", "delay": 0.0, "args": ["--rel-count", "3000", "--deadline-s", "170"]},
            {"name": "B", "scenario": "reliable", "delay": 0.0, "args": ["--rel-count", "3000", "--deadline-s", "170"]},
        ],
    },
    {
        "name": "unreliable",
        "purpose": "500 unreliable 200-byte messages/s for 6 s per peer under the same impairments: "
        "delivery tracks the loss rate, no duplicates reach the application, reordered ones are discarded as stale",
        "fake": LOSSY,
        "server": ["--sessions", "2"],
        "clients": [
            {"name": "A", "scenario": "unreliable", "delay": 0.0, "args": ["--snap-rate", "500", "--snap-ms", "6000"]},
            {"name": "B", "scenario": "unreliable", "delay": 0.0, "args": ["--snap-rate", "500", "--snap-ms", "6000"]},
        ],
    },
    {
        "name": "priority",
        "purpose": "critical reliable pings (50/s, lane 0) beside 1000-byte snapshots offered at 400/s "
        "on a 128 KB/s link (lane 1, about 3x capacity): with the discard policy critical latency stays near "
        "baseline (median; 1% loss); without it (and on one lane) the contrast is recorded",
        "fake": {"--loss": "1", "--lag": "15"},
        "server": ["--sessions", "2", "--deadline-s", "150"],
        "clients": [
            {"name": "A", "scenario": "priority", "delay": 0.0,
             "args": ["--send-rate", "131072", "--snap-rate", "400", "--snap-bytes", "1000", "--snap-ms", "6000",
                      "--deadline-s", "140"]},
            {"name": "B", "scenario": "priority", "delay": 0.0,
             "args": ["--send-rate", "131072", "--snap-rate", "400", "--snap-bytes", "1000", "--snap-ms", "6000",
                      "--deadline-s", "140"]},
        ],
    },
    {
        "name": "priority-clean",
        "purpose": "the priority scenario on a loss-free link (15 ms lag), so latency tails reflect the lanes and "
        "the discard policy alone: with the policy the critical p95/p99 stay within 25 ms of baseline",
        "fake": {"--lag": "15"},
        "server": ["--sessions", "2", "--deadline-s", "150"],
        "clients": [
            {"name": "A", "scenario": "priority", "delay": 0.0,
             "args": ["--send-rate", "131072", "--snap-rate", "400", "--snap-bytes", "1000", "--snap-ms", "6000",
                      "--deadline-s", "140"]},
            {"name": "B", "scenario": "priority", "delay": 0.0,
             "args": ["--send-rate", "131072", "--snap-rate", "400", "--snap-bytes", "1000", "--snap-ms", "6000",
                      "--deadline-s", "140"]},
        ],
    },
    {
        "name": "crash-idle",
        "purpose": "a peer's process is killed while the listener sends it nothing: records how long GNS takes "
        "to notice an idle dead peer (first keepalive after 10 s of silence, then the 5 s timeout)",
        "fake": {"--timeout-ms": "5000"},
        "server": ["--sessions", "1"],
        "clients": [
            {"name": "B", "scenario": "reconnect-crash", "delay": 0.0, "args": ["--crash-after-ms", "1000"],
             "expect_exit": 99},
        ],
    },
    {
        "name": "reconnect",
        "purpose": "A closes and reconnects from the same process, then holds a pinging session; B's process is "
        "killed without closing; a new B process retries while the dead session still holds a slot; the "
        "listener, sending 20 Hz heartbeats as a game server would, detects the dead session by timeout (5 s) "
        "and then admits the new one",
        "fake": {"--timeout-ms": "5000"},
        "server": ["--max-active", "2", "--sessions", "4", "--min-rejects", "1", "--heartbeat-ms", "50"],
        "clients": [
            {"name": "A", "scenario": "reconnect-clean", "delay": 0.0, "args": ["--hold-ms", "8000"]},
            {"name": "B", "scenario": "reconnect-crash", "delay": 0.0, "args": ["--crash-after-ms", "1000"],
             "expect_exit": 99},
            {"name": "B", "scenario": "reconnect-retry", "delay": 0.3, "after": 1, "args": ["--retry-limit", "40"],
             "label": "B2"},
        ],
    },
]


def winpath(path: Path) -> str:
    s = str(path)
    m = re.match(r"^/mnt/([a-z])/(.*)$", s)
    if not m:
        raise SystemExit(f"not a Windows-mounted path: {s}")
    return f"{m.group(1).upper()}:\\" + m.group(2).replace("/", "\\")


def sha256(path: Path) -> str:
    return hashlib.sha256(path.read_bytes()).hexdigest()


def wait_for_line(path: Path, needle: str, timeout: float) -> bool:
    end = time.time() + timeout
    while time.time() < end:
        if path.exists() and needle in path.read_text(errors="replace"):
            return True
        time.sleep(0.05)
    return False


def load_json(path: Path):
    try:
        return json.loads(path.read_text(errors="replace"))
    except (OSError, ValueError) as exc:
        return {"error": f"{type(exc).__name__}: {exc}"}


def run_scenario(exe: Path, out: Path, sc: dict, port: int) -> dict:
    sdir = out / sc["name"]
    sdir.mkdir()
    address = f"127.0.0.1:{port}"
    fake = [x for kv in sc["fake"].items() for x in kv]
    record = {"name": sc["name"], "purpose": sc["purpose"], "address": address, "fake": sc["fake"], "processes": []}

    def start(label, role, args):
        summary = sdir / f"{label}.summary.json"
        stdout = sdir / f"{label}.trace.jsonl"
        cmd = [str(exe), role, "--listen" if role == "server" else "--connect", address,
               "--summary", winpath(summary)] + args
        fh = open(stdout, "wb")
        proc = subprocess.Popen(cmd, stdout=fh, stderr=subprocess.STDOUT, cwd=str(exe.parent))
        return {"label": label, "role": role, "cmd": cmd, "proc": proc, "fh": fh, "summary": summary,
                "trace": stdout, "started": time.time()}

    t0 = time.time()
    server = start("server", "server", ["--scenario", sc["name"]] + sc["server"] + fake)
    procs = [server]
    if not wait_for_line(server["trace"], '"event":"listening"', 10):
        record["error"] = "listener did not start"
    clients = []
    for c in sc["clients"]:
        label = c.get("label", c["name"])
        if "after" in c:
            clients[c["after"]]["proc"].wait(timeout=60)
        time.sleep(c["delay"])
        args = ["--scenario", c["scenario"], "--name", c["name"]] + c["args"] + fake
        p = start(label, "client", args)
        p["expect_exit"] = c.get("expect_exit", 0)
        clients.append(p)
        procs.append(p)
    deadline = time.time() + 240
    for p in procs:
        try:
            p["proc"].wait(timeout=max(1.0, deadline - time.time()))
        except subprocess.TimeoutExpired:
            p["proc"].kill()
            p["timed_out"] = True
    for p in procs:
        p["fh"].close()
        expect = p.get("expect_exit", 0)
        entry = {
            "label": p["label"],
            "role": p["role"],
            "command": " ".join(p["cmd"][1:]),
            "exit_code": p["proc"].returncode,
            "expected_exit": expect,
            "timed_out": p.get("timed_out", False),
            "trace": str(p["trace"].relative_to(WORKSPACE)),
        }
        if p["summary"].exists():
            entry["summary"] = load_json(p["summary"])
        entry["pass"] = (p["proc"].returncode == expect and not entry["timed_out"]
                         and (expect != 0 or entry.get("summary", {}).get("pass") is True))
        record["processes"].append(entry)
    record["elapsed_s"] = round(time.time() - t0, 1)
    record["pass"] = all(e["pass"] for e in record["processes"]) and "error" not in record
    return record


def run_selftests(build: Path, out: Path) -> list:
    bindir = build / "gns" / "bin"
    results = []
    for name, args, timeout in (("test_crypto", [], 600), ("test_connection", ["suite-quick"], 900)):
        exe = bindir / f"{name}.exe"
        log = out / f"selftest-{name}.log"
        if not exe.exists():
            results.append({"name": name, "missing": True, "pass": False})
            continue
        t0 = time.time()
        with open(log, "wb") as fh:
            try:
                rc = subprocess.run([str(exe)] + args, stdout=fh, stderr=subprocess.STDOUT, cwd=str(bindir),
                                    timeout=timeout).returncode
            except subprocess.TimeoutExpired:
                rc = None
        results.append({"name": name, "args": args, "exit_code": rc, "elapsed_s": round(time.time() - t0, 1),
                        "log": str(log.relative_to(WORKSPACE)), "pass": rc == 0})
    return results


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("run")
    ap.add_argument("--build", required=True)
    ap.add_argument("--only", default="")
    ap.add_argument("--skip-selftest", action="store_true")
    a = ap.parse_args()
    if not re.fullmatch(r"[a-zA-Z0-9-]+", a.run):
        raise SystemExit("run names must match ^[a-zA-Z0-9-]+$")
    out = LOGS / f"run-{a.run}"
    if out.exists():
        raise SystemExit(f"run directory exists: {out}")
    build = BUILDS / a.build
    exe = build / "probe" / "gns-probe.exe"
    dll = build / "probe" / "GameNetworkingSockets.dll"
    if not exe.exists() or not dll.exists():
        raise SystemExit(f"probe not built: {exe}")
    out.mkdir(parents=True)
    only = {s for s in a.only.split(",") if s}
    result = {
        "run": a.run,
        "build": a.build,
        "started_utc": time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime()),
        "probe_sha256": sha256(exe),
        "dll_sha256": sha256(dll),
        "host": "single Windows host, loopback 127.0.0.1",
        "scenarios": [],
    }
    for i, sc in enumerate(SCENARIOS):
        if only and sc["name"] not in only:
            continue
        rec = run_scenario(exe, out, sc, 27100 + i)
        result["scenarios"].append(rec)
        print(f"{sc['name']:<11} {'PASS' if rec['pass'] else 'FAIL'}  {rec['elapsed_s']} s", flush=True)
        for e in rec["processes"]:
            checks = e.get("summary", {}).get("checks", [])
            failed = [c["name"] for c in checks if not c["pass"]]
            print(f"   {e['label']:<7} exit={e['exit_code']} expect={e['expected_exit']}"
                  f" {'ok' if e['pass'] else 'FAIL'} {('failed: ' + ', '.join(failed)) if failed else ''}", flush=True)
    if not a.skip_selftest and not only:
        result["selftests"] = run_selftests(build, out)
        for t in result["selftests"]:
            print(f"selftest {t['name']:<16} {'PASS' if t['pass'] else 'FAIL'} exit={t.get('exit_code')}"
                  f" {t.get('elapsed_s', '')} s", flush=True)
    result["finished_utc"] = time.strftime("%Y-%m-%dT%H:%M:%SZ", time.gmtime())
    result["pass"] = all(s["pass"] for s in result["scenarios"]) and all(
        t["pass"] for t in result.get("selftests", []))
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("overall", "PASS" if result["pass"] else "FAIL", "->", (out / "result.json").relative_to(WORKSPACE))
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

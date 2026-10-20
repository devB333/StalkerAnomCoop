#!/usr/bin/env python3
"""Run the project's protocol/transport tests built by Build-Protocol.ps1.

Usage: python3 tools/run-protocol-tests.py <run> --build <build-run> [--task S04-T02]
                                           [--filter a,b] [--exe transport-tests]

Writes builds/logs/<task>/run-<run>/ (stdout.log, report.json, result.json).
Also checks that only protocol/src/net/gns_transport.cpp includes the
transport library's headers.
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


def winpath(path: Path) -> str:
    m = re.match(r"^/mnt/([a-z])/(.*)$", str(path))
    if not m:
        raise SystemExit(f"not a Windows-mounted path: {path}")
    return f"{m.group(1).upper()}:\\" + m.group(2).replace("/", "\\")


def library_include_check() -> dict:
    """Only the GNS backend may include the transport library."""
    offenders = []
    for root in ("protocol", "tests"):
        for f in sorted((WORKSPACE / root).rglob("*")):
            if f.suffix not in (".h", ".hpp", ".cpp"):
                continue
            rel = f.relative_to(WORKSPACE).as_posix()
            text = f.read_text(errors="replace")
            if re.search(r'#include\s*[<"]steam/', text) and rel != "protocol/src/net/gns_transport.cpp":
                offenders.append(rel)
    return {"name": "library_headers_only_in_gns_backend", "offenders": offenders, "pass": not offenders}


def main() -> int:
    ap = argparse.ArgumentParser()
    ap.add_argument("run")
    ap.add_argument("--build", required=True)
    ap.add_argument("--task", default="S04-T02")
    ap.add_argument("--filter", default="")
    ap.add_argument("--exe", default="transport-tests")
    a = ap.parse_args()
    if not re.fullmatch(r"[a-zA-Z0-9-]+", a.run):
        raise SystemExit("run names must match ^[a-zA-Z0-9-]+$")
    out = WORKSPACE / "builds" / "logs" / a.task / f"run-{a.run}"
    if out.exists():
        raise SystemExit(f"run directory exists: {out}")
    exe = WORKSPACE / "builds" / "protocol" / "build" / a.build / f"{a.exe}.exe"
    if not exe.exists():
        raise SystemExit(f"not built: {exe}")
    out.mkdir(parents=True)
    report = out / "report.json"
    cmd = [str(exe), "--json", winpath(report)]
    if a.filter:
        cmd += ["--filter", a.filter]
    t0 = time.time()
    with open(out / "stdout.log", "wb") as fh:
        proc = subprocess.Popen(cmd, stdout=subprocess.PIPE, stderr=subprocess.STDOUT, cwd=str(exe.parent))
        for line in proc.stdout:
            fh.write(line)
            sys.stdout.write(line.decode(errors="replace"))
            sys.stdout.flush()
        rc = proc.wait()
    include = library_include_check()
    result = {
        "run": a.run,
        "build": a.build,
        "task": a.task,
        "command": " ".join(cmd[1:]),
        "exe_sha256": hashlib.sha256(exe.read_bytes()).hexdigest(),
        "exit_code": rc,
        "elapsed_s": round(time.time() - t0, 1),
        "host": "single Windows host, loopback 127.0.0.1",
        "checks": [include],
    }
    if report.exists():
        r = json.loads(report.read_text())
        result["tests_run"] = r["run"]
        result["tests_passed"] = r["passed"]
        result["failed"] = [t["name"] for t in r["tests"] if not t["pass"]]
    result["pass"] = rc == 0 and include["pass"]
    (out / "result.json").write_text(json.dumps(result, indent=2) + "\n")
    print("include check", "PASS" if include["pass"] else f"FAIL {include['offenders']}")
    print("overall", "PASS" if result["pass"] else "FAIL", "->", (out / "result.json").relative_to(WORKSPACE))
    return 0 if result["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

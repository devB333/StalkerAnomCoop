#!/usr/bin/env python3
"""Report the S10-T03e passage sweep (plan.md S10-T03e; docs/validation/S10-T03e.md).
Usage: python3 tools/compare-coop-sweep.py <session logs> [--passages builds/logs/S10-T01/passages.tsv]
                                           [--out compare-sweep.json] [--tsv sweep-passages.tsv]
Inputs: server/coop-server.log (fixture-sweep lines), result.json, each client's xray_*.log.
The fixture (--sweep c1,c2, profile sweep_party; CoopServerHost.cpp sweep_fixtures) travels every passage it can
reach. Every passage of the server's graph (fixture-sweep edge) gets one result: ok (the party arrived and both
clients rejoined there), refused-<reason> (every changer of it refused, 24: a disabled changer), failed, timeout,
wrong-level, no-travel, or unreached. The S10-T01 table (passages.tsv) is compared with the graph.
Checks:
  graph      the sweep built its graph (the spawn data's level changers); the differences from passages.tsv (the
             S10-T01 string scan) are listed
  coverage   every passage of the graph has a result (ok, refused, failed...) or is listed unreached
  travels    no trip failed after an agreed request (failed, timeout, wrong-level, no-travel: none)
  clients    no FATAL in a client's engine log (exit codes reported; a long run's teardown crash is B5)
The pass/fail of the slice is the user's reading of the list: an unreached or refused passage is a finding.
"""
import argparse
import csv
import json
import re
import sys
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def kv(line):
    return dict(re.findall(r"(\w+)=([^\s\"]+|\"[^\"]*\")", line))


def grep(log, marker):
    return [kv(ln) for ln in log if marker in ln]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--passages", default="builds/logs/S10-T01/passages.tsv")
    ap.add_argument("--out", default="")
    ap.add_argument("--tsv", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    log = lines(root / "server" / "coop-server.log")
    edges = grep(log, " fixture-sweep edge ")
    graph = {}
    for e in edges:
        graph.setdefault(f"{e['from']}>{e['to']}", []).append(e["changer"])
    expected = set()
    if Path(a.passages).exists():
        with open(a.passages, newline="") as f:
            for row in csv.DictReader(f, delimiter="\t"):
                expected.add(f"{row['source']}>{row['destination']}")
    results = {}
    trips = []
    for p in grep(log, " fixture-sweep passage "):
        trips.append(p)
        if p.get("sweeping", "1") == "1" or p.get("result") != "ok":
            results[p["key"]] = p["result"]
    for u in grep(log, " fixture-sweep unreached "):
        results.setdefault(u["key"], "unreached")
    for k in graph:
        results.setdefault(k, "unreached")
    done = grep(log, " fixture-sweep done ")
    checks = {}
    missing = sorted(expected - set(graph))
    # the runtime graph is the spawn data's own; S10-T01's string scan (passages.tsv) is compared, not required
    checks["graph"] = {"pass": bool(graph), "passages": len(graph), "tsv_rows": len(expected),
                       "tsv_not_in_graph": missing, "graph_not_in_tsv": sorted(set(graph) - expected)}
    checks["coverage"] = {"pass": bool(done) and all(k in results for k in graph), "done": done[:1]}
    bad = {k: v for k, v in results.items() if v in ("failed", "timeout", "wrong-level", "no-travel")}
    checks["travels"] = {"pass": not bad, "bad": bad}
    fatal = [str(p.relative_to(root)) for p in root.glob("c*/xray_*.log")
             if "FATAL ERROR" in p.read_text(errors="replace")]
    exits = {}
    result = root / "result.json"
    if result.exists():
        exits = {k: v.get("exit_code") for k, v in json.loads(result.read_text(encoding="utf-8-sig"))
                 .get("clients", {}).items()}
    checks["clients"] = {"pass": not fatal, "fatal": fatal, "exits": exits}
    summary = {}
    for v in results.values():
        key = v if not v.startswith("refused") else "refused"
        summary[key] = summary.get(key, 0) + 1
    report = {"checks": checks, "summary": summary, "trips": len(trips), "results": dict(sorted(results.items())),
              "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    if a.tsv:
        with open(a.tsv, "w", newline="") as f:
            w = csv.writer(f, delimiter="\t")
            w.writerow(["passage", "result", "changers"])
            for k in sorted(results):
                w.writerow([k, results[k], ", ".join(graph.get(k, []))])
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print(f"passages {len(graph)}, trips {len(trips)}: {summary}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

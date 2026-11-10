#!/usr/bin/env python3
"""Check the S10-T03d relocation fixture (plan.md S10-T03d; docs/validation/S10-T03d.md).
Usage: python3 tools/compare-coop-relocate.py <session logs> --variant 0|1 [--out compare-relocate.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log and xray-campaign-server_*.log; c1/ and c2/:
coop-c<n>.log and xray_*.log; result.json.
Variant 0 (--relocate c1,c2): a guide's dialogue action in p2's context (fee 1000, a 2 h 30 min skip, ChangeLevel
to a Darkscape smart terrain), refused by p1, then agreed. Variant 1 (--relocate c1,c2 --relocate-variant 1
--relocate-test c2:N): client 2 does what a same-level fast travel does; p1 agrees.
Checks:
  wrap       the server's compat layer wrapped ChangeLevel and the guide's fee (relocate wrap: changelevel+fee)
  request    v0: the guide's action became a relocation proposal (fee 1000, 150 min, source 4) with the fee paid;
             v1: client 2 noted the 60 min skip and sent the request (source 1), its actor did not move by itself,
             the server proposed it on the same level
  refusal    v0 only: p1's refusal refunded the fee and no time passed
  arrival    the party was carried to the point (travel-reloaded ok=1), both actors within 5 m of it, the time
             skipped once by the trip (v0 2.5 h, v1 1 h), v0: the fee paid once; fixture-relocate done failures=0
  clients    both clients rejoined on the destination level; clients exit 0, no FATAL
"""
import argparse
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
    ap.add_argument("--variant", type=int, choices=(0, 1), required=True)
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    log = lines(root / "server" / "coop-server.log")
    xlog = "".join(p.read_text(errors="replace") for p in (root / "server").glob("xray-campaign-server_*.log"))
    c2 = lines(root / "c2" / "coop-c2.log")
    checks = {}
    wrap = re.findall(r"S10-T03d compatibility layer relocate wrap: (\S+)", xlog)
    checks["wrap"] = {"pass": bool(wrap) and wrap[-1] == "changelevel+fee", "wrap": wrap[-1:]}
    proposed = grep(log, " relocate-proposed ")
    if a.variant == 0:
        gp = grep(log, " fixture-relocate guide-proposed ")
        checks["request"] = {"pass": bool(gp) and gp[0].get("pass") == "1" and len(proposed) >= 1
                             and proposed[0].get("source") == "4" and proposed[0].get("fee") == "1000"
                             and proposed[0].get("minutes") == "150", "fixture": gp[:1], "proposed": proposed[:2]}
        gr = grep(log, " fixture-relocate guide-refused ")
        refund = grep(log, " relocate-refund ")
        checks["refusal"] = {"pass": bool(gr) and gr[0].get("pass") == "1" and bool(refund)
                             and refund[0].get("refunded") == "1", "fixture": gr[:1], "refund": refund[:1]}
    else:
        noted = grep(c2, " relocate-skip-noted ")
        sent = grep(c2, " relocate-request-sent ")
        fr = grep(log, " fixture-relocate request ")
        checks["request"] = {"pass": bool(noted) and noted[0].get("minutes") == "60" and bool(sent)
                             and sent[0].get("source") == "1" and sent[0].get("minutes") == "60" and bool(fr)
                             and fr[0].get("pass") == "1" and bool(proposed)
                             and proposed[0].get("same_level") == "1", "noted": noted[:1], "sent": sent[:1],
                             "fixture": fr[:1], "proposed": proposed[:1]}
    agreed = grep(log, " relocate-agreed ")
    reloaded = grep(log, " travel-reloaded ")
    arrived = grep(log, " fixture-relocate arrived ")
    done = grep(log, " fixture-relocate done ")
    checks["arrival"] = {"pass": len(agreed) == 1 and bool(reloaded) and reloaded[-1].get("ok") == "1" and bool(arrived)
                         and arrived[0].get("pass") == "1" and bool(done) and done[0].get("failures") == "0",
                         "agreed": agreed, "reloaded": reloaded[-1:], "arrived": arrived[:1], "done": done[:1]}
    level = arrived[0].get("level") if arrived else None
    clients = {}
    for c in ("c1", "c2"):
        cl = lines(root / c / f"coop-{c}.log")
        committed = [e for e in grep(cl, " travel-event ") if e.get("state") == "8"]
        clients[c] = {"committed_to": [e.get("level") for e in committed]}
    exits = {}
    result = root / "result.json"
    if result.exists():
        exits = {k: v.get("exit_code") for k, v in json.loads(result.read_text(encoding="utf-8-sig"))
                 .get("clients", {}).items()}
    fatal = [str(p.relative_to(root)) for p in root.glob("c*/xray_*.log")
             if "FATAL ERROR" in p.read_text(errors="replace")]
    checks["clients"] = {"pass": all(level in v["committed_to"] for v in clients.values()) and bool(exits)
                         and all(v == 0 for v in exits.values()) and not fatal,
                         "clients": clients, "exits": exits, "fatal": fatal}
    report = {"variant": a.variant, "checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

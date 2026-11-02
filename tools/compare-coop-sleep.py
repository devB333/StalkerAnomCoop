#!/usr/bin/env python3
"""Check the S10-T03c sleep fixture (plan.md S10-T03c; docs/validation/S10-T03c.md).
Usage: python3 tools/compare-coop-sleep.py <session logs> [--out compare-sleep.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log and xray-campaign-server_*.log; c1/ and c2/:
coop-c<n>.log and xray_*.log; result.json.
The fixture (--sleep c1,c2, profile sleep_party; CoopServerHost.cpp sleep_fixtures) drives the intents the clients'
sleep window and keys send, on the server.
Checks:
  wrap      the server's compat layer wrapped ui_sleep_dialog.sleep_forced (S10-T03c ... sleep wrap: forced)
  shelter   G10 (T03c-2): out of shelter p2's request is refused 47 and p1's accept of p2's sheltered request is
            refused 47 (the proposal stays and p2 cancels it); both players then stand in the nearest sleep zone
            (shelter-round pass=1)
  refusals  a stale answer is refused 28, the requester's own accept 29, a travel request while the sleep proposal is
            up 23; the partner's refusal ends it (refused-round pass=1) and no time passes (refused-time pass=1)
  agreed    the second request agreed: the world's time skipped 3 h once, both players' sleep need dropped
            (agreed-time pass=1), sleep-slept with requester=1 partner=1 published=1
  blackout  p2 passed out: no time skip, p2's input held, p1's not (blackout pass=1, sleep-blackout slot=1); when it
            woke p2's need dropped and p1's did not (woke pass=1); fixture-sleep done failures=0
  clients   each client got the proposal (state 1) twice, the refusal (3) and the sleep (2) with 3 hours, and stepped
            its clock by about 3 h (clock-step apart_ms >= 10,000,000); only client 2 got the blackout (11); no
            presentation error; clients exit 0, no FATAL
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
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    log = lines(root / "server" / "coop-server.log")
    xlog = "".join(p.read_text(errors="replace") for p in (root / "server").glob("xray-campaign-server_*.log"))
    checks = {}
    wrap = re.findall(r"S10-T03c compatibility layer sleep wrap: (\S+)", xlog)
    checks["wrap"] = {"pass": bool(wrap) and wrap[-1] == "forced", "wrap": wrap[-1:]}
    sr = grep(log, " fixture-sleep shelter-round ")
    checks["shelter"] = {"pass": bool(sr) and sr[0].get("pass") == "1", "round": sr[:1]}
    rr = grep(log, " fixture-sleep refused-round ")
    rt = grep(log, " fixture-sleep refused-time ")
    checks["refusals"] = {"pass": bool(rr) and rr[0].get("pass") == "1" and rr[0].get("stale") == "28"
                          and rr[0].get("own_accept") == "29" and rr[0].get("travel") == "23" and bool(rt)
                          and rt[0].get("pass") == "1", "round": rr[:1], "time": rt[:1]}
    ar = grep(log, " fixture-sleep agreed-round ")
    at = grep(log, " fixture-sleep agreed-time ")
    slept = grep(log, " sleep-slept ")
    checks["agreed"] = {"pass": bool(ar) and ar[0].get("pass") == "1" and bool(at) and at[0].get("pass") == "1"
                        and len(slept) == 1 and slept[0].get("skipped") == "1" and slept[0].get("requester") == "1"
                        and slept[0].get("partner") == "1" and slept[0].get("published") == "1",
                        "round": ar[:1], "time": at[:1], "slept": slept}
    bo = grep(log, " fixture-sleep blackout ")
    woke = grep(log, " fixture-sleep woke ")
    sb = grep(log, " sleep-blackout ")
    done = grep(log, " fixture-sleep done ")
    checks["blackout"] = {"pass": bool(bo) and bo[0].get("pass") == "1" and len(sb) == 1 and sb[0].get("slot") == "1"
                          and bool(woke) and woke[0].get("pass") == "1" and bool(done)
                          and done[0].get("failures") == "0",
                          "blackout": bo[:1], "server": sb, "woke": woke[:1], "done": done[:1]}
    clients = {}
    ok = True
    for c in ("c1", "c2"):
        cl = lines(root / c / f"coop-{c}.log")
        states = [e.get("state") for e in grep(cl, " sleep-event ")]
        steps = [int(s.get("apart_ms", "0")) for s in grep(cl, " clock-step ")]
        errors = [ln for ln in cl if "sleep-present-error" in ln]
        want_blackout = c == "c2"
        good = (states.count("1") >= 2 and "3" in states and "2" in states and ("11" in states) == want_blackout
                and any(s >= 10_000_000 for s in steps) and not errors)
        ok = ok and good
        clients[c] = {"pass": good, "states": states, "big_clock_steps": [s for s in steps if s >= 1_000_000],
                      "errors": errors[:3]}
    exits = {}
    result = root / "result.json"
    if result.exists():
        exits = {k: v.get("exit_code") for k, v in json.loads(result.read_text(encoding="utf-8-sig"))
                 .get("clients", {}).items()}
    fatal = [str(p.relative_to(root)) for p in root.glob("c*/xray_*.log")
             if "FATAL ERROR" in p.read_text(errors="replace")]
    checks["clients"] = {"pass": ok and bool(exits) and all(v == 0 for v in exits.values()) and not fatal,
                         "clients": clients, "exits": exits, "fatal": fatal}
    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

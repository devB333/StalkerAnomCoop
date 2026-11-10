#!/usr/bin/env python3
"""Measure the presentation records a session's clients got (plan.md S10-T05d; docs/validation/S10-T05d.md).
Usage: python3 tools/compare-coop-presentation.py <session logs> [--baseline <session logs>] [--out compare.json]
Inputs (builds/logs/<task>/run-<run>/): c1/ and c2/ coop-c<n>.log ("present … <status>" lines: every record a client
applied or could not, except the applied ones of the chatty kinds), server/coop-server.log (loops-joined,
presentation-stop) and server/xray-campaign-server_*.log (the script-context stop: presented, broadcast, dropped).
Checks (S10-T05d1):
  orphans   a client got almost no record for a sound or effector it does not have: "absent" records are under
            1 % of the run's records (before: a volume for every loop every update, a remove for effects never on)
  loops     every client entering the world got the looped sounds already playing: at least as many looped plays
            (flags bit 1) applied within 10 s of each join as the join replayed, and at least one (since S10-T07 the
            ambient loops are each player's own, started in its context, and the replay may have none)
  passloops when the run has a surge, each player's pass played its own looped sounds (emission surge-pass-loop
            lines for every slot with a pass) and that player's client applied looped plays during the surge
  dropped   the server's outboxes evicted fewer records than the baseline (when one is given)
  statics   (S10-T05d2, when the run has a surge) the exposed player's client (c2) showed the emission's psi
            indicator (hud-static-add atm_indik_psi_*) and the covered player's (c1) did not; on every client log
            each indicator added was removed by the end; no hud-static record was absent
The baseline's counts are printed beside the run's.
"""
import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def looped_plays(path):
    """(utc ms, kind) of every looped sound play a client applied."""
    out = []
    for ln in lines(path):
        if " present " not in ln or not ln.endswith(" applied"):
            continue
        m = re.match(r"(\d+) .*kind=(sound|sound-at-pos) .*flags=(\d+)", ln)
        if m and int(m.group(3)) & 1:
            out.append(int(m.group(1)))
    return out


def counts(root):
    out = {}
    for c in ("c1", "c2"):
        k = Counter()
        for ln in lines(root / c / f"coop-{c}.log"):
            if " present " not in ln:
                continue
            m = re.search(r"kind=(\S+)", ln)
            k[(m.group(1) if m else "?", ln.rsplit(" ", 1)[-1])] += 1
        out[c] = k
    xlog = "".join(p.read_text(errors="replace") for d in ("server", "server-first-run")
                   for p in (root / d).glob("xray-campaign-server_*.log"))
    stop = re.findall(r"presented=(\d+) present_no_context=\d+ present_broadcast=(\d+) dropped=(\d+)", xlog)
    server = {"presented": sum(int(s[0]) for s in stop), "broadcast": sum(int(s[1]) for s in stop),
              "dropped": sum(int(s[2]) for s in stop)}
    return out, server


def summary(k):
    total = sum(k.values())
    absent = sum(v for (kind, st), v in k.items() if st == "absent")
    top = {f"{kind}/{st}": v for (kind, st), v in k.most_common(6)}
    return {"records": total, "absent": absent, "top": top}


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--baseline", default="")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    run, server = counts(root)
    base = counts(Path(a.baseline)) if a.baseline else None
    checks = {}
    sums = {c: summary(k) for c, k in run.items()}
    checks["orphans"] = {"pass": all(s["records"] and s["absent"] < 0.01 * s["records"] for s in sums.values()),
                         "run": sums, "baseline": {c: summary(k) for c, k in base[0].items()} if base else None}
    log = lines(root / "server" / "coop-server.log")
    joins = [dict(re.findall(r"(\w+)=(\S+)", ln)) for ln in log if " loops-joined " in ln]
    applied = {c: sum(v for (kind, st), v in run[c].items() if kind in ("sound", "sound-at-pos") and st == "applied")
               for c in run}
    plays = {str(i): looped_plays(root / c / f"coop-{c}.log") for i, c in enumerate(("c1", "c2"))}
    for ln, j in zip([ln for ln in log if " loops-joined " in ln], joins, strict=True):
        at = int(ln.split(" ", 1)[0])
        j["applied"] = str(sum(1 for t in plays.get(j.get("to"), []) if at <= t <= at + 10000))
    checks["loops"] = {"pass": bool(joins) and {j.get("to") for j in joins} >= {"0", "1"}
                       and all(int(j["applied"]) >= max(int(j["loops"]), 1) for j in joins),
                       "joins": joins, "sounds_applied": applied,
                       "stop": [ln.split(" ", 1)[1] for ln in log if " presentation-stop " in ln]}
    starts = [dict(re.findall(r"(\w+)=(\S+)", ln)) | {"at": ln.split(" ", 1)[0]}
              for ln in log if " surge-pass-start " in ln]
    pass_loops = Counter(dict(re.findall(r"(\w+)=(\S+)", ln)).get("slot") for ln in log if " surge-pass-loop " in ln)
    slots = sorted({st.get("slot") for st in starts})
    during = {sl: sum(1 for t in plays.get(sl, []) if starts and t >= min(int(st["at"]) for st in starts))
              for sl in slots}
    checks["passloops"] = {"pass": all(pass_loops.get(sl, 0) > 0 and during[sl] > 0 for sl in slots),
                           "slots": slots, "pass_loops": dict(pass_loops), "looped_applied_in_surge": during}
    if starts:
        hud = {}
        for name in ("c1", "c2", "c2-first-run"):
            log_c = root / name / f"coop-{name.split('-')[0]}.log"
            if not log_c.exists():
                continue
            shown, adds, absent = {}, 0, 0
            for ln in lines(log_c):
                m = re.search(r" present .*kind=(hud-static-add|hud-static-remove) .*detail=(\S+) .* (\w+)$", ln)
                if not m:
                    continue
                absent += m.group(3) == "absent"
                if m.group(2).startswith("atm_indik_psi_"):
                    adds += m.group(1) == "hud-static-add"
                    shown[m.group(2)] = m.group(1) == "hud-static-add"
            hud[name] = {"psi_adds": adds, "left_shown": sorted(k for k, v in shown.items() if v), "absent": absent}
        c2_adds = sum(v["psi_adds"] for k, v in hud.items() if k.startswith("c2"))
        checks["statics"] = {"pass": c2_adds > 0 and hud.get("c1", {}).get("psi_adds", 1) == 0
                             and all(not v["left_shown"] and not v["absent"] for v in hud.values()),
                             "clients": hud,
                             "joined": [dict(re.findall(r"(\w+)=(\S+)", ln)) for ln in log if " statics-joined " in ln]}
    checks["dropped"] = {"pass": base is None or server["dropped"] < base[1]["dropped"], "run": server,
                         "baseline": base[1] if base else None}
    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

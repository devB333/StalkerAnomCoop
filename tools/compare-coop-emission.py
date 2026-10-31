#!/usr/bin/env python3
"""Check the S10-T05a emission fixture (plan.md S10-T05a; docs/validation/S10-T05a.md).
Usage: python3 tools/compare-coop-emission.py <session logs> --variant 0|1 [--out compare-emission.json]
Inputs (builds/logs/<task>/run-<run>/): server/coop-server.log ("emission ..." lines from the compat layer,
fixture-emission lines), server/xray-campaign-server_*.log, server/coop-deaths.csv; c1/ and c2/: coop-c<n>.log
and xray_*.log; result.json.
Variant 0 (--emission c1,c2): player 1 placed in the nearest emission cover, player 2 outside, a surge.
Variant 1 (--emission c1,c2 --emission-variant 1): player 1 placed 350+ m from player 2, a psi-storm.
Checks:
  wrap        the server's compat layer wrapped the surge and the psi-storm (emission wrap: surge+psi)
  placed      v0: player 1 in cover, player 2 not; v1: the players 350+ m apart
  v0 passes   each player's pass of the surge ran (surge-pass-start slot 0 and 1), no pass error, each pass saw
              its own player's cover (slot 0 in cover, slot 1 out of it)
  v0 outcome  player 1 lived and lost no health (under 0.01); player 2 lost health to the psy hits and died
              to the surge (its killer is itself: the installed hits and kill name the actor as their source)
  v0 shown    each client got the surge's camera effectors and its own waves (particle handles of one
              client's waves are not the other's: each player's pass made its own); after a late join
              (client 2 restarted during the surge) its pass starts again and the client gets the phase's
              earthquake and post-process from then on
  v1 vortices vortices came down around each player; every vortex hit was on a player within 200 m of it
              (the installed max_distance_psi_hit) and each player was hit at least once
  clients     clients exit 0, no FATAL
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
    return {k: v.strip('"') for k, v in re.findall(r"(\w+)=(\"[^\"]*\"|[^\s\"]+)", line)}


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
    checks = {}
    wrap = re.findall(r"S10-T05a compatibility layer emission wrap: (\S+)", xlog)
    checks["wrap"] = {"pass": bool(wrap) and wrap[-1] == "surge+psi", "wrap": wrap[-1:]}
    placed = grep(log, " fixture-emission placed ")
    checks["placed"] = {"pass": bool(placed) and placed[0].get("pass") == "1", "placed": placed[:1]}
    done = grep(log, " fixture-emission done ")
    if a.variant == 0:
        starts = grep(log, " emission surge-pass-start ")
        passes = grep(log, " emission surge-pass ")
        errors = [ln for ln in log if " emission surge-pass-error " in ln]
        by_slot = {s: [p for p in passes if p.get("slot") == s and p.get("ended") == "0"] for s in ("0", "1")}
        covered0 = [p.get("cover") for p in by_slot["0"]]
        alive1 = [p for p in by_slot["1"] if p.get("alive") == "1"]
        checks["passes"] = {"pass": {s.get("slot") for s in starts} >= {"0", "1"} and not errors
                            and bool(covered0) and all(c not in ("false", "nil") for c in covered0)
                            and bool(alive1) and all(p.get("cover") in ("false", "nil") for p in alive1),
                            "starts": starts[:4], "errors": errors[:3], "slot0_covers": sorted(set(covered0)),
                            "slot1_covers": sorted({p.get("cover") for p in alive1})}
        # a hit lands a frame after the pass that made it: the health samples every 5 s show it
        status = grep(log, " fixture-emission status ")
        h1 = [float(r["p1_health"]) for r in status if r.get("p1_alive") == "1"]
        h2 = [float(r["p2_health"]) for r in status if r.get("p2_alive") == "1"]
        hits = {"0": round(max(0.0, h1[0] - min(h1)), 3) if h1 else -1,
                "1": round(max(0.0, h2[0] - min(h2)), 3) if h2 else -1}
        deaths = []
        dpath = root / "server" / "coop-deaths.csv"
        if dpath.exists():
            with open(dpath, newline="") as f:
                deaths = [r for r in csv.DictReader(f) if r.get("slot") in ("0", "1")]
        p2_deaths = [r for r in deaths if r["slot"] == "1"]
        p1_deaths = [r for r in deaths if r["slot"] == "0"]
        self_kill = bool(p2_deaths) and p2_deaths[0].get("killer") == p2_deaths[0].get("entity")
        checks["outcome"] = {"pass": bool(done) and done[0].get("p1_alive") == "1" and done[0].get("p2_alive") == "0"
                             and done[0].get("finished") == "1" and not p1_deaths and self_kill
                             and 0 <= hits["0"] < 0.01 and hits["1"] > 0.05,
                             "done": done[:1], "hits": hits, "p2_deaths": p2_deaths[:1], "p1_deaths": p1_deaths[:1]}
        shown = {}
        for c in ("c1", "c2"):
            cl = lines(root / c / f"coop-{c}.log")
            pres = grep(cl, " present ")
            quakes = [p for p in pres if p.get("kind") == "camera-effector" and "earthquake" in p.get("detail", "")]
            waves = {p.get("handle") for p in pres
                     if p.get("kind") == "particles" and "blowout_wave" in p.get("detail", "")}
            shown[c] = {"quakes": len(quakes), "wave_handles": sorted(waves)}
        disjoint = not (set(shown["c1"]["wave_handles"]) & set(shown["c2"]["wave_handles"]))
        # a late join (Run-CoopSession -Restart c2:...): player 2's pass starts again when its client is back,
        # and the client gets the phase's effects from then on
        restarts = [ln for ln in log if " emission surge-pass-start slot=1 " in ln][1:]
        if restarts:
            at = int(restarts[0].split()[0])
            late = [p for ln in lines(root / "c2" / "coop-c2.log") if " present " in ln and int(ln.split()[0]) >= at
                    for p in [kv(ln)]]
            shown["late"] = {"rejoin": kv(restarts[0]),
                             "quakes": sum(1 for p in late if p.get("kind") == "camera-effector"
                                           and "earthquake" in p.get("detail", "")),
                             "post_process": sum(1 for p in late if p.get("kind") == "post-process"),
                             "waves": sum(1 for p in late if p.get("kind") == "particles"
                                          and "blowout_wave" in p.get("detail", ""))}
        late_ok = "late" not in shown or (shown["late"]["quakes"] > 0 and shown["late"]["post_process"] > 0)
        checks["shown"] = {"pass": all(shown[c]["quakes"] > 0 and shown[c]["wave_handles"] for c in ("c1", "c2"))
                           and disjoint and late_ok,
                           "clients": shown, "disjoint": disjoint}
    else:
        vortices = grep(log, " emission psi-vortex ")
        vhits = grep(log, " emission psi-vortex-hit ")
        around = {v.get("around") for v in vortices}
        hit = [h for h in vhits if float(h.get("hit", "0")) > 0]
        far = [h for h in hit if float(h.get("distance", "0")) >= 200]
        slots_hit = {h.get("slot") for h in hit}
        checks["vortices"] = {"pass": around >= {"0", "1"} and not far and slots_hit >= {"0", "1"}
                              and bool(done) and done[0].get("finished") == "1",
                              "vortices": len(vortices), "around": sorted(around), "hits": len(hit),
                              "hits_far": far[:3], "slots_hit": sorted(slots_hit), "done": done[:1]}
    exits = {}
    result = root / "result.json"
    if result.exists():
        exits = {k: v.get("exit_code") for k, v in json.loads(result.read_text(encoding="utf-8-sig"))
                 .get("clients", {}).items()}
    fatal = [str(p.relative_to(root)) for p in root.glob("c*/xray_*.log")
             if "FATAL ERROR" in p.read_text(errors="replace")]
    checks["clients"] = {"pass": bool(exits) and all(v == 0 for v in exits.values()) and not fatal,
                         "exits": exits, "fatal": fatal}
    report = {"variant": a.variant, "checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

#!/usr/bin/env python3
"""Check the S10-T03a ALife identity fixture (plan.md S10-T03a; docs/validation/S10-T03a.md).
Usage: python3 tools/compare-coop-alife.py <session logs> [--variant restart|travel] [--out compare-alife.json]
Inputs (builds/logs/<task>/run-<run>/): server-first-run/ (the run before -ServerKill) and server/ (the restarted
run): coop-server.log, coop-lifecycle.csv and xray-campaign-server_*.log; each client's coop-world-final.csv and
xray_*.log; result.json.
The fixture (--alife c1,c2, profile alife_party; CoopServerHost.cpp alife_fixtures) runs with -ServerKill and a
client 2 -Restart (it leaves and rejoins before the kill): variant restart. Variant travel: with --travel c1,c2
--travel-variant 2 and no kill or client restart, the party travels after the subjects are marked; the restored and
absent checks are replaced by travelled (the same subject checks and a clean scan on the destination level).
Checks:
  wrap       both server runs wrapped the ALife functions (S10-T03a compatibility layer ALife wrap: respawn+squad)
  baseline   the first run's baseline identity scan is clean (no member in two squads, no member whose group is not
             its squad, no two alive creatures with one name, no two objects with one story id)
  guard      p1 moved 300+ m away; the respawn smart terrain beside p2 forced to respawn created no squad, held by
             the nearest-participant distance while the distance to actor 0 alone was beyond its radius
             (fixture-alife guard pass=1, in both runs)
  respawn    the forced respawn far from both players created one squad with members
  kill       the victim died; the scan after it is clean and no creature dead at the baseline is alive again
  marked     the subjects were written into the scripts' saved state and a checkpoint was asked for (published=1)
  absent     while client 2 was away its actor kept its area online (fixture-alife absent pass=1) and it came back
  restored   after the restart the victim is still dead under its name, the respawned squad exists once with its
             members, no other squad from its respawn point appeared, the scan is clean, failures=0
  clients    every client's final replica matches the server's online set at the commit it applied
             (compare-coop-world identity and presence; a client run that ended before the kill against the first
             server run; one whose applied commit is past the killed server's last flushed lifecycle row is
             reported unverifiable), the respawned squad's online members included; clients exit 0, no FATAL
The CPU cost with the players together and apart is reported (fixture-alife cost), not judged.
"""
import argparse
import csv
import json
import re
import subprocess
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
    ap.add_argument("--variant", choices=("restart", "travel"), default="restart")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    travel = a.variant == "travel"
    second_dir = root / "server"
    first_dir = second_dir if travel else root / "server-first-run"
    first = lines(first_dir / "coop-server.log")
    second = lines(second_dir / "coop-server.log")
    checks = {}
    wraps = [re.findall(r"S10-T03a compatibility layer ALife wrap: (\S+)",
                        "".join(p.read_text(errors="replace") for p in d.glob("xray-campaign-server_*.log")))
             for d in (first_dir, second_dir)]
    checks["wrap"] = {"pass": all(w and w[-1] == "respawn+squad" for w in wraps), "wraps": [w[-1:] for w in wraps]}
    base = grep(first, " fixture-alife scan phase=baseline ")
    checks["baseline"] = {"pass": bool(base) and base[0].get("clean") == "1", "scan": base[:1]}
    guards = grep(first, " fixture-alife guard ") + grep(second, " fixture-alife guard ")
    moved = grep(first, " fixture-alife moved ")
    if travel:
        guards = grep(first, " fixture-alife guard ")
    checks["guard"] = {"pass": len(guards) == (1 if travel else 2) and all(g.get("pass") == "1" for g in guards)
                       and bool(moved),
                       "guards": guards, "moved": moved[:1], "select": grep(first, " fixture-alife select ")[:1]}
    resp = grep(first, " fixture-alife respawn ")
    checks["respawn"] = {"pass": bool(resp) and resp[0].get("pass") == "1", "respawn": resp[:1]}
    after = grep(first, " fixture-alife scan phase=after ")
    check = grep(first, " fixture-alife check ")
    kill = grep(first, " fixture-alife kill ")
    checks["kill"] = {"pass": bool(kill) and bool(check) and check[0].get("pass") == "1" and bool(after)
                      and after[0].get("clean") == "1" and after[0].get("resurrected") == "0",
                      "kill": kill[:1], "check": check[:1], "scan": after[:1]}
    marked = grep(first, " fixture-alife marked ")
    checks["marked"] = {"pass": bool(marked) and marked[0].get("published") == "1" and marked[0].get("failures") == "0",
                        "marked": marked[:1]}
    if travel:
        moved_on = grep(first, " fixture-alife travelled ")
        scan_t = grep(first, " fixture-alife scan phase=travelled ")
        done_t = grep(first, " fixture-alife done phase=travelled ")
        checks["travelled"] = {"pass": bool(moved_on) and moved_on[0].get("pass") == "1" and moved_on[0].get("from")
                               != moved_on[0].get("to") and bool(scan_t) and scan_t[0].get("clean") == "1"
                               and scan_t[0].get("resurrected") == "0" and bool(done_t)
                               and done_t[0].get("failures") == "0",
                               "travelled": moved_on[:1], "scan": scan_t[:1], "done": done_t[:1]}
    absent = [] if travel else grep(first, " fixture-alife absent ")
    back = grep(first, " fixture-alife back ")
    if not travel:
        checks["absent"] = {"pass": bool(absent) and absent[0].get("pass") == "1" and bool(back), "absent": absent[:1],
                            "back": back[:1]}
    restored = grep(second, " fixture-alife restored ")
    r_scan = grep(second, " fixture-alife scan phase=restored ")
    r_done = grep(second, " fixture-alife done ")
    same = bool(restored) and bool(kill) and restored[0].get("victim") == kill[0].get("victim")
    if not travel:
        checks["restored"] = {"pass": same and restored[0].get("pass") == "1" and bool(r_scan)
                          and r_scan[0].get("clean") == "1" and bool(r_done) and r_done[0].get("failures") == "0",
                              "restored": restored[:1], "scan": r_scan[:1], "done": r_done[:1]}

    # every client's final replica against the restarted server's lifecycle
    worlds = {}
    member_ids = check[0].get("member_ids", "").split(",") if check else []
    for world in sorted(root.glob("c*/coop-world-final.csv")):
        # a client's run that ended before the server kill (c2-first-run) against the first server run
        server_dir = first_dir if world.parent.name.endswith("-first-run") else second_dir
        out = subprocess.run([sys.executable, str(Path(__file__).with_name("compare-coop-world.py")),
                              str(server_dir / "coop-lifecycle.csv"), str(world)], capture_output=True, text=True)
        try:
            res = json.loads(out.stdout)
        except ValueError:
            res = {"match": False, "error": out.stderr[-300:]}
        with open(world) as f:
            f.readline()
            rows = {r["id"]: r for r in csv.DictReader(f)}
        res["respawned_members_in_set"] = sum(1 for m in member_ids if m and rows.get(m, {}).get("in_set") == "1")
        logged = 0
        with open(server_dir / "coop-lifecycle.csv", newline="") as f:
            for row in csv.DictReader(f):
                if row.get("section") is not None:
                    logged = int(row["commit"])
        if res.get("client_applied_commit", 0) > logged:
            # the killed server's last lifecycle rows were never flushed: informational
            res = {"match": None, "unverifiable": f"client applied {res.get('client_applied_commit')}, "
                                                  f"server log ends at {logged}",
                   "respawned_members_in_set": res["respawned_members_in_set"]}
        worlds[str(world.relative_to(root))] = {k: res.get(k) for k in
                                                ("match", "client_set", "server_online_at_commit",
                                                 "respawned_members_in_set", "unverifiable", "error") if k in res}
    exits = {}
    result = root / "result.json"
    if result.exists():
        exits = {k: v.get("exit_code") for k, v in json.loads(result.read_text(encoding="utf-8-sig"))
                 .get("clients", {}).items()}
    fatal = [str(p.relative_to(root)) for p in root.glob("c*/xray_*.log")
             if "FATAL ERROR" in p.read_text(errors="replace")]
    verified = [w for w in worlds.values() if w.get("match") is not None]
    checks["clients"] = {"pass": len(verified) >= 2 and all(w.get("match") for w in verified) and bool(exits)
                         and all(v == 0 for v in exits.values()) and not fatal,
                         "worlds": worlds, "exits": exits, "fatal": fatal}
    report = {"checks": checks, "cost": grep(first, " fixture-alife cost "),
              "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    for c in report["cost"]:
        print(f"cost {c.get('phase')}: cpu {c.get('cpu_pct')} %, online creatures {c.get('online_creatures')}, "
              f"players {c.get('d_p1_p2')} m apart")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

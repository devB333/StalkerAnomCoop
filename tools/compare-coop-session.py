#!/usr/bin/env python3
"""Check a session-control run (plan.md S10-T06; docs/validation/S10-T06.md).
Usage: python3 tools/compare-coop-session.py <session logs> [--out compare.json]
Inputs (builds/logs/S10-T06/run-<run>/): server-campaign-1/ (the first campaign's server), server-campaign-2/ (the
new game's), server/ (the first campaign loaded back), c1/ and c2/ (coop-c<n>.log), result.json (the relaunches).
Checks:
  owner      the first key the fresh server accepted (client 1) owns the session
  partner    client 2's creation screen was kept as its loadout, and its load was refused (reason 43, not the owner)
  refused    the owner's create that breaks the creation rules was refused (reason 46) and nothing switched
  create     the owner's create made a new campaign (stalker, skadovsk_shipwreck) with the partner's loadout, told
             both clients and relaunched the server on it (exit code 75, the launcher's relaunch)
  newgame    the relaunched server started a new game on that campaign and gave player 2 its chosen loadout
  load       the owner loaded the first campaign back: a second relaunch, and the server recovered that campaign
  clients    both clients followed both switches (session-switch twice) and were in each world (world-live 3 times)
  exits      the last server and every client process exited 0
"""
import argparse
import json
import re
import sys
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def fields(line):
    return dict(re.findall(r"(\w+)=(\[[^\]]*\]|\"[^\"]*\"|\S+)", line))


def find(log, word):
    return [fields(ln) for ln in log if f" {word} " in ln or ln.endswith(f" {word}")]


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    s1 = lines(root / "server-campaign-1" / "coop-server.log")
    s2 = lines(root / "server-campaign-2" / "coop-server.log")
    s3 = lines(root / "server" / "coop-server.log")
    c1 = lines(root / "c1" / "coop-c1.log")
    c2 = lines(root / "c2" / "coop-c2.log")
    result = json.loads((root / "result.json").read_text()) if (root / "result.json").exists() else {}
    relaunches = result.get("relaunches", []) or []
    if isinstance(relaunches, dict):
        relaunches = [relaunches]
    checks = {}

    owner = find(s1, "session-owner")
    checks["owner"] = {"pass": len(owner) == 1 and owner[0].get("slot") == "0", "owner": owner}

    loadout = find(s1, "session-loadout")
    load_refused = find(s1, "session-load-refused")
    checks["partner"] = {"pass": any(e.get("slot") == "1" and e.get("written") == "1" for e in loadout)
                         and any(e.get("slot") == "1" and e.get("reason") == "43" for e in load_refused),
                         "loadout": loadout, "load_refused": load_refused}

    bad = find(s1, "session-create-refused")
    creates = find(s1, "session-create")
    switches1 = find(s1, "session-switch")
    checks["refused"] = {"pass": any(e.get("slot") == "0" for e in bad) and len(creates) == 1 and len(switches1) == 1,
                         "refused": bad}

    first = (find(s1, "session-campaign") or [{}])[0].get("id", "")
    create = creates[0] if creates else {}
    new_id = create.get("id", "")
    checks["create"] = {"pass": bool(new_id) and create.get("faction") == "stalker"
                        and create.get("map") == "skadovsk_shipwreck" and create.get("loadout2", "[]") != "[]"
                        and switches1 and switches1[0].get("why") == "create" and switches1[0].get("told") == "2"
                        and bool(find(s1, "session-relaunch")) and len(relaunches) >= 1
                        and relaunches[0].get("campaign") == new_id and relaunches[0].get("why") == "create",
                        "create": create, "switch": switches1, "relaunch": relaunches[:1]}

    camp2 = find(s2, "session-campaign")
    given = find(s2, "new-game-loadout")
    checks["newgame"] = {"pass": bool(camp2) and camp2[0].get("id") == new_id and bool(find(s2, "new-game-server"))
                         and any(e.get("slot") == "1" and e.get("list") == create.get("loadout2") for e in given),
                         "campaign": camp2, "loadout_given": given}

    switches2 = find(s2, "session-switch")
    camp3 = find(s3, "session-campaign")
    checks["load"] = {"pass": len(switches2) == 1 and switches2[0].get("why") == "load"
                      and switches2[0].get("to") == first
                      and len(relaunches) >= 2 and relaunches[1].get("campaign") == first
                      and bool(camp3) and camp3[0].get("id") == first and not find(s3, "new-game-server"),
                      "switch": switches2, "campaign": camp3, "first": first}

    def client(log):
        return {"switches": len(find(log, "session-switch")),
                "lives": sum(1 for ln in log if " world-live " in ln),
                "done": any("session-driver done" in ln for ln in log)}
    cl = {"c1": client(c1), "c2": client(c2)}
    checks["clients"] = {"pass": all(v["switches"] == 2 and v["lives"] >= 3 for v in cl.values()) and cl["c1"]["done"],
                         "clients": cl}

    exits = {k: v.get("exit_code") for k, v in (result.get("clients") or {}).items()}
    checks["exits"] = {"pass": result.get("server_exit_code") == 0 and bool(exits)
                       and all(v == 0 for v in exits.values()),
                       "server": result.get("server_exit_code"), "clients": exits}

    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

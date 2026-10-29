#!/usr/bin/env python3
"""Check the S10-T04b info-portion and actor-state fixture (plan.md S10-T04b; docs/validation/S10-T04b.md).

Usage: python3 tools/compare-coop-info.py <session logs> [--out compare-info.json]

Inputs (builds/logs/<task>/run-<run>/): server-first-run/ and server/: coop-server.log (the session is
run with an external server kill and restart: -ServerKill <s> -ServerRestartAfter 10; the same profile
runs the fixture's recovered phase); c1/, c2/: coop-c*.log.

The fixture (--info c1,c2 --auto-respawn 5, profile info_party; CoopServerHost.cpp info_fixtures):
  give       p1 is given a story portion, p2 a personal one and a pstor probe; every read is logged as an
             info-check line (expect / got) in each player's context
  dead       p1's actor is killed and respawned by its client
  respawned  p2 is given a second story portion; p1's new body must hold both; p1 saves a probe on the new
             body; a checkpoint is requested
  recovered  the restarted server reads everything again

Checks:
  reads       every info-check line has got == expect, in both server runs
  phases      the first run reached the respawned phase and requested its checkpoint; the restarted run
              ran the recovered phase (so the restart's generation holds the probes)
  mirror      the story portions were mirrored (info-mirror lines >= 2: STORY1 to p2, STORY2 to p1's new
              body) and the personal one was not (info-personal line, no mirror of it)
  states      every participant actor without the installed binder got an actor-state entry
              (actor-state-attached: p2's actor and p1's respawned body in the first run) and the restarted
              run restored a saved pstor into at least one of them (restored > 0)
  respawn     p1's respawned body received the first story portion (the respawn copy) and the second (the
              mirror): the respawned-phase reads say so
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    first = lines(root / "server-first-run" / "coop-server.log")
    second = lines(root / "server" / "coop-server.log")
    if not first:
        first, second = second, []
    checks = {}
    detail = {}

    def reads(log):
        out = []
        for ln in log:
            if " info-check " in ln:
                d = kv(ln)
                out.append({k: d.get(k) for k in ("phase", "slot", "actor", "via", "info", "expect", "got", "ok")})
        return out
    r1, r2 = reads(first), reads(second)
    detail["reads_first"] = r1
    detail["reads_restart"] = r2
    bad = [r for r in r1 + r2 if r["ok"] != "1"]
    checks["reads"] = {"pass": bool(r1) and not bad, "first": len(r1), "restart": len(r2), "failed": bad}

    phases_first = sorted({r["phase"] for r in r1})
    phases_second = sorted({r["phase"] for r in r2})
    published = any("fixture-info done phase=respawned" in ln and "publication=1" in ln for ln in first)
    checks["phases"] = {"pass": phases_first == ["give", "respawned"] and phases_second == ["recovered"] and published,
                        "first": phases_first, "restart": phases_second, "publication_requested": published}

    mirrors = [kv(ln) for ln in first + second if " info-mirror " in ln]
    personal = [kv(ln) for ln in first + second if " info-personal " in ln]
    mirrored_infos = {m["info"] for m in mirrors}
    personal_infos = {p["info"] for p in personal}
    checks["mirror"] = {"pass": len(mirrors) >= 2 and not (mirrored_infos & personal_infos) and bool(personal),
                        "mirrors": len(mirrors), "mirrored": sorted(mirrored_infos), "personal": sorted(personal_infos)}

    attached_first = [kv(ln) for ln in first if " actor-state-attached " in ln]
    attached_second = [kv(ln) for ln in second if " actor-state-attached " in ln]
    slots_first = sorted({int(x["slot"]) for x in attached_first})
    restored = sum(max(int(x["restored"]), 0) for x in attached_second)
    checks["states"] = {"pass": slots_first == [0, 1] and len(attached_first) >= 2 and restored > 0,
                        "first_slots": slots_first, "first_count": len(attached_first),
                        "restart_count": len(attached_second), "restart_restored": restored}

    resp = [r for r in r1 if r["phase"] == "respawned" and r["slot"] == "0" and r["via"] == "has_info"]
    story_ok = [r for r in resp if r["info"].startswith("zat_a9") and r["ok"] == "1"]
    respawned = any("p1-respawned=1" in ln for ln in first)
    checks["respawn"] = {"pass": respawned and len(story_ok) >= 2, "respawned": respawned,
                         "story_reads_ok": len(story_ok)}

    report = {"checks": checks, "detail": detail, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

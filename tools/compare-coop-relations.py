#!/usr/bin/env python3
"""Check the S10-T04d relations fixture (plan.md S10-T04d; docs/validation/S10-T04d.md).
Usage: python3 tools/compare-coop-relations.py <session logs> [--out compare-relations.json]
Inputs (builds/logs/<task>/run-<run>/): server-first-run/ and server/: coop-server.log (the session is run
with an external server kill and restart: -ServerKill <s> -ServerRestartAfter 10; the fixture publishes a
checkpoint after its changes, so the restart recovers them); c1/, c2/: coop-c*.log.
The fixture (--relations c1,c2, profile relations_party; CoopServerHost.cpp relations_fixtures):
  goodwill    in p2's context the installed Lua export changes the stalker goodwill of character 0 by -1000
  relation    the stalker/bandit relation set to 2000 (world)
  reputation  p1's reputation +100
  published   a checkpoint; recovered: the restarted server reads the three values back
Checks:
  goodwill    fixture-relations goodwill p1=a->a p2=b->(b-1000) (character 0 meant the context's player)
  relation    fixture-relations relation stalker/bandit=...->2000
  reputation  fixture-relations reputation p1=a->(a+100)
  c2          c2 relations-applied kind=1 with stalker=<p2 after> in nonzero (its own goodwill, not p1's)
  c1          c1 relations-applied kind=1 without the -1000 value for stalker; kind=2 stalker/bandit 2000;
              kind=3 reputation ...->(a+100) applied=1
  both        both clients applied the joined relation table (kind=2 pairs=... lines) and the world change
  recovered   fixture-relations recovered goodwill_p2=(b-1000) relation=2000 reputation_p1=(a+100)
  failures    both fixture-relations done lines have failures=0 and the first says published=1
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


def first_kv(log, marker):
    for ln in log:
        if marker in ln:
            return kv(ln)
    return {}


def arrow(value):
    m = re.match(r"(-?\d+)->(-?\d+)", value or "")
    return (int(m.group(1)), int(m.group(2))) if m else (None, None)


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
    c1 = lines(root / "c1" / "coop-c1.log")
    c2 = lines(root / "c2" / "coop-c2.log")
    checks = {}

    gw = first_kv(first, " fixture-relations goodwill ")
    p1a, p1b = arrow(gw.get("p1"))
    p2a, p2b = arrow(gw.get("p2"))
    checks["goodwill"] = {"pass": p1a is not None and p1a == p1b and p2a is not None and p2b == p2a - 1000, "line": gw}

    rel = first_kv(first, " fixture-relations relation ")
    ra, rb = arrow(rel.get("bandit")) if "bandit" in rel else (None, None)
    rel_after = arrow(rel.get("stalker/bandit"))[1] if "stalker/bandit" in rel else rb
    checks["relation"] = {"pass": rel_after == 2000, "line": rel}

    rep = first_kv(first, " fixture-relations reputation ")
    rep_a, rep_b = arrow(rep.get("p1"))
    checks["reputation"] = {"pass": rep_a is not None and rep_b == rep_a + 100, "line": rep}

    def goodwill_lines(log):
        return [kv(ln) for ln in log if " relations-applied kind=1 " in ln]

    def has_stalker(entry, value):
        return f"stalker={value}" in entry.get("nonzero", "")

    c2_gw = goodwill_lines(c2)
    checks["c2"] = {"pass": p2b is not None and any(has_stalker(e, p2b) for e in c2_gw), "lines": c2_gw[-3:]}

    c1_gw = goodwill_lines(c1)
    c1_rel = [kv(ln) for ln in c1 if " relations-applied kind=2 from=stalker to=bandit " in ln]
    c1_rep = [kv(ln) for ln in c1 if " relations-applied kind=3 " in ln]
    c1_rep_ok = rep_b is not None and any(arrow(e.get("reputation"))[1] == rep_b and e.get("applied") == "1"
                                          for e in c1_rep)
    c1_gw_ok = bool(c1_gw) and not any(p2b is not None and p2a != p2b and has_stalker(e, p2b) for e in c1_gw)
    checks["c1"] = {"pass": c1_gw_ok and any(e.get("value") == "2000" for e in c1_rel) and c1_rep_ok,
                    "goodwill": c1_gw[-2:], "relation": c1_rel[:2], "reputation": c1_rep[-2:]}

    joined = [any(" relations-applied kind=2 pairs=" in ln for ln in log) for log in (c1, c2)]
    world = [any(" relations-applied kind=2 from=stalker to=bandit value=2000" in ln for ln in log) for log in (c1, c2)]
    checks["both"] = {"pass": all(joined) and all(world), "joined": joined, "world_change": world}

    recovered = first_kv(second, " fixture-relations recovered ")
    checks["recovered"] = {"pass": recovered.get("goodwill_p2") == str(p2b) and recovered.get("relation") == "2000"
                           and recovered.get("reputation_p1") == str(rep_b), "line": recovered}

    done1 = first_kv(first, " fixture-relations done phase=changed ")
    done2 = first_kv(second, " fixture-relations done phase=recovered ")
    checks["failures"] = {"pass": done1.get("failures") == "0" and done1.get("published") == "1"
                          and done2.get("failures") == "0", "first": done1, "restart": done2}

    report = {"checks": checks, "pass": all(c["pass"] for c in checks.values())}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    for name, c in checks.items():
        print(f"{name}: {'pass' if c['pass'] else 'FAIL'}")
    print("PASS" if report["pass"] else "FAIL")
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

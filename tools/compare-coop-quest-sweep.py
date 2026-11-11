#!/usr/bin/env python3
"""Check the B57 quest sweep fixture (docs/validation/S10-T07.md; CoopServerHost.cpp quest_sweep_fixtures).

Usage: python3 tools/compare-coop-quest-sweep.py <session logs> --list builds/quest-sweep/<name>.json
                                                 [--out compare-quest-sweep.json] [--md table.md]

Inputs (builds/logs/<task>/run-<run>/server/): coop-server.log, the server's xray log; the list's .json
(tools/make-quest-sweep-list.py) for each task's expected rewards.

Per task (one fixture-qsweep result line each) the fixture's own verdict, then these checks on a pass:
  money     a task whose on_complete pays money: both players' money rose (a fetch task: each player
            who handed in items, G08; the sweep splits the items between them)
  items     a task whose on_complete gives an item: quest items were given (items_given >= 2: one per player)
Whole run:
  finished  the sweep logged fixture-qsweep done (the server did not stop or hang inside it)
  fatal     no FATAL / stack trace in the server's xray log
  refnil    no lua-refnil-cleared line (B57: luabind's shared nil slot stayed nil)
A task is "pass", "fail" or "skip" (giver not on this level, task already active); the summary counts them
and groups failures by their reason.
"""

import argparse
import json
import re
import sys
from collections import Counter
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def kv(line):
    return dict(re.findall(r"(\w+)=(\S+)", line))


def money_pair(value):
    out = []
    for part in value.split(","):
        a, _, b = part.partition("->")
        out.append((int(a), int(b)) if a.isdigit() and b.isdigit() else (0, 0))
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--list", required=True, help="the list's .json from make-quest-sweep-list.py")
    ap.add_argument("--out", default="")
    ap.add_argument("--md", default="")
    a = ap.parse_args()
    root = Path(a.logs)
    server = lines(root / "server" / "coop-server.log")
    expected = {t["id"]: t for t in json.loads(Path(a.list).read_text())["tasks"]}
    xray = []
    for p in sorted((root / "server").glob("xray*.log")):
        xray += lines(p)

    results = []
    for ln in server:
        if " fixture-qsweep result " not in ln:
            continue
        r = kv(ln)
        task = r.get("task", "?")
        exp = expected.get(task, {})
        verdict = r.get("verdict", "fail")
        why = r.get("why", "-")
        notes = []
        if verdict == "pass":
            m = money_pair(r.get("money", "0->0,0->0"))
            rose = sum(1 for before, after in m if after > before)
            goal = r.get("goal", "")
            if r.get("kind") == "fetch" and goal.startswith("items:"):
                # G08 (B60): each player is paid for the items he handed in; the sweep splits them
                made = int(goal.rsplit(":", 1)[1].split("/")[0] or 0)
                if exp.get("money") and rose < min(made, 2):
                    notes.append("money-not-per-holder")
            elif r.get("kind") == "fetch":
                pass  # a fetch task whose goal was forced hands in no items: it pays nothing (G08)
            elif exp.get("money") and rose < 2:
                notes.append("money-not-both")
            if exp.get("items") and int(r.get("items_given", 0)) < 2:
                notes.append("items-not-both")
            if notes:
                verdict, why = "fail", "+".join(notes)
        results.append({"n": int(r.get("n", -1)), "task": task, "kind": r.get("kind"), "giver": r.get("giver"),
                        "accept": r.get("accept"), "goal": r.get("goal"), "turnin": r.get("turnin"),
                        "native": r.get("native"), "money": r.get("money"), "items_taken": r.get("items_taken"),
                        "items_given": r.get("items_given"), "verdict": verdict, "why": why,
                        "fixture_verdict": r.get("verdict"), "fixture_why": r.get("why"),
                        "expected": {k: exp.get(k) for k in ("money", "items", "goodwill", "stash", "status_functor")}})

    done = [kv(ln) for ln in server if " fixture-qsweep done " in ln]
    loaded = [kv(ln) for ln in server if " fixture-qsweep loaded " in ln]
    fatal = [ln for ln in xray if "FATAL" in ln or "stack trace" in ln]
    refnil = [ln for ln in server if " lua-refnil-cleared " in ln]
    counts = Counter(r["verdict"] for r in results)
    reasons = Counter((r["verdict"], r["why"]) for r in results if r["verdict"] != "pass")
    run_checks = {
        "finished": {"pass": bool(done), "done": done[-1] if done else None},
        "fatal": {"pass": not fatal, "lines": fatal[:5]},
        "refnil": {"pass": not refnil, "lines": refnil[:5]},
        "ran_all": {"pass": bool(loaded) and len(results) == int(loaded[-1].get("tasks", -1)),
                    "loaded": loaded[-1] if loaded else None, "results": len(results)},
    }
    summary = {"tasks": len(results), "pass": counts.get("pass", 0), "fail": counts.get("fail", 0),
               "skip": counts.get("skip", 0),
               "reasons": {f"{v}:{w}": n for (v, w), n in reasons.most_common()},
               "run_pass": all(c["pass"] for c in run_checks.values())}
    report = {"summary": summary, "run_checks": run_checks, "results": results}
    if a.out:
        Path(a.out).write_text(json.dumps(report, indent=1) + "\n")
    if a.md:
        rows = ["| # | Task | Kind | Accept | Goal | Turn-in | Money | Verdict | Why |",
                "|---|---|---|---|---|---|---|---|---|"]
        rows += [f"| {r['n']} | `{r['task']}` | {r['kind']} | {r['accept']} | {r['goal']} | {r['turnin']} | "
                 f"{r['money']} | {r['verdict']} | {r['why']} |" for r in results]
        Path(a.md).write_text("\n".join(rows) + "\n")
    print(json.dumps({"summary": summary, "run_checks": {k: v["pass"] for k, v in run_checks.items()}}, indent=1))
    return 0 if summary["run_pass"] and summary["fail"] == 0 else 1


if __name__ == "__main__":
    sys.exit(main())

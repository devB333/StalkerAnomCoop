#!/usr/bin/env python3
"""Check a co-op session running the S08-T04 party quest fixture (Sidorovich, esc_m_trader_task_3).

Usage: python3 tools/compare-coop-quest.py <session logs> [--out compare-quest.json] [--expect-restart]

Inputs (builds/logs/<task>/run-<run>/): server/ (and server-first-run/): coop-server.log;
c1/, c2/: coop-c*.log, coop-transfer-client.csv (the driver's steps and results).

Rounds (CoopServerHost.cpp quest_fixtures; `other` is the player who did not accept):
  0 p2 turns in before anyone accepted       -> 18 NotActive
  1 both accept at once                      -> one Accepted, the other 16 Active
  2 the holder picks up its half             -> pickups Accepted
  3 the other runs the installed completion and reward locally (step 0: no result, nothing on the
    server), then turns in too early         -> 19 NotFinished
  4 the other picks up its half              -> pickups Accepted
  5 an unsupported task id, an undefined op  -> 15, 2
  6 variant 0: both turn in at the same tick -> both Accepted with the one completion's commit
    variant 1: the other drops its connection, the one who accepted turns in -> Accepted (the absent
    contributor's patches counted and paid)
  7 the one who accepted turns in again      -> 18 NotActive
  8 asking a dead stalker for work           -> 15

Checks:
  rounds          every round's results as above
  completed_once  one quest-completed per instance; no second completion of the same instance
  patches         the items taken are exactly the need, each released (ok=1), from both players' inventories
  paid_per_owner  each player paid once, for the number of patches taken from his own inventory
  goodwill_same   both party actors got the same goodwill changes
  duplicates      a same-id repeat of the Accepted turn-in returned its original commit
  news            both clients showed the party's news (accepted, the count, complete)
  local_no_effect the client's local completion attempt ran and changed nothing on the server: the
                  server's final money of each party actor is what the quest paid on top of its own
"""

import argparse
import csv
import json
import re
import sys
from pathlib import Path


def lines(path):
    return path.read_text(errors="replace").splitlines() if path.exists() else []


def field(line, name, cast=str):
    m = re.search(r"\b" + re.escape(name) + r"=(\S+)", line)
    return cast(m.group(1)) if m else None


def rows(path):
    if not path.exists():
        return []
    with open(path, newline="") as f:
        return list(csv.DictReader(x for x in f if not x.startswith("#")))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--out", default="compare-quest.json")
    ap.add_argument("--expect-restart", action="store_true")
    a = ap.parse_args()
    logs = Path(a.logs)
    incarnations = [d for d in (logs / "server-first-run", logs / "server") if (d / "coop-server.log").exists()]
    every = [x for d in incarnations for x in lines(d / "coop-server.log")]
    report = {"checks": {}}
    clients = sorted(p.name for p in logs.iterdir() if p.is_dir() and re.fullmatch(r"c\d+", p.name))
    slot_of = {}
    results, firsts, dups = {}, {}, []
    for c in clients:
        crows = rows(logs / c / "coop-transfer-client.csv")
        for r in crows:
            slot_of[c] = int(r["slot"])
            if r["event"] == "result" and int(r["step"]) >= 0:
                key = (int(r["round"]), int(r["step"]))
                results.setdefault(key, {"client": c, "outcome": int(r["outcome"]), "reason": int(r["reason"]),
                                         "commit": int(r["commit"]), "action": int(r["action"])})
                firsts.setdefault((c, r["operation"]), r)
        for r in crows:
            if r["event"] == "dup-result":
                o = firsts.get((c, r["operation"]))
                dups.append({"client": c, "round": int(r["round"]), "accepted": bool(o) and o["outcome"] == "1",
                             "same": bool(o) and (o["outcome"], o["commit"]) == (r["outcome"], r["commit"])})

    def got(rnd, step):
        r = results.get((rnd, step))
        return (r["outcome"], r["reason"]) if r else None
    rounds = {}
    rounds[0] = got(0, 0) == (2, 18)
    pair = sorted(x for x in (got(1, 0), got(1, 1)) if x)
    rounds[1] = pair == [(1, 0), (2, 16)]
    for rnd in (2, 4):
        steps = [v for k, v in results.items() if k[0] == rnd]
        rounds[rnd] = bool(steps) and all((s["outcome"], s["reason"]) == (1, 0) for s in steps)
    rounds[3] = got(3, 1) == (2, 19)
    rounds[5] = sorted(x for x in (got(5, 0), got(5, 1)) if x) == [(2, 2), (2, 15)]
    r6 = [x for x in every if " fixture-quest-round round=6 " in x]
    variant = 1 if r6 and ":op5:" in r6[-1] else 0
    report["variant"] = variant
    if variant == 0:
        r0, r1 = results.get((6, 0)), results.get((6, 1))
        rounds[6] = bool(r0 and r1) and (r0["outcome"], r1["outcome"]) == (1, 1) and r0["commit"] == r1["commit"]
    else:
        rounds[6] = got(6, 1) == (1, 0)
    rounds[7] = got(7, 0) == (2, 18)
    rounds[8] = got(8, 0) == (2, 15)
    report["results"] = {f"{k[0]}:{k[1]}": v for k, v in results.items()}
    report["checks"]["rounds"] = {"pass": all(rounds.values()), "rounds": rounds}

    completed = [x.split(" ", 1)[1] for x in every if " quest-completed " in x]
    instances = [field(x, "instance") for x in completed]
    report["checks"]["completed_once"] = {"pass": len(completed) >= 1 and len(instances) == len(set(instances)),
                                          "completed": completed}

    accepted = [x for x in every if " quest-accepted " in x]
    need = field(accepted[-1], "need", int) if accepted else None
    taken = [x for x in every if " quest-item-taken " in x]
    from_counts = {}
    for x in taken:
        from_counts[field(x, "from")] = from_counts.get(field(x, "from"), 0) + 1
    report["checks"]["patches"] = {"pass": bool(taken) and len(taken) == need and all(" ok=1" in x for x in taken)
                                   and len(from_counts) == 2, "need": need, "taken": len(taken), "from": from_counts}

    paid = [x for x in every if " quest-paid " in x]
    pay = {field(x, "actor"): {"patches": field(x, "patches", int), "amount": field(x, "amount", int),
                               "money": field(x, "money")} for x in paid}
    report["checks"]["paid_per_owner"] = {
        "pass": bool(pay) and len(paid) == len(pay) and all(pay.get(k, {}).get("patches") == n
                                                             for k, n in from_counts.items()),
        "paid": pay}

    gw = {}
    for x in every:
        if " quest-goodwill " in x:
            gw.setdefault(field(x, "actor"), []).append((field(x, "community"), field(x, "delta", int)))
    report["checks"]["goodwill_same"] = {"pass": len(gw) == 2 and len({tuple(v) for v in gw.values()}) == 1,
                                         "goodwill": {k: v for k, v in gw.items()}}

    turnin_dups = [d for d in dups if d["round"] in (1, 6)]
    same = all(d["same"] for d in turnin_dups if d["accepted"])
    report["checks"]["duplicates"] = {"pass": bool(turnin_dups) and same,
                                      "turn_in_repeats": turnin_dups}

    news = {}
    for c in clients:
        shown = [x for x in lines(logs / c / f"coop-{c}.log") if " news-shown " in x]
        news[c] = {"accepted": any("Party task accepted" in x for x in shown),
                   "count": any(" of " in x and "bandit patches" in x for x in shown),
                   "complete": any("Party task complete" in x for x in shown), "lines": len(shown)}
    # variant 1: the client that dropped its connection was away when the completion's news went out
    # (presentations are not replayed after a rejoin)
    away = {c for c in clients if any(" transfer-step-disconnect " in x for x in lines(logs / c / f"coop-{c}.log"))}
    report["checks"]["news"] = {"pass": bool(news) and all(v["accepted"] and v["count"] and (v["complete"] or c in away)
                                                           for c, v in news.items()), "clients": news,
                                "away": sorted(away)}

    attempts = {c: [x.split(" ", 1)[1] for x in lines(logs / c / f"coop-{c}.log") if " quest-local-attempt " in x]
                for c in clients}
    final = {field(x, "actor"): field(x, "money", int) for x in every if " quest-final " in x}
    after = {k: int(v["money"].split("->")[1]) for k, v in pay.items() if v.get("money")}
    report["checks"]["local_no_effect"] = {
        "pass": any(" ran=1 " in x for v in attempts.values() for x in v) and bool(final) and
        all(final.get(k) == m for k, m in after.items()) and len(completed) == 1,
        "attempts": attempts, "final_money": final, "money_after_payment": after}

    answered = [x for x in every if "(quest " in x and "answered after generation" in x]
    published = {field(x, "generation", int) for x in every if " checkpoint-published " in x}
    gens = [int(re.search(r"after generation (\d+)", x).group(1)) for x in answered]
    report["checks"]["answered_after_publication"] = {
        "pass": bool(answered) and all(g in published for g in gens), "answers": len(answered),
        "generations": sorted(set(gens))}
    if variant == 1:
        report["checks"]["absent_contributor_paid"] = {
            "pass": len(pay) == 2 and all(v["patches"] > 0 for v in pay.values()) and
            any(v for v in [x for c in clients for x in lines(logs / c / f"coop-{c}.log")
                            if " transfer-step-disconnect " in x]), "paid": pay}

    report["pass"] = all(v["pass"] for v in report["checks"].values())
    out = Path(a.out)
    out.write_text(json.dumps(report, indent=2) + "\n")
    for k, v in report["checks"].items():
        print(f"{'PASS' if v['pass'] else 'FAIL'} {k}")
    print("overall", "PASS" if report["pass"] else "FAIL", "->", out)
    return 0 if report["pass"] else 1


if __name__ == "__main__":
    sys.exit(main())

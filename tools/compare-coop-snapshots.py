#!/usr/bin/env python3
"""Compare where the server had its moving entities with where each client
showed them (S06-T02).

Usage: python3 tools/compare-coop-snapshots.py <session logs> [--lag-ms 120]
                                               [--out compare-snapshots.json]

Inputs (written by the engine, see docs/coop-session.md):
  server/coop-snapshots.csv          the server's states at every snapshot
                                     (participants, creatures; physics objects
                                     only while they move)
  <client>/coop-snapshots-client.csv where the client showed every tracked
                                     entity, 10 times a second

For each client row the server's position for that entity at (client time -
lag) is interpolated from the server's log; the distance is the presentation
error. A client shows the server's state from about one render delay
(100 ms) plus one-way latency ago, hence the default lag. Rows are split by
the server's kind (0 participant actor, 1 creature, 2 physics object) and by
whether the entity was moving (server speed over +-0.4 s > 0.3 m/s). Crows
(section m_crow) are reported apart as "flyer": the server moves them in
jumps of about 6 m every half second while a client shows them flying
smoothly between those points, so their error measures the server's steps,
not presentation; they are not in the checks.

Reference (rule change after snap-9, recorded in docs/validation/S06-T02.md):
the server moves a distant NPC only when it is scheduled, about every 0.6 s,
so its logged position is a step function; a smooth client is off it by up
to half a step (about 2 m for a running dog). The reference is therefore the
line through the server's position changes: equal positions held for less
than 1 s are taken as a scheduling step and dropped, longer holds are kept
as stops. The raw step reference is reported too ("errors_raw").

Coverage: every creature and object that moved more than 1 m on the server
while a client was live must have been shown moving on that client: its
shown path at least 70 % of the server's. Objects that came to rest must be
shown at their resting place (within 5 cm) at the end.

Checks (all must pass):
  creatures_moving_p95  p95 error of moving creatures <= 0.5 m on each client
  objects_moving_p95    p95 error of moving objects <= 0.5 m on each client
  creature_coverage     >= 95 % of moved creatures shown moving
  object_coverage       >= 90 % of moved objects shown moving
  object_rest           >= 95 % of rested objects at their resting place
  participants_p95      p95 error of participant actors <= 0.5 m
"""

import argparse
import bisect
import csv
import json
import math
import statistics
import sys
from collections import defaultdict
from pathlib import Path

KINDS = {0: "participant", 1: "creature", 2: "object", 3: "flyer"}


def pct(values, p):
    if not values:
        return None
    v = sorted(values)
    k = min(len(v) - 1, max(0, int(round(p / 100.0 * (len(v) - 1)))))
    return v[k]


def summary(values):
    if not values:
        return {"count": 0}
    return {"count": len(values), "median": round(statistics.median(values), 4),
            "p95": round(pct(values, 95), 4), "max": round(max(values), 4)}


class Series:
    def __init__(self):
        self.t = []
        self.p = []

    def add(self, t, p):
        self.t.append(t)
        self.p.append(p)

    def finish(self):
        order = sorted(range(len(self.t)), key=lambda i: self.t[i])
        self.t = [self.t[i] for i in order]
        self.p = [self.p[i] for i in order]

    def at(self, t, max_gap=200):
        """Position at t (linear), None if no samples within max_gap around t."""
        i = bisect.bisect_left(self.t, t)
        if i < len(self.t) and self.t[i] == t:
            return self.p[i]
        if i == 0 or i == len(self.t):
            return None
        a, b = self.t[i - 1], self.t[i]
        if b - a > max_gap:
            return None
        k = (t - a) / (b - a)
        pa, pb = self.p[i - 1], self.p[i]
        return tuple(pa[j] + (pb[j] - pa[j]) * k for j in range(3))

    def speed(self, t, half=400):
        """Mean speed over t +- half ms: the server moves distant NPCs in
        steps (every ~0.6 s), so consecutive rows say little."""
        a, b = self.at(t - half, max_gap=1000), self.at(t + half, max_gap=1000)
        if a is None or b is None:
            return 0.0
        return math.dist(a, b) / (2 * half / 1000.0)

    def path(self, t0, t1):
        d = 0.0
        prev = None
        for t, p in zip(self.t, self.p, strict=True):
            if t < t0 or t > t1:
                continue
            if prev is not None:
                d += math.dist(prev, p)
            prev = p
        return d

    def last_before(self, t):
        i = bisect.bisect_right(self.t, t)
        return (self.t[i - 1], self.p[i - 1]) if i > 0 else None


def load_server(path, lifecycle):
    sections = {}
    if lifecycle.exists():
        for r in csv.DictReader(open(lifecycle, newline="")):
            if r["kind"] == "spawn":
                sections[(int(r["entity"]), int(r["generation"]))] = r["section"]
    series = defaultdict(Series)
    kind = {}
    for r in csv.DictReader(open(path, newline="")):
        key = (int(r["entity"]), int(r["generation"]))
        series[key].add(int(r["utc_ms"]), (float(r["x"]), float(r["y"]), float(r["z"])))
        kind[key] = int(r["kind"])
        if kind[key] == 1 and sections.get(key, "").startswith("m_crow"):
            kind[key] = 3
    for s in series.values():
        s.finish()
    smooth = {}
    for key, s in series.items():
        smooth[key] = smoothed(s) if kind.get(key) in (1, 3) else s
    return series, smooth, kind


def smoothed(s, hold_ms=1000):
    """The line through the positions where s changed; a hold shorter than
    hold_ms is a scheduling step (its start only), a longer one a stop (both
    ends)."""
    out = Series()
    i, n = 0, len(s.t)
    while i < n:
        j = i
        while j + 1 < n and s.p[j + 1] == s.p[i]:
            j += 1
        out.add(s.t[i], s.p[i])
        if j > i and (s.t[j] - s.t[i] >= hold_ms or j == n - 1):
            out.add(s.t[j], s.p[j])
        i = j + 1
    return out


def load_client(path):
    series = defaultdict(Series)
    for r in csv.DictReader(open(path, newline="")):
        key = (int(r["entity"]), int(r["generation"]))
        series[key].add(int(r["utc_ms"]), (float(r["x"]), float(r["y"]), float(r["z"])))
    for s in series.values():
        s.finish()
    return series


def compare_client(name, client, server, kind, lag, raw=None):
    errors = defaultdict(list)
    errors_raw = defaultdict(list)
    t_all = [t for s in client.values() for t in s.t]
    if not t_all:
        return {"rows": 0}
    t0, t1 = min(t_all), max(t_all)
    for key, cs in client.items():
        ss = server.get(key)
        if not ss:
            continue
        k = KINDS.get(kind.get(key, -1), "other")
        rs = raw.get(key) if raw else None
        for t, p in zip(cs.t, cs.p, strict=True):
            q = ss.at(t - lag, max_gap=2000)
            if q is None:
                continue
            moving = ss.speed(t - lag) > 0.3
            errors[(k, moving)].append(math.dist(p, q))
            r = rs.at(t - lag) if rs else None
            if r is not None:
                errors_raw[(k, moving)].append(math.dist(p, r))
    # Coverage of what moved on the server while this client was live.
    coverage = {"creature": [0, 0], "object": [0, 0]}
    ratios = defaultdict(list)
    missing = []
    rest_ok, rest_total, rest_errors = 0, 0, []
    for key, ss in server.items():
        k = KINDS.get(kind.get(key))
        if k not in ("creature", "object"):
            continue
        moved = ss.path(t0 + 1000, t1 - 1000)
        if moved < 1.0:
            continue
        coverage[k][1] += 1
        cs = client.get(key)
        shown = cs.path(t0 + 1000 + lag, t1 - 1000 + lag) if cs else 0.0
        ratios[k].append(shown / moved)
        if shown >= 0.7 * moved:
            coverage[k][0] += 1
        else:
            missing.append({"entity": key[0], "generation": key[1], "kind": k, "server_path_m": round(moved, 2),
                            "client_path_m": round(shown, 2), "client_rows": len(cs.t) if cs else 0})
        if k == "object" and cs and ss.t[-1] < t1 - 3000:
            # came to rest before the client's end: the last shown position
            rest = ss.p[-1]
            last = cs.last_before(t1)
            if last:
                rest_total += 1
                e = math.dist(last[1], rest)
                rest_errors.append(e)
                rest_ok += 1 if e <= 0.05 else 0
    out = {"rows": sum(len(s.t) for s in client.values()), "window_utc": [t0, t1], "errors": {}}
    for (k, moving), v in sorted(errors.items()):
        out["errors"][f"{k}{'_moving' if moving else '_still'}"] = summary(v)
    out["errors_raw"] = {f"{k}{'_moving' if m else '_still'}": summary(v) for (k, m), v in sorted(errors_raw.items())}
    out["coverage"] = {k: {"shown": c[0], "moved": c[1],
                           "path_ratio_median": round(statistics.median(ratios[k]), 3) if ratios[k] else None}
                       for k, c in coverage.items()}
    out["missing"] = missing[:20]
    out["object_rest"] = {"ok": rest_ok, "total": rest_total, "errors": summary(rest_errors)}
    # The lag that fits best (moving creatures), for the record.
    best = None
    for test_lag in range(0, 401, 20):
        v = []
        for key, cs in client.items():
            if kind.get(key) != 1 or key not in server:
                continue
            ss = server[key]
            for t, p in zip(cs.t, cs.p, strict=True):
                if ss.speed(t - test_lag) <= 0.3:
                    continue
                q = ss.at(t - test_lag)
                if q is not None:
                    v.append(math.dist(p, q))
        if v and (best is None or statistics.median(v) < best[1]):
            best = (test_lag, statistics.median(v))
    out["best_lag_ms"] = best[0] if best else None
    out["best_lag_median_m"] = round(best[1], 4) if best else None
    return out


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("logs")
    ap.add_argument("--lag-ms", type=int, default=120)
    ap.add_argument("--out", default="")
    a = ap.parse_args()
    logs = Path(a.logs)
    raw, server, kind = load_server(logs / "server/coop-snapshots.csv", logs / "server/coop-lifecycle.csv")
    result = {"lag_ms": a.lag_ms, "clients": {}, "checks": {}}
    for d in sorted(p for p in logs.iterdir() if p.is_dir() and p.name != "server"):
        f = d / "coop-snapshots-client.csv"
        if f.exists():
            result["clients"][d.name] = compare_client(d.name, load_client(f), server, kind, a.lag_ms, raw)
    checks = {}
    for name, c in result["clients"].items():
        e = c.get("errors", {})
        cov = c.get("coverage", {})

        def p95(key, e=e):
            return e.get(key, {}).get("p95")
        checks[f"{name}/creatures_moving_p95"] = p95("creature_moving") is not None and p95("creature_moving") <= 0.5
        checks[f"{name}/objects_moving_p95"] = p95("object_moving") is None or p95("object_moving") <= 0.5
        cr, ob = cov.get("creature", {}), cov.get("object", {})
        checks[f"{name}/creature_coverage"] = bool(cr.get("moved")) and cr["shown"] >= 0.95 * cr["moved"]
        checks[f"{name}/object_coverage"] = not ob.get("moved") or ob["shown"] >= 0.9 * ob["moved"]
        rest = c.get("object_rest", {})
        checks[f"{name}/object_rest"] = not rest.get("total") or rest["ok"] >= 0.95 * rest["total"]
        pp = [v.get("p95") for k, v in e.items() if k.startswith("participant") and v.get("p95") is not None]
        checks[f"{name}/participants_p95"] = bool(pp) and max(pp) <= 0.5
    result["checks"] = checks
    result["match"] = bool(checks) and all(checks.values())
    text = json.dumps(result, indent=2)
    if a.out:
        Path(a.out).write_text(text + "\n")
    print(text)
    return 0 if result["match"] else 1


if __name__ == "__main__":
    sys.exit(main())

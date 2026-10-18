"""Compare two S02-T03c bone captures (server and client CSV files).

Usage: python3 tools/compare-bone-capture.py <server.csv> <client.csv> [tolerance]

Both files must list the same steps, bones and bone names. Every transform value
must match within the tolerance (default 1e-4, fixture assertion H05). Prints a
JSON summary and exits 1 on any mismatch.
"""

import csv
import json
import sys


def load(path):
    with open(path, newline="") as handle:
        return list(csv.DictReader(handle))


def main():
    if len(sys.argv) not in (3, 4):
        raise SystemExit(__doc__)
    tolerance = float(sys.argv[3]) if len(sys.argv) == 4 else 1e-4
    server, client = load(sys.argv[1]), load(sys.argv[2])
    keys = ["ix", "iy", "iz", "jx", "jy", "jz", "kx", "ky", "kz", "cx", "cy", "cz"]
    summary = {"tolerance": tolerance, "server_rows": len(server), "client_rows": len(client)}
    if len(server) != len(client) or not server:
        summary["result"] = "row count differs or empty"
        print(json.dumps(summary, indent=1))
        raise SystemExit(1)

    worst = 0.0
    worst_at = None
    identical = 0
    for a, b in zip(server, client, strict=True):
        if (a["step"], a["bone"], a["name"]) != (b["step"], b["bone"], b["name"]):
            summary["result"] = f"row identity differs: {a['step']}/{a['bone']} vs {b['step']}/{b['bone']}"
            print(json.dumps(summary, indent=1))
            raise SystemExit(1)
        if all(a[k] == b[k] for k in keys):
            identical += 1
        for k in keys:
            diff = abs(float(a[k]) - float(b[k]))
            if diff > worst:
                worst, worst_at = diff, (a["step"], a["name"], k)

    summary.update({
        "steps": len({row["step"] for row in server}),
        "bones": len({row["bone"] for row in server}),
        "identical_rows": identical,
        "max_abs_difference": worst,
        "max_at": worst_at,
        "result": "match" if worst <= tolerance else "mismatch",
    })
    print(json.dumps(summary, indent=1))
    raise SystemExit(0 if worst <= tolerance else 1)


if __name__ == "__main__":
    main()

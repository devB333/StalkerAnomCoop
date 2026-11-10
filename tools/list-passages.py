#!/usr/bin/env python3
"""List the level changers of an Anomaly ``all.spawn`` and the passages they form.

A string scan (S10-T01): for every ``level_changer`` section string in the
spawn file, the object name that follows it and the first destination level
name among the following strings are taken as the changer's name and
destination; the next string is the arrival graph point. Changers whose
destination is not found are reported with an empty destination. The source
level of a changer is inferred from its name prefix (``esc_`` is Cordon and so
on), which is a convention of the installed content, not data in the file.

Writes ``changers.tsv`` (name, destination, arrival point) and
``passages.tsv`` (source, destination, changers) into the output directory
and prints a summary.

Usage::

    python3 tools/list-passages.py <all.spawn> <levels-dir> <out-dir>
"""

import argparse
import collections
import re
import sys
from pathlib import Path

PREFIX_LEVELS = {
    "esc": "l01_escape", "gar": "l02_garbage", "agr": "l03_agroprom",
    "agru": "l03u_agr_underground", "val": "l04_darkvalley", "bar": "l05_bar",
    "ros": "l06_rostok", "mil": "l07_military", "yan": "l08_yantar",
    "cit": "l09_deadcity", "lim": "l10_limansk", "rad": "l10_radar",
    "red": "l10_red_forest", "pri": "l11_pripyat", "aes": "l12_stancia",
    "aes2": "l12_stancia_2", "gen": "l13_generators", "mar": "k00_marsh",
    "ds": "k01_darkscape", "tc": "k02_trucks_cemetery", "pol": "y04_pole",
    "jup": "jupiter", "jupiter": "jupiter", "zaton": "zaton",
    "pripyat": "pripyat", "katacomb": "l11_hospital", "sar": "l12u_sarcofag",
    "warlab": "l13u_warlab", "labx8": "labx8", "cop_pripyat": "pripyat",
}
# Changers whose names do not follow the prefix convention.
NAMED_SOURCES = {
    "exit_to_bunker_01": "l10_radar",
    "exit_to_control_monolith_01": "l12_stancia_2",
    "exit_to_radar_01": "l10u_bunker",
    "exit_to_sarcofag_01": "l12_stancia_2",
    "exit_to_stancia2_01": "l12u_control_monolith",
    "level_changer_to_darkvalley_01": "l04u_labx18",
    "level_changer_to_yantar_01": "l08u_brainlab",
    "level_changer_to_yantar_tunnel_01": "l08u_brainlab",
    "level_changer_warlab": "l13u_warlab",
    "warlab_level_changer": "l13u_warlab",
    "warlab_level_changer_0000": "l13u_warlab",
    "labx8_level_changer": "labx8",
    "fake_level_changer": "k00_marsh",
    "jup_ug_level_changer_to_jupiter": "jupiter_underground",
    "jup_und_to_pri": "jupiter_underground",
    "pas_b400_level_changer": "jupiter_underground",
    "pri_to_jup_und": "l11_pripyat",
}
STRING = re.compile(rb"[\x20-\x7e]{2,64}\0")


def level_names(levels_dir):
    names = set()
    for path in Path(levels_dir).glob("level_*.db0"):
        names.add(path.name[len("level_"):-len(".db0")])
    return names


def scan_changers(data, levels):
    rows = []
    for match in re.finditer(rb"level_changer\0", data):
        segment = data[match.start():match.start() + 1500]
        strings = [s[:-1].decode() for s in STRING.findall(segment)]
        if len(strings) < 2:
            continue
        name = strings[1]
        dest = point = ""
        for i, s in enumerate(strings[2:], 2):
            if s == "level_changer":
                break
            cleaned = re.sub(r"^[^a-z]+", "", s)
            if cleaned in levels:
                dest = cleaned
                point = strings[i + 1] if i + 1 < len(strings) else ""
                break
        rows.append((name, dest, point))
    return rows


def source_level(name):
    if name in NAMED_SOURCES:
        return NAMED_SOURCES[name]
    for prefix in ("jupiter_to", "jupiter_level", "zaton", "pripyat_level"):
        if name.startswith(prefix):
            return PREFIX_LEVELS[prefix.split("_")[0]]
    if "_level_changer" in name:
        key = name.split("_level_changer")[0]
    else:
        key = name.split("_")[0]
    return PREFIX_LEVELS.get(key, key)


def main():
    ap = argparse.ArgumentParser(description=__doc__.split("\n\n", 1)[0])
    ap.add_argument("spawn", help="the extracted all.spawn")
    ap.add_argument("levels_dir", help="directory holding level_<name>.db0 archives")
    ap.add_argument("out_dir", help="where changers.tsv and passages.tsv are written")
    args = ap.parse_args()

    levels = level_names(args.levels_dir)
    data = Path(args.spawn).read_bytes()
    rows = scan_changers(data, levels)
    out = Path(args.out_dir)
    out.mkdir(parents=True, exist_ok=True)
    with open(out / "changers.tsv", "w", encoding="utf-8") as fh:
        fh.write("name\tdestination\tarrival_point\n")
        for row in rows:
            fh.write("%s\t%s\t%s\n" % row)

    edges = collections.defaultdict(set)
    unparsed = []
    for name, dest, _ in rows:
        if not dest or not re.fullmatch(r"[a-z][a-z0-9_]*", name) or name in levels:
            unparsed.append((name, dest))
            continue
        edges[(source_level(name), dest)].add(name)
    with open(out / "passages.tsv", "w", encoding="utf-8") as fh:
        fh.write("source\tdestination\tchangers\n")
        for (src, dest), names in sorted(edges.items()):
            fh.write("%s\t%s\t%s\n" % (src, dest, ", ".join(sorted(names))))

    graph_levels = {lvl for edge in edges for lvl in edge}
    print("%d level_changer entries, %d distinct named changers, %d directed passages, %d levels"
          % (len(rows), sum(len(v) for v in edges.values()), len(edges), len(graph_levels)))
    print("not parsed (no destination, binary name or a stray section):", unparsed)
    return 0


if __name__ == "__main__":
    sys.exit(main())

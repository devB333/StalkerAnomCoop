"""Create an isolated, bounded Stage 2 startup fixture."""

import hashlib
import json
import ntpath
import re
from pathlib import Path
import shutil
import subprocess
import sys


workspace = Path(__file__).resolve().parents[1]
baseline = workspace / "builds/build_2/test-install/source-client"
# Usage: prepare-stage2-smoke.py <run> [<task-id>]; the task id picks the
# runtime folder (default S02-T02, the campaign smoke).
run_name = sys.argv[1] if len(sys.argv) >= 2 else "smoke-01"
task = sys.argv[2] if len(sys.argv) == 3 else "S02-T02"
if not run_name.replace("-", "").isalnum():
    raise SystemExit("Run name must be alphanumeric with optional hyphens")
if not re.fullmatch(r"S\d\d-T\d\d[a-z]?", task):
    raise SystemExit("Task id must look like S02-T03c")
runtime = workspace / "builds/stage2/test-install" / task / run_name
if runtime.exists():
    raise SystemExit(f"Preserving existing run: {runtime}")
for dirname in ("appdata", "appdata/logs", "appdata/savedgames", "tmp"):
    (runtime / dirname).mkdir(parents=True, exist_ok=False)


def winpath(path):
    return subprocess.check_output(["wslpath", "-w", str(path)], text=True).strip()


source = baseline / "fsgame.ltx"
source_hash = hashlib.sha256(source.read_bytes()).hexdigest()
expected_hash = json.loads((workspace / "docs/accepted-baseline.json").read_text())["runtime"]["filesystem_config"]["sha256"]
if source_hash != expected_hash:
    raise SystemExit("Accepted filesystem configuration changed")

roots = {
    "$fs_root$": winpath(runtime) + "\\",
    "$app_data_root$": winpath(runtime / "appdata") + "\\",
    "$arch_dir$": winpath(baseline / "db") + "\\",
    "$game_data$": winpath(runtime / "gamedata") + "\\",
}
lines = []
aliases = {}
for line in source.read_text().splitlines():
    name = line.split("=", 1)[0].strip()
    if name in roots:
        fields = line.split("|", 2)
        if name == "$fs_root$":
            line = "$fs_root$ = false | false | " + roots[name]
        else:
            line = fields[0] + "|" + fields[1] + "| " + roots[name]
    lines.append(line)
    clean = line.split(";", 1)[0].strip()
    if not clean or "=" not in clean:
        continue
    key, value = clean.split("=", 1)
    parts = [part.strip() for part in value.split("|")]
    if len(parts) < 3:
        continue
    parent = aliases.get(parts[2], parts[2])
    if parent.startswith("$"):
        raise SystemExit(f"Unresolved filesystem alias: {parent}")
    aliases[key.strip()] = ntpath.normpath(ntpath.join(parent, parts[3] if len(parts) > 3 else ""))

run_windows = winpath(runtime)
baseline_windows = winpath(baseline)
workspace_windows = winpath(workspace)
for name, resolved in aliases.items():
    if ntpath.commonpath([workspace_windows, resolved]).lower() != workspace_windows.lower():
        raise SystemExit(f"Alias escapes workspace: {name}={resolved}")
    if name in ("$app_data_root$", "$logs$", "$screenshots$", "$game_saves$", "$downloads$"):
        if ntpath.commonpath([run_windows, resolved]).lower() != run_windows.lower():
            raise SystemExit(f"Writable alias escapes run: {name}={resolved}")
    if name == "$arch_dir$":
        if ntpath.commonpath([baseline_windows, resolved]).lower() != baseline_windows.lower():
            raise SystemExit(f"Content alias differs from accepted fixture: {name}={resolved}")

# Archive entry_point=gamedata mounts at $fs_root$\\gamedata, independently
# of $game_data$. Keep virtual archive files and the loose overlay together.
if aliases["$game_data$"].lower() != ntpath.join(aliases["$fs_root$"], "gamedata").lower():
    raise SystemExit("Game data and archive mount paths differ")
shutil.copytree(baseline / "gamedata", runtime / "gamedata")
overlay = []
for original in sorted((baseline / "gamedata").rglob("*")):
    if original.is_file():
        relative = original.relative_to(baseline / "gamedata")
        digest = hashlib.sha256(original.read_bytes()).hexdigest()
        if hashlib.sha256((runtime / "gamedata" / relative).read_bytes()).hexdigest() != digest:
            raise SystemExit(f"Loose overlay copy differs: {relative}")
        overlay.append({"path": str(relative), "sha256": digest})

config = runtime / "fsgame.ltx"
config.write_text("\n".join(lines) + "\n")
manifest = {
    "run_root": str(runtime.relative_to(workspace)),
    "accepted_config_sha256": source_hash,
    "config_sha256": hashlib.sha256(config.read_bytes()).hexdigest(),
    "content_root": str(baseline.relative_to(workspace)),
    "aliases": aliases,
    "loose_overlay": overlay,
    "expected_mode": "S02-T02 startup smoke only",
}
(runtime / "fixture.json").write_text(json.dumps(manifest, indent=2) + "\n")
print(config)

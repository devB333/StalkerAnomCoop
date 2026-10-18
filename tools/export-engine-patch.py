"""Export engine-work changes as the next patch in patches/engine/series.

Usage: python3 tools/export-engine-patch.py <patch-file-name> <check-number>

1. Clones the pinned reference into builds/stage2/patch-check-<n>a, applies the
   existing series, copies every changed file from engine-work and writes the
   difference as the new patch.
2. Applies the whole series in a second fresh clone (patch-check-<n>b) and
   checks every changed file is byte-equal to engine-work.
3. Writes builds/logs/patch-reconstruction-<n>.json and prints patch hashes.
Both clone directories must not exist yet. Submodules are not initialized.
"""

import hashlib
import json
import shutil
import subprocess
import sys
from pathlib import Path

PIN = "c57fe5b9fab4fe0506659db03137c52d2500eb2f"
workspace = Path(__file__).resolve().parents[1]
reference = workspace / "engine/xray-monolith"
work = workspace / "builds/build_1/engine-work"
patches = workspace / "patches/engine"
series = patches / "series"


def git(cwd, *args, capture=False):
    result = subprocess.run(["git", "-C", str(cwd), *args], check=True,
                            capture_output=capture, text=True)
    return result.stdout if capture else None


def sha256(path):
    return hashlib.sha256(Path(path).read_bytes()).hexdigest()


def fresh_clone(path):
    if path.exists():
        raise SystemExit(f"Preserving existing directory: {path}")
    subprocess.run(["git", "clone", "-q", "--no-hardlinks", str(reference), str(path)], check=True)
    git(path, "checkout", "-q", "--detach", PIN)


def apply_series(path, names):
    for name in names:
        git(path, "apply", "--check", str(patches / name))
        git(path, "apply", str(patches / name))


def changed_files():
    tracked = git(work, "diff", "--name-only", capture=True).split()
    untracked = git(work, "ls-files", "--others", "--exclude-standard", capture=True).split()
    return sorted(set(tracked) | set(untracked))


def main():
    if len(sys.argv) != 3:
        raise SystemExit(__doc__)
    name, number = sys.argv[1], sys.argv[2]
    if not name.endswith(".patch") or (patches / name).exists():
        raise SystemExit(f"Need a new .patch name: {name}")
    if git(work, "rev-parse", "HEAD", capture=True).strip() != PIN:
        raise SystemExit("engine-work is not at the pinned revision")
    existing = [line.strip() for line in series.read_text().splitlines() if line.strip()]
    files = changed_files()

    build = workspace / f"builds/stage2/patch-check-{number}a"
    fresh_clone(build)
    apply_series(build, existing)
    git(build, "add", "-A")
    git(build, "-c", "user.name=patch-export", "-c", "user.email=patch-export@localhost",
        "commit", "-q", "--allow-empty", "-m", "existing series")
    for relative in files:
        target = build / relative
        target.parent.mkdir(parents=True, exist_ok=True)
        shutil.copyfile(work / relative, target)
    git(build, "add", "-A")
    diff = git(build, "diff", "--cached", capture=True)
    if not diff:
        raise SystemExit("No changes beyond the existing series")
    (patches / name).write_text(diff)
    with series.open("a") as handle:
        handle.write(name + "\n")
    print(git(build, "diff", "--cached", "--stat", capture=True))

    check = workspace / f"builds/stage2/patch-check-{number}b"
    fresh_clone(check)
    apply_series(check, existing + [name])
    matched = {}
    for relative in files:
        if sha256(check / relative) != sha256(work / relative):
            raise SystemExit(f"Reconstruction differs: {relative}")
        matched[relative] = sha256(work / relative)
    record = {
        "base": PIN,
        "scratch_clone": str(check.relative_to(workspace)),
        "patches": [{"name": n, "sha256": sha256(patches / n)} for n in existing + [name]],
        "matched_source_files": matched,
    }
    out = workspace / f"builds/logs/patch-reconstruction-{number}.json"
    out.write_text(json.dumps(record, indent=1) + "\n")
    print(f"{len(matched)} files byte-equal; record {out.relative_to(workspace)}")
    for item in record["patches"]:
        print(item["sha256"], item["name"])


if __name__ == "__main__":
    main()

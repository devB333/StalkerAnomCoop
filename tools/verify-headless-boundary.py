#!/usr/bin/env python3
"""Check S02-T01 citations and input identity; does not execute the engine."""

import hashlib
import json
from pathlib import Path
import re
import subprocess


ROOT = Path(__file__).resolve().parents[1]


def git(checkout, *args):
    return subprocess.check_output(
        ["git", "-C", str(checkout), *args], stderr=subprocess.PIPE
    )


def main():
    fixture = json.loads((ROOT / "docs/headless-fixture.json").read_text())
    baseline = json.loads((ROOT / fixture["accepted_baseline"]).read_text())
    source = ROOT / fixture["source_checkout"]
    revision = git(source, "rev-parse", "HEAD").decode().strip()
    if revision != fixture["source_revision"] or revision != baseline["source_revision"]:
        raise ValueError("Source revision differs from the accepted fixture baseline")
    optick = git(source / "src/3rd party/optick-git", "rev-parse", "HEAD").decode().strip()
    if optick != fixture["dependency_revision"]["optick"]:
        raise ValueError("Optick revision differs from the fixture baseline")

    for key in ("content_manifest", "overlay_manifest", "snapshot_manifest"):
        json.loads((ROOT / fixture[key]).read_text())

    inputs = []
    for item in fixture["inputs"]:
        path = ROOT / item["path"]
        digest = hashlib.sha256(path.read_bytes()).hexdigest()
        if digest != item["sha256"]:
            raise ValueError(f"Fixture input hash mismatch: {item['path']}")
        inputs.append({**item, "bytes": path.stat().st_size})

    document = (ROOT / "docs/headless-service-boundary.md").read_text()
    references = sorted(set(re.findall(
        r"`((?:xrEngine|xrGame|xrPhysics|Layers)/[^`:\s]+):(\d+)-(\d+)`",
        document,
    )))
    if not references:
        raise ValueError("No source citations found")
    files = {}
    for name, first, last in references:
        if name not in files:
            path = source / "src" / name
            raw = path.read_bytes()
            # Compare normalized text because checkout line endings may be CRLF.
            pinned = git(source, "show", f"{revision}:src/{name}")
            if raw.replace(b"\r\n", b"\n") != pinned.replace(b"\r\n", b"\n"):
                raise ValueError(f"Cited source differs from pinned revision: {name}")
            files[name] = {
                "sha256": hashlib.sha256(raw).hexdigest(),
                "lines": len(raw.splitlines()),
            }
        if not 1 <= int(first) <= int(last) <= files[name]["lines"]:
            raise ValueError(f"Source citation outside file: {name}:{first}-{last}")

    print(json.dumps({
        "status": "passed",
        "scope": "Static source citation and fixture identity checks only",
        "source_revision": revision,
        "optick_revision": optick,
        "source_ranges_checked": len(references),
        "source_files": files,
        "fixture_inputs": inputs,
        "runtime_executed": False,
    }, indent=2))


if __name__ == "__main__":
    main()

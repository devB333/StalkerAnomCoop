# Repository Guidelines

## Project Structure & Module Organization

This is an investigation workspace for a two-player Anomaly mod. Start with `README.md` and `docs/engine-investigation.md`; design decisions are in `docs/architecture.md`, `docs/multiplayer-feasibility.md`, and `docs/implementation-roadmap.md`. `reverse-engineering/` holds evidence and a persistent Ghidra project. `tools/` contains Ghidra scripts. `engine/xray-monolith/` is an ignored, pinned reference checkout with its own Git repository. `builds/` is reserved for outputs. `protocol/` holds project-owned networking code (S04-T02 transport adapter) and `tests/` its tests, built by `tools/Build-Protocol.ps1` and run by `tools/run-protocol-tests.py`; create `client/` and `server/` when needed.

## Build, Test & Development Commands

The first unmodified Windows DX11 build succeeded using `tools/Build-Stage1Client.ps1 -Run build-1`; see `docs/validation/S01-T03.md` and the pinned toolchain manifest. Choose a new run name for later builds, and check for a running test client before replacing its executable. Stage 1/G01 is accepted for development with the two user-approved fixture waivers in `docs/validation/G01.md`. Use `builds/build_1/engine-work` for Stage 2; preserve user changes in the reference checkout. S02-T01 and S02-T02 are complete: Core and Services smokes pass; real game-state/ALife construction moved to S02-T05/T06; next work is S02-T03/T04; see `docs/handoffs/current/session-handoff.md`. Do not describe an unrun build or gameplay test as working. Useful current checks are `git status --short` for workspace changes, `git diff --check` for whitespace errors in tracked changes, and `python3 -m json.tool reverse-engineering/persistence-verification.json` for JSON syntax. Keep any future build, test, runtime, and Ghidra output inside this workspace.

## Coding Style & Naming Conventions

Follow the surrounding Monolith C++ style when changing engine code; avoid unrelated formatting. The existing Ghidra Java scripts use four-space indentation. Use descriptive Markdown filenames such as `docs/state-ownership.md` and record the source revision, file, function, and line range behind technical claims. No formatter or linter is configured for project-owned code yet; document one before enforcing it.

## Testing Guidelines

No test framework or coverage threshold is established. Add focused tests under `tests/` with each implementation milestone. Report exact build/test commands, observed results, and remaining limits in `docs/implementation-roadmap.md`. Treat a source inspection or successful Ghidra analysis as evidence, not a gameplay test.

## Commit & Pull Request Guidelines

This repository has no commits, so there is no existing commit-message convention. Use short imperative subjects with a scope, such as `docs: record actor singleton evidence`. Pull requests should describe the behavior or finding, cite the source revision and evidence, list validation performed, and identify unresolved risks. Link an issue when one exists.

## Workspace Safety & Analysis

The original installation at `/mnt/c/Users/djb20/StalkerAnom` is strictly read-only. Never run it or write logs, saves, patches, or modified binaries there. Preserve `reverse-engineering/ghidra/Anomaly153-c857b6f49d32`; verify a binary hash before reuse and never overwrite or delete that database. Generated binaries, archives, logs, temporary files, and Ghidra databases stay out of Git. The user authorized execution of plan.md and Stage 1 build/runtime repair. Follow its stage gates; this does not authorize skipping baseline acceptance or modifying the original installation.

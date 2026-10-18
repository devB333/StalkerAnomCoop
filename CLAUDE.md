# CLAUDE.md

This file provides guidance to Claude Code (claude.ai/code) when working with code in this repository.

@AGENTS.md

## Where things stand

- `plan.md` is the authoritative roadmap. It holds the stage gates (§D), the handoff rules (§G) and the progress table (§I). `docs/session-handoff.md` is the short entry point for resuming work. `docs/implementation-roadmap.md` is legacy, so only add links to it and never a competing task order.
- Task statuses must be exactly `Not Started`, `In Progress`, `Blocked` or `Completed`. Every task gets a `docs/validation/<task-ID>.md` report containing the exact commands, fixtures, expected vs. observed results and limits. Large logs go in `builds/logs/<task-ID>/`, and the report links to them instead of pasting them.
- Engine source is available at `engine/xray-monolith/src`, pinned to upstream `c57fe5b9`. Read and cite the source first. Use Ghidra only for behaviour the source can't answer, such as checking the shipped binary.

## Running tools from WSL

- Run PowerShell scripts with Windows PowerShell 5.1 and a Windows path. `pwsh` is not installed.
  `powershell.exe -NoLogo -NoProfile -NonInteractive -ExecutionPolicy Bypass -File C:\Users\djb20\StalkerAnomCoop\tools\<Script>.ps1 ...`
- Run Python tools with `python3 tools/<script>.py` from WSL.
- Stage 2 build: `Build-Stage2.ps1 -Run <name> -Product Client|Campaign [-EvaluateOnly]`. It builds from `builds/build_1/engine-work`, not the reference checkout.
- Stage 2 smoke: run `python3 tools/prepare-stage2-smoke.py <name>` first, then `Run-Stage2Smoke.ps1 -Run <name> -Scope Core|GameState`.
- Lint Python tools with `builds/tools/ruff-0.16.9/ruff check tools/`, configured in `ruff.toml`. The binary is ignored. To reinstall it, run `gh release download 0.16.9 -R astral-sh/ruff -p 'ruff-x86_64-unknown-linux-gnu.tar.gz*'` and verify the `.sha256`. There's no pip. Existing tools have lint findings that haven't been fixed yet, so only fix lint in files you are already changing.
- Run names must match `^[a-zA-Z0-9-]+$` and must be new for every run. Check existing names under `builds/logs/S02-T02/`. Stage 1 paths in older docs may point at `builds/engine-work`, but the current tree is `builds/build_1/`.

## Engine changes

- Edit only `builds/build_1/engine-work`. Never edit `engine/xray-monolith`. Changes become version-controlled only as ordered patches in `patches/engine/` (`series`).
- After a change: export the patches, check they apply cleanly to a fresh scratch clone using the steps in `patches/engine/README.md`, then update the patch SHA-256s in `docs/source-manifest.json`.
- Don't add placeholder workarounds such as a dummy actor, fake clock or permanent global player. Decisions D1–D8 in `plan.md` are binding.

## Ghidra

- Ghidra 12.1.4 is installed at `/home/djb20/tools/ghidra_12.1.4_PUBLIC`. `analyzeHeadless` is not on PATH. Open existing projects with `-process AnomalyDX11.exe -noanalysis` and read `docs/reverse-engineering-evidence.md` first. A new binary gets its own database keyed by its hash.

## Git

- Commit at task milestones, after the validation report and handoff are updated. Use scoped imperative subjects such as `docs: record S02-T02 smoke result`. Never commit generated binaries, logs or Ghidra databases.

<!-- imported-from: codex:project:instructions -->
# Repository Guidelines

## Project Structure & Module Organization

This is an investigation workspace for a two-player Anomaly mod. Start with `README.md` and `docs/engine-investigation.md`; design decisions are in `docs/architecture.md`, `docs/multiplayer-feasibility.md`, and `docs/implementation-roadmap.md`. `reverse-engineering/` holds evidence and a persistent Ghidra project. `tools/` contains Ghidra scripts. `engine/xray-monolith/` is an ignored, pinned reference checkout with its own Git repository. `builds/` is reserved for outputs. Create `client/`, `server/`, `protocol/`, and `tests/` when implementation begins; no project gameplay code or tests exist yet.

## Build, Test & Development Commands

The first unmodified Windows DX11 build succeeded using `tools/Build-Stage1Client.ps1 -Run build-1`; see `docs/validation/S01-T03.md` and the pinned toolchain manifest. Choose a new run name for later builds, and check for a running test client before replacing its executable. Stage 1/G01 is accepted for development with the two user-approved fixture waivers in `docs/validation/G01.md`. Use `builds/build_1/engine-work` for Stage 2; preserve user changes in the reference checkout. S02-T01 and S02-T02 are complete: Core and Services smokes pass; real game-state/ALife construction moved to S02-T05/T06; next work is S02-T03/T04; see `docs/session-handoff.md`. Do not describe an unrun build or gameplay test as working. Useful current checks are `git status --short` for workspace changes, `git diff --check` for whitespace errors in tracked changes, and `python3 -m json.tool reverse-engineering/persistence-verification.json` for JSON syntax. Keep any future build, test, runtime, and Ghidra output inside this workspace.

## Coding Style & Naming Conventions

Follow the surrounding Monolith C++ style when changing engine code; avoid unrelated formatting. The existing Ghidra Java scripts use four-space indentation. Use descriptive Markdown filenames such as `docs/state-ownership.md` and record the source revision, file, function, and line range behind technical claims. No formatter or linter is configured for project-owned code yet; document one before enforcing it.

## Testing Guidelines

No test framework or coverage threshold is established. Add focused tests under `tests/` with each implementation milestone. Report exact build/test commands, observed results, and remaining limits in `docs/implementation-roadmap.md`. Treat a source inspection or successful Ghidra analysis as evidence, not a gameplay test.

## Commit & Pull Request Guidelines

This repository has no commits, so there is no existing commit-message convention. Use short imperative subjects with a scope, such as `docs: record actor singleton evidence`. Pull requests should describe the behavior or finding, cite the source revision and evidence, list validation performed, and identify unresolved risks. Link an issue when one exists.

## Workspace Safety & Analysis

The original installation at `/mnt/c/Users/djb20/StalkerAnom` is strictly read-only. Never run it or write logs, saves, patches, or modified binaries there. Preserve `reverse-engineering/ghidra/Anomaly153-c857b6f49d32`; verify a binary hash before reuse and never overwrite or delete that database. Generated binaries, archives, logs, temporary files, and Ghidra databases stay out of Git. The user authorized execution of plan.md and Stage 1 build/runtime repair. Follow its stage gates; this does not authorize skipping baseline acceptance or modifying the original installation.

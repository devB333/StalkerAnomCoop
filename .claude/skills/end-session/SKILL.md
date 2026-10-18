---
name: end-session
description: End-of-session handoff per plan.md §G — snapshot the old handoff, write a new docs/handoffs/current/session-handoff.md, and sync plan.md status, then commit the milestone. Use when wrapping up a work session or completing a task.
---

1. Read `plan.md` §G (Persistent session handoff) and §I (Progress tracking), and the current `docs/handoffs/current/session-handoff.md`.
2. If any engine-work changes haven't been exported yet, run `/export-patches` first.
3. Copy the current handoff to `docs/handoffs/done/snapshot-before-<what-comes-next>.md`, following the existing naming pattern, and add `## Complete` as its first line. Fix relative links in the snapshot, since it lives in `done/` rather than `current/`. `docs/handoffs/current/` must contain only `session-handoff.md`.
4. Rewrite `docs/handoffs/current/session-handoff.md` so it stays short. It should cover: the current task and status, what was observed (with links to validation reports and run names), what is still unverified, and the exact next commands.
5. Update the "Roadmap at a glance" table and §I in `plan.md` so they match the handoff. Use only the statuses `Not Started`, `In Progress`, `Blocked` or `Completed`. Don't mark anything `Completed` unless its gate evidence exists.
6. Run the checks:
   - `git diff --check` on tracked changes
   - `python3 -m json.tool` on each changed JSON file
7. Commit with a scoped imperative subject such as `docs: record S02-T02 handoff`. Stage only source and docs, never binaries, logs or Ghidra data.

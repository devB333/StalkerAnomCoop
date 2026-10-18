# Engine source ownership

The root repository owns `docs/source-manifest.json`, this file, `series`, and
the ordered patches. The ignored reference checkout is immutable investigation
evidence. `0001-campaign-server-startup.patch` adds the S02-T02 campaign role,
separate executable name, core/game-factory smoke path and conditional teardown.
`0002-campaign-core-smoke.patch` adds command-only console initialization,
persistent stage checks and explicit Core/GameState smoke scopes. GameState
reports the missing native level prerequisite; it is not a runtime pass.
`0003-campaign-services-smoke.patch` adds the server-owned CPU campaign services
(spatial DBs, script engine, character registries) with reverse-order owner
teardown, a Services smoke scope and a test-only `-campaign-fail-at` unwind check.
Its xrGame functions are additive; the client's game-globals functions are unchanged.
`0004-materials-without-media.patch` (S02-T03a) adds a material-library switch
that keeps pair media names but creates no pair sounds or wallmarks; it defaults
on, so the client is unchanged. The campaign server loads materials with it off.
Export later patches with `python3 tools/export-engine-patch.py <name> <check-number>`.

Reconstruct in a new, absent workspace directory (never overwrite an existing tree):

```sh
git clone --no-hardlinks engine/xray-monolith builds/engine-work
git -C builds/engine-work checkout --detach c57fe5b9fab4fe0506659db03137c52d2500eb2f
git -C builds/engine-work submodule update --init --recursive
```

Verify HEAD and submodule SHAs against the manifest before building. The submodule
download requires network access. This procedure was executed on 2026-10-15:
the independent working checkout began clean at the pinned revision and Optick
was initialized at its pinned gitlink. Stage 2 source edits are now exported by
the patch series. The reference checkout remains unchanged.
The source commit also pins bundled SDK inputs; record hashes of externally
installed compiler/SDK/dependency inputs when S01-T01 resolves them.

Apply patches in `series` order with `git apply --check`, then `git apply`, from
the working tree. Use relative patch filenames, one per line. Before ending each
editing session, export reviewed source changes as ordered text patches here,
including newly created files; verify reconstruction in a fresh scratch clone.
Never rely on ignored working-tree edits as the only copy. Update the manifest
and task report with patch names and SHA-256 hashes in the same root change.
Do not export proprietary assets or generated outputs. Revert by reconstructing
the pinned source with only the preceding patches in a new directory, preserving
the old tree until its changes are accounted for.

Root commit identity plus source manifest plus ordered patches defines project
source identity. Root currently has no commits; these artifacts are pending
version control, not a published or committed baseline.

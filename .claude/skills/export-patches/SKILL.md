---
name: export-patches
description: Export engine-work source changes into the ordered patch series in patches/engine/, verify they reconstruct the tree in a fresh scratch clone, and update hashes in docs/source-manifest.json. Use after reviewed engine edits and before ending a session.
---

Follow `patches/engine/README.md`. In short:

1. Review `git -C builds/build_1/engine-work status --short` and `git diff --stat`. Include new files using `git add -N` or the equivalent. Don't export generated outputs or proprietary assets.
2. Decide whether the change goes into a new numbered patch (`000N-<topic>.patch`) or replaces the latest one in its own series. Write the patches to `patches/engine/` and list each new file in `series`. Never edit `engine/xray-monolith`.
3. Verify in a **new, absent** directory such as `builds/stage2/patch-check-0N`, and never overwrite an existing tree:
   ```sh
   git clone --no-hardlinks engine/xray-monolith builds/stage2/patch-check-0N
   git -C builds/stage2/patch-check-0N checkout --detach c57fe5b9fab4fe0506659db03137c52d2500eb2f
   ```
   Apply each patch in `series` order with `git apply --check` and then `git apply`. Confirm the result matches engine-work using `git diff --no-index --stat` on the changed files.
4. Update the patch names and SHA-256s (`sha256sum patches/engine/*.patch`) in `docs/source-manifest.json`, then validate it with `python3 -m json.tool docs/source-manifest.json`.
5. Record the patch names, hashes and verification directory in the task's validation report.

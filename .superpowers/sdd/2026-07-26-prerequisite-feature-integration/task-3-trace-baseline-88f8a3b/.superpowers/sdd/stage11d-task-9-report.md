# Stage 11-D Task 9 Report: Formal Raylib Loot Evidence

Task 9 adds a production-path formal raylib evidence suite for the Stage 11-D ground-loot filter. No commit was created.

## Test-first RED

The four formal tests were registered before their harness/validator/guard files existed:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11d\.loot_(formal|evidence_validator|evidence_guard|evidence_guard_self_test)$" --output-on-failure
```

Initial result: `0/4`; the absent formal target and scripts produced the intended RED.

## Implementation

- Added exactly six `Stage11DLootValidationScenario` values plus `none`.
- The formal harness drives the production `RaylibHost` through physical input bindings, real dungeon/session state, persisted settings draft/apply/cancel transactions, the production combat renderer, and the single post-`EndDrawing()` screenshot helper.
- `CombatRenderer::draw()` now returns the exact `GroundLootView` consumed by both the room labels and HUD. The host uses this shared production view only for post-present semantic evidence; it does not inject or rebuild a second view.
- Added deterministic production-path automation for three ordinary-loot rarities, rare-only abyss claim, settings preview/cancel rollback, and automatic-pickup receipt.
- Added six 1280x720 formal scenarios: `show-all`, `magic-plus`, `rare-only`, `rare-abyss`, `preview-cancel`, and `pickup-feedback`.
- Added a PowerShell semantic validator that checks freshness, dimensions, unique PNG hashes, meaningful label/HUD pixels, exact production item IDs, filtering without inventory transfer, abyss visibility/claim ownership, preview rollback, and confirmed pickup notice text.
- Added a fail-closed evidence guard and a 21-mutation structural self-test: seven formal mutations (including malicious root, reparse escape, and recursive deletion), eight host-seam mutations (`TestAccess`, snapshot override, direct request/complete/publish pickup, committed/live settings writes, and direct `result=pass`), plus six snapshot/capture/renderer/validator mutations.
- Updated the existing renderer-integration guard to accept the new return type while requiring one shared-view return. Its self-test still rejects four divergent mutations and accepts two semantics-preserving formatting/rename variants.
- The guard now isolates 15 named Stage11D-only host regions and rejects any `stage11d` code outside them. Zero-count host bypass tokens are also rejected across the complete host, while the two legitimate production `live_settings.loot_filter_mode` rollback writes must retain their canonical form and exact count.
- Formal cleanup no longer recursively deletes a CLI-supplied directory. It accepts only a dedicated `stage11d loot evidence` root under build/temp, rejects filesystem roots, source roots, symlink/reparse components and canonical path escapes, and deletes only named evidence, command, save-slot, and settings-slot files.
- Added `stage11d.loot_root_safety` on the independent `stage11d-root-safety/stage11d loot evidence` directory. It creates an unknown eight-byte sentinel and a known evidence file, resets the root, proves the sentinel survives and the known file is removed, and still rejects a malicious source-root argument. `stage11d.loot_formal` is registered with `RUN_SERIAL TRUE` so one CTest process cannot race other foreground raylib suites.

## Fresh evidence

Committed evidence directory:

```text
docs/validation/evidence/stage11d/
```

It contains six fresh PNGs, six semantic summaries, and `stage11d-loot-evidence.txt`. Latest deterministic roots are ordinary root `62`, room `979`, depth `1`, and abyss root `1`, room `6`. The manifest records six distinct hashes and `result=pass`:

```text
show-all_hash=3906286069499726852
magic-plus_hash=15567429798435001534
rare-only_hash=1222560037361426616
rare-abyss_hash=5381394698943442220
preview-cancel_hash=10701433826634318442
pickup-feedback_hash=4207614990933268893
```

## Final GREEN

Formal raylib evidence, independent root safety, and semantic validation:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11d\.loot_(formal|root_safety|evidence_validator)$" --output-on-failure
```

Result: `3/3`, `0` failures, total `72.27s`, in registration order `formal -> root_safety -> validator`. The validator passing after root safety proves the independent safety reset does not delete the formal fixture.

- `stage11d.loot_formal`: `71.80s`
- `stage11d.loot_root_safety`: `0.04s`
- `stage11d.loot_evidence_validator`: `0.43s`

Final cleanup/guard chain:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^stage11d\.loot_(root_safety|evidence_guard|evidence_guard_self_test)$" --output-on-failure
```

Result: `3/3`, `0` failures, total `61.53s`.

- `stage11d.loot_root_safety`: `0.02s`
- `stage11d.loot_evidence_guard`: `2.59s`
- `stage11d.loot_evidence_guard_self_test`: `58.91s`

The repository-fixed MSVC 19.44 / Windows SDK 26100 build completed successfully. CTest JSON metadata confirmed `stage11d.loot_formal` has `RUN_SERIAL=True`, timeout `600`, and fixture setup `stage11d_loot_evidence`; root safety uses its independent directory and has no fixture dependency; the validator requires `stage11d_loot_evidence`.

Final renderer integration guard rerun: `2/2`, `0` failures, total `0.11s`.
The final verbose mutation self-test rerun passed in `56.23s` and printed `bad_mutations=21`.

Focused production, architecture, renderer, item, and settings regressions:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(dungeon\.units|platform\.units|stage11d\.architecture\.loot_boundaries|stage11d\.architecture\.loot_boundaries_self_test|items\.units|settings\.units|stage11d\.renderer_integration_guard|stage11d\.renderer_integration_guard_self_test)$" --output-on-failure
```

Result: `8/8`, `0` failures, total `312.86s`. This includes `dungeon.units` (`180.63s`), the full Stage 11-D architecture guard/self-test (`28.05s` / `100.61s`), and both renderer integration tests.

`git diff --check` passed; its only output was the repository's existing LF-to-CRLF normalization warnings. `git status --short` was inspected and no unrelated files were modified.

## Known risk

The deterministic combat/navigation automation exists only behind the formal validation scenario seam and is intentionally tailored to stable evidence seeds. Player-facing runtime behavior is unchanged when the scenario is `none`.

# Task 5 implementation report

## Status and scope

- Worktree: `E:/game/.worktrees/integration-large-square-rooms`
- Branch: `codex/integration-large-square-rooms`
- Start HEAD: `5a0e117`
- Status: all implementation/test gates GREEN; independent final review and
  commit are pending.
- Ported the approved semantics from `7b0731e`, `ca854bd`, `bfafd1e`, and
  `a3b30ff` in order, without selecting an old renderer, host, material-pack,
  registry, or validator side wholesale.
- Added the 1254x1254 authored health-potion source and published atlas cell 30
  into the 1024x1024 color/material item atlases.
- Added the potion sprite/manifest/material channel, direct world icon, pure
  opaque `生命药` label, pure opaque `生命药恢复 +N HP` feedback, deterministic
  potion/material feedback queue, and explicit U+836F (`药`) font coverage.
- Added bounded deterministic fixed-capacity labels, stable anchors, clamping,
  malformed-viewport/full-capacity diagnostics, and allocation pressure checks.
- Stage 17 Storm validation uses a valid lane, moves the player only after the
  first hit, and requires exactly three hits in C++ and PowerShell.
- Unified equipment/material/potion label layout uses one stack-only obstacle
  set of exactly 881 entries. Order is actors -> equipment -> secondary labels;
  player/active matching-generation monsters are protected, and icons remain
  visible if a label is dropped.
- The new previous/current/alpha/camera render-plan overload retains the old
  current-only wrapper. Render order remains environment -> icons -> actors ->
  labels -> HUD.

## RED evidence

- The U+836F assertion failed before the one-glyph production addition.
- Presentation tests preceded their corresponding production slices; verified
  platform totals advanced `482 -> 484 -> 487 -> 488`.
- Unified-label tests extended existing registered cases, keeping total 488.
  The real serial RED build exited 2 at
  `tests/platform/hud_host_integration_tests.cpp:347` because the required
  seven-argument `make_combat_render_plan` overload did not exist.
- The earlier missing `stdarg.h` result was an unloaded MSVC/SDK environment
  failure and was not accepted as RED.

## GREEN evidence

- Serial Debug build of `arpg_platform_tests`: exit 0.
- Direct platform executable: `488 cases, 0 failures`.
- Ground-loot pressure output:
  `builds=100000 unchanged=50000 alternating=50000 allocations=0`.
- `git diff --check`: passed.
- Focused `platform.units|stage12.item_material_asset_pipeline`: 2/2 passed in
  12.33 seconds; the direct executable still reports `488 cases, 0 failures`.
- Stage 10 formal host path: 1/1 passed in 27.02 seconds; the fresh summary
  records `depth=2`, proving the Task 5 plan's conditional validation-only
  acceleration requirement before the Stage 10 skill injection was retained.
- Stage 12 formal actual-capture run: 1/1 passed in 222.25 seconds. Its fresh
  report records `integration_result=pass`, `input_hole_regression=pass`,
  `screenshot_decode=pass`, and `result=pass`.
- Stage 12 pristine validator: passed in 9.4 seconds against that fresh run.
- Stage 12 authoritative 80-mutation self-test: passed in 188.9 seconds under
  the unchanged 240-second timeout. This includes three partial item
  degradations and four published-atlas palette-collapse negatives.
- Published item-atlas palette checks use alpha >= 96, the largest 4-connected
  component, and 4-bit RGB. Fresh palette counts are m0=376, m6=430, m9=177,
  and m10=316 against hard minima 45/72/72/64.
- Stage 17 skill-stone selection: 3/3 passed in 23.89 seconds (real raylib,
  evidence validator, and validator self-test).

## Asset evidence

| Artifact | Dimensions | SHA-256 |
| --- | ---: | --- |
| `art_source/stage12/items/health-potion-v1.png` | 1254x1254 | `AFF8F47B3AF88265F1CCC3943C7E552D8CB901653AB8845F0FBB918A27E29017` |
| `assets/stage12/items_ui.png` | 1024x1024 | `1E4123D31C8665E93E911FEB830CF4F95B4B492287C193922FF2731060BB5B50` |
| `assets/stage12/items_ui_material.png` | 1024x1024 | `537239648778BA89003A0AFF62096280151B119E16764E561749B055567755BF` |

## Reviewed exceptions

- `material_pack.cpp`: `BeginShaderMode` now precedes shader uniform setters;
  the old order left the material sampler ineffective. Same-ROI A/B evidence
  showed B increased changed pixels `954 -> 1688`, largest component
  `765 -> 1441`, bright pixels `382 -> 627`, and colors `83 -> 274`.
- Stage 12 validator checks were equivalently reordered so cheap pristine,
  report, alias, native, and PNG-header preflight runs before expensive checks.
  No threshold, required field, mutation/sample count, or timeout was reduced.
- Validator repairs include CRLF-safe ROI parsing, required replacement helpers,
  stale no-op mutation handling, and water/lightning screenshot SHA identity
  rejection. Palette quality moved from label/background-contaminated screenshot
  pixels to the published atlas's opaque connected subject, while isolated live
  screenshots gained stronger m0/m6/m9/m10 contour and chroma gates. The
  80-mutation self-test is authoritative.
- `combat_renderer.cpp/.hpp` changed for reviewed actor-aware unified labels;
  this is a presentation extension and retains the old wrapper/stages.
- Label dropping is intentional bounded presentation degradation, separately
  counted from source-capacity saturation; the world icon remains visible and
  valid eligible count equals retained plus dropped.
- `stage12_material_formal_game_validation.cpp` adds the independently named
  icon-only/baseline captures and runtime fields required to prove icons without
  loot-label contamination; actual export success remains mandatory.
- `raylib_host.cpp` retains exactly two active-skill requests inside
  `stage10_validation_input`. A fresh no-skill binary failed the Stage 12
  `input_hole_regression`; the rebuilt validation-only path passed and reached
  depth 2. Source tests prove no such request exists outside that function.
- The six-file preservation guard is intentionally non-empty after the approved
  separate reviews: `material_pack.cpp` (shader order),
  `combat_renderer.cpp/.hpp` (unified labels), and the Stage 12 formal/validator
  trio (isolated evidence plus stronger negative contracts). No old file side
  was copied wholesale.

## Pending completion gates

- PENDING: independent final review of frozen snapshot
  `8472f48ae956ef5908a64c55ec6760b8af0b2105`, including explicit acceptance of
  the protected-file exceptions and the four-file repair delta from the earlier
  snapshot `866836d7396c4dd544c1f807929853f9e2a4b2ee`.
- PENDING: final `git diff --check`, exact staging audit (including the ignored
  authored source but excluding generated `tools/__pycache__`), and commit
  exactly `feat: present high-resolution health potions`.
- No push has been performed. Task 6 has not started.

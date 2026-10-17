# Task 7 implementation report

Status: DONE_WITH_CONCERNS

Commit: `feat: localize responsive debug overlay`

## Scope delivered

- Replaced the direct F1 renderer formatting path with a fixed `DebugOverlayView`: exactly 12 `std::array<char, 192U>` lines for both combat and transition snapshots, all formatted through `ui_utf8_format()` without per-frame heap allocation.
- Added `DebugOverlayLayout`: widths below 1280 use one 12-line column; widths at or above 1280 use two 6-line columns. The 800×450, 1280×720, and 1920×1080 column backings and line boxes remain on-screen.
- Each real column is drawn first with opaque `#0B1019`. Normal, warning, and error lines use distinct Chinese status text and colors measured at or above 4.5:1 against that exact backing.
- Routed overlay and world debug text through the existing borrowed `SharedUiFont` and `draw_crisp_ui_text`; the task adds no font owner, font load, or fallback default-font path.
- Localized the F1-visible world labels to `辅助判定 / 攻击判定 / 受击判定`, monster roles through `ui_monster_name`, all monster phases, the defeat marker `已击败`, catalog-backed affix abbreviations, and `词缀Ⅰ / 词缀Ⅱ / 词缀Ⅲ`.
- Extended the single UI catalog visitor with the new dynamic glyph corpus without changing its 285-entry visitor contract. Foundation validation confirms the glyph plan remains within the 640-codepoint cap.
- Added a real Task 7 source guard and reversible self-test. It locks the fixed UTF-8 buffers, 12-line model, responsive breakpoint/rows, opaque backing, borrowed-font call chain, world label mappings, monster role/phase, defeat marker, and affix tiers. Thirteen mutants, including a syntactically valid dead shared-font branch, are rejected.
- Added the Stage11C formal-side pure contract check for the 12-line UTF-8 view and supported layouts. Per the brief, the formal executable was compiled/linked only and was not run.

## TDD evidence

- RED: after adding only the five public View/Layout behavior cases and their suite registration, the MSVC build failed because `DebugOverlayView`, `DebugOverlayLayout`, `make_debug_overlay_view`, `debug_overlay_layout`, and the world-label mapping did not exist. The failure was the intended missing-feature failure, not an environment or spelling error.
- GREEN: the minimum fixed model, layout, Chinese mappings, and borrowed-font renderer compiled. The targeted suite then reached `183 cases, 0 failures` after correcting one test expectation (`攻击生效`) and one minimal visible formatting detail (`音频不可用`).
- The five cases cover: combat/transition fixed 12-line valid UTF-8 output with old player-facing English absent; the exact responsive breakpoint and six-line split; supported viewport bounds plus real backing/normal/warning/error contrast; 10,000 allocation-free builds; and exhaustive player attack/state plus world debug/monster/affix mappings.
- Source-guard self-test RED/GREEN: the first run exposed an over-broad affix-tier guard that accepted an English M1 mutant because another fallback still contained `词缀Ⅰ`. The guard was tightened to each concrete m1/m2/m3 switch branch; the complete mutant set then passed.

## Verification

All builds and tests below ran from Visual Studio 2022 Developer PowerShell 17.14.34; builds and CTest used parallelism 2.

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2` — passed.
- `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY=1 arpg_platform_tests.exe` — 15 cases, 0 failures.
- `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1 arpg_platform_tests.exe` — 183 cases, 0 failures.
- `ctest -R ^platform.task7_debug_overlay_source_guard(_self_test)?$ --output-on-failure --parallel 2` — 2/2 passed; pristine guard and all 13 reversible mutants passed.
- `cmake --build --preset windows-msvc-debug --target arpg_stage11c_hud_formal --parallel 2` — compiled and linked successfully; executable intentionally not run.
- Full `arpg_platform_tests.exe` — 671 actual cases against expected 670, with exactly two preserved failures: `ecology_material_coverage.ecology props match emitted layout records` at line 1163 and the intentional `actual 671 != expected 670` count mismatch. No new Task 7 regression remains.

## Self-review

- The renderer owns no strings, font atlas, or dynamic container; view/layout construction is bounded and `noexcept`.
- Both transition and combat paths write all 12 non-empty valid UTF-8 lines.
- The overlay contains no `DrawText`, `DrawTextEx`, `TextFormat`, `GetFontDefault`, or font-loading path. F1 world labels consume the same borrowed font.
- Existing full-suite expectations that required English affix abbreviations and `GetFontDefault()` were updated in-place without adding cases; the full expected count is 670, preserving the pre-existing +1 drift.
- No Task 8 or `raylib_host` implementation was changed.
- `git diff --check` is clean apart from Git's existing LF-to-CRLF conversion warnings.

## Concerns

- Stage11C formal gameplay/capture and subjective Windows visual acceptance were deliberately not run under this task's explicit compile-only rule. The pure formal contract compiles, but final on-screen typography and feel remain for the later authorized Windows visual acceptance step.
- The two known broad-suite failures described above remain intentionally visible and were not absorbed into Task 7.

## Review fix round 1

Status: DONE_WITH_CONCERNS

Commit: `0179b69 fix: harden debug overlay text coverage`

### Scope delivered

- Added the real F1 phrases `挥击 / 冲击汇总 / 是 / 否` to the complete shared glyph-plan behavior test, then extended the existing single catalog visitor. The catalog entry count remains 285, and the Foundation test still enforces the 640-codepoint ceiling.
- Replaced the monster affix F1 label's `TextFormat("%s %s", ...)` path with a 64-byte stack buffer formatted by `ui_utf8_format`; `draw_scene_label` consumes that buffer directly. The unrelated ordinary damage-number `TextFormat("%d", effect.value)` path remains unchanged.
- Extended the Task 7 source guard to require that real affix-label buffer/formatter/consumer chain and reject `TextFormat` specifically inside the extracted `draw_monster_presentation` function.
- Added syntactically valid reversible mutants for both an unsafe visible affix `TextFormat` draw and `if (shared_ui_font_ready && false)` around the real overlay draw. Dead-branch detection remains scoped to extracted relevant function bodies and rejects direct `if(false/0)` plus the new `&& false` form without scanning unrelated whole files.

### TDD evidence

- C1 RED: after adding only the real glyph-plan assertion, `shared_ui_font.complete catalog coverage` failed on `ui_glyph_plan_covers_text(plan, u8"挥击 冲击汇总 是 否")` (15 cases, 1 failure). GREEN: extending the existing catalog corpus produced Foundation 15 cases, 0 failures, including the existing `<= kUiGlyphCapacity` assertion.
- I1 RED: the old guard accepted `unsafe_affix_textformat`; after adding the real function-body contract, pristine production then failed with `Task 7 affix labels must use a fixed UTF-8 stack buffer`. GREEN: the minimum stack-buffer/`ui_utf8_format` production change passed the complete self-test.
- I2 RED: the old guard accepted `compound_false_shared_font_decoy`. An initially over-broad condition-token check correctly exposed a false positive on the live `material_pack.draw(..., false, ...)` argument; root-cause narrowing to the branch-killing `&& false` operand preserved pristine source while rejecting the requested mutant. GREEN: the pristine guard passed and the complete 15-mutant self-test reported `rejected all mutations`.

### Fresh verification

All successful builds/tests below ran in Visual Studio 2022 Developer PowerShell 17.14.34 with `amd64` host/target. Builds and CTest used parallelism 2.

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2` — passed.
- `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY=1 arpg_platform_tests.exe` — 15 cases, 0 failures.
- `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1 arpg_platform_tests.exe` — 183 cases, 0 failures.
- `ctest --test-dir out/build/windows-msvc-debug -R ^platform.task7_debug_overlay_source_guard(_self_test)?$ --output-on-failure --parallel 2` — 2/2 passed; all 15 reversible mutants rejected.
- `cmake --build --preset windows-msvc-debug --target arpg_stage11c_hud_formal --parallel 2` — compiled/linked successfully; executable intentionally not run.
- Full `arpg_platform_tests.exe` — actual 671 against expected 670, with exactly the two preserved failures: ecology material coverage at `ecology_material_coverage_tests.cpp:1163` and `actual 671 != expected 670`.

### Self-review

- The tracked diff is limited to the shared catalog/test, actor affix formatter, and Task 7 guard/self-test; it does not touch Task 8 or `raylib_host`.
- The real affix draw consumes the stack buffer; no per-frame allocation or dead-code decoy was introduced.
- `rg TextFormat actor_renderer.cpp` shows only the explicitly preserved normal damage-number call.
- `git diff --check` passes; line-ending conversion notices are advisory only.

### Concerns

- Stage11C formal gameplay/capture and subjective visual acceptance remain intentionally unrun under the compile-only rule.
- The two known full-suite ecology/count failures remain visible and unchanged.

## Review fix round 2

Status: DONE_WITH_CONCERNS

Commit: `2e686c0 fix: back Chinese scene labels with contrast`

### Finding addressed

- The F1 world-volume labels in `debug_renderer.cpp` and the monster affix, role, and phase labels in `actor_renderer.cpp` drew text directly over the world. Their outline or requested color could not establish the required >=4.5:1 contrast against an actual backing, and their raw screen positions were not clamped to the viewport.

### Scope delivered

- Added allocation-free `SceneLabelStyle` and `SceneLabelLayout` pure contracts. The style owns an opaque `#0B1019` backing, forces text alpha to 255, and replaces any requested color below 4.5:1 with the established high-contrast UI foreground. The layout centers measured text inside padded backing geometry and clamps that geometry to the viewport.
- Routed all three real debug-volume labels and the real monster affix/role/phase label helper through `MeasureTextEx -> scene_label_layout -> DrawRectangleRec -> draw_crisp_ui_text`, in that order, with the existing borrowed `SharedUiFont` and real viewport dimensions.
- Kept the fixed `ui_utf8_format` affix buffer from round 1; no heap container, font owner, default-font path, launcher, pause menu, or inventory work was added.
- Strengthened the Task 7 extracted-function source guard and reversible self-test for backing presence/order, viewport consumption, shared-font rendering, default-font rejection, clamp-helper bypasses, and dead backing decoys. The full set is now 23 mutants.

### TDD evidence

- RED: after adding only the pure behavior tests and renderer guard mutations, the first MSVC platform build failed at `debug_overlay_renderer_tests.cpp:165` with `C2039: SceneLabelStyle is not a member of arpg::platform`; subsequent diagnostics were the expected missing `scene_label_style`, `SceneLabelLayout`, and `scene_label_layout` symbols.
- GREEN: the minimum pure style/layout implementation plus the two real renderer integrations compiled and linked `arpg_platform_tests.exe` successfully.
- Behavior coverage stays inside the existing suite count: 800x450, 1280x720, and 1920x1080 each exercise all four viewport corners and the center; backing bounds and text centering are asserted; the three volume colors, role/phase colors, and all 12 concrete affix colors are checked against the actual opaque backing; 10,000 helper calls remain allocation-free.
- Guard refinement: the first focused guard run exposed only a test-token line-break assumption around `MeasureTextEx`. The token was narrowed to the real same-line parameter sequence; pristine guard and all 23 syntactically valid reversible mutants then passed.

### Fresh verification

- MSVC x64 `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2` — passed.
- `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY=1 arpg_platform_tests.exe` — 15 cases, 0 failures.
- Exact Task 7 CTest regex `^platform.task7_debug_overlay_source_guard(_self_test)?$` with `--parallel 2` — 2/2 passed; self-test rejected all 23 mutants.
- On the same shared working tree, control-agent verification reported MENU_HUD 183 cases, 0 failures, and full platform 671 cases with exactly the two existing failures: ecology material coverage at line 1163 and the preserved `actual 671 != expected 670` count mismatch.

### Self-review

- The pure helper performs only bounded arithmetic and returns fixed aggregates; the 10,000-iteration allocation probe covers both style and layout.
- Both renderer functions draw the opaque backing before the borrowed-font text and consume the layout's clamped `text_position`; no default font or dead-code decoy satisfies the guard.
- The tracked Task 7 scope is seven files and excludes the concurrent Task 8 plan/launcher work plus pause/selector/count edits.

### Concerns

- This round did not run Stage11C gameplay/capture or subjective visual acceptance. The task retains its compile-only/automated evidence boundary.
- The two known broad-suite ecology/count failures remain visible and unchanged.

## Review fix round 3

Status: DONE_WITH_CONCERNS

Commit: `7b0e71e fix: preserve scene label anchors`

### Round 2 review findings

- I1 Important (addressed): `scene_label_layout` interpreted its `anchor` as the backing center, but both real renderer call paths already supplied the desired text top-left. This shifted un-clamped labels left/up by half their measured size and made the lane-0 role backing overlap the resource bar drawn immediately before it.
- M1 Minor (retained): text wider than the entire viewport cannot be made fully visible without clipping/truncation or a wider API contract. Every fixed Task 7 scene label is far narrower than the minimum supported 800-pixel viewport, so this does not block the requested fixed-label behavior. No per-frame allocation or speculative API was added.

### TDD evidence

- RED: the existing real layout behavior test was changed first so its four-corner inputs represent desired text top-left positions at 800x450, 1280x720, and 1920x1080. It also locks un-clamped center preservation, the real debug `(+4,-14)` text offset, text centering after backing clamp, and lane-0 role-backing separation from the real `resource_bar_offset_y = -128` five-pixel bar geometry.
- The first MSVC build completed, then the MENU_HUD selector failed exactly one of 183 cases at `debug_overlay_renderer_tests.cpp:204`: the old center-anchor implementation did not clamp the right-corner backing to the viewport edge. Production was unchanged until this RED was observed.
- GREEN: the minimum implementation changed the backing's desired origin from `anchor - backing/2` to `desired text top-left - padding`; the existing viewport clamp and backing-relative text centering remain unchanged. The real callers therefore retain their established coordinates while the backing surrounds the text without covering the lane-0 resource bar.

### Fresh verification

- MSVC x64 `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2` — passed after the minimum production change.
- `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1 arpg_platform_tests.exe` — 183 cases, 0 failures.
- Exact Task 7 CTest regex `^platform.task7_debug_overlay_source_guard(_self_test)?$` with `--parallel 2` — 2/2 passed; static count confirms all 23 reversible mutants ran through the self-test.
- `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY=1 arpg_platform_tests.exe` — 15 cases, 0 failures.
- No round-3 full-suite or Stage11C formal command was run under the control-agent's scoped verification gate. Round 2's shared full result remains actual 671 / expected 670 with only the known ecology and count failures.

### Self-review and concerns

- The regression test exercises the real allocation-free layout helper and the real presentation-plan offset; it does not substitute a source-string existence check for geometry. The existing source self-test continues binding the debug call path and rejecting bypass/dead-decoy mutations.
- The tracked round-3 diff is limited to `combat_view_math.cpp` and `debug_overlay_renderer_tests.cpp`; concurrent plan, pause, selector, and platform-main changes remain outside Task 7.
- M1 remains a documented Minor for hypothetical text wider than the viewport. The fixed Chinese scene labels in scope remain covered at the 800-pixel minimum without heap allocation.

## Review fix round 4

Status: DONE_WITH_CONCERNS

Commit: `fix: separate scaled scene label plates`

### Findings addressed

- I1 Important (addressed): round 3 checked lane-0 separation only at projection scale 1.0. The real projection range is 0.70 through 1.0, where the fixed five-pixel resource bar outgrew the scaled eight-pixel nominal offset and overlapped the role-label backing by up to 2.4 pixels.
- Same-scope layout defect (addressed): role, phase, and affix text advanced by only `font_size + 2` while every opaque plate added three pixels of padding above and below. Consecutive plates therefore overlapped by four pixels even when their text baselines remained ordered.

### TDD evidence

- Bar/role RED: the existing real geometry case was first expanded to projection scales 0.70, 0.85, and 1.0, using the real `MonsterPresentationPlan` resource offset, scaled 54-pixel width, five-pixel bar height, and actual scene-label padding. The build succeeded and MENU_HUD failed exactly one of 183 cases at the lane-0 overlap assertion (`debug_overlay_renderer_tests.cpp:258`).
- Bar/role GREEN: `monster_role_label_anchor()` now derives the desired text top-left from the actual primary resource-bar rectangle, its bottom edge, the real label padding, a one-pixel gap, and the unchanged lane offset. The renderer passes that same rectangle to the bar draw path, so bar height is no longer duplicated between drawing and label placement. MENU_HUD then passed 183/183.
- Plate-stack RED: before changing the stack step, the test constructed role, phase, and two affix layouts from their real font sizes and scene padding at all three projection scales. MENU_HUD again failed exactly one of 183 cases, now at the first adjacent-plate overlap assertion (`debug_overlay_renderer_tests.cpp:308`).
- Plate-stack GREEN: `next_stacked_scene_label_anchor()` advances by previous text height, previous bottom padding, one pixel, and next top padding. The actor caller uses it successively for role to phase and every affix, preserving lane offsets and label order. The final build and MENU_HUD run passed 183/183.

### Fresh verification

- MSVC x64 `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2` — passed.
- `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1 arpg_platform_tests.exe` — 183 cases, 0 failures.
- `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY=1 arpg_platform_tests.exe` — 15 cases, 0 failures.
- Exact Task 7 CTest regex `^platform.task7_debug_overlay_source_guard(_self_test)?$` — 2/2 passed; the self-test contains and rejected all 23 reversible mutants.
- Full `arpg_platform_tests.exe` — actual 671 against expected 670, with exactly the two preserved failures: ecology material coverage at `ecology_material_coverage_tests.cpp:1163` and `actual 671 != expected 670`.

### Self-review and concerns

- Both new helpers are fixed-arithmetic `noexcept` paths; the existing 10,000-iteration allocation probe now executes both before constructing the clamped layout and remains green.
- Role, phase, and affix text still enter `scene_label_layout()` as desired top-left anchors. Viewport clamp, shared-font drawing, Chinese mappings, label ordering, and the four-lane vertical offset remain unchanged.
- The tracked production/test scope is limited to actor scene-label geometry and its pure math/test contracts. Concurrent completion-gate plan, pause-boundary, and platform-main edits remain outside this round.
- M1 for hypothetical text wider than the viewport remains the accepted non-blocking Minor; no clipping or dynamic text fitting API was added.

## Review fix round 5 and controller adjudication

Status: DONE

Commits: `6c28bca fix: lay out monster labels as one stack` plus the controller follow-up commit recorded in the progress ledger.

### Findings addressed

- The renderer now measures only the labels actually visible in the current mode, lays the fixed-capacity sequence out once, applies the lane offset to the whole stack, checks every visible resource bar plus conservative actor bounds, and tries above, below, right, then left before failing closed.
- Final independent review found two remaining specification gaps. Non-finite entry, padding, actor, bar, or projection values could be normalized and displayed, and the behavior matrix used top-center instead of the real center while leaving candidate order and no-safe-position unasserted. Both findings were accepted and fixed directly by the controller rather than opening an additional automatic review round.

### TDD and fresh verification

- RED: after adding NaN/Inf fail-closed assertions to the existing real layout case, MENU_HUD reported exactly `183 cases, 1 failures` at the first malformed text-size assertion.
- GREEN: the layout entrance now rejects over-capacity counts, non-finite/non-positive scale, and non-finite or negative active geometry before calculating candidates. MENU_HUD then passed `183 cases, 0 failures`.
- The matrix now contains the actual 800x450 center plus four corners. Deterministic fixtures assert above, below, right, left order and `visible=false/plate_count=0` when no candidate is safe.
- The architecture guard now locks exact candidate order and the single post-collision visibility publication. Two new reversible mutants (candidate reorder and fail-open after the loop) are rejected; Task 7 guard/self-test passed 2/2, with self-test time 285.35 seconds.
- Foundation passed 15/15. Full platform remained 671/670 with only the known ecology alpha-bounds failure and the known +1 count mismatch.

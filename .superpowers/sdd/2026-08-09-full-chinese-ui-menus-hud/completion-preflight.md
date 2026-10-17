# Full Chinese Menus/HUD — completion preflight

Date: 2026-08-10  
Mode: read-only preflight. No tracked file was edited; no configure/build/test/commit command was run.

## Snapshot

- Plan base is `567fe77`; inspected `HEAD` is `86ebaa7`.
- The only dirty tracked file is `tests/launcher/launcher_window_smoke_test.ps1` (`124` added / `15` removed), belonging to the separate Task 8 smoke-guard fix round. This report does not assess that uncommitted mutation as a completed result.
- The plan ledger still says `Task 8: fix round 1/5 (0 addressed, 2 open)`. Therefore the plan is not ready for a completion claim even before the gate defects below are considered.

## 1. `platform.pause_menu_state_boundary`: real failure and minimal repair

### Why it passes at plan base and fails now

At `567fe77`, `pause_menu_state.cpp` only includes `pause_menu_state.hpp` plus the standard headers permitted by `pause_menu_state_boundary_test.cmake`; the guard is therefore compatible with the base source.

Task 1's pagination enum/state changes do not introduce an extra include. Task 3 added the legitimate static-catalogue dependency:

```cpp
#include "ui_text_catalog.hpp"
```

in `src/platform/raylib/pause_menu_state.cpp:3`, so the state machine can retain `ui_text()` static pointers as required by Task 3. The boundary guard remains byte-for-byte the base version and its allowlist accepts only `pause_menu_state.hpp`, `platform/settings/settings_types.hpp`, and five standard headers. It consequently reports this allowed UI dependency as an unapproved preprocessor line.

This is a stale guard, not a Task 8 regression: Task 8's report correctly established no diff in either the state source or boundary guard from Task 8 base `0179b69`. But relative to the full menus/HUD plan it was introduced by Task 3 and must be fixed before the plan completion gate can be green.

### Smallest correct ownership boundary and TDD

Change only `tests/platform/pause_menu_state_boundary_test.cmake`:

1. Add a self-test case that treats the exact literal include `#include "ui_text_catalog.hpp"` as allowed. With the current regex this is a RED false-positive in the guard self-test.
2. Extend only the exact quoted-header alternative in `arpg_pause_preprocessor_is_allowed()` to permit `ui_text_catalog.hpp`; keep every other quoted header forbidden.
3. Re-run the single CTest `platform.pause_menu_state_boundary`; then run the plan's three-guard command. No production refactor, no broad `platform/raylib/*` allowance, and no removal of the `ui_text_catalog` include is warranted.

This preserves the original architectural intent: no raylib/device/filesystem/combat/dungeon/storage dependency in the pure state machine, while explicitly allowing the already-required static UI catalogue.

## 2. Completion gate coverage is incomplete

The written completion command runs the `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY` selector (183 cases) plus only:

- `platform.pause_menu_state_boundary`
- `platform.pause_menu_host_layout_source`
- `platform.hole_prompt_binding_source`

The selector includes `pause_menu_state`, `pause_menu_view`, `control_hints`, `hud_view_model`, `hud_notice_state`, `active_skill_view`, `dungeon_view_math`, `hud_layout`, `debug_overlay_renderer`, `hud_font`, `hud_render_plan`, and `hud_host_integration` suites. It does **not** include `host_input_suite`, `host_settings_runtime_suite`, `dungeon_runtime_suite`, or `host_window_lifetime_suite`; launcher tests are a separate target entirely.

| Task | Covered by current selector/three guards | Missing from completion proof |
|---|---|---|
| 1 pagination | pause state suite; boundary guard | boundary currently stale/fails |
| 2 menu layout | pause view suite; host-layout guard | none material, but no guard self-test in gate |
| 3 settings messages/key labels | pause-state portion | `host_settings_runtime` and `host_input` suites |
| 4 structured HUD | main model suites | `hud_notice_structured_binding_source` and `hud_renderer_structured_model_source` guards/self-tests |
| 5 room/abyss/hole | dungeon/HUD suites; hole guard | hole guard self-test |
| 6 recovery page | partial host/HUD integration only | `dungeon_runtime` recovery cases and `platform.task6_recovery_screen_source_guard`/self-test |
| 7 F1 overlay | debug-overlay unit suite | `platform.task7_debug_overlay_source_guard`/self-test and declared formal compile/link boundary |
| 8 game title/launcher | none in the selector | `host_window_lifetime`, launcher units, Chinese-copy guard, and `launcher.window_smoke` |

Thus the stated expectation “all Task 1–8 player paths are proven” cannot be met by the current gate. At minimum, the final reviewer must separately require the omitted suites and Task 6/7/8 guards; ideally the gate should enumerate them instead of relying on historical task reports.

## 3. Static player-path audit from `567fe77` to current HEAD

### Confirmed static positives

- The default host title is now `u8"无限地城"` in `raylib_host.hpp:207`; `HostWindowLifetime` forwards `config.window_title` to `InitWindow`.
- Task 5 room/hole/abyss path measures shared-CJK text, draws the opaque prompt backing before text, and uses the structured interaction binding. The existing hole guard has concrete checks for those tokens.
- Task 6 recovery rendering uses a valid shared font, a centered panel and measured centered strings, with its model assigning the contrast style backing/text colours.
- Task 7 overlay uses an opaque `#0B1019` column backing before SharedUiFont text; its unit source contains explicit 4.5:1 checks for overlay-line colours.
- The changed plan-visible sources contain no newly authored, untranslated player English. Remaining ASCII is either physical keys (`Enter`, `Esc`, `F1`, `J1`, `A/B`), numeric formatting, technical diagnostics/logging, immutable names, or the Task 8 launcher allowlisted technical details (`LOCALAPPDATA`, API-stage names, `HRESULT`). `hud_renderer.cpp`'s `FIRE/WATER/LIGHTNING/CHAOS/UNKNOWN` helper has no call site in the production tree, so it is dead residue rather than an active HUD route.

### Static risks requiring reviewer disposition

1. **World-debug labels lack an actual backing/contrast contract.** `debug_renderer.cpp` draws F1 world-volume labels with scene-dependent colour directly over the world (`draw_projected_aabb`); `actor_renderer.cpp` likewise draws role/phase/affix scene labels without a dedicated backing. Task 7's guard proves SharedUiFont/no-default-font and Chinese mapping, but not a backing or 4.5:1 contrast for these labels. If the global “正文与实际背板 >= 4.5:1” constraint applies to F1 world labels, this is an unmet static requirement. Either add a small opaque backing plus contrast calculation/test, or record a narrowly justified debug-world-label exception before completion.
2. **Legacy default-font fallbacks remain in callable render APIs.** `PauseMenuRenderer::draw()` falls back to `GetFontDefault()` when `font_` is invalid, and exported `draw_pause_menu()` always calls the default font. `HudRenderer` has equivalent fallback branches. The normal host path initializes `SharedUiFont` first and passes it to both renderers, so this is not evidence of a normal bootstrap failure; however, neither the completion gate nor a source guard prohibits an alternate caller from rendering Chinese through the fallback. Treat it as a resilience/ownership risk, not a confirmed normal-player-path defect.
3. **No visual run was performed in this preflight.** Static containment/contrast evidence cannot replace the required Windows on-screen acceptance for menu pagination, recovery screen, F1 overlay and launcher copy.

No further active English, forced left alignment, or missing backing was found in the main pause/recovery/room/overlay paths inspected. The caveats above are the remaining static boundary risks, not green visual evidence.

## 4. Ecology failure and case-count mismatch are independent baseline defects

The evidence supports treating both as pre-plan independent baselines, subject to a fresh runtime confirmation later:

- `task-1-report.md` records **before Task 1**: full platform `actual 655 / expected 654` and the same `ecology_material_coverage` `alpha_bounds.x` failure. Task 1 then added exactly two cases and changed expected only `654 -> 656`, preserving the `+1` mismatch.
- Current expected is `670`; the task additions recorded in reports total `+16` (`2 + 2 + 0 + 1 + 5 + 1 + 5 + 0`), exactly transforming the old `654` expected baseline to `670` while preserving actual `expected + 1` (`655 -> 671`).
- `tests/platform/ecology_material_coverage_tests.cpp` has the same blob ID at `567fe77` and current HEAD, and has no commit after plan base. The plan did not alter its assertion/source.

Therefore neither defect should be “fixed” by silently changing expected counts or weakening the ecology assertion during this plan. They do still prevent a globally green platform run; final evidence must state them explicitly and prove any observed failure set contains no third/new plan regression.

## Final high-risk list for whole-plan review

1. Repair the stale pause-state boundary allowlist by TDD; otherwise the declared completion command is deterministically red.
2. Do not accept the three-guard/MENU_HUD gate as Task 1–8 coverage. Add or separately execute the omitted Task 3/6/7/8 suites and guards, including launcher verification.
3. Resolve/document the F1 world-label backing/contrast exception; current code proves font choice but not the global actual-backing contrast rule.
4. Decide whether callable default-font fallbacks are an allowed compatibility seam. If not, remove/disable them or add a fail-closed source guard; normal host initialization alone is weaker than an API invariant.
5. Wait for the separate Task 8 smoke-guard change to be finalized and independently reviewed; it is currently the sole dirty tracked file.
6. Preserve the ecology and `actual=expected+1` failures as named baseline constraints, then perform a fresh final run and Windows visual acceptance before declaring completion.

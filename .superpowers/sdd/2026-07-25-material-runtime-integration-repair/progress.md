# SDD ledger — plan: docs/superpowers/plans/2026-07-25-material-runtime-integration-repair.md
Branch: codex/material-runtime-integration
Plan commit: c5fe1bc
Merge base: 44f2e590413b9e17ddbf915a09b6acd9cac511fb
Task 1: dispatched (base c5fe1bc; brief task-1-brief.md; report task-1-report.md)
Task 1: clarification — `room_renderer.cpp` authorized because Step 5 explicitly requires `draw_room_background_only()`; Files list omission corrected for execution.
Task 1: complete (commits c5fe1bc..71d1e86, review clean)
Task 2: dispatched (base 71d1e86; brief task-2-brief.md; report task-2-report.md)
Task 2: implementer replaced after two no-progress intervals; replacement continues from preserved RED tests with the same base and scope.
Task 2: clarification — `tests/platform/room_background_render_plan_tests.cpp` authorized only to update `MaterialAtlasId::count` from 26 to 27; required by the new `element_doors` enum and omitted from the Files list.
Task 2: clarification — `tests/platform/material_asset_validation_tests.cpp` authorized only to add 2,097,152 bytes for the common door color/material pair to four exact expected totals; the brief explicitly requires extending the budget test, but the file was omitted.
Task 2: review round 1 — changes requested (1 Critical, 3 Important); see task-2-review-1.md.
Task 2: fix round 1 committed (28f7229); scoped rereview package review-0429d6b..28f7229.diff.
Task 2: review round 2 — one Important remains (invalid canonical state must fail closed); one Minor ledgered; see task-2-review-2.md.
Task 2: fix round 2 committed (e317569); scoped rereview package review-28f7229..e317569.diff.
Task 2: complete (commits 71d1e86..e317569; final review Approved; Minor ledger: C++ per-ecology name uniqueness is redundantly covered by Python).
Task 3: dispatched (base e317569; brief task-3-brief.md; report task-3-report.md)
Task 3: clarification — `src/platform/raylib/material_pack.cpp` authorized only to route the four `environment_door_*` sprites to `element_doors`; required by the Task 3 healthy-common-atlas contract and omitted from Files.
Task 3: complete (commits e317569..9b35f07; review clean).
Task 4: dispatched (base 9b35f07; brief task-4-brief.md; report task-4-report.md)
Task 4: review round 1 — changes requested (2 Important); one flip-anchor Minor ledgered; see task-4-review-1.md.
Task 4: clarification — `src/platform/raylib/material_residency.cpp` authorized only to scan every fixed monster slot and filter `active`; required by Task 4 sparse cross-ecology healthy-path contract.
Task 4: clarification — approved a minimal pure material/legacy/silhouette draw-decision seam in listed material-animation files, consumed by actor_renderer and the existing third case; no new case or file.
Task 4: fix round 1 committed (cbd5a47); scoped rereview package review-a50d25e..cbd5a47.diff.
Task 4: complete (commits 9b35f07..cbd5a47; final review Approved; Minor ledger: mirror non-centered explicit foot anchors on flip_x).
Task 5: dispatched (base cbd5a47; brief task-5-brief.md; report task-5-report.md)
Task 5: clarification — `tests/platform/room_background_render_plan_tests.cpp` authorized only to update `MaterialAtlasId::count` from 27 to 29 for the two new skill atlases; omitted from Files.
Task 5: clarification — `src/platform/raylib/material_asset_validation.cpp` authorized only to include both skill atlases in `legal_worst_residency_request()` and expose the real 226,800,928-byte peak; required by the final legal-budget contract and omitted from Files.
Task 5: complete (commits cbd5a47..7652568; review clean; fresh serial verification clean).
Task 6: dispatched (base 7652568; brief task-6-brief.md; report task-6-report.md)
Task 6: review round 1 — changes requested (1 Important); see task-6-review-1.md.
Task 6: fix round 1 committed (28c6f56); scoped rereview package review-ca49689..28c6f56.diff.
Task 6: complete (commits 7652568..28c6f56; final review clean).
Task 7: dispatched (base 28c6f56; brief task-7-brief.md; report task-7-report.md)
Task 7: review round 1 — changes requested (1 Important); exact-scale assertion Minor ledgered; see task-7-review-1.md.
Task 7: fix round 1 committed (903b6b8); scoped rereview package review-756e800..903b6b8.diff.
Task 7: complete (commits 28c6f56..903b6b8; final review Approved; Minor ledger: recommended prop scales are not asserted exactly).
Task 4: complete
Task 5: complete
Task 6: complete
Task 7: complete
Task 8: pending
Task 8: dispatched (base 903b6b8; brief task-8-brief.md; report task-8-report.md)
Task 8: clarification — `src/platform/raylib/ui_text_renderer.hpp/.cpp` authorized only to add a per-call shadow-pixel override to `draw_crisp_ui_text`; its default remains the existing 2 px, while Task 8 damage/DEFEATED paths pass the required physical 1 px. Global HUD contrast behavior must not change.
Task 8: implementation committed (0296846); review package review-903b6b8..02968463.diff dispatched to a fresh reviewer.
Task 8: review round 1 — changes requested (1 Important: a valid passed HUD font must not fall back merely because `hud_font_ready` is false); see task-8-review-1.md.
Task 8: fix round 1 committed (891826d); scoped rereview package review-02968463..891826d8.diff.
Task 8: complete (commits 903b6b8..891826d; final review Approved).
Task 9: pending
Task 9: dispatched (base 891826d; brief task-9-brief.md; report task-9-report.md)
Task 9: clarification — `tests/platform/host_input_tests.cpp` authorized only to change the obsolete main-number pressed query expectation from 2 to 1 and assert each KP1..KP5 query occurs exactly once. Production must not retain redundant discarded main-number queries.
Task 9: implementation committed (236b5ce); review package review-891826d8..236b5ce.diff dispatched to a fresh reviewer.
Task 9: review round 1 — changes requested (1 Important: the Win32 pressed edge must be sampled even when raylib reports pressed, preventing a duplicate edge next frame); see task-9-review-1.md.
Task 9: fix round 1 committed (d55585e); scoped rereview package review-236b5ce..d55585e.diff.
Task 9: complete (commits 891826d..d55585e; final review Approved).
Task 10: pending
Task 10: dispatched (base d55585e; brief task-10-brief.md; report task-10-report.md)
Task 10: clarification — `src/platform/raylib/material_pack.hpp/.cpp` authorized only for monotonic diagnostic texture load/unload call counters and read-only accessors. Count every real API call/failed attempt, never reset during object lifetime, and do not use the counters in rendering, residency, or gameplay decisions.
Task 10: clarification — `tests/platform/CMakeLists.txt` authorized only to declare `arpg_stage12_material_formal`'s real dependency on `arpg_stage10_formal_game_validation`, which its input-hole regression launches. Legacy ecology showcase checks may assert their three observed ecology atlas pairs only; production room-background integration remains independently mandatory in native and new production-room gates.
Task 10: clarification — `src/platform/raylib/raylib_host.cpp` authorized only to fix `present_frame_and_maybe_capture`: read the current back buffer before `EndDrawing`, always end drawing exactly once, then export. Do not swap artifact names or alter gameplay/input/scenario behavior.
Task 10: clarification — visual validation made 800x450 HUD clipping and the death modal's bright fill/center ornament blocking failures. Authorized production scope is limited to `hud_layout.cpp` (minimum scale 0.625) and `material_pack.cpp` (`ui_warning_modal` dark local 64,64 sample and no center ornament), with extensions to existing cases only in `hud_layout_tests.cpp`, `material_asset_validation_tests.cpp`, and `death_overlay_view_tests.cpp`; the platform case total must remain 473. Stage 12 must gate three-resolution HUD viewport safety and bright-gray death interiors. Lightning keeps its active showcase; C++ must export ROIs derived from production projection plus actual material frame/scale, and PowerShell retains connected background-difference proof without fixed stale coordinates or weakened thresholds.
Task 10: clarification — two intentionally slender item assets use explicit per-asset contracts only: `material_9` colors >=32 plus authored >=200, and `material_10` height >=10 plus authored >=200; all other generic gates remain unchanged and the exception keys/counts are locked.
Task 10: clarification — rejected lowering whole-frame lightning brass/cyan thresholds because the monster-free background already satisfied the proposed values. Removed those polluted global percentages as material proof; retained source-atlas palette gates, atlas/frame telemetry, and per-monster background-difference connected contours.
Task 10: implementation committed (`db1271d`); review package `review-d55585e..db1271d.diff` dispatched to a fresh reviewer. Debug units 4/4, Debug targeted 8/8, Release 4/4, package tests 23/23, real package generation, and the 60/60 original-resolution visual matrix passed. Worktree and tracked process snapshot were clean at handoff.
Task 10: review round 1 — rejected (1 Critical, 1 Important): Stage 12 reconstructed a predicted skill plan after rendering instead of consuming actual draw results; the healthy material storm path still invoked the procedural triangle-sword renderer while reporting zero procedural main visuals; Stage 17 likewise treated pure timeline functions as production-host renderer evidence. Fix brief: `task-10-review-fix-brief.md`.
Task 10: review-fix clarification — package generation remains fail-closed at the corrected 70 screenshot/70 capture-prime contract (64 original captures plus six skill-off baselines). Authorized only the matching exact-count updates in `tools/package_material_pack_v2.py` and the existing 23-case `tests/platform/package_material_pack_v2_tests.py`; no other package gate or membership may change.
Task 10: review-fix clarification — capture completion must be gated by a valid image and the real `ExportImage` result. Authorized `tests/dungeon/stage10_evidence_guard_test.cmake` only to update its exact positive helper contract for the new boolean result; all negative/order guards remain unchanged.
Task 10: review-fix verification — Debug Stage 12 2/2, Release Stage 12/17 4/4, final Debug Stage 17 2/2, Stage 10/11B/11C/11D capture guards 4/4, and package tests 23/23 passed. Final archive has 417 members, size 516640016 bytes, SHA-256 `8FF414D9CC948B7F1B41E45EF839F3FF22852401B3AC7DF45A042A6F7CB309CF`.
Task 10: review round 2 — Approved after the actual-render binding and fail-closed export repair; Critical/Important/Minor all zero. See `task-10-review-2.md`.
Task 10: review fix committed (`d710e01`); Task 10 complete (commits `d55585e..d710e01`).

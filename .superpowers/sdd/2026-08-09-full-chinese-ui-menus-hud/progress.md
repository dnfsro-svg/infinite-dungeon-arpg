# SDD ledger — plan: docs/superpowers/plans/2026-08-09-full-chinese-ui-menus-hud.md

Plan base: 567fe77
Foundation dependency: complete and focused verification green
Task 1: in progress (base 567fe77)
Task 1 fix round 1: complete — CPLAY028/menu-HUD 32/32 green; full expected 656 preserves known pre-Task1 +1 drift
Task 1: complete (commits 567fe77..48c529b, review clean)
Task 2: in progress (base 48c529b)
Task 2 fix round 1: complete (long capture labels fit with real font metrics; full expected 658 preserves known +1 drift)
Task 2: complete (commits 48c529b..7c52b96, review clean)
Task 3: in progress (base 7c52b96)
Task 3 fix round 1: complete (preserved UiTextId 0–31; actionable busy message)
Task 3: complete (commits 7c52b96..424a37e, review clean)
Task 4: in progress (base 424a37e)
Task 2 fix round 1: complete — real-font capture fit at 800/1280/1920; MENU_HUD 47/0, CPLAY014 55/0, layout guards 3/3; full actual 659 / expected 658 preserves only known count+ecology failures
Task 2: complete (commits 48c529b..7c52b96, review fix round 1)
Task 3: implementation complete — commit 3cf22d3; RED/GREEN recorded in task-3-report.md; awaiting task review
Task 3 fix round 1: implementation complete — commit 424a37e; Base UiTextId ordinals 0-31 restored and guarded; busy notice clarified; awaiting scoped re-review
Task 4: implementation complete — commit 461192e; MENU_HUD 130/130, foundation 15/15, stress 1/1, structured source guards 2/2; full actual 660 / expected 659 preserves only known count+ecology failures; awaiting scoped review
Task 4 fix round 1: implementation complete — commit 91c641a; initialized-state cache invalidation, real renderer fixtures, function-body guards; MENU_HUD 130/130, CPLAY033/foundation 15/15, stress+guards 5/5; full 660/659 preserves known ecology+count failures; awaiting scoped re-review
Task 4 fix round 2: implementation complete — commit 30fffc8; exact function-body notice/renderer contracts and same-owner fixture guard; reversible dead-decoy mutants GREEN; MENU_HUD 130/130, CPLAY033/foundation 15/15, stress+guards 5/5; full expected remains 659; awaiting scoped re-review
Task 4 fix round 3: implementation complete — commit 19a6ddc; single-pass C++ lexical parser, code-only notice bans, dead-branch renderer rejection, refresh-to-consumer invalidation guard; MENU_HUD 130/130, CPLAY033/foundation 15/15, stress+guards 6/6; full 660/659 preserves known ecology+count failures; awaiting scoped re-review
Task 4 fix round 4: implementation complete — commit 9f3370c; explicit non-empty digit-separator match checks with zero-adjacent decimal/hex fixtures; MENU_HUD 130/130, CPLAY033/foundation 15/15, stress+guards 6/6; full 660/659 preserves known ecology+count failures; awaiting scoped re-review
Task 4: complete (commits 424a37e..9f3370c, review clean)
Task 5: in progress (base 9f3370c)
Task 5: implementation complete — commit 110a284; exhaustive Chinese mappings, structured Q prompts, shared-CJK real rendering, contrast/layout/glyph guards; MENU_HUD 177/177, foundation 15/15, CPLAY030 22/22, CPLAY033 16/16, Task9 57/57, source guards 2/2; full actual 665 / expected 664 preserves only known count+ecology failures; awaiting scoped review
Task 5 fix round 1: implementation complete — commit 59661f2; abyss overlay shared-font guard with nine mutants, viewport-safe uniform text scaling, and exact contrast-color preservation; MENU_HUD 177/177, foundation 15/15, CPLAY030 22/22, CPLAY033 16/16, Task9 57/57, source guards 2/2; full actual 665 / expected 664 preserves only known count+ecology failures; awaiting scoped re-review
Task 5: fix round 1/5 (3 addressed, 0 open; commits 110a284..59661f2)
Task 5: complete (commits 9f3370c..59661f2, review clean)
Task 6: complete — `feat: localize save recovery screen`; TDD RED recorded, MSVC build green, foundation 15/15, MENU_HUD 178/178, recovery source guard+self-test 2/2; full platform 666 cases has only the known ecology alpha-bounds failure, and the broad host guard remains blocked at its unrelated death-overlay lifecycle check.
Task 6 fix round 1: complete — `fix: preserve platform test count baseline`; restored full expected 665 so actual 666 preserves the known +1 drift; fresh full run has exactly ecology alpha-bounds plus count mismatch, while foundation 15/15 and MENU_HUD 178/178 remain green.
Task 6: fix round 1/5 (1 addressed, 0 open; commits 825e6ed..0ce9c29)
Task 6: complete (commits 59661f2..0ce9c29, review clean)
Task 7: implementation complete — `feat: localize responsive debug overlay`; strict RED/GREEN recorded in task-7-report.md; MENU_HUD 183/183, foundation 15/15, source guard+self-test 2/2, Stage11C formal compile/link only; full actual 671 / expected 670 preserves only known ecology+count failures; awaiting scoped review
Task 7 fix round 1: implementation complete — `0179b69 fix: harden debug overlay text coverage`; glyph-plan coverage now includes 挥/汇/是/否, F1 affix labels use a fixed stack UTF-8 buffer, and extracted-function guards reject unsafe TextFormat plus compound-false dead decoys; foundation 15/15, MENU_HUD 183/183, source guard+self-test 2/2 (15 mutants), Stage11C formal compile/link only; full actual 671 / expected 670 preserves only known ecology+count failures; awaiting scoped re-review
Task 7: fix round 1/5 (3 addressed, 0 open; commits a12f569..0179b69)
Task 7: complete (commits 0ce9c29..0179b69, review clean)
Task 8: implementation complete — `9e4215b feat: localize game and launcher titles`; exact Chinese game title and parenthesized launcher technical error detail, launcher units 13/13, MENU_HUD 183/183, Chinese-copy guard 7 mutants, window smoke 1/1; full actual 671 / expected 670 preserves known ecology+count failures; awaiting task review
Task 8: fix round 1/5 (0 addressed, 2 open — authored ordinary strings/macros bypass filtering; noncanonical dead-call decoys bypass active-source approximation; commits 9e4215b..86ebaa7)
Task 8: fix round 2/5 (1 addressed, 1 open — all authored strings/macros and lexer edge cases covered; render_failure_message can still satisfy structure from an uninvoked lambda while live return leaks technical English; commits 86ebaa7..08d1e91)
Task 8: fix round 3/5 (1 addressed, 1 open — formatter live return and character-literal edges covered; adjacent-function regex can still truncate at a forged nested block/local fallback_rectangle sentinel; commits 08d1e91..d14f61b)
Task 8: fix round 4/5 (1 addressed, 1 open — lexical brace-depth extraction rejects the forged nested boundary; render_failure_message is not yet bound to both real failure consumers; commits d14f61b..be0cb64)
Task 8: fix round 5/5 (3 addressed, 0 open — real formatter consumers bound; dynamic Win32 system detail excluded from player UI; Direct2D and fallback share opaque >=4.5 status palettes; commits be0cb64..94c27d6)
Task 7 fix round 2: implementation complete — `2e686c0 fix: back Chinese scene labels with contrast`; F1 volume and monster affix/role/phase labels now use allocation-free viewport-clamped centered layout, opaque #0B1019 backing, >=4.5 actual text contrast, and borrowed SharedUiFont; strict RED/GREEN recorded in task-7-report.md; Foundation 15/15, MENU_HUD 183/183, Task7 guards 2/2 (23 mutants), full actual 671 / expected 670 preserves only known ecology+count failures; awaiting scoped re-review
Plan completion gate fix: implementation ready — exact `ui_text_catalog.hpp` boundary RED/GREEN, unchanged MENU_HUD 183 selector plus independent 86-case completion supplement, and executable aggregate coverage for Task 1/2/4/5/6/7 guards/self-tests/parser, Task 8 launcher units/Chinese guard/smoke, and Task 7 formal compile-only; focused evidence boundary 1/1, supplement 86/86, aggregate guards 13/13; full aggregation awaits stable final Task 7 HEAD.
Plan completion gate aggregate at workspace HEAD 7b0e71e: exit 0 — build with parallel 2 including Stage11C formal compile/link-only; MENU_HUD 183/183; completion supplement 86/86; guards/self-tests/parser 13/13; launcher units 1/1; Task 8 Chinese-copy guard pristine+3 variants/18 mutants; launcher smoke 1/1; diff check clean. Task 7 round 4 will change HEAD, so final aggregation must be rerun afterward.
Plan completion gate final aggregate at Task 7 round 4 HEAD 8ec999b: exit 0 — configure/build with parallel 2 and Stage11C formal compile/link-only; MENU_HUD 183/183; completion supplement 86/86; guards/self-tests/parser 13/13; launcher units 1/1; Task 8 Chinese-copy guard pristine+3 variants/18 mutants; launcher smoke 1/1; final diff check clean. Ready for scoped commit.
Plan completion gate fix: complete — commit `7288ec2`; exact four-file scoped commit, final aggregate green, review pending.
Plan completion gate fix round 1: implementation verified on Task 7 final HEAD d18ee04 — pure self-test 7/7; all ARPG *_ONLY isolation/restoration and exact 183/86 summaries guarded; 13 CTests plus launcher units/smoke are individually anchored and fail-closed; working and cached diff checks enabled; full aggregate exit 0. Scoped two-file commit pending.
Plan completion gate fix round 1: committed as `67e5b84`; exact two-file scope, aggregate green, awaiting scoped re-review.
Task 7 fix round 3: implementation complete — `7b0e71e fix: preserve scene label anchors`; I1 addressed by restoring desired-text-top-left anchor semantics while preserving clamped centered backing geometry and lane-0 resource-bar separation; RED MENU_HUD 183/1 then GREEN 183/0, Foundation 15/15, Task7 guard+self-test 2/2 (23 mutants); M1 ultra-wide text retained as non-blocking Minor; awaiting scoped re-review.
Task 7 fix round 4: implementation complete — `fix: separate scaled scene label plates`; real projection scales 0.70/0.85/1.0 now keep lane-0 bar-to-role and role-to-phase-to-affix plates at least 1px apart through allocation-free bar-derived/stack-step helpers; two strict MENU_HUD RED 183/1 cycles then GREEN 183/0, Foundation 15/15, Task7 guards 2/2 (23 mutants), full 671/670 preserves only known ecology+count failures; awaiting scoped re-review.
Task 7: fix round 4/5 (2 addressed, 0 open; base 7b0e71e)
Task 7 fix round 5: implementation complete — `6c28bca fix: lay out monster labels as one stack`; fixed-capacity whole-stack layout with real bars/actor exclusion, normal/F1 visibility, center+corners/scales/lanes matrix, MENU_HUD 183/183, Foundation 15/15, Task7 guards 2/2; final review found non-finite fail-open and candidate-test gaps.
Task 7 final controller adjudication: complete — accepted and fixed both final Important findings with strict RED 183/1 then GREEN 183/0; NaN/Inf and over-capacity input now fail closed, candidate order/no-safe behavior are locked by behavior tests and two new reversible mutants; full remains 671/2 only known ecology+count failures.

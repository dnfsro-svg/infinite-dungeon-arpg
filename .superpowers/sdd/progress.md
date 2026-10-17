# Stage 1 SDD Progress

Plan: docs/superpowers/plans/2026-07-11-raylib-stage1-combat-lab.md
Integration branch: milestone/m01-combat-lab
Combat branch: task/m01-combat-core
Task branch base: 5207d7e381107ee1449838fa06f1eba778f83036
Baseline: windows-msvc-core-debug Fresh passed (core.units + architecture.core_no_raylib, 2/2)

Task 1: complete (commits 5207d7e..6cd16bd, spec compliant and quality approved)
  Minor for final review: attack_catalog_tests.cpp has two cases that dereference catalog pointers after only another case proves non-null; regression diagnostics could crash instead of report.
  Controller checks: bacc15a is ancestor; branch is task/m01-combat-core; direct Combat 3/0; src/combat has no forbidden raylib token; core-only raylib DLL count 0.
Task 2: complete (commits 6cd16bd..e901d65, spec compliant and quality approved; no findings)
  Controller checks: direct Combat 7/0, input-buffer allocation-token scan empty, worktree clean.
Task 3: complete (commits e901d65..7c660d6, spec compliant and quality approved; no findings)
  Controller checks: direct Combat 11/0, forbidden-token scan empty, worktree clean.
Task 4: complete (commits 7c660d6..1ac0a8e plus review fix 59c71a6; final re-review approved; no remaining findings)
  Review fix: player Hit Stop no longer returns from the whole world tick; actor-local freeze flow preserved for Tasks 5-6.
  Controller checks: fresh external MSVC 19.44 build, direct Combat 15/0, CTest 4/4, diff-check and forbidden-token scan clean, worktree clean.
Task 5: complete (commits 59c71a6..b124b8e plus landing-event review fix 8f4e70c; final re-review approved)
  Build-system issue discovered during RED/GREEN: localized MSVC /showIncludes prefix is mojibake in Ninja rules, so header changes can leave stale objects. Task 5 uses a fresh VSLANG=1033 build; persist and verify the Configure.ps1 fix as a separate reviewed infrastructure commit before Combat integration.
  Formal review Important fixed: player landing emits exactly one landing event for the Task 9 event pipeline.
  Minors for final review: collision tests do not directly lock left-facing Y/Z and all three hurtbox extents; assist tests omit exact 0.35/0.45 boundaries, equal-gap tie, left-facing and room clamp; event tests omit several fields and queue saturation/overflow; hit-confirm test omits J2 connected timing and input-aging freeze regression.
  Additional landing-test Minor: event position only locks Z; nonzero X/Y landing position is not directly asserted.
Build dependency fix: complete (commit 2697027; independent review approved; no findings)
  Fresh Ninja deps changed from #deps 0 to #deps 5 for movement_jump_tests.obj and recorded combat_world.hpp; touching the header scheduled and rebuilt all six dependent objects plus relinking; Combat 20/0 and CTest 4/4.
Task 6: complete in behavior and final review (production commit 0dcde8a; review-test follow-up approved)
  Review-strengthening tests committed as 6498964; Combat 25/0 before Task 7.
  Final review: C/I 0; one Minor for final review because the brief's heavy-hitstun 22-tick constant has no current attack-table path.
Task 7: implementation and final re-review complete; pending local commit.
  RED: 30 cases, 4 expected failures for armor suppression, Broken lifecycle, Defeated priority path and reset event.
  GREEN: Debug and Release Combat 30/0; Debug and full raylib Release CTest 4/4; 36,000-tick allocation/overflow gate passed; forbidden dependency scan and diff-check clean.
  Full Release reused the already verified pinned raylib 6.0 source from the Stage 1 integration cache because the Combat worktree's historical out/downloads directory denied writes.
  Final re-review after fixes: Critical 0, Important 0, Minor 0; Ready to merge Yes.

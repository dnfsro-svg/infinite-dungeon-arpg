# Task 9 gate recovery report — 2026-08-01

## Outcome

The focused Stage 10/11 recovery gate is green. Formal Stage 10 now clears the
production-generated mixed 450-monster abyss room, defeats the previously
stranded ordinal-19 lightning shooter, and descends through the hole. V9
restart validation resumes the exact started-room checkpoint.

The Stage 11 transaction fixture now uses the production V9 commit worker and
double-slot arbitration. Normal/deep prove A1 to B2 death to A3 continue;
abyss preserves its live constructor `abyss_start` pending save and proves A1
to B2 to A3 death to B4 continue. Each restart destroys worker/storage,
fresh-scans the winning V9 envelope, compares durable and room-progress
checkpoints, restores the room, releases loaded slots, and starts a new worker.
Persistence revision is not used as the gameplay commit-generation receipt.

This report does not claim full Task 9 completion. Full Debug/Release CTest,
current real-window acceptance, and submission remain pending.

## 2026-08-02 continuation

- Stage 12 formal material validation, validator, and mutation self-test pass
  after restoring the real Raylib fixture, normalizing the lightning showcase
  ROI, and creating the mutation root before each negative case.
- Stage 11B settings formal validation, validator, source guard, and mutation
  self-test pass with V9 transaction-scoped character fingerprints and final
  envelope validation for both save slots.
- The Debug suite reaches `stage11c.hud_formal`. Its combat, low-health, and
  debug scenarios pass. The three full-clear scenarios are temporarily blocked
  by the already-merged 100x-room population (350 monsters) while the planned
  camera-filtered render snapshot is still absent. The old physical driver dies
  at tick 223 after four kills; a ranged physical-input driver survives beyond
  that point, but rendering the entire uncullled population causes per-frame
  material residency churn. The high-cost focused run was stopped rather than
  left consuming the machine. This gate must be repeated after Square Tasks
  7+8 connect the bounded visible set; it is not waived.

## TDD evidence

- V9 tests first rejected restart drift, then passed after exact local room and
  combat bits were encoded and restored.
- Local recovery tests first showed premature stalled-lease release; the final
  driver keeps the lease through local route arrival or bounded failure.
- Sole melee recovery first failed because the target was reacquired in the
  same tick. The final state machine releases it only after a global waypoint
  is reached or the route hits the rejoin limit, and advances the waypoint.
- The exact root `408182` run then exposed a streaming edge: ordinal 19 was not
  resident on the release tick and later returned while global sweep suppressed
  selection. An inactive-to-active RED reproduced this. The final retry is
  limited to one remaining target, cleared stalled/local/recovery state, and a
  melee chain. A separate inactive-to-active local-lease test proves the retry
  cannot bypass local detour ownership.
- Exact formal history during diagnosis: death path exit 20, then hole exit 26,
  then hole-only 449/450 with ordinal 19 at 110 HP. The final exact witness
  exited 0 at tick 82588 with depth 2 and a descent transition.

## Final commands and results

```text
cmake --build ... --target arpg_platform_tests arpg_stage10_formal_game_validation --config Debug
ctest ... -R platform.units -C Debug -j1
PASS: 544 cases, 0 failures; platform test 14.57 s

ctest ... -I 19,19 --output-on-failure -C Debug -j1
PASS: stage10.formal_game.capture_after_present; 125.78 s

ctest ... -R ^stage10.evidence.(no_private_injection|mutation_self_test)$ ...
PASS: 2/2; 17.04 s and 776.63 s

ctest ... -R ^platform.host_validation_source$ ...
PASS: 1/1; 364.96 s
```

The formal fixture uses `new (std::nothrow)` in a checked `unique_ptr` inside
its `noexcept` validator. The mask-assignment evidence regex distinguishes
assignment from equality, while the mutation suite retains an explicit
`generated_mask = 0U` rejection witness.

All temporary `--validate-hole-only`, hole/death/navigation trace output, and
diagnostic API hooks were removed before final gates. No staging, commit, or
push was performed.

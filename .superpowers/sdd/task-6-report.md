# Stage 7 Task 6 Report

## Status

Completed. `DungeonRuntime` now services every `DungeonSession::PendingSave`,
including passive-tree mutations, through the existing A/B-slot `SaveStore`.

## Delivered

- Added `DungeonRuntime::service_pending_save()`.
- The runtime reads `session_->pending_save()`, commits exactly its supplied
  next state, maps the durable receipt to `PendingSaveResult`, and resolves it
  through `session_->resolve_pending_save()`.
- Successful commits report `saved`; not-committed receipts report `error` and
  leave the session retryable; indeterminate receipts fault both the session
  and runtime.
- Kept `service_pending_transition()` as a compatibility forwarding wrapper so
  the existing host continues to drive the same generic save service without a
  host change in this task.
- Added four platform regressions: restart persistence, pre-publish rollback,
  after-publish faulting, and door/descent rejection while a passive save is
  pending. Updated the platform suite count from 41 to 45.

## TDD Evidence

The inherited runtime-test draft was inspected first. In the configured MSVC
Developer Command Prompt it failed at compilation because
`DungeonRuntime::service_pending_save()` did not exist (four call sites), which
is the expected RED state. The minimal generic runtime service was then added;
the platform executable completed 45 cases with 0 failures.

## Verification

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests`
- `out\\build\\windows-msvc-debug\\bin\\arpg_platform_tests.exe`
  - 45 cases, 0 failures
- `ctest --test-dir out\\build\\windows-msvc-debug -R "platform.view_math|persistence.units" --output-on-failure`
  - 2/2 passed
- `git diff --check`

## Scope

No passive rules, combat, checkpoint codec, `DungeonSession`, or raylib host
source was changed. The existing untracked `.review_task3.diff` was preserved.

## Important Review Follow-up

- The passive-pending rejection test now invokes `attempt_exit(right)` through
  the existing friend-backed dungeon test helper. It verifies the door path
  increments `rejected_exit_count` while the passive pending save remains the
  only pending transaction.
- The not-committed runtime case now verifies that the stable generation is
  unchanged and that a newly initialized runtime reads the old durable passive
  bitset and generation from disk.

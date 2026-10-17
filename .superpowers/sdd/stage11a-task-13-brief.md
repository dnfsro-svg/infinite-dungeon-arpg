# Stage 11-A Task 13 Finalization Brief

Finalize Stage 11-A exactly as Task 13 specifies.

## Documentation

- Update README with death/retreat rules, the E/F12/V/F1/Esc-only death input set, and a link to the formal validation report.
- Create `docs/validation/stage11a-death-continue.md` documenting the one-way module boundary, two atomic commits, V6 layout/migration, five formal paths, 1000-death stress, evidence directory, exact reproduction commands, and test-name requirement mapping.
- Explicitly list settings, item filtering, complete HUD, macOS, and Stage 11-B as not implemented/out of scope.

## Fresh verification

- Run the exact `--fresh`, `--clean-first`, and full CTest sequence for Debug and Release.
- Do not reuse stale binaries or omit graphics/formal/stress tests.
- Record exact test totals, elapsed times, and any failures/fixes in `.superpowers/sdd/stage11a-task-13-report.md`.
- Verify Release links raylib 6.0 and generates/validates all five Stage 11 captures.
- Inspect git status/diff/log/stat and ensure no build/PNG/save/log artifacts are tracked.

## Review and commit

- Commit only documentation initially as `docs: complete stage 11a death milestone`.
- Do not declare final completion; the controller will conduct two independent full-diff reviews. Any finding must be fixed and narrowly retested, followed by a final Debug full run as the plan requires.
- Do not merge main or start Stage 11-B.

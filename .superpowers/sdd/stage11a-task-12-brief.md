# Stage 11-A Task 12 Implementation Brief

Implement Task 12 exactly as specified in the plan: a headless production-API fixture, five real raylib validation paths, screenshot content validation, and positive/negative evidence guards.

## Hard evidence boundaries

- The fixture and formal game must cause death through real Combat ticks/input. No TestAccess, private member access, forced HP/phase/state, fabricated death snapshot/checkpoint, or pending-save injection.
- The headless fixture must use real `SaveStore::commit/load`, `DungeonSession::tick`, `pending_save_view`, committed receipts, reconstruction, and `request_death_continue`.
- Host scenarios may choose deterministic seeds and generate production movement/actions/key requests only. They must use real runtime save servicing and separate real save directories.
- Keep exactly one screenshot implementation: the existing host helper must call `EndDrawing()` before `LoadImageFromScreen()`/`ExportImage()`. Do not add another capture helper/path.
- Produce exactly the five named fresh 1280x720 PNGs and a five-path summary with the required target/hash/depth/abyss invariants.
- The PowerShell validator must inspect actual pixels and panel-region hashes, not only file existence.
- Positive guard and two WILL_FAIL negative fixtures must prove private injection and pre-present capture are rejected.

## TDD and verification

1. Add fixture/CTest/evidence tests first and record valid RED.
2. Implement minimally without Task 13 documentation or broad host refactoring.
3. Run the exact Task 12 build/CTest command; also run platform boundary/input latency and Stage 10 evidence guards affected by host changes.
4. Inspect all five PNGs and summary artifacts for freshness and content.
5. Commit as `test: validate stage 11a death flow in raylib` and write `.superpowers/sdd/stage11a-task-12-report.md` with RED/GREEN evidence, screenshot paths/hashes, and commit hash.

Do not start Task 13.

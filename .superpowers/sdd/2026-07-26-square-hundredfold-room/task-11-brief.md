### Task 11: Validate functionality, determinism, visuals, and bounded cost

**Files:**
- Create: `tests/dungeon/large_room_end_to_end_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Create: `tests/platform/large_room_render_plan_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`
- Create: `docs/validation/square-hundredfold-room.md`
- Create: `docs/validation/evidence/square-hundredfold-room/`

**Interfaces:**
- Consumes Tasks 1-10.
- Produces the clean functional baseline required by the separate 240 FPS/8-core plan.

- [ ] **Step 1: Run deterministic headless traces**

Task 11 validation proved that frozen V9 has no field capable of distinguishing reward-eligible defeats from non-reward self-destruction, so it cannot restore unsettled room experience exactly. Keep the V9 encoder, decoder, validator semantics, and golden byte fixtures frozen; production saves now use V10, which appends one explicit little-endian `pending_room_experience` u64 and retains V1-V9 decoding.

Run normal min/max and abyss max rooms with 0, 1, 7, and 31 scheduled reloads. Repeat each schedule twice and require identical blueprint hashes, defeat bits, exit-open tick, drops/claims, event ordering, transition target, and current V10 bytes within that schedule. Across different reload schedules, require equal durable progression/rewards/final target and all persisted room authority while allowing only the explicitly versioned transient clear behavior from Task 5. After warm-up, require zero allocation in fixed-tick, residency, snapshot, render-preparation, and main-thread checkpoint-capture paths; measure worker encode/write/readback separately rather than hiding them in that gate.

- [ ] **Step 2: Run focused Debug suites serially**

```powershell
. ./scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(core|combat|dungeon|persistence|platform)\.units$|^persistence\.save_commit_stack_guard$' --output-on-failure
```

- [ ] **Step 3: Run formal compatibility gates**

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(stage10\.|stage11c\.|stage11d\.|stage12\.material_|stage16\.|stage17\.)' --output-on-failure
```

Regenerate only owned evidence. Stage 11c must explicitly retain opaque pure-color Chinese HUD text with no overlap at all three Task 10 resolutions. Do not weaken old screenshots/validators because the camera changed; update fixtures to navigate the production room.

- [ ] **Step 4: Perform real fullscreen acceptance**

At 1920×1080, verify all four edges, camera clamping, unchanged actor size, no camera delay, door/hole ranges, readable pure-color HUD, normal 300-monster and abyss 1125-blueprint rooms, 25% early exit, continued combat, full clear, death/continue, save/reload, and no stationary screen-space props.

- [ ] **Step 5: Record bounded-cost evidence**

At equal visible density, compare old-room and new-room CPU active time over at least 10,000 Release frames. Total room population must not make per-frame monster/prop/drop preparation scale with 1,125; visible-set preparation P99 may regress by no more than 10% before the dedicated performance plan.

- [ ] **Step 6: Write validation record and commit**

```powershell
git add tests/dungeon/large_room_end_to_end_tests.cpp tests/dungeon/CMakeLists.txt tests/dungeon/dungeon_test_main.cpp tests/platform/large_room_render_plan_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp docs/validation/square-hundredfold-room.md docs/validation/evidence/square-hundredfold-room
git commit -m "test: validate hundredfold square rooms"
```

- [ ] **Step 7: Independent review and push**

Require one specification review and one code-quality review. Then run:

```powershell
git diff --check
git status --short --branch --untracked-files=all
```

Require the status output to contain only the branch header before pushing `codex/integration-large-square-rooms`. Do not merge to main until both reviews are clean.

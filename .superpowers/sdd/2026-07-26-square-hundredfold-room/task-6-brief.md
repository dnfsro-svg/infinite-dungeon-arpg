### Task 6: Unlock exits at 25% without clearing combat

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/room_progress_checkpoint.cpp`
- Modify: `src/platform/raylib/dungeon_view_math.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/hud_notice_state.cpp`
- Create: `tests/dungeon/dungeon_exit_unlock_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/persistence/checkpoint_v9_tests.cpp`
- Modify: `tests/persistence/persistence_test_main.cpp`

**Interfaces:**
- Produces `RoomProgressState { uint32_t initial, defeated, required; bool exits_unlocked; bool full_clear; }`.
- Produces `DungeonSnapshot::exits_unlocked` independent from `RoomPhase`.
- Changes `attempt_exit` and `request_descent` to accept `combat && exits_unlocked` or existing full-clear/awaiting states.
- Produces one exactly committed `DungeonEventKind::exits_opened` at threshold and one later `room_cleared` at 100%.

- [ ] **Step 1: Write threshold and save RED tests**

```cpp
ARPG_REQUIRE(required_kills(300U) == 75U);
ARPG_REQUIRE(required_kills(301U) == 76U);
ARPG_REQUIRE(required_kills(1125U) == 282U);
```

At required-minus-one, doors/hole reject transition. Crossing required creates one V9 `room_unlock` transaction; until its exact commit, doors/hole remain closed. After commit, all exits open, phase remains combat, attacks/skills still succeed, AI ticks, and remaining monsters can die. At total kills, the V9 full-clear transaction and rewards occur once.

- [ ] **Step 2: Split exit and clear state**

Replace phase-derived `exits_open` with the committed V9 flag. Do not set `RoomPhase::awaiting_exit` at 25%. Keep `prepare_room_clear()` exclusively behind `defeated==initial`; a failed threshold save retries the same pending state without duplicate events.

- [ ] **Step 3: Define early-exit reward semantics**

Normal early exit atomically commits earned monster XP and claimed loot, clears old `room_progress`, discards living monsters/unpicked ground objects, and grants no clear bonus/vacuum. Abyss early exit atomically commits `AbyssLifecycle::failed`, a failed/abandoned `LastAbyssResolution`, zero clear rewards, the selected next room, and cleared old room progress. `not_committed` leaves the player in the original room; `indeterminate` faults.

- [ ] **Step 4: Update host prompts and hole range**

All four door visuals and an existing hole's E prompt consume `exits_unlocked`, not `awaiting_exit`. Preserve the current widened hole interaction radius and do not change input bindings. Combat/HUD counters continue updating after unlock.

- [ ] **Step 5: Run transaction/death/reload tests and commit**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|persistence|platform)\.units$' --output-on-failure
git add src/dungeon src/platform/raylib tests/dungeon tests/persistence
git commit -m "feat: unlock room exits after one quarter kills"
```

---


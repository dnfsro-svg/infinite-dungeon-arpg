### Task 5: Add V9 room-progress persistence before new transactions

**Files:**
- Modify: `src/core/deterministic_rng.hpp`
- Modify: `src/core/deterministic_rng.cpp`
- Create: `src/combat/room_combat_checkpoint.hpp`
- Create: `src/combat/room_combat_checkpoint.cpp`
- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/player_damage_history.hpp`
- Modify: `src/combat/player_damage_history.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `src/dungeon/room_progress_checkpoint.hpp`
- Create: `src/dungeon/room_progress_checkpoint.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Create: `src/persistence/room_progress_codec.hpp`
- Create: `src/persistence/room_progress_codec.cpp`
- Create: `src/persistence/save_commit_worker.hpp`
- Create: `src/persistence/save_commit_worker.cpp`
- Create: `src/persistence/save_commit_storage.hpp`
- Create: `src/persistence/save_commit_storage.cpp`
- Modify: `src/persistence/CMakeLists.txt`
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/save_slot.cpp`
- Modify: `src/dungeon/dungeon_progression.cpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/platform/raylib/dungeon_runtime.hpp`
- Modify: `src/platform/raylib/dungeon_runtime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/core/deterministic_rng_tests.cpp`
- Modify: `tests/core/test_main.cpp`
- Create: `tests/combat/room_combat_checkpoint_tests.cpp`
- Modify: `tests/combat/player_damage_history_tests.cpp`
- Modify: `tests/combat/CMakeLists.txt`
- Modify: `tests/combat/combat_test_main.cpp`
- Create: `tests/persistence/checkpoint_v9_tests.cpp`
- Create: `tests/persistence/save_commit_worker_tests.cpp`
- Create: `tests/persistence/save_commit_storage_tests.cpp`
- Create: `tests/persistence/save_commit_stack_guard_test.cmake`
- Modify: `tests/persistence/CMakeLists.txt`
- Modify: `tests/persistence/persistence_test_main.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/stage17_skill_stones_game_validation.cpp`
- Modify: `tests/platform/stage17_skill_stones_validator.ps1`
- Modify: `tests/platform/stage17_skill_stones_validator_self_test.ps1`

**Interfaces:**
- Produces checkpoint format 9 with magic `ARPGSV9\0`, explicit little-endian fields, CRC, and an 8 MiB decode ceiling while keeping decoders for V1-V8.
- Produces `RoomProgressCheckpoint` containing lifecycle, room identity/seed, separate nonzero monster/environment generator versions and canonical hashes, generated/defeated/required counts, exits/full-clear/reward flags, 18-word defeat and equipment-claim bitsets, a 37-word secondary-claim bitset, `RoomCombatCheckpoint`, and ordered unclaimed equipment/material/potion records. Its storage is fixed: at most 1,152 touched monsters, 1,152 equipment ground records, and 2,320 tagged secondary ground records (`2*1152+16`), each with an explicit count; no checkpoint-owned `std::vector` or other growth allocation is allowed.
- Produces field-wise `combat::PlayerCombatCheckpoint`, ordered `combat::MonsterCombatCheckpoint` records for every touched living ordinal, and `combat::RoomCombatCheckpoint` containing `CombatWorld::tick_`, exported 4×`uint64_t` evasion RNG state, `AbyssEnvironmentRuntime`, ordinary `AttackRuntime` including the 1,152-ordinal hit latch, all stateful/destructible environment records including fire-crate `intact/broken_tick`, `PlayerDamageHistory`'s 300 buckets/active tick/initialized flag, and the authoritative death snapshot when present. No raw struct bytes, padding, pointers, active-slot handles, renderer state, or presentation diagnostics enter the file.
- Produces checked `DeterministicRng::export_state()`/`import_state()` and equivalent field-wise `PlayerDamageHistory` checkpoint APIs. Import rejects the forbidden all-zero RNG state, invalid active/history ticks, overflowed damage totals, invalid attack phase/latch bits, and malformed obstacle/death state.
- Produces allocation-free `CombatWorld::capture_room_checkpoint(RoomCombatCheckpoint& out) const noexcept` and `restore_room_checkpoint(const RoomCombatCheckpoint&)`; `DungeonSession` writes directly into the caller-selected heap job slot. Checkpoint/job copy and move operations are deleted, and no API returns a checkpoint by value. Restore rebuilds and validates both immutable blueprint versions/hashes before applying any player, monster, or obstacle position/state.
- Produces heap-owned `SaveCommitStorage` containing two preallocated checkpoint/job slots and two preallocated 8 MiB codec buffers. `DungeonRuntime` allocates it once through `std::unique_ptr` during initialization; no host/runtime/worker automatic object embeds those arrays. `SaveCommitWorker` contains only stable pointers and small synchronization/state members, with `sizeof(SaveCommitWorker)<=1024` enforced.
- Produces a persistent `SaveCommitWorker` with one in-flight/pending queue entry, condition-variable sleep, revision-tagged completion, and `stop_and_join()` before storage teardown. The exact computed `SaveCommitStorage` resident bytes are recorded and hard-capped at 32 MiB outside the texture budget.
- V8 migration preserves durable inventory/ownership, sets `state.room_progress={}`, marks the state migrated, and starts the current room fresh rather than inventing partial large-room progress.
- Until Task 7 removes legacy room-drop claims from production, a checked adapter mirrors only the existing low 3 equipment words and low 7 secondary words into/out of V9; V9 is already the canonical save representation and the remaining high words stay zero.

- [ ] **Step 1: Write V9 round-trip and corruption RED tests**

```cpp
ARPG_REQUIRE(decoded.state.room_progress.generated_monsters == 1125U);
ARPG_REQUIRE(decoded.state.room_progress.defeated_monsters == 282U);
ARPG_REQUIRE(decoded.state.room_progress.required_kills == 282U);
ARPG_REQUIRE(decoded.state.room_progress.exits_unlocked);
ARPG_REQUIRE(decoded.state.room_progress.monster_blueprint_hash != 0U);
ARPG_REQUIRE(decoded.state.room_progress.environment_blueprint_hash != 0U);
```

Reject seed/room mismatch, either blueprint hash mismatch, either generator version zero/unsupported, CRC/length overflow, popcount/count mismatch, required-count mismatch, out-of-bounds or colliding player/monster position against intact environment obstacles, set bits beyond generated count, unlocked state below threshold, duplicate/out-of-order monster or ground records, defeated ordinals with live records, invalid RNG/attack/history/abyss/obstacle/death state, invalid enums/effects, and any capacity excess.

- [ ] **Step 2: Define exact reload semantics**

On V9 reload, rebuild the monster blueprint, then the environment blueprint against it, and require both recorded versions/hashes before mutating the live session. Restore defeat/claim bits, all ordered ground records, combat tick/RNG/abyss/ordinary-attack/obstacle/damage-history/death state, player position/velocity/facing/HP/barrier/status/cooldowns, and every touched monster's position/velocity/facing/reaction/armor/AI/timers/HP/break/shield/affix effects. Clear only explicitly transient buffered/input edges, currently executing active-skill instance, projectiles, hazards, hit-stop presentation, visual event queue, and audio/renderer queues; buffered gameplay input is discarded and must be physically released before rearming. Those clear rules are versioned and tested, not accidental omissions.

- [ ] **Step 3: Implement fixed binary layout and migration**

Encode records in strictly increasing ordinal order with explicit little-endian fields and CRC, never raw struct writes. After finite/range validation, preserve every authoritative `float` losslessly by copying its IEEE-754 binary32 bit pattern into `std::uint32_t` with `std::memcpy` and writing that integer explicitly little-endian; decode reverses the operation and rejects non-finite or out-of-contract values. Encode integer timers/resources with checked widths. Preserve V8 magic/decoder and mark migrated states for rewrite on the next normal save. Change only the Stage 17 save-version assertions/validator fixture from 8 to 9; its skill timing, frame, hit, and material expectations stay unchanged.

- [ ] **Step 4: Add bounded save triggers before exposing exit/drop features**

Capture immutable checkpoint revision `N` on the main thread between fixed ticks; capture is allocation-free and targets P99 at or below 0.50 ms. The save worker alone performs codec work, temporary-file write/flush/replace, readback decode, and exact verification. It may touch only the submitted slot and returns `{revision,exact_result}`; worker completion time is measured separately and cannot choose RNG, advance a tick, or emit gameplay events.

Mark progress dirty on authoritative combat/drop changes and coalesce background saves to once per 300 fixed ticks (5 seconds). A background save does not pause authority: commit of revision `N` marks only `<=N` durable, and a newer authority revision remains dirty. If the one-entry worker queue is busy, background requests coalesce without allocating or replacing a transactional request.

Full clear, death, existing pickup/heal transactions, room transition, and clean host shutdown are exact transactional requests; Tasks 6-7 add threshold and new pickup/heal hooks to the same API. Ordinary monster death and ground-drop creation only bump the revision/dirty flag for the five-second background cadence; they never pause authority for an exact transaction. At an exact boundary, authority enters a deterministic `committing` state and stops before publishing the mutation, while presentation continues from the last complete immutable frame. Gameplay input is gated during this state: pending edges are cleared, new gameplay edges are discarded, and held controls must be physically released before rearming, so variable I/O duration cannot queue future authority actions. If a background job is active, the transaction waits in the sole pending slot and supersedes only redundant background work. Apply the exact result between ticks, then resume authority so wall-clock completion order cannot alter tick/event/RNG ordering. Closing remains inside the host loop until the transaction completes and never synchronously encodes/writes on the render thread. `not_committed` cancels the requested mutation (and shutdown closing) while keeping the game running; `indeterminate` enters the existing fault path without pretending the state was saved. Do not write every tick or every ordinary kill.

- [ ] **Step 5: Run persistence, reload, death, and deterministic tests**

```powershell
. ./scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --target arpg_core_tests arpg_combat_tests arpg_persistence_tests arpg_dungeon_tests arpg_platform_tests arpg_stage17_skill_stones_game_validation -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(core|combat|persistence|dungeon|platform)\.units$|^persistence\.save_commit_stack_guard$|^stage17\.skill_stones\.' --output-on-failure
```

Inject worker completions before capture, after capture, after 1/7/31 presentation frames, and behind an active background job. Require identical next authority tick, event order, RNG count, transaction result, and V9 bytes for every schedule. At capture boundaries where the explicitly cleared transient sets are empty, compare uninterrupted versus save/reload for at least 10,000 following authority ticks and require identical per-tick authority hashes, exported RNG state, damage-history totals, obstacle state, and event sequence; separately seed each declared transient and prove only that named field clears. Require idle worker CPU sleep, bounded queue/slot reuse, exact shutdown join, `sizeof`/resident-budget gates, partial-allocation and worker-start failure cleanup, no large checkpoint/job automatic variables via a source guard, no post-initialization allocation, and no raylib/live-authority calls from the worker translation unit. Storage allocation failure makes runtime initialization fail explicitly as persistence unavailable; it never falls back to an unsafe stack buffer.

- [ ] **Step 6: Commit**

```powershell
git add src/core/deterministic_rng.* src/combat src/dungeon src/persistence src/items src/platform/raylib/dungeon_runtime.* src/platform/raylib/raylib_host.cpp tests/core/deterministic_rng_tests.cpp tests/core/test_main.cpp tests/combat tests/persistence tests/dungeon tests/platform/dungeon_runtime_tests.cpp tests/platform/platform_test_main.cpp tests/platform/stage17_skill_stones_game_validation.cpp tests/platform/stage17_skill_stones_validator*.ps1
git commit -m "feat: persist large room progress in checkpoint v9"
```

---


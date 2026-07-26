# Prerequisite Feature Integration Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Integrate the approved desktop launcher, extreme early-floor monster nerf, and atomic HP-potion feature onto `main=d710e01` without regressing the current material pack, active-skill atlases, Chinese text rendering, or formal evidence gates.

**Architecture:** The launcher branch is merged first because its only textual conflict is the platform test count. The HP-potion branch is not merged wholesale: gameplay commits are replayed in dependency order, while material and validation changes are rebuilt against the current `MaterialPack`/residency pipeline. Every integration slice has a focused test gate and a separately reviewable commit so later large-room work starts from an auditable baseline.

**Tech Stack:** C++17, raylib 6.0, Win32/Direct2D launcher, CMake 3.25+, Ninja, MSVC, PowerShell, Python 3, CTest.

## Global Constraints

- Work only in `E:/game/.worktrees/integration-large-square-rooms` on `codex/integration-large-square-rooms`.
- Do not modify the dirty control checkout at `E:/game`.
- Preserve `main` material residency, native high-resolution backgrounds, active-skill draw-runtime validation, pure-color Chinese text, and Stage 12/17 evidence semantics.
- Do not merge `codex/stage19-large-density-rooms`; that branch is handled only as a later reference.
- Run builds with `CMAKE_BUILD_PARALLEL_LEVEL=1` or Ninja `-j1`; never start a second build while one is alive.
- The launcher source must not touch `%LOCALAPPDATA%/InfiniteDungeon/save` during tests except inside an explicitly owned temporary fixture.
- HP potions restore 25% maximum HP, drop at 30%, are auto-consumed only while HP is at or below 75%, and become authoritative only after an exact save commit.
- Floors 1-3, including abyss rooms, generate zero monster affixes; depth 4 and later retain the existing distributions.
- No source/test timeout, image threshold, expected event sequence, or material validation requirement may be relaxed to make a test pass.

---

### Task 1: Merge the desktop launcher without material regressions

**Files:**
- Merge: `codex/desktop-game-launcher`
- Resolve: `tests/platform/platform_test_main.cpp`
- Review: `src/platform/raylib/raylib_host.cpp`
- Review: `src/platform/raylib/dungeon_runtime.cpp`
- Review: `src/platform/raylib/dungeon_runtime.hpp`
- Review: `tests/platform/CMakeLists.txt`
- Add from branch: `src/launcher/*`
- Add from branch: `tests/launcher/*`
- Add from branch: `scripts/DeployLauncher.ps1`
- Add from branch: `assets/launcher/infinite_dungeon.ico`

**Interfaces:**
- Produces: `platform::make_production_host_config(HostLaunchOptions) -> RaylibHostConfig`.
- Produces: `DungeonRuntimeConfig::continue_pending_death_on_initialize` and `RaylibHostConfig::continue_pending_death_on_launch`.
- Produces: native `arpg_launcher` plus `arpg_launcher_tests`.
- Preserves: `CombatRenderer::synchronize_residency(...)` and `ActiveSkillDrawRuntimeStatus` from `main`.

- [ ] **Step 1: Record the clean integration baseline**

```powershell
git status --short --branch --untracked-files=all
git rev-parse HEAD
git diff --name-status d710e01..HEAD
ctest --preset windows-msvc-core-debug -R '^core\.units$' --output-on-failure
```

Expected: branch is `codex/integration-large-square-rooms`; the only differences from source baseline `d710e01` are the three approved files under `docs/superpowers/plans/`; and `core.units` passes.

- [ ] **Step 2: Perform a no-commit merge and verify the expected conflict set**

```powershell
git merge --no-ff --no-commit codex/desktop-game-launcher
git diff --name-only --diff-filter=U
```

Expected: exactly `tests/platform/platform_test_main.cpp` is unmerged. Abort if any material asset, renderer, host, or launcher production file is unmerged.

- [ ] **Step 3: Resolve the platform test total without choosing either side**

Replace the conflict with the combined count:

```cpp
return arpg::test::run_suites(suites, 477,
    "HUD and UI material slice contract");
```

The value is `443` common-base cases + `30` current-main cases + `4` launcher cases. Run `rg -n '^(<<<<<<<|=======|>>>>>>>)' src tests CMakeLists.txt` and require no hits.

- [ ] **Step 4: Audit the automatic merge before staging the resolution**

```powershell
git diff HEAD -- src/platform/raylib/raylib_host.cpp
git diff HEAD -- src/platform/raylib/combat_renderer.cpp src/platform/raylib/room_renderer.cpp
git diff HEAD -- assets/skills assets/stage12
```

Expected: the host only gains launch-option/death-resume plumbing; renderer and material asset diffs are empty. The merge must not delete current skill atlases, element doors, environment props, font code, or material-residency files.

- [ ] **Step 5: Run launcher and process-guard tests**

```powershell
. ./scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --target arpg_launcher_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(launcher\.(units|icon_asset|manifest|deploy_self_test)|audio\.process_guard_common_self_test|(stage14|stage15)\.audio_integration_guard)$' --output-on-failure
```

Expected: 7/7 tests pass. The audio helper may mask only exact bare `CreateProcessW(` and `ShellExecuteW(` calls in `src/launcher/launcher_platform_win32.cpp`; all other process APIs remain rejected.

- [ ] **Step 6: Build and test the real launcher path**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_launcher arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|launcher\.window_smoke)$' --output-on-failure
```

Expected: `platform.units` reports exactly 477 cases and the launcher smoke test renders a non-white readable client with bounded process cleanup.

- [ ] **Step 7: Commit the reviewed merge**

```powershell
git add tests/platform/platform_test_main.cpp
git diff --check --cached
git commit
```

Use the merge commit message `merge: integrate native desktop launcher`.

---

### Task 2: Validate launcher deployment and pending-death recovery

**Files:**
- Modify if a defect is found: `src/platform/raylib/dungeon_runtime.cpp`
- Modify if a defect is found: `src/platform/raylib/raylib_host.cpp`
- Test: `tests/platform/dungeon_runtime_tests.cpp`
- Test: `tests/platform/host_launch_options_tests.cpp`
- Modify if a new case is added: `tests/platform/platform_test_main.cpp`
- Test: `tests/launcher/launcher_window_smoke_test.ps1`
- Create: `docs/validation/desktop-game-launcher.md`
- Create: `docs/validation/evidence/desktop-launcher/launcher-validation.txt`

**Interfaces:**
- Consumes: `DungeonRuntimeConfig::continue_pending_death_on_initialize` from Task 1.
- Produces: idempotent launch recovery: no pending death is changed when disabled; one exact continuation is committed when enabled; save failures remain pending.
- Produces: deployed launcher and shortcut paths that contain no `.worktrees` component.

- [ ] **Step 1: Add/confirm RED tests for exact death-resume semantics**

```cpp
ARPG_REQUIRE(runtime.initialize() == DungeonRuntimeStatus::ready);
ARPG_REQUIRE(runtime.snapshot().death.has_value() == false);
ARPG_REQUIRE(store.load().state.death.lifecycle
    == dungeon::checkpoint::DeathLifecycle::none);
```

Cover disabled, enabled-success, `not_committed`, `indeterminate`, repeated initialization, and already-clear saves. Corrupt receipts must fault instead of silently clearing death.

- [ ] **Step 2: Run the focused runtime tests**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
```

Expected: all 477 cases pass; if a newly added assertion is RED, implement only the missing transaction check and rerun.

- [ ] **Step 3: Commit a narrow recovery fix only if Step 2 found a defect**

If and only if the focused RED test required production/test changes, review the exact diff, rerun Step 2, and commit it before Release/deployment validation:

```powershell
git diff --check
git add src/platform/raylib/dungeon_runtime.cpp src/platform/raylib/raylib_host.cpp tests/platform/dungeon_runtime_tests.cpp tests/platform/host_launch_options_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: make launcher death recovery idempotent"
```

Skip this commit when those paths are unchanged; never create an empty commit.

- [ ] **Step 4: Build Release and run the five launcher gates**

```powershell
. ./scripts/Configure.ps1 -Preset windows-msvc-release -Fresh
cmake --build --preset windows-msvc-release --target arpg_launcher arpg_launcher_tests -- -j1
ctest --test-dir out/build/windows-msvc-release -R '^launcher\.(units|manifest|window_smoke|icon_asset|deploy_self_test)$' --output-on-failure
```

Expected: 5/5 pass with no launcher, game, CMake, CTest, compiler, or linker process left running.

- [ ] **Step 5: Deploy to an owned test directory, then the real desktop**

```powershell
./scripts/DeployLauncher.ps1 -GameBuildDirectory ./out/build/windows-msvc-release/bin
```

Read back the `.lnk` through `WScript.Shell`: `TargetPath`, `WorkingDirectory`, and `IconLocation` must point to `%LOCALAPPDATA%/InfiniteDungeon/app`, never the worktree. Record save directory count/size/time before and after deployment and require equality.

- [ ] **Step 6: Capture visual and dependency evidence**

Record ready and missing-assets states at 100%, 125%, 150%, and 200% DPI. Require pure-color status text, non-overlapping Chinese labels, a visible Start button, and a non-white client. Run `dumpbin /dependents` on both executables and record the results in `launcher-validation.txt`.

- [ ] **Step 7: Commit validation evidence**

```powershell
git add docs/validation/desktop-game-launcher.md docs/validation/evidence/desktop-launcher
git commit -m "test: validate desktop launcher integration"
```

---

### Task 3: Replay the approved early-floor monster balance

**Files:**
- Modify: `src/combat/monster_catalog.cpp`
- Modify: `src/combat/monster_affix_generation.cpp`
- Test: `tests/combat/monster_catalog_tests.cpp`
- Test: `tests/combat/monster_affix_generation_tests.cpp`
- Test: `tests/combat/monster_affix_runtime_tests.cpp`
- Test: `tests/combat/monster_affix_trigger_tests.cpp`
- Test: `tests/combat/break_stress_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Test: `tests/combat/monster_melee_tests.cpp`
- Test: `tests/combat/player_build_tests.cpp`
- Test: `tests/combat/player_death_snapshot_tests.cpp`
- Modify only if the traced golden changes: `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- Modify only if the traced golden changes: `tests/dungeon/stage16_loot_reinforcement_simulation.cpp`

**Interfaces:**
- Produces the approved HP/damage pairs in catalog order: `(100,36)`, `(190,27)`, `(315,21)`, `(135,0)`, `(110,12)`, `(125,18)`, `(120,14)`, `(145,11)`.
- Produces zero normal and abyss monster affixes for depths 1-3.
- Preserves the existing depth-4+ affix weights and all movement, timing, threat-cost, tag, shield, break, projectile, and feedback fields.

- [ ] **Step 1: Replay the approved catalog change and repaired death fixtures**

```powershell
git cherry-pick -n cd31982 c738053
git diff --check
git diff --name-only
```

Expected: only `src/combat/monster_catalog.cpp` and the catalog/break/melee/player-build/death/affix combat fixtures listed by those two commits change. Require the exact catalog pairs above; do not import unrelated branch state.

- [ ] **Step 2: Verify the catalog replay and commit it**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^combat\.units$' --output-on-failure
```

Expected: `combat.units` passes with repaired death-snapshot fixtures.

```powershell
git add src/combat tests/combat
git commit -m "balance: sharply reduce monster base stats"
```

- [ ] **Step 3: Replay only the shallow-affix commit**

```powershell
git cherry-pick -n 54a4f9f
```

Review that the first affix band becomes `100/0/0/0` and that `supplement_abyss_affixes_with_catalog` returns an empty set after input/catalog validation when `depth <= 3U`. Resolve any `combat_test_main.cpp` count from the combined suite registry and verify it by running the executable; never select an old side wholesale.

- [ ] **Step 4: Verify GREEN and deterministic golden changes**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests arpg_dungeon_tests arpg_stage16_loot_reinforcement_simulation -- -j1
ctest --preset windows-msvc-core-debug -R '^(combat|dungeon)\.units$|^stage16\.loot_reinforcement\.simulation$' --output-on-failure
```

Update only a frozen hash whose trace proves the sole semantic change is the approved shallow zero-affix rule.

```powershell
git diff --check
git add src/combat/monster_affix_generation.cpp tests/combat/combat_test_main.cpp tests/combat/monster_affix_generation_tests.cpp tests/dungeon/dungeon_abyss_stress_tests.cpp tests/dungeon/stage16_loot_reinforcement_simulation.cpp
git commit -m "balance: suppress monster affixes through depth three"
```

- [ ] **Step 5: Record the two reviewed balance commits**

Expected history: one catalog/death-fixture commit and the reviewed `54a4f9f` shallow-affix commit. Do not cherry-pick `742ce70`, `b57930e`, or `2422d13` here.

---

### Task 4: Integrate deterministic atomic HP-potion gameplay

**Files:**
- Create: `src/dungeon/health_potion_loot.hpp`
- Create: `src/dungeon/health_potion_loot.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Test: `tests/combat/player_health_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Create: `tests/dungeon/dungeon_health_potion_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `tests/dungeon/dungeon_test_support.hpp`
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- Modify: `tests/dungeon/stage16_loot_reinforcement_simulation.cpp`

**Interfaces:**
- Produces: `CombatWorld::restore_player_health_percent(std::uint16_t basis_points) noexcept -> int`.
- Produces: `roll_health_potion_drop(room_seed, spawn_ordinal) noexcept` at 3000 basis points in an independent RNG domain.
- Produces: fixed records `GroundHealthPotion`, `GroundHealthPotionSnapshot`, `PendingHealthPotionClaim`, and `HealthPotionPickupReceipt`.
- Consumes: existing exact-save `PendingSaveResult`; claimed potion state shares the per-monster secondary material claim ordinal exactly as the approved branch does.

- [ ] **Step 1: Port domain and health tests before production code**

```cpp
ARPG_REQUIRE(world.restore_player_health_percent(2500U) == 250);
ARPG_REQUIRE(world.player_health() == 750);
ARPG_REQUIRE(health_potion_auto_use_eligible(750, 1000));
ARPG_REQUIRE(!health_potion_auto_use_eligible(751, 1000));
```

Also freeze 30% boundary seeds, independent RNG streams, fixed capacity, ordered batch choice, and no allocation.

- [ ] **Step 2: Verify RED**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests arpg_dungeon_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^(combat|dungeon)\.units$' --output-on-failure
```

Expected: compile failures for the missing APIs/types.

- [ ] **Step 3: Replay gameplay commits through `2eabc1b` in dependency order**

Replay `90c4348`, then `82379ae`, then `2eabc1b`, stopping after each commit to resolve test totals from the combined suite registry and run its focused executable. These commits establish percent healing, the independent fixed-capacity RNG domain, ground records, and snapshot plumbing. Do not import renderer, font, Stage 12, host, or validation-evidence files from the source worktree.

- [ ] **Step 4: Apply the atomic transaction series as one reviewed unit**

```powershell
git cherry-pick -n 37a0848 a83531f 78da4bf 6ffa3cf
git diff --check
```

Review the combined result as one transaction change. A potion must not heal, disappear, or publish a receipt before an exact commit; `not_committed` retries the same claim; `indeterminate` faults without duplicating healing.

- [ ] **Step 5: Cover room-clear and abyss-clear ordering**

Tests must prove that material vacuum, potion claims, abyss-rule cleanup, final maximum HP, healing, and the clear transaction produce one coherent next state. Above 75% HP leaves potions on the ground. Transitioning rooms clears their runtime presentation state.

- [ ] **Step 6: Port only the missing deterministic comparisons**

Manually transfer the potion fields in `tests/dungeon/dungeon_stress_tests.cpp`, then regenerate and explain the affected golden values in `tests/dungeon/dungeon_abyss_stress_tests.cpp` and `tests/dungeon/stage16_loot_reinforcement_simulation.cpp`. Do not copy any dirty Stage 10/11 evidence, guards, screenshots, or `raylib_host.cpp` changes from `E:/game/.worktrees/balance-hp-potions`.

- [ ] **Step 7: Run core suites**

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests arpg_dungeon_tests -- -j1
ctest --preset windows-msvc-core-debug -R '^(combat|dungeon)\.units$' --output-on-failure
```

Expected: `combat.units` and `dungeon.units` pass; the dungeon case total includes all 306 approved HP-potion cases.

- [ ] **Step 8: Commit the atomic transaction unit**

```powershell
git add src/combat src/dungeon tests/combat tests/dungeon
git commit -m "feat: add atomic health potion drops"
```

---

### Task 5: Rebuild HP-potion presentation on the current material runtime

**Files:**
- Add: `art_source/stage12/items/health-potion-v1.png`
- Modify: `assets/stage12/items_ui.png`
- Modify: `assets/stage12/items_ui_material.png`
- Modify: `src/platform/raylib/material_loot_view.hpp`
- Modify: `src/platform/raylib/material_loot_view.cpp`
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/hud_font.cpp`
- Modify: `tests/platform/material_loot_view_tests.cpp`
- Modify: `tests/platform/item_material_asset_pipeline_tests.py`
- Modify: `tests/platform/hud_font_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/stage17_skill_stones_game_validation.cpp`
- Modify: `tests/platform/stage17_skill_stones_validator.ps1`
- Test: `tests/platform/stage17_skill_stones_validator_self_test.ps1`
- Modify: `tools/build_item_material_atlas.py`

**Interfaces:**
- Consumes Task 4 potion snapshots/receipts.
- Produces atlas cell 30 for the health-potion sprite and material map.
- Produces pure-color `生命药` ground text and `生命药恢复 +N HP` feedback.
- Preserves current material residency synchronization, active-skill atlas validation, native room backgrounds, and actual-capture success semantics.

- [ ] **Step 1: Port presentation tests from commits `7b0731e`, `ca854bd`, `bfafd1e`, and `a3b30ff`**

Cherry-pick these four commits one at a time in the listed order, resolve against the current material APIs, run the focused platform executable after each resolution, and do not begin the next commit until the current one is clean. Require source resolution, atlas cell dimensions, keyed transparency/material channels, bounded label rectangles at 1024×576/1280×720/1920×1080, stable anchors, queue order, and zero stack overflow under full capacities.

- [ ] **Step 2: Add the missing Chinese glyph test**

```cpp
ARPG_REQUIRE(font_codepoints_contain(0x836FU)); // 药
```

The test must fail before `药` is added to the production font codepoint list.

- [ ] **Step 3: Implement against current renderer APIs**

Rebuild only the potion atlas cell and loot-label/feedback paths. Do not copy the HP branch versions of `CombatRenderer`, `RoomRenderer`, `MaterialPack`, Stage 12 validator, or `raylib_host.cpp` wholesale. Manually transfer only the `药` addition in `src/platform/raylib/hud_font.cpp` and its `tests/platform/hud_font_tests.cpp` coverage; keep text colors opaque/pure.

- [ ] **Step 4: Resolve the combined platform case count**

Resolve the platform count after each presentation commit instead of guessing from one side of a conflict: `7b0731e` -> 482, `ca854bd` -> 484, `bfafd1e` -> 487, and `a3b30ff` -> 488. Verify every value by running the executable.

- [ ] **Step 5: Run focused platform and asset tests**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|stage12\.item_material_asset_pipeline)$' --output-on-failure
```

Expected: all tests pass, `platform.units` reports exactly 488 cases, and the asset test confirms high-resolution source/published atlases.

- [ ] **Step 6: Run the unchanged Stage 12 material truth gates**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage12_material_formal -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^stage12\.material_(formal|evidence_validator)$' --output-on-failure
```

Expected: Stage 12 actual draw/capture validation passes; no test marks a frame complete before image export succeeds.

- [ ] **Step 7: Align Stage 17 with the approved lower monster HP**

Manually port only these semantics from `742ce70`: prepare a valid Storm lane before activation, move the validation player only after the first Storm hit, and require three Storm hits in both the C++ validation and PowerShell validator. Do not cherry-pick the commit wholesale. Do not apply `b57930e` unless the current Stage 10 gate actually fails for the same proved cause; never apply `2422d13` wholesale.

- [ ] **Step 8: Build and run the adjusted Stage 17 gates**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage17_skill_stones_game_validation -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^stage17\.skill_stones\.' --output-on-failure
```

Expected: the real-raylib validation and both evidence validators pass against the approved lower-HP timing.

- [ ] **Step 9: Prove current material-runtime files were preserved**

```powershell
git diff d710e01 -- src/platform/raylib/material_pack.cpp src/platform/raylib/combat_renderer.cpp src/platform/raylib/combat_renderer.hpp tests/platform/stage12_material_formal_game_validation.cpp tests/platform/stage12_material_validator.ps1 tests/platform/stage12_material_validator_self_test.ps1
```

Expected: no diff. If potion work truly requires one of these files, stop for a separate review instead of weakening this guard.

- [ ] **Step 10: Commit**

```powershell
git add art_source/stage12/items/health-potion-v1.png assets/stage12 src/platform/raylib tests/platform tools
git commit -m "feat: present high-resolution health potions"
```

---

### Task 6: Close prerequisite integration with bounded regression

**Files:**
- Create: `docs/validation/prerequisite-feature-integration.md`
- Update only when regenerated intentionally: owned validation evidence under `docs/validation/evidence/`

**Interfaces:**
- Consumes Tasks 1-5.
- Produces a clean, pushed integration baseline for the square-room plan.

- [ ] **Step 1: Run the core Debug preset serially**

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='1'
./scripts/Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

Expected: all core CTest entries pass. Stop on the first repeatable failure; do not start graphical tests while core tests are active.

- [ ] **Step 2: Run focused graphical truth gates instead of an automatic full graphics sweep**

```powershell
. ./scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|launcher\.|stage12\.material_(formal|evidence_validator)|stage17\.skill_stones\.)' --output-on-failure
```

Expected: all selected tests pass with no leftover game/launcher/build processes. This is the proportional regression gate; a full graphical CTest is not required unless a selected gate exposes broader coupling.

- [ ] **Step 3: Inspect scope and repository health**

```powershell
git diff main...HEAD --check
git diff main...HEAD --name-status
git status --short --untracked-files=all
```

Expected: only launcher, approved balance/HP potion, their tests/assets/docs, and intentional validation evidence differ from main.

- [ ] **Step 4: Write the validation record and commit**

Record exact commands, pass counts, elapsed times, build concurrency, available-memory sample, and any intentionally skipped expensive suite in `docs/validation/prerequisite-feature-integration.md`.

```powershell
git add docs/validation/prerequisite-feature-integration.md
git commit -m "test: close prerequisite feature integration"
```

- [ ] **Step 5: Push the isolated branch**

```powershell
git push -u origin codex/integration-large-square-rooms
```

Do not merge to `main` until an independent diff review confirms no material/runtime regression.

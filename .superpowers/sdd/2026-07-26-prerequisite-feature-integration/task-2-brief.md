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


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

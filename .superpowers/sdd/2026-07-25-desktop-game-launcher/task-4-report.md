# Task 4 Report — Validated LocalAppData deployment and desktop shortcut

## Status

Complete. The deployment script accepts only a directly supplied Release `bin` directory, validates it before any deployment write, publishes through a unique sibling staging directory, restores a prior `app` directory if publish fails, and creates the desktop shortcut only after published-layout validation.

## Scope

- Added `scripts/DeployLauncher.ps1`.
- Added `tests/launcher/deploy_launcher_self_test.ps1`.
- Registered `launcher.deploy_self_test` in `tests/launcher/CMakeLists.txt` with `headless;launcher;deployment` labels.
- Did not perform Task 5 real deployment and did not change gameplay, launcher UI, or the dirty `E:\game` checkout.

## RED command and raw result summary

Command (after loading the project MSVC/SDK environment):

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_launcher --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.deploy_self_test$' --output-on-failure
```

Raw result summary: configuration and target build completed; CTest ran one test and failed as expected. PowerShell reported that `E:/game/.worktrees/desktop-game-launcher/scripts/DeployLauncher.ps1` was not recognized because the script did not exist. Result: `0% tests passed, 1 tests failed out of 1` (`launcher.deploy_self_test`).

An initial unconfigured direct `cmake --build` attempt was blocked before CTest by the project guard `Stage 0 requires Windows SDK 10.0.26100.0; detected ''`; loading `scripts/Configure.ps1` supplied the required environment and produced the valid RED run above.

## GREEN commands and results

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_launcher --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.' --output-on-failure
```

Results: target build completed successfully (`ninja: no work to do`). All five launcher tests passed: `launcher.units`, `launcher.icon_asset`, `launcher.manifest`, `launcher.window_smoke`, and `launcher.deploy_self_test`; CTest reported `100% tests passed, 0 tests failed out of 5`.

## Git diff check

Ran `git diff --check` before staging: exit code 0 and no whitespace errors. Scope check showed only `scripts/DeployLauncher.ps1`, `tests/launcher/deploy_launcher_self_test.ps1`, and `tests/launcher/CMakeLists.txt` as Task 4 source changes (the report is in the plan scratch workspace).

## Concerns

None. The temporary self-test uses its own install and desktop roots and never calls the deployment script with the real LocalAppData target; actual user deployment remains Task 5.

## Fix round 1 — conflicting CMake build-type assignments

Independent review found that the original Release check accepted a Cache containing both `CMAKE_BUILD_TYPE:STRING=Release` and a conflicting later `CMAKE_BUILD_TYPE:STRING=Debug` entry. The deployment self-test now creates that conflicting input, requires deployment to fail, and verifies the pre-existing installed-app sentinel remains unchanged.

### RED command and raw result

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.deploy_self_test$' --output-on-failure
```

Result: `launcher.deploy_self_test` failed as expected against the original script with `Conflicting CMAKE_BUILD_TYPE assignments unexpectedly deployed.` (`0% tests passed, 1 tests failed out of 1`). This proved the old positive-match check accepted the conflicting Cache.

### GREEN commands and results

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.deploy_self_test$' --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.' --output-on-failure
```

Results: the focused deployment test passed (`1/1`, 100%); the launcher suite passed (`5/5`, 100%). The corrected script extracts every `CMAKE_BUILD_TYPE:STRING=` assignment and accepts only one assignment with the exact value `Release`.

# Death fixed-E hint repair report

## Scope

- Started from `92f36ec8a6d2b6ae7dc5b1858a744c819e2cd126`.
- Initial authorized tracked scope: `src/platform/raylib/combat_renderer.cpp`,
  `tests/platform/ui_material_slice_tests.cpp`, and
  `tests/platform/platform_test_main.cpp`.
- Review found the aggregate recipe still expected 12 cases. The coordinator
  explicitly authorized the minimal expansion to these two additional files:
  `tests/platform/full_chinese_inventory_progression_gate.ps1` and
  `tests/platform/full_chinese_inventory_progression_gate_self_test.ps1`.
- No other tracked files were changed.

## TDD evidence

### RED

Exact command:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug; cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2; if ($LASTEXITCODE -eq 0) { $env:ARPG_CPLAY032_DEATH_RETRY_HINT_ONLY='1'; & .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe; $code=$LASTEXITCODE; Remove-Item Env:ARPG_CPLAY032_DEATH_RETRY_HINT_ONLY; exit $code } else { exit $LASTEXITCODE }
```

Exit: `1`.

Relevant output:

```text
[FAIL] ui_material_slice.death overlay material fallbacks are independent: combat.find( "death_overlay_.draw(current_hud, material_pack_, interact_binding_label)") == std::string::npos (E:\game\.worktrees\loot-suction-animation\tests\platform\ui_material_slice_tests.cpp:153)
16 cases, 1 failures
```

### GREEN

Exact command:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug; cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2; if ($LASTEXITCODE -eq 0) { $env:ARPG_CPLAY032_DEATH_RETRY_HINT_ONLY='1'; & .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe; $code=$LASTEXITCODE; Remove-Item Env:ARPG_CPLAY032_DEATH_RETRY_HINT_ONLY; exit $code } else { exit $LASTEXITCODE }
```

Exit: `0`.

Relevant output:

```text
16 cases, 0 failures
```

## Aggregate recipe verification

PowerShell AST parse command:

```powershell
$files = @('.\tests\platform\full_chinese_inventory_progression_gate.ps1', '.\tests\platform\full_chinese_inventory_progression_gate_self_test.ps1'); foreach ($file in $files) { $tokens = $null; $errors = $null; [void][System.Management.Automation.Language.Parser]::ParseFile((Resolve-Path -LiteralPath $file), [ref]$tokens, [ref]$errors); if ($errors.Count -ne 0) { $errors | ForEach-Object { Write-Error $_ }; exit 1 } }; 'PowerShell AST parse: 2 files, 0 errors'
```

Exit: `0`; output: `PowerShell AST parse: 2 files, 0 errors`.

Pure mock recipe self-test command:

```powershell
& .\tests\platform\full_chinese_inventory_progression_gate_self_test.ps1 -GateScript .\tests\platform\full_chinese_inventory_progression_gate.ps1; exit $LASTEXITCODE
```

Exit: `0`; output: `Inventory progression gate self-test passed`.

The first attempted self-test invocation omitted its mandatory `GateScript`
parameter and exited `1` before executing the script body; it did not launch
any build, aggregate gate, CTest, or window.

## Self-review and independent review

- `git diff --check`: exit `0` before staging.
- `git diff --cached --check`: exit `0`.
- Staged-file audit matched exactly the five authorized tracked files.
- Source assertions now reject the dynamic death-overlay argument, require
  `stable_key_label(settings::StableKey::e)`, and retain the dynamic world
  interaction label at `draw_room`.
- The death host gate independently consumes `settings::StableKey::e`.
- Independent read-only final review: `无 C/I/M`.

## Commit

- `82a8be3c76015eb77baf21438a8748ad1a47c117`
- `fix(ui): show the fixed death retry key`

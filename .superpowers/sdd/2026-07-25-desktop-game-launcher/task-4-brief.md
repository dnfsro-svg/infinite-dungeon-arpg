### Task 4: Validated LocalAppData deployment and desktop shortcut

**Files:**
- Create: `scripts/DeployLauncher.ps1`
- Create: `tests/launcher/deploy_launcher_self_test.ps1`
- Modify: `tests/launcher/CMakeLists.txt`

**Interfaces:**
- Consumes: one Release `bin` directory containing `无限地下城启动器.exe`, `arpg_game.exe`, and all six required asset directories.
- Produces: `%LOCALAPPDATA%\InfiniteDungeon\app` and a known-folder Desktop shortcut `无限地下城.lnk`.

- [ ] **Step 1: Write the failing deployment self-test**

The self-test creates a temporary fake CMake Release tree with `bin` and a sibling `CMakeCache.txt` containing `CMAKE_BUILD_TYPE:STRING=Release`. It creates dummy launcher/game files plus all six asset directories, calls:

```powershell
& $DeployScript `
  -GameBuildDirectory $fakeBin `
  -InstallRoot $installRoot `
  -DesktopDirectory $desktopRoot
```

Then assert:

- every required file/directory exists under `$installRoot`;
- `$desktopRoot\无限地下城.lnk` exists;
- COM readback gives `TargetPath = $installRoot\无限地下城启动器.exe` and `WorkingDirectory = $installRoot`;
- the shortcut icon points to the installed launcher;
- a second fake input missing `assets/skills` exits nonzero and leaves a pre-existing sentinel in the installed app unchanged;
- no path named `save` is created, moved, or removed.

Register `launcher.deploy_self_test` with labels `headless;launcher;deployment`. Run it and expect RED because `DeployLauncher.ps1` is absent.

- [ ] **Step 2: Implement strict input validation**

Use this parameter contract:

```powershell
[CmdletBinding()]
param(
    [Parameter(Mandatory)][string]$GameBuildDirectory,
    [string]$InstallRoot = (Join-Path $env:LOCALAPPDATA 'InfiniteDungeon\app'),
    [string]$DesktopDirectory = [Environment]::GetFolderPath('Desktop')
)
```

Resolve all three paths. Before writing anything, require the two EXEs, all six asset directories, and a sibling `CMakeCache.txt` containing exactly `CMAKE_BUILD_TYPE:STRING=Release`. Never search another build directory or worktree.

- [ ] **Step 3: Implement staged deployment and shortcut creation**

Create a unique sibling staging directory, copy launcher/game/assets into it, and re-run the same validation against staging. If an app directory exists, rename it to unique `app.previous.<timestamp>.<pid>`; rename staging to the final app name. If publishing fails, restore the previous app name. Create/overwrite the shortcut only after the final app validates:

```powershell
$shell = New-Object -ComObject WScript.Shell
$shortcut = $shell.CreateShortcut((Join-Path $DesktopDirectory '无限地下城.lnk'))
$shortcut.TargetPath = Join-Path $InstallRoot '无限地下城启动器.exe'
$shortcut.WorkingDirectory = $InstallRoot
$shortcut.IconLocation = "$($shortcut.TargetPath),0"
$shortcut.Description = '启动无限地下城'
$shortcut.Save()
```

Do not enumerate, rename, copy, or remove `%LOCALAPPDATA%\InfiniteDungeon\save`.

- [ ] **Step 4: Run the deployment self-test and launcher suite**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_launcher
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.' --output-on-failure
```

Expected: deployment self-test and all launcher tests pass.

- [ ] **Step 5: Commit**

```powershell
git add scripts/DeployLauncher.ps1 tests/launcher
git commit -m "feat: deploy launcher to desktop"
```

---


### Task 5: Full verification, real deployment, and visual acceptance

**Files:**
- Create: `docs/validation/evidence/desktop-launcher/launcher-ready.png`
- Create: `docs/validation/evidence/desktop-launcher/launcher-missing-assets.png`
- Create: `docs/validation/evidence/desktop-launcher/launcher-validation.txt`

**Interfaces:**
- Consumes: completed launcher branch and the verified Release `bin` directory.
- Produces: actual LocalAppData installation, desktop shortcut, visual/process evidence, and no persistent test game process.

- [ ] **Step 1: Run fresh Debug verification**

Run:

```powershell
./scripts/Test.ps1 -Preset windows-msvc-debug
```

Expected: configure/build succeeds and all Debug CTest entries, including every `launcher.*` test, pass.

- [ ] **Step 2: Run fresh Release verification**

Run:

```powershell
./scripts/Test.ps1 -Preset windows-msvc-release
```

Expected: configure/build succeeds and all Release CTest entries pass. Do not suppress any existing graphics/formal validation failure; diagnose and report it if present.

- [ ] **Step 3: Record native dependency evidence**

Run `dumpbin /dependents` on both Release executables and record the exact DLL lists in `launcher-validation.txt`. The launcher must not depend on raylib or .NET DLLs; any non-system game dependency must be present on the machine before deployment is called successful.

- [ ] **Step 4: Deploy the verified Release directory**

Run:

```powershell
./scripts/DeployLauncher.ps1 `
  -GameBuildDirectory (Resolve-Path 'out/build/windows-msvc-release/bin')
```

Read back the shortcut via `WScript.Shell`, resolve its target and working directory, and append both to `launcher-validation.txt`. Confirm no deployment path points into `.worktrees`.

- [ ] **Step 5: Validate ready and missing-resource UI states**

Open the desktop shortcut with Computer Use. Capture `launcher-ready.png` and inspect at original resolution: all text must be sharp, opaque, within measured bounds, and non-overlapping. Temporarily rename only the installed `assets/skills` directory, press `检查游戏文件`, capture `launcher-missing-assets.png`, verify the red missing-resource message and disabled Start button, then restore the directory before continuing.

- [ ] **Step 6: Validate the real launch flow**

From the restored ready state, click `开始游戏`. Verify:

- the launcher PID exits;
- a new `arpg_game.exe` PID has executable path under `%LOCALAPPDATA%\InfiniteDungeon\app`;
- the game window renders;
- `%LOCALAPPDATA%\InfiniteDungeon\save` remains the game's save directory;
- no new game-validation flags appear in the process command line.

Stop only the newly launched installed-game PID after evidence is captured; do not stop or alter any older paused game process.

- [ ] **Step 7: Verify DPI behavior**

Use Windows DPI APIs or isolated UI scaling checks at 100%, 125%, 150%, and 200%. At each scale verify the client layout remains 920×560 DIPs, the title and buttons remain inside the client area, and no text overlaps. Record results in `launcher-validation.txt`.

- [ ] **Step 8: Commit evidence and final verification record**

```powershell
git add docs/validation/evidence/desktop-launcher
git commit -m "test: validate desktop launcher deployment"
```

- [ ] **Step 9: Whole-branch review and finish**

Generate the full branch diff from `git merge-base main HEAD`, request a whole-branch review, fix any load-bearing findings once, re-run the covering tests, and then use `superpowers:finishing-a-development-branch`. Do not merge into dirty `E:\game` without an explicit safe integration decision.

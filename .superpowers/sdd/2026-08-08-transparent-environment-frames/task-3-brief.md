### Task 3: Windows Release 视觉验收与问题记录

**Files:**
- Modify: `out/play-continuous-20260808-120721/issues.md`
- Create: `out/cplay019-transparent-environment-<timestamp>/screenshots/*.png`

**Interfaces:**
- Consumes: Task 2 生成的现有 atlas 文件和已有静音设置。
- Produces: 安装版哈希读回、门/洞口内部 F12 图片、CPLAY-019 闭环记录。

- [ ] **Step 1: Build and deploy exactly once**

  Run:

  ```powershell
  .\scripts\Build.ps1 -Preset windows-msvc-release
  .\scripts\DeployLauncher.ps1 -GameBuildDirectory E:\game\out\build\windows-msvc-release\bin
  ```

  Expected: both commands exit 0; build/install `arpg_game.exe` SHA-256 values are identical.

- [ ] **Step 2: Capture the affected visuals with muted audio**

  Launch the installed game with an isolated copied save, `--settings-dir E:\game\out\play-cycle5-20260808-171507\settings`, and a new screenshot directory. Use the PID-targeted keyboard driver only to resume/move/F12 as required. Capture at least one lightning door/hole and one chaos door/hole frame; do not use external CopyFromScreen as visual evidence.

- [ ] **Step 3: Review against explicit acceptance criteria**

  PASS only if the subject silhouettes have transparent surroundings, no square floor/wall plate, no clipped glow, no atlas bleed, and unchanged world scale/position. If any criterion fails, stop before logging completion and return to Task 2 with the screenshot as a new RED fixture.

- [ ] **Step 4: Close the game and append CPLAY-019**

  Gracefully close the tested game process, confirm no `arpg_game`, compiler, CMake, Ninja, or CTest process remains, and append root cause, exact asset/test/build results, SHA-256, and screenshot paths to `out/play-continuous-20260808-120721/issues.md`.

- [ ] **Step 5: Commit the issue-log update only if tracked**

  ```powershell
  git status --short -- out/play-continuous-20260808-120721/issues.md
  git add -- out/play-continuous-20260808-120721/issues.md
  git commit -m "docs: record transparent environment validation"
  ```

  If the log is intentionally ignored/untracked, retain it locally and do not force-add it.

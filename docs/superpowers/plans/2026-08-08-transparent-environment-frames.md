# Transparent Environment Frames Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用现有高分辨率键色源确定性重建四元素门与水/雷/混沌洞口，消除方形墙地背板。

**Architecture:** 只改 Stage 12 离线资产管线和输出 PNG/JSON；运行时 manifest、atlas 帧坐标、selector、renderer 和玩法保持不变。管线先从洋红/绿色键色源生成平滑 alpha，保留最大主体轮廓，再以固定色板生成元素变体；色图和 material map 复用同一 alpha。

**Tech Stack:** Python 3，Pillow，`unittest`，CMake/MSVC Release，raylib 6.0 内部 F12 截图。

## Global Constraints

- 保持 C++17 与 raylib 6.0；本任务不改 C++ 运行时接口。
- 保持 256×256 帧尺寸、`element_doors` 顺序、生态 `hole=(0,512)` 与现有 manifest ID。
- 色图与 material map alpha 必须逐像素一致。
- 不改 500 只怪物、25% 开门、碰撞、门位置、洞口交互或任何玩法规则。
- 不重置 Git，不覆盖无关工作树改动；每次提交只包含本任务明列文件。
- 只运行资产管线窄测，不跑全量 Dungeon/平台压力测试。
- Windows 视觉结论只来自安装版内部 F12 图像；音频继续使用全 0 设置。

---

### Task 1: 键色主体提取与元素调色原语

**Files:**
- Modify: `tools/build_environment_props.py:13-177`
- Test: `tests/platform/environment_prop_asset_pipeline_tests.py:32-70`
- Test: `tests/platform/environment_prop_asset_pipeline_tests.py:62-110`

**Interfaces:**
- Consumes: `PIL.Image.Image`，键色源图左上角 RGB，元素名 `fire|water|lightning|chaos`。
- Produces: `_keyed_subject(path: Path) -> Image.Image` 和 `_element_variant(subject: Image.Image, element: str) -> Image.Image`。

- [ ] **Step 1: Write the failing primitive tests**

  在测试文件动态加载 builder，对两张真实键色源断言角点 alpha 为 0、主体非空、bbox 占比非近乎实心矩形，并断言四个变体 alpha 完全一致且 RGB 互不相同：

  ```python
  door = module._keyed_subject(ROOT / "art_source/stage12/door-concept-v1.png")
  hole = module._keyed_subject(ROOT / "art_source/stage12/abyss-hole-concept-v1.png")
  for subject in (door, hole):
      alpha = subject.getchannel("A")
      self.assertEqual(alpha.getpixel((0, 0)), 0)
      self.assertIsNotNone(alpha.getbbox())
  variants = [module._element_variant(door, name)
              for name in ("fire", "water", "lightning", "chaos")]
  self.assertEqual(len({image.getchannel("A").tobytes() for image in variants}), 1)
  self.assertEqual(len({image.convert("RGB").tobytes() for image in variants}), 4)
  ```

- [ ] **Step 2: Run the primitive test to verify RED**

  Run: `python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_keyed_sources_produce_shared_isolated_element_variants -v`

  Expected: `ERROR` because `_keyed_subject` / `_element_variant` do not exist.

- [ ] **Step 3: Implement deterministic keyed extraction and tinting**

  在 builder 中导入 `ImageOps`，加入固定色板：

  ```python
  ELEMENT_PALETTES = {
      "fire": ((52, 14, 8), (255, 120, 36)),
      "water": ((7, 28, 58), (58, 196, 255)),
      "lightning": ((38, 32, 5), (255, 226, 72)),
      "chaos": ((35, 8, 52), (208, 72, 255)),
  }
  ```

  `_keyed_subject` 必须使用左上角键色，RGB 距离平方 `<=12**2` 设 alpha 0，`>=96**2` 保留原 alpha，中间做整数线性过渡；只保留阈值 8 以上的最大 8 连通主体，用 2px GaussianBlur 恢复抗锯齿，最后按 alpha bbox 加 8px 裁剪。`_element_variant` 用 `ImageOps.colorize(subject.convert("L"), dark, light)` 生成 RGB，再原样写回 subject alpha。

- [ ] **Step 4: Run primitive GREEN**

  Run: `python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_keyed_sources_produce_shared_isolated_element_variants -v`

  Expected: `OK` with `Ran 1 test`.

- [ ] **Step 5: Commit the primitive**

  ```powershell
  git add -- tools/build_environment_props.py tests/platform/environment_prop_asset_pipeline_tests.py
  git commit -m "feat: add keyed environment subject pipeline"
  ```

### Task 2: 重建公共门与生态洞口输出

**Files:**
- Modify: `tools/build_environment_props.py:196-369`
- Modify: `tests/platform/environment_prop_asset_pipeline_tests.py:72-248`
- Modify: `assets/stage12/element_doors.png`
- Modify: `assets/stage12/element_doors_material.png`
- Modify: `assets/stage12/water_environment.png`
- Modify: `assets/stage12/water_environment_material.png`
- Modify: `assets/stage12/lightning_environment.png`
- Modify: `assets/stage12/lightning_environment_material.png`
- Modify: `assets/stage12/chaos_environment.png`
- Modify: `assets/stage12/chaos_environment_material.png`
- Modify: `assets/stage12/environment-props-build.json`

**Interfaces:**
- Consumes: Task 1 `_keyed_subject` / `_element_variant`，`_fit_to_cell`，`_material_map`，现有 `LAYOUT` / `ECOLOGIES`。
- Produces: 现有尺寸的 `element_doors*.png`、三张生态 atlas/material map，以及 schema 2 的 `environment-props-build.json`。

- [ ] **Step 1: Write failing public-output contracts**

  新增 `assert_isolated_silhouette(cell, margin)`：获取 alpha bbox，计算 `opaque(alpha>8) / bbox_area`，要求 `<0.88`；要求 bbox 四边安全边距不小于 margin；统计 bbox 内 alpha>8 覆盖超过 95% 宽/高的行列，各自不得超过 bbox 高/宽的 25%。将它应用到四个门 cell 与水/雷/混沌 `hole=(0,512)` cell。同时把 manifest 契约改为：

  ```python
  self.assertEqual(manifest["schema_version"], 2)
  self.assertEqual(manifest["sources"], {
      "door-concept-v1.png": sha256(ROOT / "art_source/stage12/door-concept-v1.png"),
      "abyss-hole-concept-v1.png": sha256(ROOT / "art_source/stage12/abyss-hole-concept-v1.png"),
  })
  ```

  将旧的“从生态 atlas bootstrap 门”用例改为“修改生态门 cell 不改变键色源生成结果”；将“无效旧 report hash 必须失败”改为“旧 report 不能冻结已发布错误帧，builder 必须从键色源重生成”。

- [ ] **Step 2: Run the asset suite to verify RED**

  Run: `python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v`

  Expected: public door/hole isolation and schema 2/source provenance assertions fail on the current published assets; atomic-publication tests remain green.

- [ ] **Step 3: Wire keyed variants into the existing atlas layout**

  - Remove `_canonical_element_doors`; `build_element_doors` always reads `door-concept-v1.png`, generates four variants in order `fire, water, lightning, chaos`, and fits each with `_fit_to_cell(subject, 12, 244)`.
  - In `build_ecology_environment`, build the keyed hole subject once from `abyss-hole-concept-v1.png`, use `_element_variant(..., ecology)` for `object_sources["hole"]`, and bypass `_retain_subject` only for `wall` and the already-isolated `hole`; fit the hole into the unchanged `(0,512,256,256)` cell.
  - Keep all non-hole ecology props and the opaque authored wall path unchanged.
  - Set report `schema_version` to `2` and add exact SHA-256 source provenance under `sources`; update `_validate_staged_outputs` to require schema 2 and exact source keys/hashes.
  - Do not add runtime selectors, shader branches, new texture files, or manifest entries.

- [ ] **Step 4: Regenerate assets atomically and run GREEN**

  Run:

  ```powershell
  python tools/build_environment_props.py --root E:\game
  python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
  python tools/build_environment_props.py --root E:\game
  python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_builder_repeat_generation_is_byte_for_byte_stable -v
  ```

  Expected: full asset suite `OK`; repeat generation leaves every PNG/JSON SHA-256 unchanged; forced staged-validation test proves destination files remain unchanged on failure.

- [ ] **Step 5: Inspect generated frames before commit**

  Open `assets/stage12/element_doors.png` and each ecology atlas. Confirm no visible rectangular wall/floor plate in the four door cells or three hole cells, all subjects fit within their cells, and no alpha halo is clipped. Run `git diff --check` on the Python/JSON files.

- [ ] **Step 6: Commit only pipeline and generated outputs**

  ```powershell
  git add -- tools/build_environment_props.py tests/platform/environment_prop_asset_pipeline_tests.py assets/stage12/element_doors.png assets/stage12/element_doors_material.png assets/stage12/water_environment.png assets/stage12/water_environment_material.png assets/stage12/lightning_environment.png assets/stage12/lightning_environment_material.png assets/stage12/chaos_environment.png assets/stage12/chaos_environment_material.png assets/stage12/environment-props-build.json
  git commit -m "fix: rebuild transparent environment frames"
  ```

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

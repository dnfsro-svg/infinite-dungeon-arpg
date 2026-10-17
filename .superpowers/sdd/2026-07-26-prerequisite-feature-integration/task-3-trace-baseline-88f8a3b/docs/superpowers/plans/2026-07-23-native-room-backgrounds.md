# 四生态原生高分辨率房间背景 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 用原创原生素材替换火、水、雷、混沌被放大 7.5 倍/3.75 倍的背景裁片，在 1920×1080 下只允许下采样，并保持现有门、洞口、障碍和玩法规则不变。

**Architecture:** 每个生态由独立生成的原生地面/墙体素材片，经确定性离线构建器以 `scale <= 1.0` 拼装为 3840×2160 母版，再下采样为 2560×1440 颜色图与同尺寸材质图。运行时为四生态各增加一个独立 background atlas，只加载 common 加当前生态；旧 `*_environment` 图集继续只承载门、洞口和道具。

**Tech Stack:** C++17、raylib 6.0、CMake/Ninja/MSVC、Python 3 + Pillow 离线资产工具、现有自定义测试与 PowerShell 正式验证器。

## Global Constraints

- 全部资产原创；不得复制、描摹或拼接 DNF 或其他商业游戏画面。
- 母版必须为 3840×2160；运行时颜色图与材质图必须为 2560×1440。
- 任何进入母版的源素材绘制比例必须 `<= 1.0`；不得放大旧 256/512/1254 背景或生成稿。
- 中央 55% 为低对比、无遮挡战斗区；门、洞口、障碍、可破坏物、灯具和前景物继续作为独立层。
- 1920×1080 与 1280×720 均使用完整 2560×1440 source；运行时比例分别为 0.75 与 0.5。
- 只常驻 common 加当前生态；峰值解码 RGBA 显存必须 `<= 256 MiB`。
- 不改变存档语义、装备数值、掉落概率、房间词条、怪物配置、技能石规则、NumPad、J/K/L/WASD、门/洞口位置、E 判定或路径规则。
- 不覆盖 `deliverables/arpg-material-pack-v1.zip` 与 `deliverables/arpg-material-pack-v2-highres-text.zip`。

---

### Task 1: 火焰背景零放大离线构建证明

**Files:**
- Create: `tools/build_native_room_backgrounds.py`
- Create: `tests/platform/native_room_background_asset_pipeline_tests.py`
- Create: `art_source/stage12/backgrounds/background-sources.json`
- Consume: `art_source/stage12/backgrounds/fire/fire-floor-tile-v1.png`
- Consume: `art_source/stage12/backgrounds/fire/fire-wall-tile-v1.png`
- Create: `art_source/stage12/backgrounds/fire/fire-room-background-master.png`
- Create: `assets/stage12/fire_room_background.png`
- Create: `assets/stage12/fire_room_background_material.png`

**Interfaces:**
- Consumes: `build_ecology(ecology: str, root: pathlib.Path) -> BuildReport`，输入清单只允许 `art_source/stage12/backgrounds/<ecology>/` 下的源素材。
- Produces: `BuildReport`，包含 `master_size`、`runtime_size`、每个 source SHA-256、每次 placement 的源/目标矩形与 `scale_x/scale_y`、输出 SHA-256；JSON 记录写入 `assets/stage12/room-background-build.json`。

- [ ] **Step 1: 写失败测试**

  在 `native_room_background_asset_pipeline_tests.py` 中断言：火焰两个源片均至少 1024×1024；构建报告存在；master 精确为 3840×2160；runtime/material 精确为 2560×1440 RGBA；所有 placement 的两个比例均 `<= 1.0`；清单输入不得指向旧 `assets/stage12/fire_environment.png`；master 中央 ROI 非纯色且亮度标准差低于边缘 ROI；runtime 必须由 master 单次 LANCZOS 下采样生成。

- [ ] **Step 2: 运行 RED**

  Run: `E:\codex1\python-dev\Python313\python.exe -m unittest tests.platform.native_room_background_asset_pipeline_tests -v`

  Expected: FAIL，原因是构建器、报告和三个输出文件尚不存在。

- [ ] **Step 3: 实现最小确定性构建器**

  使用 Pillow 以原生 1:1 或缩小方式铺设墙体与地面片；重叠接缝只允许 alpha feather，不允许 `resize()` 放大。用确定性暗钢结构线、古金细边和火焰边缘光建立 2.5D 房间框架，中央区域不放交互物。master 生成后仅用一次 `Image.Resampling.LANCZOS` 下采样到 runtime；材质图从 runtime 的覆盖、亮度梯度和火焰色掩码确定性生成。

- [ ] **Step 4: 运行 GREEN 与人工检查**

  Run: `E:\codex1\python-dev\Python313\python.exe -m unittest tests.platform.native_room_background_asset_pipeline_tests -v`

  Expected: PASS；随后打开 master 与 runtime，确认无明显接缝、重复图章、中央高亮或交互对象。

- [ ] **Step 5: 提交并进入任务审查**

  Commit: `feat: author native fire room background`

### Task 2: 水、雷、混沌原生背景资产

**Files:**
- Create: `art_source/stage12/backgrounds/{water,lightning,chaos}/*-tile-v1.png`
- Create: `art_source/stage12/backgrounds/{water,lightning,chaos}/*-room-background-master.png`
- Create: `assets/stage12/{water,lightning,chaos}_room_background.png`
- Create: `assets/stage12/{water,lightning,chaos}_room_background_material.png`
- Modify: `art_source/stage12/backgrounds/background-sources.json`
- Modify: `assets/stage12/room-background-build.json`
- Modify: `tests/platform/native_room_background_asset_pipeline_tests.py`

**Interfaces:**
- Consumes: Task 1 的 `build_ecology` 与同一 placement/report 合同。
- Produces: 四生态完整 master/runtime/material 三件套与统一 provenance/build report。

- [ ] **Step 1: 为三生态扩展失败测试**

  断言四生态均满足精确尺寸、零放大、唯一 SHA-256、中央低对比、生态色占比区间和来源隔离；任一生态引用另一生态源素材即失败。

- [ ] **Step 2: 运行 RED**

  Expected: 水、雷、混沌源片和输出缺失导致明确失败。

- [ ] **Step 3: 用内置图像生成器逐生态生成原创地面/墙体素材片并构建**

  水使用湿炭黑/冷青/少量古金；雷使用暗钢/铜金/蓝白；混沌使用黑曜/紫红裂隙/克制酸绿。所有素材片保存在对应生态目录，记录提示词、生成日期、原始尺寸和 SHA-256；不得把现有 512 裁片作为输入放大。

- [ ] **Step 4: 运行 GREEN 并逐图人工验收**

  Expected: 四生态资产管线全通过；四张 2560×1440 runtime 在一秒内可区分生态且中央战斗区可读。

- [ ] **Step 5: 提交并进入任务审查**

  Commit: `feat: author native ecology room backgrounds`

### Task 3: Manifest、驻留预算与运行时接入

**Files:**
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/material_asset_validation.cpp`
- Modify: `src/platform/raylib/material_pack.cpp`
- Create: `src/platform/raylib/room_background_render_plan.hpp`
- Create: `src/platform/raylib/room_background_render_plan.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/environment_render_plan.cpp`
- Modify: `tests/platform/material_asset_validation_tests.cpp`
- Create: `tests/platform/room_background_render_plan_tests.cpp`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`

**Interfaces:**
- Produces: `RoomBackgroundRenderPlan room_background_render_plan(dungeon::DungeonElement) noexcept`，其 source 恒为 `{0,0,2560,1440}`，atlas 为对应生态独立背景。
- Produces: `resident_peak_bytes(manifest) noexcept`，按 `common + max(single ecology)` 计算峰值；全包字节另行记录而不冒充常驻量。

- [ ] **Step 1: 写并运行失败的 C++ 测试**

  覆盖四生态 atlas 一一映射、完整 source、1920/1280 比例不超过 1、背景缺失时整套环境原子回退、resident peak 不超过 256 MiB、旧 256/512 裁片不得作为背景计划。

- [ ] **Step 2: 实现四个 background atlas 与渲染计划**

  增加四个 atlas ID、颜色/材质路径和生态标签；`draw_environment_room()` 使用完整 2560×1440 source；旧生态图集只绘制门、洞和道具。

- [ ] **Step 3: 修正预算校验但不放宽保护**

  允许明确列出的 2560×1440 background atlas；拒绝更大或非 16:9 背景。显存门槛按真实驻留集合计算并保留 256 MiB 硬限制。

- [ ] **Step 4: 运行平台回归与任务审查**

  Expected: 所有平台测试 PASS；房间规则、输入、门和洞口断言不变。

- [ ] **Step 5: 提交**

  Commit: `feat: integrate native room backgrounds`

### Task 4: 正式证据、性能与 v3 交接包

**Files:**
- Modify: `tests/platform/stage12_material_formal_game_validation.cpp`
- Modify: `tests/platform/stage12_material_validator.ps1`
- Modify: `tests/platform/package_material_pack_v2_tests.py`
- Modify: `tools/package_material_pack_v2.py`
- Create: `docs/validation/native-room-backgrounds.md`

**Interfaces:**
- Produces: 四生态各 background-only/gameplay 的 1280×720 与 1920×1080 截图、背景 source/scale/驻留显存字段和完整 SHA-256 绑定。
- Produces: `deliverables/arpg-material-pack-v3-native-backgrounds.zip`，不得覆盖 v1/v2。

- [ ] **Step 1: 扩展失败的正式验证与打包测试**

  验证器拒绝缺少任一生态 1080p 截图、非 2560×1440 runtime、scale 大于 1、缺 provenance、跨生态重复背景或旧 `native-redraw-required` 状态。

- [ ] **Step 2: 输出四生态正式证据与峰值数据**

  运行真实 raylib 6.0 窗口；记录四门、洞口、出生区、路径、轮廓、resident peak 和截图字段。

- [ ] **Step 3: 将包元数据更新为已验证而不是删除审计**

  `resolution-audit.json` 保留旧倍率历史，新增四张 master/runtime 哈希、`status=native-background-verified`、source-to-screen scale 与证据路径；包名升级为 v3。

- [ ] **Step 4: 运行全量验证和独立 ZIP 回读**

  Run: 平台测试、全部 Python 资产/打包测试、Stage 12 formal、PowerShell validator、ZIP 清单逐成员 SHA-256、v1/v2 哈希不变检查。

- [ ] **Step 5: 最终审查、提交与推送隔离分支**

  Commit: `feat: deliver native high-resolution room backgrounds`

## Self-Review

- 规格覆盖：四生态母版、runtime/material、零放大、独立 atlas、真实驻留预算、原子回退、1080p 正式证据和不覆盖旧包均有任务。
- 范围边界：不重做门、洞口、怪物、UI 或玩法；这些继续作为已验收独立层。
- 类型一致：四生态统一使用 `RoomBackgroundRenderPlan`、2560×1440 source 和 `resident_peak_bytes`。
- 无占位：每项均给出文件、失败门槛、实现边界、验证与提交信息。

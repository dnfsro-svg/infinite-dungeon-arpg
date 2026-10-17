# 玩家基础动作材质重制 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 将玩家渲染从单帧占位图升级为五张原创动作图集驱动的有效帧动画。

**Architecture:** 新增公共玩家图集枚举和帧引用；动画选择器将战斗状态转换为具体剪辑，渲染器用快照 tick 在 24 fps 固定容量表中选择 UV。图集资源与现有 `MaterialPack` 同步加载、卸载。

**Tech Stack:** C++17、raylib 6.0、CMake、MSVC、CTest。

## Global Constraints

- 不修改战斗伤害、输入、存档、掉落、装备或房间规则。
- 所有图像原创、暗钢古金配色、无文字与无水印。
- 每个有效帧均有唯一 UV、感知哈希、脚底锚点和武器锚点。

---

### Task 1: 动画资源契约与失败测试

**Files:**
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/material_animation.hpp`
- Modify: `src/platform/raylib/material_animation.cpp`
- Modify: `tests/platform/material_animation_tests.cpp`

**Interfaces:**
- Produces `PlayerAnimationClipId`、`select_player_animation_clip(PlayerState, AttackId)` 与 `material_animation_frame(const AnimationClipDefinition&, uint16_t)`。

- [ ] **Step 1: 写失败测试。** 为待机、移动、跳跃、J1/J2/J3、上挑、受击、倒地、起身、死亡逐一断言最低帧数、相邻 UV 不同、锚点在单格内、颜色图集有效。
- [ ] **Step 2: 运行 RED。**

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
```

Expected: 缺少 `PlayerAnimationClipId` 或新帧校验导致失败。

- [ ] **Step 3: 实现帧契约。** 以 5 张 8×8、128×128 单元图集建立连续帧表；每条剪辑使用独立 UV 序列和 24 fps。
- [ ] **Step 4: 运行 GREEN。** 重跑 `platform.units`，期望所有测试通过。
- [ ] **Step 5: 提交。**

```powershell
git add src/platform/raylib/material_* tests/platform/material_animation_tests.cpp
git commit -m "feat: add player animation material contract"
```

### Task 2: 原创动作表与资源加载

**Files:**
- Create: `assets/player/player_locomotion.png`
- Create: `assets/player/player_combo_a.png`
- Create: `assets/player/player_combo_b.png`
- Create: `assets/player/player_reaction.png`
- Create: `assets/player/player_air.png`
- Modify: `src/platform/raylib/material_pack.cpp`
- Modify: `src/app/CMakeLists.txt`
- Modify: `tests/platform/material_asset_validation_tests.cpp`

**Interfaces:**
- Consumes五个公共 `MaterialAtlasId` 与 128 像素格帧表。
- Produces可从 EXE 目录加载、尺寸为 1024×1024 的玩家动作图集。

- [ ] **Step 1: 生成并检查原创图集。** 每张图集使用平整色键背景，去色键后保留 alpha；验证尺寸、透明角、每个已用格的像素覆盖与相邻格差异。
- [ ] **Step 2: 写失败测试。** 缺图、尺寸错误、单格颜色为空、相邻帧哈希重复时必须拒绝。
- [ ] **Step 3: 运行 RED。** 执行 `platform.units`，期望资源验证失败。
- [ ] **Step 4: 接入加载与 EXE 部署。** `MaterialPack` 加载五张公共玩家图集；应用程序构建后复制 `assets/player`。
- [ ] **Step 5: 运行 GREEN。** 执行 `platform.units`，期望通过。
- [ ] **Step 6: 提交。**

```powershell
git add assets/player src/platform/raylib/material_pack.cpp src/app/CMakeLists.txt tests/platform/material_asset_validation_tests.cpp
git commit -m "feat: load original player action atlases"
```

### Task 3: 渲染消费与真实窗口验收

**Files:**
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/material_pack.hpp`
- Modify: `tests/platform/stage12_actor_render_tests.cpp`
- Create: `tests/platform/player_animation_material_validation.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `docs/validation/player-base-animation-rebuild.md`

**Interfaces:**
- Consumes `select_player_animation_clip` 和 `MaterialPack::draw_animation_frame`。
- Produces 1280×720 动作截图和逐帧状态证据。

- [ ] **Step 1: 写失败测试。** 断言同一动作的相邻 24 fps 帧指向不同 UV，且上挑/跳跃/死亡不退回几何占位。
- [ ] **Step 2: 运行 RED。** 执行 `platform.units`，期望动画绘制接口不存在。
- [ ] **Step 3: 实现消费。** `ActorRenderer` 以 `CombatSnapshot::tick` 选择剪辑帧并保持原脚底位置；武器层在 body 后绘制，前景特效仍由原路径绘制。
- [ ] **Step 4: 运行 Debug/Release 验收。**

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|player_animation\.real_raylib)$' --output-on-failure
.\scripts\Build.ps1 -Preset windows-msvc-release
ctest --test-dir out/build/windows-msvc-release -R '^(platform\.units|player_animation\.real_raylib)$' --output-on-failure
```

- [ ] **Step 5: 提交。**

```powershell
git add src/platform/raylib/actor_renderer.cpp src/platform/raylib/material_pack.hpp tests/platform docs/validation/player-base-animation-rebuild.md
git commit -m "feat: render original player action animations"
```

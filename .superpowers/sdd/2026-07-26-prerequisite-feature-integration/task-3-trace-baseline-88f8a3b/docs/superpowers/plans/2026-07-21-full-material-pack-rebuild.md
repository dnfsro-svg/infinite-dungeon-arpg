# 全量材质包、建模与动画重制 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 完整替换现有单帧占位材质包，使玩家、八类怪物、四类房间、障碍物、掉落物、装备、材料、技能石、特效和 UI 都使用统一的原创高质量 2.5D 资源与动画。

**Architecture:** 保留现有 `MaterialPack` 加载边界，先扩展 manifest、图集、分层帧、锚点与材质描述，再以“一个可验收垂直切片”逐步替换渲染消费方。每个生态包独立加载、切房间释放；逻辑状态机只产出动画状态和事件，平台层只播放对应帧与视觉对象池。

**Tech Stack:** C++17、raylib 6.0.0、CMake 3.25、MSVC 19.44、Windows SDK 10.0.26100.0、CTest。

## 全局约束

- 视觉基准为蓝白能量、暗钢、古金、炭黑地下城；角色、武器、地面、掉落和 UI 必须同一比例体系。
- 全部资产原创；不得复制 DNF、美术截图、图集、音频或 UI 像素。
- 逻辑 60 Hz，主体动画 24–30 fps；重复帧不能充作有效帧。
- 玩家最低帧数：待机 16、移动 20、跳跃 24、J1/J2/J3 18/22/26、L 上挑 24、受击 10、倒地 16、起身 14、死亡 24、拔刀斩 36、暴风式至少 96。
- 八类怪物每类至少覆盖待机 12、移动 16、攻击/特殊 20（精英特殊 24–32）、受击 8、死亡 16；现有 7 状态不能再共用单帧占位。
- 每帧记录图集 UV、脚底锚点、武器锚点、碰撞参考点、材质类别、事件标记；颜色图缺失是硬错误。
- 当前房间仅加载玩家、通用 UI、当前生态与当前环境；显存目标 ≤256 MB。30 怪物加暴风式时目标 60 fps、1% low ≥45。
- 粒子、残影、动态灯、碎屑和特效使用对象池；低资源档只减少非关键视觉密度。
- 不改变存档语义、装备数值、掉落概率、房间词条、怪物配置、技能石规则、默认 NumPad 输入和 J/K/L/WASD。

## 当前缺口

当前 `material_manifest.hpp` 只有 3 张图集（环境 1024²、角色 2048²、特效/UI 1024²）：玩家 12 个状态和八类怪物每类 7 个状态均映射为单帧 224×224；四套房间共用单幅环境布局；掉落、效果与 UI 是少量 128×128 图标。这些占位图集、程序化扇形、三角剑和单帧动画均属于本计划的替换对象。

## 交付地图

| 里程碑 | 交付包 | 云端职责 | 本地职责 | 独立验收 |
|---|---|---|---|---|
| 0 | 通用资源管线 | manifest/帧元数据/校验/对象池测试 | 图集加载、显存采样 | 缺帧、锚点、图集边界、显存验证 |
| 1 | 玩家与两个主动技能 | 时间线、战斗事件、资源清单、单元测试 | raylib 分层演出与录制 | 36 帧拔刀、96+ 帧暴风、24 剑 |
| 2 | 火焰房间纵向切片 | 火焰生态元数据、生成与路径测试 | 地面/墙/门/光照、两怪、装饰、障碍 | 两条可行路径、破坏物与真实窗口 |
| 3 | 水、雷、混沌生态与余下六怪 | 剪辑/清单/回归测试 | 三套环境、六怪、各自效果 | 四生态全覆盖，状态帧数达标 |
| 4 | 装备、掉落、材料、技能石、UI | 资源 ID、稀有度映射、资产校验 | 图标、地面模型、背包/HUD/提示 | 所有物品与 UI 无旧占位图 |
| 5 | 压缩、低资源档与最终替换 | 清单审计、自动性能/回归 | 压力测试、实机录屏、旧资源移除 | Debug/Release 通过且无旧资源引用 |

---

### Task 0: 通用材质管线升级

**Files:**
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/material_asset_validation.*`
- Modify: `src/platform/raylib/material_pack.*`
- Modify: `src/platform/raylib/material_animation.*`
- Modify: `tests/platform/material_asset_validation_tests.cpp`
- Modify: `tests/platform/material_animation_tests.cpp`

- [ ] 建立失败测试：图集缺颜色/材质图、帧数不达动作最低值、脚底/武器锚点越界、连续帧感知哈希高度重复、生态未加载却引用、超过显存预算时均拒绝。
- [ ] 实现 `MaterialLayer`、`MaterialClass`、`AnimationClipDefinition`、`AnimationEventDefinition` 与固定容量帧索引；颜色图、法线/粗糙度/发光材质图和资源 ID 必须成对校验。
- [ ] 通过 `platform.units` 与图集内存预算测试后提交 `feat: expand material pack animation manifest`。

### Task 1: 玩家、武器与两个主动技能

**Plan:** `docs/superpowers/plans/2026-07-21-active-skills-visual-rework.md`

- [ ] 交付玩家全部基础动作与独立武器分层，不只绘制两个技能特效。
- [ ] 交付拔刀斩与暴风式事件时间线、原创帧图集、分层演出、真实 raylib 证据和性能记录。
- [ ] 提交边界：`feat: rebuild player and swordmaster active skills`。

### Task 2: 火焰房间完整纵向切片

**Files:**
- Modify: `assets/stage12/*`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`
- Modify: `tests/platform/stage12_actor_render_tests.cpp`
- Create: `tests/platform/fire_room_material_slice_tests.cpp`

- [ ] 先写失败测试：火焰地面、墙、门、火光、火炬/锁链/旗帜/武器架/骨堆、可破坏物与实体障碍均有有效分层帧；火焰爆弹兵和冲锋兵的七状态不再单帧；四门、洞口、出生区和两条路径保持可通行。
- [ ] 生成原创火焰生态图集，替换当前共用 `environment_floor_fire` 占位图与两怪的 224×224 单帧；将可破坏物和实体障碍接入前/后景遮挡与光照层。
- [ ] 本地运行固定种子真实窗口，录制战斗、破坏、绕障和换房；通过 `stage12.material_formal` 及新切片测试后提交 `feat: rebuild fire room material slice`。

### Task 3: 水、雷、混沌环境与六类怪物

**Files:**
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `tests/platform/monster_view_tests.cpp`
- Create: `tests/platform/ecology_material_coverage_tests.cpp`

- [ ] 写失败覆盖：水盾卫/水支援、雷射手/雷突进、混沌追猎/混沌危险物的七状态均有完整帧组；三套地面、墙、门、洞口、光照与装饰均不引用火焰包。
- [ ] 依次替换水、雷、混沌图集和效果层：水保持透明冷光与护盾可读性，雷保持高对比预警，混沌保持边缘清晰且不遮挡敌我轮廓。
- [ ] 每生态独立加载/释放并测量峰值显存；四生态回归、怪物状态回归和真实窗口截图通过后提交 `feat: rebuild water lightning and chaos material packs`。

### Task 4: 物品、掉落、材料、技能石和 UI

**Files:**
- Modify: `src/platform/raylib/ground_loot_view.*`
- Modify: `src/platform/raylib/material_loot_view.*`
- Modify: `src/platform/raylib/material_bag_renderer.*`
- Modify: `src/platform/raylib/inventory_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `tests/platform/ground_loot_view_tests.cpp`
- Modify: `tests/platform/material_loot_view_tests.cpp`
- Modify: `tests/platform/material_bag_renderer_tests.cpp`

- [ ] 写失败测试：每个装备槽、防具、饰品、材料、强化石/券、技能石、稀有度地面掉落和 HUD 槽都有唯一资源 ID、锚点和高对比轮廓；白/蓝/黄/深渊色不再只由文字或扁平色块区分。
- [ ] 制作原创装备/掉落模型和 UI 图标，地面掉落遵循可读性优先、背包/技能石面板遵循统一暗钢古金框架；不改物品稀有度、三合一、强化或技能石交互。
- [ ] 通过背包、掉落、暂停页、技能石槽和存档读取回归，截图验收所有图标无旧占位引用后提交 `feat: rebuild item loot and ui material packs`。

### Task 5: 全量审计、压缩与发布验收

**Files:**
- Modify: `src/platform/raylib/material_pack.*`
- Modify: `tests/platform/material_asset_validation_tests.cpp`
- Modify: `tests/platform/stage12_material_formal_game_validation.cpp`
- Create: `tests/platform/full_material_pack_validation.cpp`
- Create: `docs/validation/full-material-pack-rebuild.md`

- [ ] 写失败审计：任何 `assets/stage12/actors.png` 单帧映射、旧程序化技能几何、缺材质图、缺动作事件、未释放生态包或重复帧比例超阈值均令验证失败。
- [ ] 实现图集压缩与低资源档，保留所有关键姿态、伤害帧、敌方预警和 24 实体剑；房间切换释放非当前生态纹理。
- [ ] Debug/Release 在 30 怪物与暴风式压力场景下输出平均 FPS、1% low、峰值纹理显存、对象池峰值，执行完整非图形回归与真实 raylib 验收。
- [ ] 输出最终资源清单、证据路径、性能 CSV 和录屏索引，确认无旧占位资源引用后提交 `feat: complete full material pack rebuild`。

## 执行顺序与验收门

1. Task 0 必须先完成；它定义全部资产可被加载、验证和释放的共同边界。
2. Task 1 到 Task 4 均为独立里程碑；每个里程碑都要完成云端代码/测试审查和本地真实窗口验收后才进入下一个。
3. Task 5 只在四类内容包全部替换后执行；不允许以压缩或降级掩盖缺帧、缺包或旧占位资源。

## 自审

- 覆盖玩家、全武器分层、两个技能、八类怪物、四生态、装饰/可破坏物/实体障碍、装备/掉落/材料/技能石/UI、性能和旧资源清理。
- 现有 `MaterialPack`、`MaterialManifestDefinition`、`actor_renderer`、`room_renderer`、掉落和 UI 消费方均有对应任务；没有把全量材质包缩减成主动技能特效。
- 每一里程碑均可独立构建、测试、提交和真实窗口验收；全局任务不改变既有玩法数据与输入约定。

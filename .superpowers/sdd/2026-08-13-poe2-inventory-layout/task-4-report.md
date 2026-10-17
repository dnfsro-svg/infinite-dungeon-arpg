# Task 4 报告：接入 POE2 风格背包壳层

## 结果

- 提交：`56cb2b1 feat(ui): adopt POE2 inventory layout`（恰好 30 个任务文件）。
- 已把装备、技能石、材料三页统一接入 `InventoryShellLayout`，删除 renderer 内旧三栏/常驻详情几何。
- 已接入六个装备槽剪影、右侧物品网格、悬浮详情、材料目标、技能石装配与底部状态/动作区。
- tooltip、选中/配方/材料覆盖层、保存与强化暂态、动态按键文案均走结构化 presentation。
- Stage12、Stage16、Stage17 的确定性证据生产器与源码守卫已同步新布局和交互合同。

## TDD 证据

- Renderer direct guard RED：缺少 `InventoryFrameLayout` 活路径。
- Progression direct guard RED：`InventoryRenderer::process_input` 尚未调用 `refresh_frame_layout()`。
- 最小实现后 Debug focused build 成功：
  - `arpg_platform_tests`
  - `arpg_stage12_material_formal`
  - `arpg_stage16_loot_reinforcement_game_validation`
  - `arpg_stage17_skill_stones_game_validation`
- Inventory selector：`120 cases, 0 failures`；catalog 343；glyph 639/640；单 atlas 32 MiB；tooltip 10,000 次稳态调用 0 分配。

## 已完成的守卫验证

- `platform.host_validation_sequence_self_test`：通过，2017.51 秒；未删除 mutation，仅减少重复 sanitizer 成本。
- `platform.host_validation_source`：通过，502.55 秒。
- `platform.host_input_source`：通过，738.90 秒。
- `platform.input_latency_source`：通过，1818.15 秒。
- Inventory renderer direct+self-test：2/2 通过；191.61 秒 + 3141.93 秒。
- Progression direct：最终版本通过，53.78 秒。
- Ground direct：最终版本通过；此前 direct+self-test 组合通过，21.07 秒 + 266.98 秒。

## 按用户要求缩减的验证

- 最终 progression self-test 运行约 26 分钟后，应用户“尽量不测试”的要求主动终止；不把旧通过结果冒充最终版本结果。
- 最终 ground self-test 未重复运行；仅保留最新 direct 结果与此前组合通过记录。
- 本任务未运行 full suite、`scripts/Test.ps1`、Release 构建、窗口 producer 或实机操作。

## 独立审查

- Production/pure：Critical 0 / Important 0 / Minor 0，Ready。
- Evidence：Critical 0 / Important 0 / Minor 0，Ready。
- Guards：Critical 0 / Important 0 / Minor 0，Ready。
- 基线守卫修复独立提交：
  - `d7b62c7 test(platform): sync host validation boundaries`
  - `f4bf4a0 test(platform): normalize conditional wrapper boundaries`

## 最终轻量核对

- `git diff --check`：通过，仅有既有 LF/CRLF 提示。
- 修改范围：恰好计划内 30 个 tracked 文件。
- 已知 concern：CMake 源码 self-test 很慢；没有已知功能阻塞。

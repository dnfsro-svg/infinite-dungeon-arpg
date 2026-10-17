# Task 2 实施报告：无图形 64 节点星盘

## 状态

已完成 Task 2：固定 64 节点目录、无环对称邻接、BFS 连通规则、分配/退款、点数守恒和确定性 Modifier 投影均已实现。生产代码未引入 raylib、combat、dungeon 或 persistence。

## 实现内容

- 新增 `src/passives` 静态库 `arpg_passives`。
- 新增 `PassiveTreeState`、`PassiveNode`、`PassiveNodeType`、`PassiveTreeError` 与结果类型。
- 目录固定为 ID 0–63：中央节点、四条元素路线、四个关键节点均按批准设计编码；节点 0 永久激活。
- 节点 0 使用固定容量 12 的邻接数组，以容纳 7 个中央节点和 4 个路线入口；所有路线边双向登记。
- Stormstep 四个独立效果均保存为独立 `Modifier`；节点修正数组采用已确认的固定容量 4。
- 所有 Modifier ID 使用 `1000 + node_id * 3 + local_index`，并在目录自检中验证唯一性。
- 规则层使用固定 `std::array` BFS，检查起点位、连通性和 `spent + unspent == earned`；退款先试删后验证连通性。
- 投影按节点 ID 顺序写入固定 128 槽数组并调用 `evaluate_player_modifiers()`。
- `ModifierSpan` 增加指针/长度构造，修正评估容量从 32 扩展到 128，以支持整盘投影，不改变既有修正计算规则。
- 新增 passives 单测和平台架构边界测试。

## TDD 记录

### RED

命令：

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug --target arpg_passive_tests'
```

结果：CMake 配置失败，报告 `add_subdirectory given source "src/passives" which is not an existing directory`；这是生产目录尚未实现时的预期 RED。

实现中还发现并修正了 Stormstep 四个独立 Modifier 与容量 3 的冲突；在确认容量 4 后继续 GREEN。

### GREEN

命令：

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-debug --target arpg_passive_tests && ctest --test-dir out/build/windows-msvc-debug -R "passives.units|architecture.passives|modifiers.units|progression.units" --output-on-failure && git diff --check'
```

结果：7/7 CTest 通过；其中 `passives.units` 为 9 cases、0 failures，四个 passives 架构边界测试全部通过；`git diff --check` 无错误。

## 自检

- 未使用 `vector`、堆分配或 raylib。
- 目录边、节点类型、稳定名称、关键节点修正和 Modifier ID 均有测试或目录自检覆盖。
- 用户已有无关文件未修改。

## 疑虑

- 复审前曾发现 `max_health` 以 0 为基础会吞掉 Stormstep 的倍率；现已通过独立 `max_health_more` 修复。后续 combat 仍需使用实际生命基数计算最终生命值。

## 复审修复

- 新增 `StatId::max_health_more` 与 `PlayerModifierValues::max_health_more`；max-health-more 以 `kFixedOne` 为基础评估，Stormstep 使用独立 `more 8000`，生命平面加成仍保留在 `max_health`。
- 黄金测试验证未分配 `max_health_more == 10000`、Stormstep `== 8000`、core vitality 仍只提供 flat `max_health == 200000` 且 multiplier 保持 `10000`。
- 扩充目录测试，逐项检查中央节点、四条路线 64 节点的类型、名称、邻接顺序/数量、关键 Modifier stat/value；目录自检新增重复邻居拒绝。

### 复审 TDD 记录

RED：在缺少 `max_health_more` 字段时，`arpg_passive_tests` 编译失败并明确报告 `PlayerModifierValues` 没有该成员。

GREEN：修复后 `arpg_passive_tests` 为 9 cases/0 failures；`passives.units`、`modifiers.units` 和四个 `architecture.passives_*` 测试共 6/6 通过；`git diff --check` 无错误。

## 复审 Important：目录黄金断言补全

复审前覆盖缺口：此前测试只对部分中心/关键节点检查 `stat`，路线节点没有统一断言全部 `operation/value`，路线稳定名称也未逐项检查。为形成可观测 RED，临时将路线 B+4 的 `increased` 预期从 800 改为 801；测试按预期失败（目录名称断言和该 Modifier 断言均报告失败），随后恢复正确预期。

本次修复仅修改 `tests/passives/passive_tree_catalog_tests.cpp`：

- 表驱动断言中央 0–7 的稳定名称、modifier_count、stat、operation、value。
- 表驱动断言四条路线全部 14 个偏移的稳定名称、类型、modifier_count、完整邻接顺序。
- 逐项断言 B+2 至 B+12 的每个 Modifier 字段。
- 逐项断言 21/35/49/63 四个 Keystone 的全部收益与代价字段，包括 Stormstep 的四个独立 Modifier。

复审 GREEN：`arpg_passive_tests` 9 cases/0 failures；`passives.units`、`modifiers.units` 与四个 `architecture.passives_*` 共 6/6 通过；`git diff --check` 通过。

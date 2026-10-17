# Task 3 报告：CombatWorld 消费玩家构筑并结算五分量伤害

## Status

完成。Task 3 已实现并通过定向 combat、architecture 边界测试；未实现 dungeon、persistence 或 platform UI。

## 实现摘要

- 在 `combat_types.hpp` 增加 `DamagePacket`（物理、火、水、雷、混沌五分量）与纯数据 `PlayerCombatBuild`。
- 新增 `build_player_hit_packet`、`resolve_player_damage`、`scaled_phase_ticks`：
  - 玩家基础物理伤害保留；元素 flat/increased 独立附加，不转换物理。
  - 元素抗性按 -60% 到 75% 钳制；承伤倍率在抗性之后计算并向下取整。
  - 玩家护盾先于生命吸收伤害，生命仍保持至少 1；房间创建/健康重置时护盾回满，房间内不自动恢复。
- `CombatEncounterConfig` 接收 `PlayerCombatBuild`；HP、护盾、四项抗性在玩家初始化时投影。
- 怪物接触、投射物、地面危险区改用 `DamagePacket`；怪物目录按火/水/雷/混沌生态携带对应伤害类型，同时保留旧整数 spawn/test 重载。
- 攻击运行时缓存缩放后的 startup/recovery；active tick 保持目录值，命中窗口和 snapshot 使用同一缓存时序。
- 移速、跳跃、空中控制、水平击退和击飞冲量消费构筑倍率。
- `CombatWorldTestAccess` 新增 typed 伤害注入重载，整数重载继续包装为物理伤害以保持旧健康测试语义。

## 变更文件

实现：

- `src/combat/combat_types.hpp`
- `src/combat/combat_world.hpp`
- `src/combat/combat_world.cpp`
- `src/combat/hit_resolution.cpp`
- `src/combat/player_simulation.cpp`
- `src/combat/target_simulation.cpp`
- `src/combat/attack_catalog.hpp`
- `src/combat/attack_catalog.cpp`
- `src/combat/monster_catalog.cpp`
- `src/combat/monster_pool.hpp`
- `src/combat/monster_pool.cpp`
- `src/combat/combat_snapshot.cpp`

测试：

- `tests/combat/player_build_tests.cpp`
- `tests/combat/combat_test_support.hpp`
- `tests/combat/combat_test_main.cpp`
- `tests/combat/CMakeLists.txt`

## TDD 证据

### RED

先加入 7 个 player-build 行为测试和 typed 测试辅助，再执行构建。加载 VS 开发环境后的实际失败为：

`combat_test_support.hpp`: `DamagePacket` 不是 `arpg::combat` 的成员，随后报告 `packet/source_position/feedback` 未声明。

这证明测试在生产接口实现前确实失败。

### GREEN

完成最小实现后执行：

```text
74 cases, 0 failures
```

随后补上生态元素伤害映射并重新执行，仍为 `74 cases, 0 failures`。

## 验证

- `cmake --build --preset windows-msvc-debug --target arpg_combat_tests`：通过。
- `out/build/windows-msvc-debug/bin/arpg_combat_tests.exe`：74/74 通过。
- `ctest --test-dir out/build/windows-msvc-debug -R "combat.units|architecture.combat" --output-on-failure`：4/4 通过。
  - `combat.units`
  - `architecture.combat_no_raylib`
  - `architecture.combat_no_dungeon`
  - `architecture.combat_no_persistence`
- 完整 Debug 构建：通过，包含 game、dungeon、persistence、platform 目标。
- `git diff --check`：通过；仅有 Git 的 LF/CRLF 提示，无空白错误。

## 自检与疑虑

- Combat 只依赖 `modifiers`，不依赖 `arpg_passives`；architecture 测试已验证不链接 raylib/dungeon/persistence。
- `DamagePacket` 保留整数构造与 pool/world 重载，避免破坏已有健康、AI、容量测试。
- 当前元素 flat/increased 与 melee 都采用固定点向下取整；后续若设计要求“每个分量分别向上取整”应另开任务并补规则测试。
- 怪物 support 的 contact damage 仍为零，因为其行为是施加屏障效果而非直接攻击；这与原有 AI 行为一致。

## 审查修复

- `load_wave(..., false)` 现在保留当前 HP 与护盾；只有 `reset_player_health == true` 时才恢复最大护盾。新增回归覆盖 false 保留和 true 回满。
- 命中事件保留五分量结算后的 `packet_total`，目标护盾只影响目标实际 HP 扣减；新增火焰附加伤害 + 目标护盾测试，验证事件值 31、护盾吸收 20 后 HP 从 700 降至 689。

修复后验证：

- combat：76/76 通过。
- `combat.units` 与 3 个 combat architecture 边界测试：4/4 通过。
- `git diff --check`：通过。

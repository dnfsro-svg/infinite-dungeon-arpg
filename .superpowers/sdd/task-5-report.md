# Stage 10 Task 5 报告

## Status

完成纯 `build_abyss_encounter_plan` 与 `supplement_abyss_affixes`，未接入房间生命周期。

## RED / GREEN

- RED：先加入预算 8→12、9→14、24→36 与深度词缀下限测试；在正确加载 VS 2022 开发环境后，编译分别因 `build_abyss_encounter_plan` 和 `supplement_abyss_affixes` 不存在而失败，确认失败来自缺失功能。
- GREEN：实现预算缩放、普通 plan/普通 affix 先生成、深渊补足与强化合法性后，`combat.units` 136/136、`dungeon.units` 145/145 通过。
- Refactor：抽出共享的指定预算 plan 构造路径，使普通 `build_encounter_plan` 继续使用原 RNG 域和普通 affix 生成；Stage 9 既有 golden 测试通过。

## 确定性与命名域

- 普通词缀仍使用 Stage 9 的 count/selection/tier/context 域，未重抽。
- 深渊选择域：`ABYSEL01`（`0x41425953454C3031`）。
- 深渊 tier 域：`ABYTIER1`（`0x4142595449455231`）。
- 每个 append 输出位置都从对应命名域再按绝对 `output_index` 派生独立子流；补足从 `normal.count` 开始，只写新增槽位。
- 已满足深度下限的合法集合原值返回，包括未使用槽位，逐字节语义不变。

## 候选不足与容量

- 补足复用 required/forbidden tags、重复与双向 conflict 校验。
- 测试用受限合法 catalog 只留下一个兼容候选；深度 20 需要两条时返回 `nullopt`，不静默少给。
- `MonsterAffixSet` 仍为固定 3 槽，40+ 精确补到 3，不增加高危词缀上限。
- 强化预算为 `(normal * 3 + 1) / 2`；合法性使用 `ceil(config.max_budget*1.5)` 上限。
- 可表示边界 normal max 170→abyss max 255 构造出单波 96 spawn 并合法；171→257 超出固定 DTO 表示范围时显式 `invalid_rules`，不降级。
- 纯接口只使用固定 `std::array`/值类型；4096 次补足与 1024 次强化 plan 测试的 allocation delta 均为 0。

## 依赖重排

原计划由 Task 4 先接生命周期，但 started 门禁要求在落盘前完整调用本任务的纯构造接口。按批准的依赖重排，本任务保持 `src/dungeon/dungeon_session.cpp` 不改；`available → started` 前置计算及 runtime 接线由紧接着的 Task 4 完成。因此 available 房不会在本提交中提前启动强化战斗。

## 文件

- `src/combat/monster_affix_generation.hpp/.cpp`
- `src/dungeon/encounter_director.hpp/.cpp`
- `src/dungeon/encounter_budget.cpp`
- `tests/combat/monster_affix_generation_tests.cpp`
- `tests/combat/monster_affix_test_support.hpp`
- `tests/dungeon/encounter_director_tests.cpp`
- `tests/combat/combat_test_main.cpp`
- `tests/dungeon/dungeon_test_main.cpp`
- `.superpowers/sdd/task-5-report.md`

## 验证

```text
ctest --preset windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure
combat.units: passed (0.77 s)
dungeon.units: passed (142.99 s)
100% tests passed, 0 failed

ARPG_STAGE9_AFFIX_STRESS_ONLY=1 arpg_dungeon_tests.exe
2 cases, 0 failures
```

`git diff --check` 通过。普通 Stage 9 affix golden、掉落与压力用例均包含在上述通过套件中。

## 疑虑

- 无 Task 5 纯接口遗留疑虑。
- 生命周期尚未消费纯 plan 是刻意的依赖重排边界，必须由 Task 4 接线后再验证 started 原子门禁。

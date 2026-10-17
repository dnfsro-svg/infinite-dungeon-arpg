## Task 5: 强化遭遇预算并补足普通怪物词缀

**Files:**

- Modify: `src/dungeon/encounter_director.hpp`
- Modify: `src/dungeon/encounter_director.cpp`
- Modify: `src/dungeon/encounter_budget.cpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/combat/monster_affix_generation.hpp`
- Modify: `src/combat/monster_affix_generation.cpp`
- Modify: `tests/dungeon/encounter_director_tests.cpp`
- Modify: `tests/dungeon/dungeon_wave_tests.cpp`
- Modify: `tests/combat/monster_affix_generation_tests.cpp`

**Produces:** `ceil(normal_budget * 1.5)`、每怪先普通词缀后深渊补足的稳定结果。

**Consumes:** 普通 `build_encounter_plan`、普通 `generate_monster_affixes` 与 Task 1 深度下限。

- [ ] 增加测试：预算 8→12、9→14、24→36 且合法性上限同步为 `ceil(config.max_budget*1.5)`；深度 1/19 至少 1 条、20/39 至少 2 条、40+ 至少 3 条；原本已满足时逐字节不变；补足不重复词缀；同 seed 稳定；补足不改变普通词缀前缀；96 怪上限合法。

新增接口：

```cpp
[[nodiscard]] EncounterPlanResult build_abyss_encounter_plan(
    std::uint64_t room_seed,
    std::uint64_t depth,
    checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig& config) noexcept;

[[nodiscard]] std::optional<MonsterAffixSet> supplement_abyss_affixes(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint8_t wave_index,
    std::uint8_t spawn_index,
    const MonsterDefinition& monster,
    MonsterAffixSet normal) noexcept;
```

- [ ] 先确认预算与词缀下限测试失败，再实现独立命名域。不得重新调用或重抽普通词缀；从 `normal.count` 开始补齐，保持已有元素和顺序完全不变。

- [ ] 补足时逐个输出位置分别派生“词缀选择”和“tier”子域，继续复用 required tags、重复和冲突校验；候选不足立即返回 `nullopt` 并阻止 started 提交，不得静默少给词缀或退回普通遭遇。`MonsterAffixSet` 上限仍为 3，不扩大 Stage 9 高危词缀容量。

- [ ] 运行：

```powershell
ctest --preset windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure
```

预期：两组通过，Stage 9 的 affix golden 和掉落压力测试不漂移。

- [ ] 提交：

```powershell
git add src/combat src/dungeon tests/combat tests/dungeon
git commit -m "feat: strengthen abyss encounters deterministically"
```

---


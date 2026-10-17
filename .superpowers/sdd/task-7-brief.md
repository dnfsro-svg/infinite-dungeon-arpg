## Task 7: 实现三条固定容量环境规则

**Files:**

- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Create: `src/combat/abyss_environment.cpp`
- Modify: `src/combat/combat_snapshot.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Create: `tests/combat/abyss_environment_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `tests/combat/CMakeLists.txt`

**Produces:** Thunderstorm、Hunting Flames、Chaos Expansion 的无分配固定 tick 运行时与可绘制快照。

**Consumes:** 实际 `player_.max_hp`、`DamageDelivery::ground_or_environment` 和既有元素减伤；不走闪避。

- [ ] 新增 `AbyssEnvironmentRuntime` 固定状态，不使用动态容器：

```cpp
struct AbyssEnvironmentRuntime final {
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint16_t cycle_tick{};
    std::uint16_t stage_tick{};
    Vec3 locked_center{};
    std::uint8_t expansion_stage{};
    bool warning{};
    bool active{};
};
```

- [ ] 增加测试并精确检查：雷暴 180 周期/45 预警/0.8 半径/15% 雷电；追猎火焰 240 周期/45 预警/1.0 半径/180 持续/每 60 tick 10% 火焰；混沌扩张 1.0、2.3、3.6、4.9、6.2 半径且每 180 tick 扩张、每 60 tick 8% 混沌。

- [ ] 测试地面环境伤害：100% 闪避仍命中；对应元素减伤生效；无敌帧仍遵循现有统一伤害语义；原始伤害为 `max(1, ceil(actual_max_hp * bp / 10000))`。

- [ ] 预填满 `HazardPool` 后触发环境规则，断言旧 hazard 不被覆盖、环境生成失败、`hazard_saturation_count` 加一且没有伤害/成功事件；释放一个槽后下一周期可正常生成。

- [ ] 先运行测试，预期环境事件和快照为空。

- [ ] 扩展 `HazardKind` 为 thunderstorm/hunting_flame/chaos_expansion，并为 `HazardRuntime` 增加 `HazardSource::monster/abyss_environment`。新增 `spawn_environment_hazard` 仍使用现有 `HazardPool` 固定容量，但不要求怪物 owner；怪物死亡清理只处理 monster source。池满时拒绝新区域、增加 `hazard_saturation_count`，不得覆盖旧对象或伪造成功。

- [ ] 实现 `simulate_abyss_environment()`：雷暴在挑战 tick 180 的瞬间锁定玩家位置并产生 45 tick 预警，此后每 180 tick 重复；追猎烈焰在 tick 240 锁点并预警 45 tick，随后生成持续 180 tick、每 60 tick 伤害一次的区域；混沌扩散从 tick 0 就建立半径 1.0 的中心区域，在 180/360/540/720 tick 更新为后四档并保持。所有命中统一调用 `apply_player_damage(packet, DamageDelivery::ground_or_environment, center, FeedbackLevel::heavy)`。

- [ ] 环境 hazard 不携带怪物 affix，也不调用 chain/corrosion/death trigger 路径，确保不会递归触发连锁闪电、腐蚀或直接命中词缀。

- [ ] 运行战斗回归：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
& .\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
```

预期：全部通过；新增 600 tick 分配探针为 0。

- [ ] 提交：

```powershell
git add src/combat tests/combat
git commit -m "feat: simulate abyss environment rules"
```

---


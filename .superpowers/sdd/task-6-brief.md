## Task 6: 接入六条怪物/玩家数值规则

**Files:**

- Modify: `src/combat/combat_types.hpp`
- Modify: `src/combat/combat_world.hpp`
- Modify: `src/combat/combat_world.cpp`
- Modify: `src/combat/player_simulation.cpp`
- Modify: `src/combat/monster_pool.cpp`
- Modify: `src/combat/monster_ai_common.hpp`
- Modify: `src/combat/monster_ai_melee.cpp`
- Modify: `src/combat/monster_ai_ranged.cpp`
- Modify: `src/combat/monster_ai_special.cpp`
- Modify: `src/combat/CMakeLists.txt`
- Modify: `src/dungeon/room_combat_template.hpp`
- Modify: `src/dungeon/room_combat_template.cpp`
- Modify: `tests/combat/combat_config_tests.cpp`
- Modify: `tests/combat/player_health_tests.cpp`
- Modify: `tests/combat/movement_jump_tests.cpp`
- Modify: `tests/combat/monster_melee_tests.cpp`

**Produces:** Swift Pursuit、Abyss Bulwark、Abyss Fury、Heavy Steps、Exhausted Recovery、Life Sacrifice。

**Consumes:** `CombatEncounterConfig::abyss`，战斗层不得读取存档、room seed 或奖励状态。

- [ ] 在 `CombatEncounterConfig` 增加 `abyss::AbyssCombatConfig abyss{}`，并由 `make_combat_encounter_config` 显式传入。

- [ ] 将 `arpg_combat` PUBLIC 链接 `arpg_abyss`，因为 `combat_types.hpp` 的公共配置暴露 `AbyssCombatConfig`；确认 `arpg_abyss` 不反向链接 combat。

- [ ] 增加精确测试：

  - Swift：移动速度 115%，冷却 `ceil(base*8500/10000)`。
  - Bulwark：护甲值 130%，额外盾为实际最大生命 30%。
  - Fury：所有伤害 145%；telegraph/recovery/cooldown 为 `ceil(base*10000/14500)`；active ticks 不变。
  - Heavy Steps：仅地面水平移动 85%；跳跃、空中速度和攻击阶段不变。
  - Exhausted Recovery：进入时生命与护盾上限的 70%，未来资源恢复量 70%。
  - Life Sacrifice：最大生命 55%，当前生命按确认过的最近整数公式保持比例，至少 1；退出时反向映射，护盾不免费回满。

- [ ] 先运行并确认所有倍率仍为 100% 的失败结果。

- [ ] 实现统一整数缩放助手；不得在各 AI 文件复制浮点乘法。Fury 的 active ticks 明确绕过攻击速度缩放。Heavy Steps 只在 `PlayerState` 属于地面移动时缩放 `movement.x/y`，不修改空中速度或跳跃冲量。

- [ ] 所有 tick 缩放使用向上取整且结果至少 1 tick；深渊倍率与 Stage 9 affix profile 乘算，不能覆盖或重置已有 mighty/frenzy/swift/armored/shielding 数值。

- [ ] Fury 的伤害只经一个 `scale_monster_outgoing_damage(DamagePacket, basis_points)` 边界缩放一次；接触、投射物和怪物原生 hazard 都从该边界取得结果，禁止生成时和命中时重复乘 145%。Bulwark 同时提升 `MonsterAffixRuntimeProfile::armor_rating` 并把 `max_shield/current_shield` 增加实际 max HP 的 30%。

- [ ] 将所有“设置/恢复玩家资源”收口到 `CombatWorld::restore_player_resources(int hp, int barrier)`，由该函数应用 `player_resource_restore_bp`；初始 70% cap 只应用一次，跨 wave `reset_player_health=false` 不重复扣减。

- [ ] 运行：

```powershell
ctest --preset windows-msvc-debug -R "combat.units|dungeon.units" --output-on-failure
```

预期：新增规则测试通过，既有 J/K/L/WASD、浮空、闪避、护甲与词缀测试不变。

- [ ] 提交：

```powershell
git add src/combat src/dungeon/room_combat_template.* tests/combat
git commit -m "feat: apply abyss combat stat rules"
```

---


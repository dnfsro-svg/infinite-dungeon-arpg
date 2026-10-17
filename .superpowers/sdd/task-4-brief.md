### Task 4: 制作玩家与八种怪物帧，并替换角色绘制

**Files:**
- Create: `assets/stage12/actors.png`
- Create: `assets/stage12/actors-source.md`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/material_animation.cpp`
- Modify: `tests/platform/material_animation_tests.cpp`
- Create: `tests/platform/stage12_actor_render_tests.cpp`

**Interfaces:**
- Consumes: 现有 `CombatSnapshot`、`MonsterSnapshot`、`MaterialPack::draw`。
- Produces: `bool draw_material_actor(...) noexcept`；返回 `false` 时调用现有 `draw_monster_silhouette` 或玩家几何绘制。

- [ ] **Step 1: 写出所有状态映射和镜像测试**

```cpp
ARPG_TEST(player_attack_actions_have_distinct_sprites) {
    ARPG_REQUIRE(select_player_sprite(combat::PlayerState::attack_active, combat::AttackId::j1)
        != select_player_sprite(combat::PlayerState::attack_active, combat::AttackId::launcher));
}

ARPG_TEST(monster_phase_mapping_covers_all_existing_ids) {
    for (const auto id : all_material_monster_ids()) {
        ARPG_REQUIRE(select_monster_sprite(id, combat::MonsterAiPhase::telegraph)
            != MaterialSpriteId::missing);
    }
}
```

- [ ] **Step 2: 构建并确认新增测试在动画帧缺失前失败**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests`

Expected: FAIL；失败消息指出缺少攻击或怪物阶段帧。

- [ ] **Step 3: 生成、清理并接入角色图集**

制作固定外观玩家的待机、移动、J1/J2/J3、L 上挑、跳跃上升/下降、空中 J、落地、受击和死亡帧；制作八种现有怪物的 idle/move/telegraph/active/recovery/cooldown/defeated 帧。`actor_renderer.cpp` 保持现有插值、阴影、Y 排序、警示、血条、词缀轮廓和调试体积，只将玩家几何与怪物剪影替换为图集帧。左向仅镜像右向帧；词缀以既有效果叠加。

- [ ] **Step 4: 运行输入、战斗与角色视觉回归**

Run: `ctest --test-dir out/build/windows-msvc-debug -R "combat.units|platform.units|stage11b.settings_formal" --output-on-failure`

Expected: PASS；J/K/L/WASD、J 三段、L 上挑浮空和空中 J 的状态选择均正确，无输入延迟回归。

- [ ] **Step 5: 提交 12-C**

```powershell
git add assets/stage12/actors.png assets/stage12/actors-source.md src/platform/raylib/material_manifest.hpp src/platform/raylib/material_animation.cpp src/platform/raylib/actor_renderer.cpp tests/platform/material_animation_tests.cpp tests/platform/stage12_actor_render_tests.cpp
git commit -m "feat: add stage 12 comic actors"
```

## 12-D：特效与最终整合


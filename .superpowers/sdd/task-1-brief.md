### Task 1: 建立清单类型、纯校验和动画映射

**Files:**
- Create: `src/platform/raylib/material_asset_types.hpp`
- Create: `src/platform/raylib/material_manifest.hpp`
- Create: `src/platform/raylib/material_asset_validation.hpp`
- Create: `src/platform/raylib/material_asset_validation.cpp`
- Create: `src/platform/raylib/material_animation.hpp`
- Create: `src/platform/raylib/material_animation.cpp`
- Create: `tests/platform/material_asset_validation_tests.cpp`
- Create: `tests/platform/material_animation_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Consumes: `combat::PlayerState`、`combat::AttackId`、`combat::MonsterId`、`combat::MonsterAiPhase`。
- Produces: `MaterialSpriteId select_player_sprite(...) noexcept` 与 `MaterialSpriteId select_monster_sprite(...) noexcept`，供角色渲染器调用。

- [ ] **Step 1: 写出清单和映射失败测试**

```cpp
ARPG_TEST(material_manifest_rejects_frame_outside_atlas) {
    const MaterialAtlasDefinition atlas{MaterialAtlasId::actors, 256, 256, 4U * 256U * 256U};
    const MaterialFrameDefinition frame{MaterialSpriteId::player_idle, MaterialAtlasId::actors,
        {240, 0, 32, 32}, {16, 30}, 100};
    ARPG_REQUIRE(!validate_material_frame(atlas, frame).valid);
}

ARPG_TEST(material_animation_selects_launcher_active) {
    ARPG_REQUIRE(select_player_sprite(combat::PlayerState::attack_active,
        combat::AttackId::launcher) == MaterialSpriteId::player_launcher);
}
```

- [ ] **Step 2: 构建并运行只包含新测试的 `platform.units`，确认它们因符号不存在而失败**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests; ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure`

Expected: 编译失败，错误指向未定义的材质清单接口。

- [ ] **Step 3: 实现最小、完整的公开类型和选择器**

```cpp
enum class MaterialAtlasId : std::uint8_t { environment, actors, effects_ui, count };
enum class MaterialSpriteId : std::uint16_t {
    missing, player_idle, player_move, player_j1, player_j2, player_j3,
    player_launcher, player_jump_rise, player_jump_fall, player_air_j,
    player_landing, player_hurt, player_dead, fire_bomber_idle,
    fire_charger_idle, water_bulwark_idle, water_support_idle,
    lightning_shooter_idle, lightning_dasher_idle, chaos_chaser_idle,
    chaos_hazard_idle, count
};

struct MaterialFrameDefinition final {
    MaterialSpriteId id{MaterialSpriteId::missing};
    MaterialAtlasId atlas{MaterialAtlasId::actors};
    Rectangle source{};
    Vector2 foot_anchor{};
    std::uint16_t duration_ms{};
};
```

实现 `validate_material_frame`，拒绝零尺寸、负坐标、帧越界、无效锚点、重复资源 ID、图集超过 2048px 和总内存超过 64 MiB 的清单。实现 `select_player_sprite` 覆盖所有八个玩家状态和五个攻击 ID；`select_monster_sprite` 覆盖八个怪物及 idle/move/telegraph/active/recovery/cooldown/defeated。

- [ ] **Step 4: 运行单元测试并验证核心边界**

Run: `ctest --test-dir out/build/windows-msvc-debug -R "platform.units|architecture.(core|combat|dungeon)_no_raylib" --output-on-failure`

Expected: PASS；选择器完全覆盖既有枚举，核心模块仍无 raylib 依赖。

- [ ] **Step 5: 提交 12-A 的纯数据基础**

```powershell
git add src/platform/raylib/material_asset_types.hpp src/platform/raylib/material_manifest.hpp src/platform/raylib/material_asset_validation.* src/platform/raylib/material_animation.* tests/platform/material_*_tests.cpp src/platform/raylib/CMakeLists.txt tests/platform/CMakeLists.txt
git commit -m "feat: add stage 12 material manifest contracts"
```


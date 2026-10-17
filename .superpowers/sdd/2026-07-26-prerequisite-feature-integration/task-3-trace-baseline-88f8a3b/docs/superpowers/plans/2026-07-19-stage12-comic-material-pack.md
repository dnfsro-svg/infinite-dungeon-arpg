# Stage 12 清晰漫画化材质包 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 以三个内置纹理图集替换当前程序图形，交付一套低资源、清晰漫画化且完整可玩的地下城材质包。

**Architecture:** raylib 平台层持有 `MaterialPack`，在窗口生命周期内一次加载和卸载纹理。编译期清单把稳定资源 ID 映射为图集帧和脚底锚点；纯函数选择器把现有快照状态映射为资源 ID，缺少纹理或帧时保留现有程序绘制回退。

**Tech Stack:** C++17、raylib 6.0、CMake/Ninja/CTest、现有自定义测试框架。

## Global Constraints

- 使用 C++17 与 raylib 6.0；不增加第三方运行时依赖。
- 核心、战斗、地下城和持久化模块不得包含 raylib 或资源路径。
- 图集仅为内置默认包；不提供 MOD、换装或外部覆盖。
- 图集单边不超过 2048px，三张图集总 RGBA 纹理内存不得超过 64 MiB。
- 加载和卸载仅发生在资源生命周期；渲染热路径不得读文件、创建纹理或解析清单。
- 现有程序绘制必须作为按类别回退，且只记录一次加载警告。
- 不修改输入、碰撞、伤害、AI、固定步长、门、洞口或掉落规则。

---

## File Structure

- `assets/stage12/`: 发布随附的 `environment.png`、`actors.png`、`effects_ui.png` 与来源说明。
- `src/platform/raylib/material_asset_types.hpp`: 图集类别、稳定资源 ID、帧定义和校验结果。
- `src/platform/raylib/material_manifest.hpp`: 编译期帧表、图集描述和资源查找函数。
- `src/platform/raylib/material_pack.hpp/.cpp`: raylib 纹理的加载、回退状态、绘制和释放。
- `src/platform/raylib/material_animation.hpp/.cpp`: 快照到资源 ID 的纯动画选择器。
- `src/platform/raylib/material_asset_validation.hpp/.cpp`: 不依赖窗口的清单几何和预算校验。
- `src/platform/raylib/room_renderer.cpp`: 用环境图集替换灰盒环境，保留现有门、洞口、掉落位置和规则。
- `src/platform/raylib/actor_renderer.cpp`: 用角色图集替换玩家和八种怪物剪影，保留投影、Y 排序、预警和调试绘制。
- `src/platform/raylib/combat_renderer.hpp/.cpp`: 让渲染器拥有 `MaterialPack` 并把它传入房间、角色和特效绘制。
- `tests/platform/material_asset_*_tests.cpp`: 清单、选择器、回退和资源预算的无窗口测试。
- `tests/platform/stage12_material_formal_game_validation.cpp`: 实际 raylib 场景证据生成器。
- `tests/platform/stage12_material_validator.ps1`: 截图、资产和证据目录的结构校验。

## 12-A：资源基础设施

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

### Task 2: 实现 `MaterialPack` 生命周期、发布复制和程序回退

**Files:**
- Create: `src/platform/raylib/material_pack.hpp`
- Create: `src/platform/raylib/material_pack.cpp`
- Create: `assets/stage12/README.md`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `tests/platform/material_asset_validation_tests.cpp`

**Interfaces:**
- Consumes: `material_manifest.hpp` 的图集定义与 `raylib::LoadTexture`。
- Produces: `bool MaterialPack::load() noexcept`、`void MaterialPack::unload() noexcept`、`bool MaterialPack::available(MaterialAtlasId) const noexcept` 与 `void MaterialPack::draw(MaterialSpriteId, Vector2, bool) const noexcept`。

- [ ] **Step 1: 写出回退和重复释放测试**

```cpp
ARPG_TEST(material_pack_falls_back_when_atlas_is_unavailable) {
    MaterialPackState state{};
    state.set_available(MaterialAtlasId::actors, false);
    ARPG_REQUIRE(!state.can_draw(MaterialSpriteId::player_idle));
}

ARPG_TEST(material_pack_allows_repeated_shutdown) {
    MaterialPackState state{};
    state.reset();
    state.reset();
    ARPG_REQUIRE(!state.any_available());
}
```

- [ ] **Step 2: 运行测试，确认新增状态类型尚未定义而失败**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests`

Expected: 编译失败，错误指出 `MaterialPackState` 未定义。

- [ ] **Step 3: 实现加载、卸载、绘制和发布复制**

`MaterialPack::load` 必须依次加载三张内置 PNG，验证实际尺寸与编译期清单一致；单张失败只禁用对应类别并记录一次警告。`draw` 在类别可用时用 `DrawTexturePro` 按脚底锚点绘制；不可用时返回 `false`，由现有调用点继续程序绘制。`unload` 只对 `IsTextureReady` 的纹理调用 `UnloadTexture`，然后清空状态。CMake 使用 `add_custom_command(TARGET arpg_game POST_BUILD ...)` 把 `assets/stage12` 复制到 `bin/assets/stage12`。

- [ ] **Step 4: 运行资源生命周期和发布布局验证**

Run: `cmake --build --preset windows-msvc-debug; ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure`

Expected: PASS；可执行目录包含三张图集和来源说明；重复关闭不会崩溃。

- [ ] **Step 5: 提交完整 12-A**

```powershell
git add assets/stage12 src/platform/raylib/material_pack.* src/platform/raylib/combat_renderer.* src/platform/raylib/CMakeLists.txt CMakeLists.txt tests/platform/material_asset_validation_tests.cpp
git commit -m "feat: add stage 12 material pack lifecycle"
```

## 12-B：地下城环境包

### Task 3: 制作环境图集并替换房间、门和洞口表现

**Files:**
- Create: `assets/stage12/environment.png`
- Create: `assets/stage12/environment-source.md`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `tests/platform/material_animation_tests.cpp`
- Create: `tests/platform/stage12_environment_render_tests.cpp`

**Interfaces:**
- Consumes: `MaterialPack::draw`、`DungeonSnapshot`、`DoorRenderDecision`、`HoleVisualMode`。
- Produces: 以现有投影位置绘制的环境材质；任何环境帧不可用时返回现有灰盒、矩形门和椭圆洞口。

- [ ] **Step 1: 写出环境资源选择失败测试**

```cpp
ARPG_TEST(environment_maps_each_dungeon_element_to_a_floor_sprite) {
    ARPG_REQUIRE(select_floor_sprite(dungeon::DungeonElement::fire) != MaterialSpriteId::missing);
    ARPG_REQUIRE(select_floor_sprite(dungeon::DungeonElement::chaos) != MaterialSpriteId::missing);
}
```

- [ ] **Step 2: 编译测试，确认环境资源选择器未实现而失败**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests`

Expected: 编译失败，错误指向 `select_floor_sprite`。

- [ ] **Step 3: 生成并清理环境图集，接入 `room_renderer.cpp`**

创建原创环境图集，包含暗色地板、墙体、四元素重点色门框、锁门内衬、深渊标记、洞口和少量装饰。以现有 `project_render_world` 计算的位置、缩放和门/洞口状态绘制纹理；保留现有标签、箭头、`Press E to descend` 提示和所有交互判断。`environment-source.md` 记录生成来源、清理步骤、图集尺寸、RGBA 内存和每个帧族。

- [ ] **Step 4: 运行房间回归与实际截图检查**

Run: `ctest --test-dir out/build/windows-msvc-debug -R "platform.units|stage11d.loot_formal" --output-on-failure`

Expected: PASS；截图中四向门、元素提示、深渊标识、掉落和洞口均可见，洞口 E 范围不变。

- [ ] **Step 5: 提交 12-B**

```powershell
git add assets/stage12/environment.png assets/stage12/environment-source.md src/platform/raylib/material_manifest.hpp src/platform/raylib/room_renderer.cpp tests/platform/material_animation_tests.cpp tests/platform/stage12_environment_render_tests.cpp
git commit -m "feat: add stage 12 comic dungeon environment"
```

## 12-C：角色与怪物包

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

### Task 5: 制作效果图集、掉落图标和正式验证

**Files:**
- Create: `assets/stage12/effects_ui.png`
- Create: `assets/stage12/effects-ui-source.md`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/material_asset_validation.cpp`
- Create: `tests/platform/stage12_material_formal_game_validation.cpp`
- Create: `tests/platform/stage12_material_validator.ps1`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `docs/validation/stage12-comic-material-pack.md`

**Interfaces:**
- Consumes: `CombatFeedback`、`GroundLootView`、`MaterialPack` 和现有截图基础设施。
- Produces: 一套可自动验证的实际 raylib 证据，证明 1280×720 与 1920×1080 下的环境、角色、效果和回退。

- [ ] **Step 1: 写出效果层级、掉落稀有度和预算失败测试**

```cpp
ARPG_TEST(effect_manifest_stays_within_texture_budget) {
    ARPG_REQUIRE(validate_material_manifest(default_material_manifest()).valid);
}

ARPG_TEST(loot_rarity_uses_distinct_material_icons) {
    ARPG_REQUIRE(select_loot_sprite(items::ItemRarity::normal)
        != select_loot_sprite(items::ItemRarity::rare));
}
```

- [ ] **Step 2: 构建并确认未实现的效果选择器导致失败**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests`

Expected: 编译失败，错误指出 `select_loot_sprite` 未定义。

- [ ] **Step 3: 接入效果、物品图标和正式验证器**

制作火、水、电、混沌、命中火花、上挑轨迹、落地尘土、词缀光环与白/蓝/黄掉落图标。效果仍按现有渲染层级和事件时序绘制；物品标签与自动拾取阈值不改变。正式验证器必须生成两个分辨率的截图、清单校验报告、三图集尺寸和内存报告、资源损坏回退记录及输入/洞口回归记录。PowerShell 验证器拒绝缺失文件、错误分辨率、超过预算、未触发回退或未包含八种怪物证据的目录。

- [ ] **Step 4: 运行完整构建、测试和正式视觉验证**

Run: `./scripts/Test.ps1 -Preset windows-msvc-debug; ./scripts/Test.ps1 -Preset windows-msvc-release; ctest --test-dir out/build/windows-msvc-release -R stage12 --output-on-failure`

Expected: 全部 PASS；正式证据包含 1280×720 与 1920×1080 的真实 raylib 截图，资源回退被验证且游戏保持可运行。

- [ ] **Step 5: 写入验证记录并提交 12-D**

```powershell
git add assets/stage12/effects_ui.png assets/stage12/effects-ui-source.md src/platform/raylib tests/platform docs/validation/stage12-comic-material-pack.md
git commit -m "feat: complete stage 12 comic material pack"
```

## Plan Self-Review

- 规格覆盖：12-A 的清单/加载/回退，12-B 的环境，12-C 的玩家和八种怪物，12-D 的特效、掉落、正式验证均有独立任务。
- 占位符扫描：计划不含占位标记、未定设计或“类似此前任务”的引用；每项代码工作给出路径、接口、失败测试、验证命令和提交范围。
- 类型一致性：所有任务使用同一套 `MaterialAtlasId`、`MaterialSpriteId`、`MaterialPack` 和 `MaterialFrameDefinition` 名称；后续任务只消费前序明确产出的接口。

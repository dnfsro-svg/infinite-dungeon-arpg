### Task 1: 静态中文展示目录

**Files:**
- Create: `src/platform/raylib/ui_text_catalog.hpp`
- Create: `src/platform/raylib/ui_text_catalog.cpp`
- Create: `tests/platform/ui_text_catalog_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: `items::MaterialId`、`items::ItemSlot`、`items::ItemRarity`、`passives::PassiveNodeId`、`combat::MonsterId`、`combat::MonsterAffixId`、`settings::SettingAction`、`skills::ActiveSkillId`。
- Produces: `UiTextId`、`ui_text()`、各稳定 ID 的中文展示函数和 `visit_ui_text_catalog()`。

- [ ] **Step 1: 写目录缺失的失败测试**

在 `ui_text_catalog_tests.cpp` 创建 5 个 case：固定文案、全部枚举映射、未知值、访问器覆盖、零分配。核心断言写成：

```cpp
ARPG_REQUIRE(std::strcmp(
    platform::ui_text(platform::UiTextId::pause_title), u8"暂停") == 0);
ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(1U),
    u8"精炼之刃") == 0);
ARPG_REQUIRE(std::strcmp(platform::ui_material_name(
    arpg::items::MaterialId::reinforcement_stone), u8"强化石") == 0);
ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_short_name(
    arpg::combat::MonsterAffixId::death_blast), u8"死爆") == 0);
ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(255U),
    u8"未知") == 0);
```

注册 `ui_text_catalog_suite()`，并增加环境开关 `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY`。该入口只运行本计划新增的三个 suite，最终精确 case 数为 15。

- [ ] **Step 2: 运行测试并确认 RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

Expected: 编译失败，错误明确指出 `ui_text_catalog.hpp` 或 `UiTextId` 不存在；不得接受无关编译错误作为 RED。

- [ ] **Step 3: 实现目录接口和完整访问器**

头文件公开以下精确接口：

```cpp
enum class UiTextId : std::uint16_t {
    pause_title, pause_continue, pause_settings, pause_quit,
    settings_title, settings_audio, settings_display, settings_bindings,
    settings_restore_defaults, settings_apply, settings_cancel,
    settings_saved_prefix, settings_unknown,
    hud_health, hud_barrier, hud_experience, hud_level,
    save_saving, save_saved, save_recovered, save_error,
    recovery_title, recovery_invalid_slots,
    inventory_title, equipment_title, item_detail_title,
    passive_title, passive_points, passive_close,
    debug_ready, debug_unavailable,
    unknown,
    count,
};

[[nodiscard]] const char* ui_text(UiTextId id) noexcept;
[[nodiscard]] const char* ui_item_base_name(std::uint8_t base_id) noexcept;
[[nodiscard]] const char* ui_material_name(items::MaterialId id) noexcept;
[[nodiscard]] const char* ui_item_slot_name(items::ItemSlot id) noexcept;
[[nodiscard]] const char* ui_item_rarity_name(items::ItemRarity id) noexcept;
[[nodiscard]] const char* ui_item_affix_name(std::uint16_t affix_id) noexcept;
[[nodiscard]] const char* ui_passive_node_name(
    passives::PassiveNodeId id) noexcept;
[[nodiscard]] const char* ui_monster_name(combat::MonsterId id) noexcept;
[[nodiscard]] const char* ui_monster_affix_name(
    combat::MonsterAffixId id) noexcept;
[[nodiscard]] const char* ui_monster_affix_short_name(
    combat::MonsterAffixId id) noexcept;
[[nodiscard]] const char* ui_setting_action_name(
    settings::SettingAction id) noexcept;
[[nodiscard]] const char* ui_skill_name(skills::ActiveSkillId id) noexcept;

using UiTextVisitor = void (*)(const char*, void*) noexcept;
void visit_ui_text_catalog(UiTextVisitor visitor, void* context) noexcept;
```

`visit_ui_text_catalog()` 必须枚举全部 `UiTextId`、18 个装备底材、14 个材料、6 个槽位、3 个品质、全部实际物品词缀、64 个被动节点、8 个怪物、全部怪物词缀、10 个设置动作和全部主动技能；无效值统一返回“未知”。

- [ ] **Step 4: 运行目录测试并确认 GREEN**

Run:

```powershell
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
try {
  & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe'
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
  Remove-Item Env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY -ErrorAction SilentlyContinue
}
```

Expected: 当前已注册的目录 5 case 全部通过；UTF-8/字体 suite 尚未加入该定向入口。

- [ ] **Step 5: 提交目录任务**

```powershell
git add src/platform/raylib/ui_text_catalog.hpp src/platform/raylib/ui_text_catalog.cpp src/platform/raylib/CMakeLists.txt tests/platform/ui_text_catalog_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git diff --cached --check
git commit -m "feat: add static Chinese UI text catalog"
```

---


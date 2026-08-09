# Full Chinese UI Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 建立零运行时分配的简体中文展示目录、UTF-8 安全工具、统一字形计划和单实例共享 UI 字体，供全部玩家界面复用。

**Architecture:** 所有玩家可见字符串和稳定 ID 的中文展示名集中在 raylib 展示层；核心英文 key、枚举值和存档结构保持不变。宿主在窗口初始化后加载一次 Noto Sans CJK SC，所有 renderer 只借用同一个 `Font`。

**Tech Stack:** C++17、raylib 6.0、CMake、项目内 `test_framework.hpp`、Windows MSVC Debug/Release。

## Global Constraints

- 使用 C++17 与 raylib 6.0。
- 不修改玩法、存档格式、设置编码、稳定 ID、枚举值或版本号。
- 不引入 JSON、外部语言包、运行时映射容器或逐帧动态分配。
- 物理按键名可以保留英文；玩家可见的动作说明、状态和名称必须是中文。
- 字形容量固定为 640 个去重码点，单一 CJK 图集不超过 32 MiB，正式实例数固定为 1。
- 所有文本缓冲必须 NUL 终止并保持合法 UTF-8。
- Windows 构建时监控可用内存；正常使用 `--parallel 2`，仅在实际内存压力出现时降低并行度。

---

## File Structure

- Create: `src/platform/raylib/ui_text_catalog.hpp` — 中文静态目录及稳定 ID 展示映射接口。
- Create: `src/platform/raylib/ui_text_catalog.cpp` — 静态表、映射实现和目录访问器。
- Create: `src/platform/raylib/ui_utf8.hpp` — UTF-8 校验、拷贝、截断和格式化接口。
- Create: `src/platform/raylib/ui_utf8.cpp` — 有界 UTF-8 解码和固定缓冲实现。
- Create: `src/platform/raylib/shared_ui_font.hpp` — 统一字形计划和字体所有者。
- Create: `src/platform/raylib/shared_ui_font.cpp` — 字形收集、单次 `LoadFontEx` 和资源释放。
- Create: `tests/platform/ui_text_catalog_tests.cpp` — 目录完整性和稳定 ID 映射测试。
- Create: `tests/platform/ui_utf8_tests.cpp` — UTF-8 边界与零分配测试。
- Create: `tests/platform/shared_ui_font_tests.cpp` — 字形覆盖、容量、预算和所有权测试。
- Modify: `src/platform/raylib/death_overlay_font.hpp/.cpp` — 保留兼容入口，转发统一字形计划。
- Modify: `src/platform/raylib/hud_font.hpp/.cpp` — 只保留绘制选择和字号策略，不再维护第二份语料。
- Modify: `src/platform/raylib/hud_renderer.hpp/.cpp` — 借用共享字体，不再加载/卸载。
- Modify: `src/platform/raylib/death_overlay_renderer.hpp/.cpp` — 借用共享字体。
- Modify: `src/platform/raylib/pause_menu_renderer.hpp/.cpp` — 借用共享字体。
- Modify: `src/platform/raylib/combat_renderer.hpp/.cpp` — 接收共享字体并分发给内部 renderer。
- Modify: `src/platform/raylib/raylib_host.cpp` — 唯一拥有 `SharedUiFont` 并保证关闭顺序。
- Modify: `src/platform/raylib/CMakeLists.txt`、`tests/platform/CMakeLists.txt`、`tests/platform/platform_test_main.cpp` — 注册源文件和定向测试入口。

---

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

### Task 2: UTF-8 安全固定缓冲工具

**Files:**
- Create: `src/platform/raylib/ui_utf8.hpp`
- Create: `src/platform/raylib/ui_utf8.cpp`
- Create: `tests/platform/ui_utf8_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: NUL 终止的 UTF-8 输入和固定大小输出缓冲。
- Produces: `UiUtf8CopyResult`、`ui_utf8_is_valid()`、`ui_utf8_copy()`、`ui_utf8_fit_with_ellipsis()`、`ui_utf8_format()`。

- [ ] **Step 1: 写 6 个边界失败测试**

测试合法/非法序列、每个码点边界、中文省略号、格式化尾部修复、空目标和 10000 次零分配：

```cpp
char buffer[7]{};
const auto result = platform::ui_utf8_fit_with_ellipsis(
    buffer, sizeof(buffer), u8"设置界面");
ARPG_REQUIRE(result.truncated);
ARPG_REQUIRE(platform::ui_utf8_is_valid(buffer));
ARPG_REQUIRE(buffer[sizeof(buffer) - 1U] == '\0');
ARPG_REQUIRE(!platform::ui_utf8_is_valid("\xE8\xAE"));
```

- [ ] **Step 2: 运行并确认 RED**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2`

Expected: 编译失败于 `ui_utf8.hpp` 缺失。

- [ ] **Step 3: 实现严格有界 UTF-8 工具**

公开结构和函数：

```cpp
struct UiUtf8CopyResult final {
    std::size_t bytes_written{};
    bool input_was_valid{};
    bool truncated{};
};

[[nodiscard]] bool ui_utf8_is_valid(const char* text) noexcept;
[[nodiscard]] UiUtf8CopyResult ui_utf8_copy(
    char* destination, std::size_t capacity, const char* source) noexcept;
[[nodiscard]] UiUtf8CopyResult ui_utf8_fit_with_ellipsis(
    char* destination, std::size_t capacity, const char* source) noexcept;
[[nodiscard]] UiUtf8CopyResult ui_utf8_format(
    char* destination, std::size_t capacity, const char* format, ...) noexcept;
```

解码器逐字节验证过长编码、孤立续字节、代理区和 `> U+10FFFF`；非法码点写单字节 `?`。除 `capacity == 0` 外始终 NUL 终止。省略号只在能完整写入 UTF-8 `…` 时使用，否则输出空串。

- [ ] **Step 4: 运行 foundation 定向测试**

Run: 使用 Task 1 的 `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY` 命令。

Expected: 目录 5 case + UTF-8 6 case，共 11 case 通过。

- [ ] **Step 5: 提交 UTF-8 工具**

```powershell
git add src/platform/raylib/ui_utf8.hpp src/platform/raylib/ui_utf8.cpp src/platform/raylib/CMakeLists.txt tests/platform/ui_utf8_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git diff --cached --check
git commit -m "feat: add bounded UTF-8 UI formatting"
```

---

### Task 3: 统一字形计划

**Files:**
- Create: `src/platform/raylib/shared_ui_font.hpp`
- Create: `src/platform/raylib/shared_ui_font.cpp`
- Create: `tests/platform/shared_ui_font_tests.cpp`
- Modify: `src/platform/raylib/death_overlay_font.hpp`
- Modify: `src/platform/raylib/death_overlay_font.cpp`
- Modify: `src/platform/raylib/hud_font.hpp`
- Modify: `src/platform/raylib/hud_font.cpp`
- Modify: CMake/test registration files from Task 1.

**Interfaces:**
- Consumes: `visit_ui_text_catalog()` 和 `ui_utf8` 解码。
- Produces: `UiGlyphPlan`、覆盖检查、图集字节计算和 `SharedUiFont` 类型。

- [ ] **Step 1: 写 4 个字形计划失败测试**

断言全目录覆盖、码点唯一、容量/预算、候选字体：

```cpp
const platform::UiGlyphPlan plan = platform::ui_glyph_plan();
ARPG_REQUIRE(plan.codepoint_count > 384U);
ARPG_REQUIRE(plan.codepoint_count <= platform::kUiGlyphCapacity);
ARPG_REQUIRE(platform::ui_glyph_plan_covers_text(plan, u8"混沌之眼"));
ARPG_REQUIRE(platform::kUiFontAtlasInstanceCount == 3U);
ARPG_REQUIRE(platform::kUiFontAtlasByteBudget == 32U * 1024U * 1024U);
```

遍历所有码点并断言不存在重复；候选路径必须包含 `assets/fonts/NotoSansCJKsc-Medium.otf`。

- [ ] **Step 2: 运行并确认 RED**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2`

Expected: 编译失败于 `UiGlyphPlan` 缺失。

- [ ] **Step 3: 实现固定字形计划和兼容壳**

在 `shared_ui_font.hpp` 定义：

```cpp
inline constexpr std::size_t kUiGlyphCapacity = 640U;
// Task 3 only unifies the glyph plan. The three existing renderer-owned
// atlases remain real until Task 4 atomically transfers ownership.
inline constexpr std::size_t kUiFontAtlasInstanceCount = 3U;
inline constexpr std::size_t kUiFontAtlasByteBudget =
    32U * 1024U * 1024U;
inline constexpr int kUiFontSourceBaseSize = 96;

struct UiGlyphPlan final {
    std::array<const char*, 1U> candidate_paths{};
    std::size_t candidate_count{};
    std::array<int, kUiGlyphCapacity> codepoints{};
    std::size_t codepoint_count{};
    bool capacity_exceeded{};
};
```

先加入 ASCII 32–126，再访问完整中文目录并去重。超过容量设置 `capacity_exceeded=true`，覆盖检查返回 false。Task 3 的实例常量必须如实记录当前三个 renderer 各自拥有的图集；`death_overlay_font_plan()` 和 `hud_font_plan()` 转发同一个字形结果，不得继续手写补字。

- [ ] **Step 4: 运行 15 个 foundation case 并确认 GREEN**

Run: 使用 Task 1 的定向命令。

Expected: 目录 5 + UTF-8 6 + 字形 4，共 15 case、0 failure。

- [ ] **Step 5: 提交字形计划**

```powershell
git add src/platform/raylib/shared_ui_font.hpp src/platform/raylib/shared_ui_font.cpp src/platform/raylib/death_overlay_font.hpp src/platform/raylib/death_overlay_font.cpp src/platform/raylib/hud_font.hpp src/platform/raylib/hud_font.cpp src/platform/raylib/CMakeLists.txt tests/platform/shared_ui_font_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git diff --cached --check
git commit -m "feat: unify Chinese UI glyph planning"
```

---

### Task 4: 单实例共享字体生命周期

**Files:**
- Modify: `src/platform/raylib/shared_ui_font.hpp/.cpp`
- Modify: `src/platform/raylib/hud_renderer.hpp/.cpp`
- Modify: `src/platform/raylib/death_overlay_renderer.hpp/.cpp`
- Modify: `src/platform/raylib/pause_menu_renderer.hpp/.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp/.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/hud_font_tests.cpp`
- Modify: `tests/platform/hud_host_integration_tests.cpp`
- Modify: `tests/platform/stage12_material_formal_game_validation.cpp`
- Modify: `tests/platform/stage12_material_validator.ps1`
- Modify: `tests/platform/stage12_material_validator_self_test.ps1`
- Modify: `tests/platform/stage11d_renderer_integration_guard_test.cmake`

**Interfaces:**
- Consumes: `UiGlyphPlan`。
- Produces: 一个宿主持有的 `SharedUiFont`；renderer 的 `initialize(const SharedUiFont&)` 仅建立借用。

- [ ] **Step 1: 把现有字体测试改成单实例契约并观察 RED**

Task 4 在实现宿主唯一所有权的同一变更中，把 Task 3 的阶段性
`kUiFontAtlasInstanceCount == 3U` 原子切换为最终 `== 1U`；不得在 Task 3
提前低报仍由三个 renderer 独立加载的资源。

新增/更新断言：

```cpp
ARPG_REQUIRE(platform::kUiFontAtlasInstanceCount == 1U);
ARPG_REQUIRE(runtime.bundled_font_instance_count == 1U);
ARPG_REQUIRE(runtime.bundled_font_total_atlas_bytes
    == runtime.bundled_font_atlas_bytes);
```

源守卫还必须拒绝 `HudRenderer`、`DeathOverlayRenderer`、`PauseMenuRenderer` 中出现 `LoadFontEx` 或 `UnloadFont`。

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2`

Expected: 测试失败，显示当前实例数为 3 或 renderer 仍独立拥有字体。

- [ ] **Step 2: 实现唯一字体所有者**

`SharedUiFont` 精确公开：

```cpp
class SharedUiFont final {
public:
    ~SharedUiFont() noexcept;
    [[nodiscard]] bool initialize() noexcept;
    void shutdown() noexcept;
    [[nodiscard]] bool ready() const noexcept;
    [[nodiscard]] Font font() const noexcept;
    [[nodiscard]] std::size_t atlas_bytes() const noexcept;
    [[nodiscard]] const UiGlyphPlan& plan() const noexcept;
private:
    Font font_{};
    UiGlyphPlan plan_{};
    bool ready_{};
};
```

`run_raylib_host()` 在窗口和应用目录初始化成功后创建并初始化它；依次调用：

```cpp
SharedUiFont shared_ui_font;
if (!shared_ui_font.initialize()) {
    TraceLog(LOG_ERROR, "Bundled Chinese UI font initialization failed");
    return HostExitCode::window_initialization_failed;
}
renderer.initialize_resources(shared_ui_font);
pause_menu_renderer.initialize(shared_ui_font);
```

三个 renderer 只保存 `Font` 值和 `ready` 标志，不调用字体加载/卸载。关闭顺序固定为暂停 renderer、战斗 renderer、共享字体、窗口。

- [ ] **Step 3: 更新正式材质验证的真实资源统计**

`Stage12MaterialRuntimeStatus` 增加 `std::uint8_t bundled_font_instance_count{}`；记录真实 `atlas_bytes`、相同的 `total_atlas_bytes` 和实例数 1。PowerShell validator 期望统一改为容量 640、实例 1、总字节等于单图集字节。

- [ ] **Step 4: 运行定向和相关正式测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY -ErrorAction SilentlyContinue }
ctest --preset windows-msvc-debug --output-on-failure -R 'platform.units|stage12.material_formal'
```

Expected: foundation 15/15；相关平台和 Stage 12 字体验证全部通过；运行时实例数为 1，图集不超过 32 MiB。

- [ ] **Step 5: 提交共享生命周期**

```powershell
git add src/platform/raylib/shared_ui_font.hpp src/platform/raylib/shared_ui_font.cpp src/platform/raylib/hud_renderer.hpp src/platform/raylib/hud_renderer.cpp src/platform/raylib/death_overlay_renderer.hpp src/platform/raylib/death_overlay_renderer.cpp src/platform/raylib/pause_menu_renderer.hpp src/platform/raylib/pause_menu_renderer.cpp src/platform/raylib/combat_renderer.hpp src/platform/raylib/combat_renderer.cpp src/platform/raylib/raylib_host.cpp tests/platform/hud_font_tests.cpp tests/platform/hud_host_integration_tests.cpp tests/platform/stage12_material_formal_game_validation.cpp tests/platform/stage12_material_validator.ps1 tests/platform/stage12_material_validator_self_test.ps1 tests/platform/stage11d_renderer_integration_guard_test.cmake
git diff --cached --check
git commit -m "refactor: share one Chinese UI font atlas"
```

---

## Plan Completion Gate

Run:

```powershell
git diff --check
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY -ErrorAction SilentlyContinue }
```

Expected: 工作树无非预期改动，foundation 15 cases / 0 failures；共享字体只有一个资源所有者。随后进入 `2026-08-09-full-chinese-ui-menus-hud.md`。

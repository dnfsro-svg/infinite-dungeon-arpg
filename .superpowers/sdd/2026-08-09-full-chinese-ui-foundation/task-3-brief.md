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
ARPG_REQUIRE(platform::kUiFontAtlasInstanceCount == 1U);
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
inline constexpr std::size_t kUiFontAtlasInstanceCount = 1U;
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

先加入 ASCII 32–126，再访问完整中文目录并去重。超过容量设置 `capacity_exceeded=true`，覆盖检查返回 false。`death_overlay_font_plan()` 和 `hud_font_plan()` 转发同一个字形结果，不得继续手写补字。

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


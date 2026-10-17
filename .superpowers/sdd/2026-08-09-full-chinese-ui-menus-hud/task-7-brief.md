### Task 7: 中文 F1 调试面板

**Controller-resolved requirements:**
- 有战斗快照和转场无战斗快照时都固定输出 12 行；不得把转场路径缩成 9 行。
- `<1280` 单列 12 行，`>=1280` 两列各 6 行；800×450、1280×720、1920×1080 的列背板和全部行框必须在屏内。
- 每列使用不透明 `#0B1019` 背板；正文、警告、错误色对真实背板均至少 4.5:1，状态同时有中文文案，不只靠颜色。
- 真实绘制只借用唯一 `SharedUiFont`，使用 `draw_crisp_ui_text`；禁止 `DrawText`、`TextFormat`、`GetFontDefault` 和新字体加载。新增中文须进入唯一 catalog visitor，且 glyph plan 不溢出 640。
- F1 世界标签同属本任务：`Assist/Attack/Hurt` 分别为“辅助判定/攻击判定/受击判定”；怪物角色名使用 `ui_monster_name`，阶段全部中文；`DEFEATED` 为“已击败”；`M1/M2/M3` 显示为“词缀Ⅰ/Ⅱ/Ⅲ”。必要时可修改 `combat_view_math.cpp` 及其现有测试，这是完整汉化所需范围。
- 新增真实 source guard+self-test，锁住 fixed-buffer `ui_utf8_format`、12行模型、列背板、shared-font真实调用及世界调试标签；dead-code诱饵不得通过。
- 若恰新增 brief 的5个 platform case：当前 MENU_HUD 178→183，full expected 665→670，运行时仍保留 actual=expected+1 与 ecology 两项既有失败；不得追平吞掉基线。
- Stage11C formal 目标只编译/链接，本任务不运行耗时图形程序；不进入 Task8。

**Files:**
- Modify: `src/platform/raylib/debug_overlay_renderer.hpp`
- Modify: `src/platform/raylib/debug_overlay_renderer.cpp`
- Modify: `src/platform/raylib/debug_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Create: `tests/platform/debug_overlay_renderer_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/stage11c_hud_formal_game_validation.cpp`

**Interfaces:**
- Consumes: snapshot、runtime status、combat feedback、共享字体。
- Produces: 固定 12 行 `DebugOverlayView` 和响应式 `DebugOverlayLayout`。

- [ ] **Step 1: 写 5 个 F1 View/Layout 失败测试**

定义期望结构：

```cpp
struct DebugOverlayLine final {
    std::array<char, 192U> text{};
    Color color{};
};
struct DebugOverlayView final {
    std::array<DebugOverlayLine, 12U> lines{};
    std::size_t line_count{};
};
```

测试全部行 UTF-8 合法、无玩家英文标签；800 宽单列、1280/1920 两列；所有矩形在屏幕内；10000 次构建零分配；状态枚举“起手/生效/后摇/结束”等全覆盖。

- [ ] **Step 2: 运行并确认 RED**

Expected: 纯 View/Layout 接口不存在，当前 renderer 仍直接 `DrawText(TextFormat(...))`。

- [ ] **Step 3: 实现中文模型和共享字体渲染**

把数值格式化移入 `make_debug_overlay_view()`，通过 `ui_utf8_format()` 写固定行。`debug_overlay_layout()` 在宽度小于 1280 时单列，否则两列。renderer 先绘制半透明深色列背板，再用共享字体逐行绘制。世界盒标签改为“辅助判定、攻击判定、受击判定”。

- [ ] **Step 4: 运行定向测试并提交**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue }
git add src/platform/raylib/debug_overlay_renderer.hpp src/platform/raylib/debug_overlay_renderer.cpp src/platform/raylib/debug_renderer.cpp src/platform/raylib/actor_renderer.cpp src/platform/raylib/combat_renderer.cpp tests/platform/debug_overlay_renderer_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp tests/platform/stage11c_hud_formal_game_validation.cpp
git diff --cached --check
git commit -m "feat: localize responsive debug overlay"
```

---

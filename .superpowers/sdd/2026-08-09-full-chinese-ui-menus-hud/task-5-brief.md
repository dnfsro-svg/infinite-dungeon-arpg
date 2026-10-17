### Task 5: 房间、深渊、洞口和保存提示

**Files:**
- Modify: `src/platform/raylib/dungeon_view_math.hpp/.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp/.cpp`
- Modify: `tests/platform/dungeon_view_math_tests.cpp`
- Modify: `tests/platform/hud_layout_tests.cpp`
- Modify: `tests/platform/hud_render_plan_tests.cpp`
- Modify: `tests/platform/hole_prompt_binding_source_test.cmake`

**Interfaces:**
- Consumes: 统一中文目录、共享字体、结构化交互键。
- Produces: 中文生态/深渊/房间/保存枚举和带高对比背板的世界提示。

- [ ] **Step 1: 写全部枚举和动态键失败测试**

逐一覆盖四生态、深渊三危险等级、九条规则、九条效果说明、洞口四状态、保存四状态、房间阶段和出口方向。交互键 Q 的提示必须是 `Q：下潜` 或 `Q：进入下一层`，不得包含硬编码 E。

- [ ] **Step 2: 运行并确认 RED**

Expected: 现有 `FIRE/ABYSS/SEALED/DESCEND` 等英文断言失败。

- [ ] **Step 3: 替换展示映射并统一 CJK 绘制**

`dungeon_view_math` 只返回中文目录字符串。`room_renderer` 的门、洞口和深渊提示接收共享 `Font`，以 `MeasureTextEx` 测量，先绘制不透明 `#0B1019` 内底，再用 `draw_crisp_ui_text()` 绘制。更新 source guard 为“交互键来自宿主参数且源码中没有 `E: DESCEND`”。

- [ ] **Step 4: 运行测试并提交**

```powershell
ctest --preset windows-msvc-debug --output-on-failure -R 'platform.hole_prompt_binding_source'
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY -ErrorAction SilentlyContinue }
git add src/platform/raylib/dungeon_view_math.hpp src/platform/raylib/dungeon_view_math.cpp src/platform/raylib/room_renderer.cpp src/platform/raylib/combat_renderer.hpp src/platform/raylib/combat_renderer.cpp tests/platform/dungeon_view_math_tests.cpp tests/platform/hud_layout_tests.cpp tests/platform/hud_render_plan_tests.cpp tests/platform/hole_prompt_binding_source_test.cmake
git diff --cached --check
git commit -m "feat: localize dungeon interaction views"
```

---


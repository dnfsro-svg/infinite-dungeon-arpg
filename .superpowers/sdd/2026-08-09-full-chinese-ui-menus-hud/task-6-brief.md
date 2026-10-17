### Task 6: 中文存档恢复页

**Controller-resolved requirements:**
- 双槽说明精确为：`两个存档槽均无效。按 N 将其归档并创建新游戏。`
- 恢复页只改展示，不改 `recover_with_new_run()`、N 键、归档、保存或状态机语义。
- 新玩家文案必须进入 `ui_text_catalog`；`UiTextId` 0–43 保持原 ordinal，新 ID 只能从 44 起在 `count` 前尾部追加。
- 真实 host 调用必须借用唯一 `SharedUiFont`，禁止 `DrawText`、`TextFormat`、`GetFontDefault` 和新增字体加载。
- 800×450、1280×720、1920×1080 均须居中、屏内、实际背板对比度至少 4.5:1；固定缓冲与纯布局 10,000 次零分配。
- 必须同步现有 host recovery source guard，并增加可逆 self-test，锁定 `observe recovery -> shared-font draw -> present/capture` 的真实调用顺序。

**Files:**
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Modify: `tests/platform/hud_host_integration_tests.cpp`

**Interfaces:**
- Consumes: `SharedUiFont` 和 `DungeonRenderStatus`。
- Produces: `make_recovery_screen_view()` 纯模型与共享字体绘制。

- [ ] **Step 1: 写恢复页纯模型失败测试**

断言标题“需要恢复存档”、双槽说明、`存档错误 7`、800/1280/1920 居中面板和对比色。测试不得依赖打开 raylib 窗口。

- [ ] **Step 2: 抽取模型并替换默认 `DrawText`**

`draw_recovery_screen()` 接收 `Font`，使用共享 CJK 字体、深色全屏和居中文本；移除玩家路径中的 `SAVE RECOVERY REQUIRED`、`Both save slots...`、`Save error`。

- [ ] **Step 3: 运行测试并提交**

```powershell
git add src/platform/raylib/raylib_host.cpp src/platform/raylib/raylib_host.hpp tests/platform/dungeon_runtime_tests.cpp tests/platform/hud_host_integration_tests.cpp
git diff --cached --check
git commit -m "feat: localize save recovery screen"
```

---

## Task 9: raylib 地面表现、三栏背包与零补跑暂停

**Files:**
- Create: `src/platform/raylib/inventory_view_math.hpp`
- Create: `src/platform/raylib/inventory_view_math.cpp`
- Create: `src/platform/raylib/inventory_renderer.hpp`
- Create: `src/platform/raylib/inventory_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `src/core/fixed_step.hpp`
- Modify: `src/core/fixed_step.cpp`
- Test: `tests/core/fixed_step_tests.cpp`
- Create: `tests/platform/inventory_view_math_tests.cpp`
- Modify: `tests/platform/combat_key_bindings_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`

- [ ] **Step 1: 写暂停与视图失败测试**

固定步测试先 advance 半步、调用 clear accumulator、再 advance 半步，断言 0 tick；不得保留菜单前余量。视图测试覆盖 3 栏矩形、不同窗口缩放、虚拟网格首/末可见索引、筛选、滚动 clamp、单击/双击时间窗、Ctrl 最多 3 件、equipped 命中及 I/P 互斥。

- [ ] **Step 2: 运行 RED**

```powershell
cmake --build build-release --target arpg_core_tests arpg_platform_tests
ctest --test-dir build-release -R "core.units|platform.units" --output-on-failure
```

- [ ] **Step 3: 增加明确 fixed-step 暂停 API**

```cpp
void FixedStepRunner::clear_accumulator() noexcept;
```

只清 `accumulator_seconds_`，不清 total ticks、dropped seconds 或 invalid count。背包打开瞬间和关闭瞬间各调用一次；打开期间完全不调用 `advance()`。

- [ ] **Step 4: 实现纯视图模型**

```cpp
struct InventoryLayout final { Rectangle equipment; Rectangle grid; Rectangle detail; };
struct VisibleGridRange final { std::size_t first{}; std::size_t count{}; };

[[nodiscard]] InventoryLayout inventory_layout(int width, int height) noexcept;
[[nodiscard]] VisibleGridRange visible_grid_range(
    std::size_t filtered_count, int columns, float scroll_rows,
    float viewport_height, float cell_height) noexcept;
```

筛选索引只引用稳定 item vector，不改变获得顺序。比较面板分别求值当前完整 build 和假定替换完整 build，再做字段差。

比较求值必须复用核心层现有的 passive + equipment 组建规则。可以增加最小的只读 preview API 和“使用指定 EquipmentState 投影”重载，但不得为比较复制整个 `ItemOwnershipState::items`，尤其不得在每帧复制 65,535 件物品。比较结果仅在选中物品、装备状态或稳定代数变化时重算。

- [ ] **Step 5: 实现输入门控**

`I` 打开 inventory 仅当 passive overlay 关闭；`P` 仅当 inventory 关闭。inventory 打开时只处理 I/Esc、鼠标、Ctrl、滚轮和请求按钮；不采样/排队 WASD、J/K/L、E、门/洞/reset。pending/faulted 时按钮显示禁用状态。

打开或关闭 inventory 都调用 `clear_accumulator()`；inventory 打开时完全不调用 `advance()`。`Escape` 优先关闭 inventory/passive overlay，无 overlay 时才退出游戏，不得被 raylib 默认 Escape 直接绕过。inventory 内接受的 equip/unequip/recipe 请求必须在当前 UI 帧立即调用现有通用 `service_pending_save()`，不得因暂停 fixed step 而卡住。

背包只允许在 runtime 正常运行且无 passive overlay 时打开；若 runtime 进入 recovery/faulted/无 session，自动关闭并清 accumulator。

- [ ] **Step 6: 实现地面与三栏绘制**

地面图标按 slot 形状和 normal/magic/rare 颜色显示，位置由现有世界到屏幕变换；不显示鉴定状态。左栏绘制 6 槽和最终角色属性，中栏只遍历 visible range，右栏显示 ilvl、需求、固有、全部词缀、局部/全局标签与差值。提供全部/slot/rarity 筛选及 Combine 按钮。

布局在 800x450、1280x720 和 1920x1080 均不得越界或三栏重叠；中栏格子按获得顺序、只绘制可见区间。单击物品查看，双击装备；单击已装备槽请求卸下；Ctrl+左键切换三合一材料，最多 3 件。只有 3 件未装备、同 slot、同 rarity 时 Combine 可用。

- [ ] **Step 7: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_core_tests arpg_platform_tests arpg_game
ctest --test-dir build-release -R "core.units|platform.units" --output-on-failure
git diff --check
```

- [ ] **Step 8: 提交**

```powershell
git add src/core tests/core src/platform/raylib tests/platform
git commit -m "feat: add paused equipment inventory interface"
```

---

### Task 2: 让技能石布局消费共享壳层

**Files:**
- Modify: `src/platform/raylib/inventory_view_math.hpp`
- Modify: `src/platform/raylib/inventory_view_math.cpp`
- Modify: `src/platform/raylib/host_validation_stage17_runtime.cpp`
- Modify: `tests/platform/active_skill_loadout_view_tests.cpp`
- Modify: `tests/platform/input_latency_source_test.cmake`
- Modify: `tests/platform/stage12_material_formal_game_validation.cpp`

**Interfaces:**
- Consumes: Task 1 `InventoryShellLayout::left_panel/right_panel/action_bounds`。
- Produces:

```cpp
[[nodiscard]] ActiveSkillLoadoutLayout active_skill_loadout_layout(
    InventoryShellLayout shell) noexcept;
```

`remove_button` 必须等于 shell 的 `action_bounds`。Task 2 为保持独立编译，暂时保留 `ActiveSkillLoadoutLayout::{equipment_page_button,skill_stones_page_button}`，但新 overload 只能把它们分别复制自 `shell.tabs[equipment]` 与 `shell.tabs[skill_stones]`，不得独立计算；Task 4 在 renderer 和所有验证消费者迁完后再删除这两个临时字段。五个主槽是命中区，五个辅助槽只读且不进入 hit region。

- [ ] **Step 1: 把现有三分辨率 case 改成 shell 输入 RED**

```cpp
const auto shell = platform::inventory_shell_layout(width, height);
const auto layout = platform::active_skill_loadout_layout(shell);
ARPG_REQUIRE(layout.panel.x == shell.content.x);
ARPG_REQUIRE(layout.remove_button.x == shell.action_bounds.x);
ARPG_REQUIRE(layout.remove_button.y == shell.action_bounds.y);
ARPG_REQUIRE(layout.remove_button.width == shell.action_bounds.width);
ARPG_REQUIRE(layout.remove_button.height == shell.action_bounds.height);
for (Rectangle support : layout.support_slots) {
    ARPG_REQUIRE(inside(support, shell.left_panel));
}
for (Rectangle inventory : layout.inventory_slots) {
    ARPG_REQUIRE(inside(inventory, shell.right_panel));
}
```

保持 `pending_save_and_support_slots_never_emit_edit_commands()` 原断言，确保辅助槽不发请求；另用现有选择、装备、交换 case 证明新 shell 下右栏技能石、五个主动槽仍分别产生 `select/equip/swap`，`remove_button` 产生 `remove`，且 authority accepted 后才调用 `advance_active_skill_loadout_selection()`。

- [ ] **Step 2: 运行编译 RED**

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
if ($LASTEXITCODE -eq 0) { throw 'Task2 compile RED missing' }
```

Expected: 缺少 `active_skill_loadout_layout(InventoryShellLayout)` overload，而不是环境或测试语法失败。

- [ ] **Step 3: 实现新 overload 并更新验证消费者**

布局规则：左侧竖排五个主槽，当前槽的五个辅助槽位于左栏剩余区域；右栏排列未装备技能石。保留临时 `active_skill_loadout_layout(int,int)` wrapper，仅委托 `inventory_shell_layout(width,height)` 和新 overload，供 Task 4 前旧 renderer 编译；wrapper 不再独立计算坐标。旧 renderer 暂时读取的两个 page-button 字段也只镜像 shell tabs，不能形成第二坐标系。

把 `host_validation_stage17_runtime.cpp` 改为：

```cpp
const InventoryShellLayout shell = inventory_shell_layout(
    config.window_width, config.window_height);
const ActiveSkillLoadoutLayout layout = active_skill_loadout_layout(shell);
```

`host_validation_stage17_runtime.cpp` 中两处打开技能页的点击坐标都直接改为 `shell.tabs[static_cast<std::size_t>(InventoryPage::skill_stones)]`，不再消费临时字段。同步更新 `platform.input_latency_source` 的 exact token 和 reversible mutations；`stage12_material_formal_game_validation.cpp` 的纯 layout 检查也消费 shell，不运行窗口。

- [ ] **Step 4: 构建受影响目标并做定向 GREEN**

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug `
  --target arpg_platform_tests arpg_stage12_material_formal `
    arpg_stage17_skill_stones_game_validation `
  --parallel 2
if ($LASTEXITCODE -ne 0) { throw 'Task2 focused build failed' }
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
if ($LASTEXITCODE -ne 0) { throw 'Task2 inventory selector failed' }
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error `
  --parallel 1 -R '^platform\.input_latency_source$'
if ($LASTEXITCODE -ne 0) { throw 'Task2 input latency guard failed' }
```

Expected: targets compile/link；selector `120/0`；source guard `1/1`。不要运行 stage12/stage17 窗口。

- [ ] **Step 5: 审查并提交 Task 2**

```powershell
git diff --check
git add src/platform/raylib/inventory_view_math.hpp `
  src/platform/raylib/inventory_view_math.cpp `
  src/platform/raylib/host_validation_stage17_runtime.cpp `
  tests/platform/active_skill_loadout_view_tests.cpp `
  tests/platform/input_latency_source_test.cmake `
  tests/platform/stage12_material_formal_game_validation.cpp
git diff --cached --check
git commit -m "refactor(ui): place skill stones in inventory shell"
```

---


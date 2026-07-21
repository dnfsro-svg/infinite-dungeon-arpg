# Stage 11-D Task 6 报告

## 状态

完成 Renderer and Draft Preview Integration，并按真实 RED -> GREEN 执行 TDD。范围停在 ground loot 图标/标签渲染、HUD 字体覆盖和 Host draft/committed policy separation；未实现 pickup feedback（Task 7）或 formal evidence（Task 9）。

## 实现

- `CombatRenderPlan` 每次由 `CombatRenderer::draw` 构建一次，内部只拥有一个预构建 `GroundLootView`。
- room icon 与 HUD label 两个 stage 直接共享同一个 `CombatRenderPlan::ground_loot`；两个 pass 不再各自过滤或重建 ordinal 集。
- production render stage 顺序由可测 plan 驱动：`room -> actors -> ground_loot_labels -> normal_hud`。room 与 actors 在 `BeginMode2D` 内；标签在 actors 后结束 world mode，再由正常 HUD 绘制前呈现。
- `room_renderer.cpp` 不再遍历所有 snapshot 掉落绘制图标，而是只按传入 View 的 ordinal 查找对应 snapshot item，保留 slot/rarity/world projection 的实际图标绘制。
- `HudRenderer::draw_ground_loot(const GroundLootView&)` 复用 HUD 已加载 Font、HUD palette 和 `DrawTextEx`；标签路径不使用 `TextFormat`、`std::string` 或 `std::vector`。
- `hud_font.cpp` 在 HUD shared font plan 中补齐 `普通魔法稀有已拾取未知装备` 的 codepoint，并将完整短语列入 required text coverage。
- 新增 `CombatRenderer::set_loot_filter_mode(settings::LootFilterMode) noexcept`，仅控制 `draw` 的表现 plan。
- Host 仅在 `PauseScreen::settings` 向 renderer 发送 `pause_menu.draft.loot_filter_mode`；closed/root/capture/quit-confirm 都发送 `live_settings` 的已提交 mode。
- `PauseCommand::preview` 对音量、窗口和 VSync 继续 live preview，但构造 preview settings 时强制保留 `pause_menu.committed.loot_filter_mode`。因此 `DungeonRuntime::fixed_tick` 仍读取 `loot_pickup_policy(live_settings.loot_filter_mode)`，且该字段的语义现在真实等于 committed policy。
- Apply 成功后 committed/draft/live 一起发布新 mode；Cancel 和可成功回滚的 Apply failure 恢复旧 mode。Apply failure 仍保留其他 draft 字段用于既有重试流程，只将表现相关 loot mode 复位为 committed。

## TDD 证据

### RED 1：Host / render plan API

先只修改测试，加入 settings-only draft selection、renderer/live policy separation、单 View reuse、ordinal 与 stage ordering 断言，未修改生产接口。

共享 `out/build/task5-debug` 受当前执行身份 ACL 影响，出现 `.ninja_lock permission denied`，因此使用独立构建目录 `E:/game/task6-build`：

```powershell
cmd.exe /d /s /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake -S . -B E:/game/task6-build -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug -DFETCHCONTENT_SOURCE_DIR_RAYLIB=E:/game/.worktrees/stage11d-loot-filter/out/build/windows-msvc-debug/_deps/raylib-src && cmake --build E:/game/task6-build --target arpg_platform_tests -- -j1'
```

有效 RED：

```text
pause_host_gate_tests.cpp: error C2039/C3861:
renderer_loot_filter_mode 不是 arpg::platform 的成员 / 找不到标识符
```

补最小 Host selection helper 后继续构建，得到下一条有效 RED：

```text
hud_host_integration_tests.cpp: error C2039/C3861:
CombatRenderPlan、make_combat_render_plan、ground-loot stage 接口、
CombatRenderStage 尚不存在
```

失败原因准确来自 Task 6 production API 与 render plan 尚不存在。

### RED 2：HUD glyph coverage

完成最小 renderer/Host 接线并成功编译后运行：

```powershell
ctest --test-dir E:/game/task6-build -R '^platform\.units$' -V
```

有效 RED：

```text
[FAIL] hud_font.ground loot Chinese coverage:
death_overlay_font_covers_text(plan.shared, "普通魔法稀有已拾取未知装备")
```

同时 suite case-count guard 从 253 报告实际 257，这是新增 4 个用例后的预期登记更新。

### GREEN

补齐 HUD shared glyph plan 并更新 case-count 后：

```powershell
cmake --build E:/game/task6-build --target arpg_platform_tests -- -j1
ctest --test-dir E:/game/task6-build -R '^platform\.units$' -V
```

结果：`platform.units` 1/1 CTest 通过，257 cases、0 failures；100k GroundLootView 探针仍为 allocations=0。

Task 6 新增/扩展的测试覆盖：

- 一个 `CombatRenderPlan` 只拥有一个 View；room icon 与 HUD label 直接读取同一个 `plan.ground_loot`。
- `magic_or_better` 下 normal ordinal 被排除，icon/label 共用的 ordinal 为 20、30。
- production stage 顺序固定为 room、actors、ground labels、normal HUD。
- settings screen 使用 draft mode，其余所有 pause screens 使用 committed/live mode。
- Preview 后 renderer 可见 rare-only draft，但 `live_settings` 与 runtime pickup policy 保持 committed。
- Cancel 恢复 renderer old mode；Apply failure 恢复 live/draft renderer mode；Apply 成功后 live/committed 才发布新 mode。
- HUD-owned shared font 覆盖 `普通魔法稀有已拾取未知装备`。

## focused 回归

```powershell
ctest --test-dir E:/game/task6-build -R "^(platform\.pause_menu_state_boundary|stage11b\.architecture\.settings_boundaries|stage11c\.architecture\.hud_boundaries(_self_test)?|platform\.host_input_source|stage11b\.settings_evidence_guard(_self_test)?|stage11c\.hud_evidence_guard(_self_test)?)$" --output-on-failure
```

结果：9/9 通过、0 failures。包括 Task 4 要求的 `stage11b.settings_evidence_guard` 与 mutation self-test；唯一 fixed tick 仍为：

```cpp
runtime.fixed_tick(step_movement,
    loot_pickup_policy(live_settings.loot_filter_mode));
```

```powershell
cmake --build E:/game/task6-build --target arpg_settings_tests -- -j1
ctest --test-dir E:/game/task6-build -R '^settings\.units$' --output-on-failure
```

结果：`settings.units` 1/1 通过、0 failures。

```powershell
cmake --build E:/game/task6-build --target arpg_stage11c_hud_stress -- -j1
ctest --test-dir E:/game/task6-build -R '^stage11c\.hud_stress\.zero_alloc_100k$' --output-on-failure
```

结果：HUD stress 1/1 通过、0 failures。

## 文件

- `src/platform/raylib/combat_renderer.hpp`
- `src/platform/raylib/combat_renderer.cpp`
- `src/platform/raylib/room_renderer.cpp`
- `src/platform/raylib/hud_renderer.hpp`
- `src/platform/raylib/hud_renderer.cpp`
- `src/platform/raylib/hud_font.cpp`
- `src/platform/raylib/raylib_host.hpp`
- `src/platform/raylib/raylib_host.cpp`
- `tests/platform/hud_font_tests.cpp`
- `tests/platform/hud_host_integration_tests.cpp`
- `tests/platform/pause_host_gate_tests.cpp`
- `tests/platform/platform_test_main.cpp`
- `.superpowers/sdd/task-6-report.md`

## 提交

主题：`feat: render filtered ground loot labels`

## 自审

- `combat_renderer.cpp` 只有一处 `build_ground_loot_view` 调用；位于每次 `draw` 开头。room/HUD 两个 production stage 都直接读取同一个 `render_plan.ground_loot` 字段，不持有指针或跨帧引用。
- room icon path 不调用 `ground_loot_visible` 或 `build_ground_loot_view`；HUD label path 同样不重算 filter。
- label path 只使用固定容量 `GroundLootView` 文本和 `DrawTextEx`；源码扫描无 `TextFormat`、`std::string`、`std::vector` 或 `DrawText`。
- `CombatRenderer::set_loot_filter_mode` 只写 renderer 私有 presentation 状态；runtime、session、pickup policy 不引用该字段。
- settings draft 的 loot mode 不再写入 live runtime field。Apply 成功是唯一将新 loot mode 发布给 committed/live 的路径；Cancel/失败回滚保持旧 policy。
- `git diff --check` 通过；platform、settings、settings/HUD architecture guards 与 HUD stress 全部通过。

## 顾虑

- headless 单元测试无法直接拦截 raylib `Draw*` 调用，因此顺序和复用由真实 production `CombatRenderPlan` 驱动并由纯 plan 测试、源码扫描及现有 architecture guards 验证；本任务未做 Task 9 的像素级 formal evidence。
- Apply failure 即使 `rollback_live_settings` 失败，也会无条件恢复 live/draft loot mode；窗口、音量或 VSync 的外部 backend 状态仍遵循既有 rollback error 语义。
- 独立 build 目录位于 `E:/game/task6-build`，因为共享 task5 build 目录对当前执行身份不可写。

## 审查修复：Apply 隔离与 consumer 生命周期

本节记录 `bfec85f feat: render filtered ground loot labels` 后的 Important 审查修复。

### RED

先新增真实 rollback-failure host 测试：Apply 的首次 fullscreen preview 成功，SettingsStore replace 失败，随后第二次 window-mode set（rollback）失败。生产代码未修改时运行：

```powershell
cmake --build E:/game/task6-build --target arpg_platform_tests -- -j1
ctest --test-dir E:/game/task6-build -R '^platform\.units$' -V
```

得到有效 RED：

```text
[FAIL] pause_host_gate.Apply rollback failure restores committed loot policy:
live.loot_filter_mode == original.loot_filter_mode
258 cases, 1 failures
```

共享 View 生命周期使用独立 architecture guard 先锁定：禁止旧的 wrapper/helper，并要求两个 production stage 直接消费 `render_plan.ground_loot`，同时要求 `build_ground_loot_view` 在 renderer source 恰好一处。修改生产代码前运行：

```powershell
ctest --test-dir E:/game/task6-build \
  -R '^stage11d\.renderer_integration_guard$' -V
```

得到有效 RED：

```text
Stage11D renderer exposes forbidden legacy loot wrapper
```

### GREEN 实现

- Apply backend preview 改用 `preview_settings` 副本，其中 loot mode 强制等于 committed；因此 save committed 之前不会把 draft loot 写入 `live_settings`。
- SettingsStore 保存继续使用原始 draft（仅按既有规则调整 revision），保存成功后才把新 loot mode 一起发布到 committed/draft/live/input。
- Apply preview failure 与 save failure 均在检查 backend rollback 结果之前，无条件把 `live_settings.loot_filter_mode` 和 `pause_menu.draft.loot_filter_mode` 恢复为 committed。即使窗口 rollback 失败，fixed tick/live 与 renderer/draft 仍保持旧 loot policy。
- 删除旧的 ground-loot wrapper/helper。production room stage 和 ground-label stage 直接将同一个 `render_plan.ground_loot` const 引用传入 renderer，不再存在可空或 rvalue 悬空指针 API。
- C++ plan 测试直接绑定 `plan.ground_loot` 的两个 stage 引用并验证地址、ordinal 和 draw order；Stage11D guard 验证 production 直连、wrapper 缺失和单次 builder 调用。

### GREEN 与回归

小步 GREEN：

```text
platform.units: 258 cases, 0 failures
stage11d.renderer_integration_guard: Passed
```

提交前 focused 回归命令：

```powershell
cmake --build E:/game/task6-build \
  --target arpg_platform_tests arpg_settings_tests arpg_stage11c_hud_stress -- -j1
ctest --test-dir E:/game/task6-build \
  -R "^(platform\.units|settings\.units|stage11c\.hud_stress\.zero_alloc_100k|platform\.pause_menu_state_boundary|stage11b\.architecture\.settings_boundaries|stage11c\.architecture\.hud_boundaries(_self_test)?|platform\.host_input_source|stage11b\.settings_evidence_guard(_self_test)?|stage11c\.hud_evidence_guard(_self_test)?|stage11d\.renderer_integration_guard)$" \
  --output-on-failure
```

结果：13/13 CTest 通过、0 failures；首次完整回归 43.72 秒，提交前 fresh 重跑 39.62 秒。包含 `platform.units` 258 cases、`settings.units`、HUD 100k stress、Task4 settings guard/self-test、Host/HUD architecture guards 和新 Stage11D renderer guard。

### 修复文件与提交

- `src/platform/raylib/combat_renderer.hpp`
- `src/platform/raylib/combat_renderer.cpp`
- `src/platform/raylib/raylib_host.cpp`
- `tests/platform/hud_host_integration_tests.cpp`
- `tests/platform/pause_host_gate_tests.cpp`
- `tests/platform/platform_test_main.cpp`
- `tests/platform/CMakeLists.txt`
- `tests/platform/stage11d_renderer_integration_guard_test.cmake`
- `.superpowers/sdd/task-6-report.md`

修复提交主题：`fix: isolate loot preview and render plan lifetime`

## 审查修复二：renderer guard 语义与 mutation self-test

本节记录 `362e469 fix: isolate loot preview and render plan lifetime` 后对 architecture guard 的加固；production renderer 无需改动。

### RED

先注册新的 mutation self-test，并在旧 guard 上运行：

```powershell
cmd.exe /d /s /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build E:/game/task6-build --target arpg_platform_tests -- -j1 && ctest --test-dir E:/game/task6-build -R "^stage11d\.renderer_integration_guard_self_test$" -V'
```

得到有效 RED；旧 guard 没有约束 `CombatRenderer::draw` 内每帧只调用一次 plan factory：

```text
Stage11D guard accepted bad duplicate_factory mutation
0/1 tests passed, 1 test failed
```

### GREEN 实现与 mutation 结果

- guard 现在读取 `combat_renderer.cpp`、`room_renderer.cpp` 和 `hud_renderer.cpp`，要求 `CombatRenderer::draw` 内 `make_combat_render_plan` 恰好调用一次，且 factory 内 `build_ground_loot_view` 恰好一处。
- room/HUD source 禁止独立调用 `build_ground_loot_view`、`ground_loot_visible`、`make_combat_render_plan` 或引入 loot-filter mode 重算。
- guard 从 factory 结果声明中解析局部 plan 名称，再把 room icon 与 HUD label 参数解析回同一个 `plan.ground_loot` 数据源；允许空白/换行变化、局部 plan 改名以及 `const GroundLootView&` / `const auto&` 别名。
- self-test 的所有替换都 fail closed；replacement token 缺失或替换未改变 source 会立即失败。
- 4 个坏 mutation 全部被正确原因拒绝：重复 factory、room 重建、HUD 重建、consumer 复制分叉。
- 2 个语义等价 variant 全部接受：局部 plan 改名并多行格式化、room/HUD 共用 const-reference alias。总计 6 个变体。

GREEN 命令与输出：

```powershell
ctest --test-dir E:/game/task6-build -R "^stage11d\.renderer_integration_guard_self_test$" -V
```

```text
[stage11d-renderer-guard-self-test] bad_mutations=4 equivalent_variants=2
1/1 tests passed, 0 tests failed
```

### 完整回归

```powershell
cmd.exe /d /s /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --build E:/game/task6-build --target arpg_platform_tests arpg_stage11c_hud_stress -- -j1 && ctest --test-dir E:/game/task6-build -R "^(platform\.units|stage11c\.hud_stress\.zero_alloc_100k|stage11b\.architecture\.settings_boundaries|stage11c\.architecture\.hud_boundaries(_self_test)?|platform\.host_input_source|stage11b\.settings_evidence_guard(_self_test)?|stage11c\.hud_evidence_guard(_self_test)?|stage11d\.renderer_integration_guard(_self_test)?)$" --output-on-failure'
```

结果：12/12 CTest 通过、0 failures，总耗时 41.78 秒；覆盖 `platform.units`、HUD 100k stress、Stage11B/11C architecture/evidence guards 与 self-test、host input source，以及 Stage11D guard 与新 self-test。

`platform.units` 的详细复跑结果：

```powershell
ctest --test-dir E:/game/task6-build -R "^platform\.units$" -V
```

```text
[stage11d-ground-loot] builds=100000 unchanged=50000 alternating=50000 allocations=0
258 cases, 0 failures
1/1 tests passed, 0 tests failed
```

### 修复文件与提交

- `tests/platform/CMakeLists.txt`
- `tests/platform/stage11d_renderer_integration_guard_test.cmake`
- `tests/platform/stage11d_renderer_integration_guard_self_test.cmake`
- `.superpowers/sdd/task-6-report.md`

修复提交主题：`test: harden ground loot renderer guard`


---

# Stage 16 Task 6 Report - Currency Crafting and Recipe Transactions

## Delivered

- Added `DungeonSession::request_craft(MaterialId, item_id, optional<DirectedCategory>)`.
  It validates stable ownership and material count, runs pure `items::craft_item`
  first, then consumes exactly one material only after success. The item ID is
  retained and the existing item-save path recomputes an equipped target build.
- Added `PendingSaveKind::craft` and included it in pending-build consistency
  and commit publication.
- Tightened three-to-one inputs to exact base ID plus rarity. The output keeps
  that base, uses floor average item level, and begins at reinforcement zero.
- Added material-bag interaction: select an owned material, click an item to
  use it, right-click to cancel; Directed Core cycles
  damage/defense/speed/element on its own slot. Reinforced recipe inputs show
  a loss warning.

## TDD and verification

- RED: the new dungeon transaction test failed because `request_craft` did
  not exist. A material-bag cancellation test also failed before its API was
  implemented.
- GREEN: `stage16.crafting_transaction.units` 2/2 PASS;
  `items.units` 45/45 PASS; `platform.units` 356/356 PASS.
- Built Debug/MSVC x64 targets: `arpg_dungeon_tests`, `arpg_item_tests`,
  `arpg_platform_tests`, and `arpg_game`.
- `git diff --check` passed before the implementation commit.

## Commit

`0a7df09 feat: add item currency crafting and recipe safeguards`

## Stage 16 Task 6 审查修复：前端配方契约一致性

- RED：新增背包缓存用例，三个均为武器且同稀有度、但底材为
  `1/7/8` 时，旧缓存错误地将三合一按钮标记为可用；
  `platform.units` 357 cases 中该用例失败。
- GREEN：`refresh_inventory_view_cache` 改为严格比较 `base_id` 与稀有度，
  与 `request_recipe` 的后端规则一致；不改动后端合成、随机或碰撞逻辑。
- 同时更新全量地城中的两个旧夹具：压力测试候选三件改为同底材，深渊奖励
  碰撞夹具明确将第二、第三件设为第一件的底材，保留原 ID、种子和碰撞语义。
- 验证：`dungeon.units` 268/268 PASS（189.11s），`items.units` 45/45 PASS，
  `platform.units` 357/357 PASS，`stage16.crafting_transaction.units` 2/2 PASS。

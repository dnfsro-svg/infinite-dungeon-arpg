# Task 2 报告：真实门边界与镜头偏移接线

## 范围

- 仅修改 `combat_renderer.hpp/.cpp`、`room_renderer.cpp`、`hole_prompt_binding_source_test.cmake`、`hole_prompt_binding_source_self_test.cmake`。
- 未修改 Task 1 纯几何、洞口/深渊调用参数、绘制阶段或 HUD 位置。

## TDD 证据

### RED direct guard

在生产接线前运行：

```powershell
cmake -DRAYLIB_SOURCE_DIR="$PWD/src/platform/raylib" -P tests/platform/hole_prompt_binding_source_test.cmake
```

结果：exit 1。精确首因：

```text
world prompt must measure, back, and draw shared CJK text: door_bounds != nullptr
```

按要求未在 RED 阶段运行长 self-test。

### Debug 增量构建

最终生产修正后运行：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

结果：exit 0；`room_renderer.cpp` 重新编译，`arpg_raylib.lib` 与 `arpg_platform_tests.exe` 链接成功，无代码错误或警告。

### MENU/HUD selector

清理并在结束后恢复全部 `ARPG_*_ONLY` 环境变量，仅设置：

```powershell
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY = '1'
& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

最终结果：exit 0，精确摘要：

```text
183 cases, 0 failures
```

### Direct guard 与 mutation self-test

最终运行：

```powershell
ctest --preset windows-msvc-debug --output-on-failure --no-tests=error --parallel 1 -R '^platform\.hole_prompt_binding_source(_self_test)?$'
```

结果：`2/2` 通过，0 failures：

```text
platform.hole_prompt_binding_source ............. Passed 44.60 sec
platform.hole_prompt_binding_source_self_test ... Passed 301.57 sec
Total Test time (real) = 346.23 sec
```

Self-test 对以下错误活路径逐一做了可逆 mutation 并确认 guard 拒绝：材质成功仍用 fallback frame、只用 slot bounds、丢失 camera offset、绕过 door layout、要求藏在 `if constexpr(false)`、要求藏在未调用 `auto&&` lambda。

首次 GREEN exact-pair 调用暴露 guard 提取器将默认参数 `Rectangle hud_blocker = {}` 误认为函数体，导致既有 `MeasureTextEx` 要求误判。诊断确认提取结果为空后，将 guard 精确锚定到唯一 `draw_world_prompt` 签名末尾 `) noexcept {`；未删除或放宽 `MeasureTextEx`、CJK、字体或门接线要求。修后 direct guard 单独通过，随后 exact pair 通过。

## 接线与自审

- `CombatRenderer::draw` 将同一个 `CameraOffset` 作为 `Vector2` 沿 `draw_room` 只传给 `draw_doors`；`BeginMode2D` 和洞口路径不变。
- `material_destination_bounds` 从 `default_material_manifest()` 的 source 与 foot anchor 推导 `MaterialPack::draw(..., flip_x=false)` 的真实 destination，不复制 256/244 常量；无效或溢出结果返回空。
- 材质 draw 成功且 destination 有面积时使用材质边界；draw 失败或 destination 无效时使用既有 82x70 scaled frame。
- `draw_doors` 每次调用只计算一次 `active_skill_hud_layout`，有效时使用完整 `visual_bounds`。
- 只有门调用传 `door_bounds`、HUD blocker 与 screen offset；洞口两条提示和深渊标题仍使用通用 `world_prompt_layout`。
- Source guard 继续禁止受保护活函数体中的默认字体、旧文字绘制/测量，并拒绝 dead decoy。
- `git diff --check` 无 whitespace 错误；仅有工作副本 LF 到 CRLF 提示。

## 分配与关注点

- 本任务接线只使用固定大小值对象和 manifest 静态视图，未引入动态容器或显式分配。
- MENU/HUD selector 中 Task 1 的 10,000 次纯门布局 allocation probe 继续通过；Task 2 未新增 renderer 分配计数测试。
- 无 Task 2 自动化阻塞项。

## 明确未运行

- 未运行 full、formal、`scripts/Test.ps1`、Release 或游戏窗口。
- 因任务明确禁止启动窗口，Windows 本机最终画面与操作观感未验收，不声称已完成视觉人工验收。

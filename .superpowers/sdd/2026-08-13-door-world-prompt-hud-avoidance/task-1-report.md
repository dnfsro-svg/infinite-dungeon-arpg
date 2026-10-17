# Task 1 报告：门世界提示与 HUD 纯几何

## 范围

- 仅修改 `active_skill_view.hpp/.cpp`、`dungeon_view_math.hpp/.cpp`、`active_skill_view_tests.cpp`、`hud_layout_tests.cpp`。
- 未修改 Task 2 的 renderer、camera offset 接线或 source guard。
- 未新增平台 `TestCase`；扩展了既有 active-skill responsive-layout 与 world-prompt case。

## TDD 证据

### RED

命令：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

结果：exit 1。MSVC 配置成功；首个业务失败为：

```text
tests/platform/active_skill_view_tests.cpp(574): error C2039:
"visual_bounds" is not a member of "arpg::platform::ActiveSkillHudLayout"
```

同一次并行编译也确认 `door_world_prompt_layout` 未声明。

### GREEN build

一次未重新加载 VS 环境的独立 PowerShell build 调用在 0.18 秒内以 `raylib.h: fatal error C1083: stdarg.h: No such file or directory` 退出；这是进程环境问题，未构成代码验证。随后按计划将 Configure 与 build 放在同一 PowerShell 进程执行：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

结果：exit 0，`arpg_platform_tests.exe` 链接成功；构建输出无代码警告或错误。

### MENU/HUD selector

清理并在结束后恢复全部 `ARPG_*_ONLY` 环境变量，仅设置：

```powershell
$env:ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY = '1'
& .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

结果：exit 0，精确摘要：

```text
183 cases, 0 failures
```

10,000 次 warmed door-layout 探针覆盖四个 `{+/-9, +/-9}` offset；allocation counter 保持不变，该断言随 selector 通过。

## 自审

- `ActiveSkillHudLayout::visual_bounds` 是五个槽位完整 `text_regions[].visual_bounds` 的并集，且布局返回 valid 前验证其有限、正面积并位于 viewport 内。
- `door_world_prompt_layout` 在最终屏幕坐标中执行门来源正面积可见性、通用 prompt 布局、`12 * ui_viewport_scale` blocker 避让和 fail-closed 校验，返回前减去 screen offset。
- 覆盖三种支持分辨率、四个震动极值、材质/fallback 可见边界、非碰撞结果等价、震动后入屏、有限空 blocker、NaN/Inf、无面积门和零分配。
- `git diff --check` 无 whitespace 错误；仅出现仓库工作副本的 LF 到 CRLF 提示。

## 明确未运行

- 未运行 full、formal、`scripts/Test.ps1`、Release、游戏窗口或 Task 2 的 guard/self-test。
- Task 2 renderer 接线与 Windows 本机画面/手感验收仍待后续任务；本 Task 1 不声称完成端到端画面修复。

## 关注点

- 无 Task 1 纯几何阻塞项。

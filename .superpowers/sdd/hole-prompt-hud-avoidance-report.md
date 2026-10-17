# 洞口提示避开主动技能栏报告

## 范围

- 基线：`82a8be3 fix(ui): show the fixed death retry key`。
- 跟踪文件仅三处：
  - `src/platform/raylib/room_renderer.cpp`
  - `tests/platform/hole_prompt_binding_source_test.cmake`
  - `tests/platform/hole_prompt_binding_source_self_test.cmake`
- 未新增或改名公共 API。`draw_hole` 私有渲染接线接收已有的
  `screen_offset`，将投影洞口椭圆转换为 `hole_bounds`，通过既有
  `door_world_prompt_layout` 入口使用其 blocker-aware 布局。

## RED

命令（工作目录为 worktree 根目录）：

```powershell
cmake '-DSOURCE_ROOT=E:/game/.worktrees/loot-suction-animation' -P tests/platform/hole_prompt_binding_source_test.cmake
```

结果：exit 1。首个失败是生产洞口路径缺少
`active_skill_hud_layout(static_cast<int>(screen_width), static_cast<int>(screen_height))`：

```text
CMake Error at tests/platform/hole_prompt_binding_source_test.cmake:133 (message):
  hole renderer must use its live bounds, HUD blocker, offset, dynamic key,
  and shared font: active_skill_hud_layout( static_cast<int>(screen_width),
  static_cast<int>(screen_height))
```

## GREEN

命令：

```powershell
cmake '-DSOURCE_ROOT=E:/game/.worktrees/loot-suction-animation' -P tests/platform/hole_prompt_binding_source_test.cmake
```

结果：exit 0。

```text
-- [CPLAY-030] door and hole prompts use live bounds/offset/HUD blocker; abyss stays generic
```

有效的同进程 Windows 定向验证命令（先加载已安装的 Build Tools x64
开发者环境，并设置项目要求的 `WindowsSDKVersion=10.0.26100.0\\`）：

```powershell
$developerEnvironment = & cmd.exe /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && set'
foreach ($line in $developerEnvironment) { $pair = $line -split '=', 2; if ($pair.Count -eq 2) { Set-Item -Path ("Env:{0}" -f $pair[0]) -Value $pair[1] } }
$env:WindowsSDKVersion = '10.0.26100.0\\'
cmake --preset windows-msvc-debug
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
$env:ARPG_CPLAY030_HOLE_PROMPT_ONLY = '1'
& '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe'
if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
```

结果：exit 0；配置成功，重编译 `room_renderer.cpp`、链接
`arpg_platform_tests.exe`，随后输出：

```text
22 cases, 0 failures
```

首次直接配置因 `WindowsSDKVersion` 为空失败；设置 SDK 版本后又因未加载
开发者环境而缺少 `stdarg.h` 失败。这两次均未作为通过证据，且第一次随后
运行的旧测试二进制输出未计入结果。确认安装的 SDK 为 `10.0.26100.0`、
Build Tools 路径后，才执行了上述有效命令。

## 实现与自审

- 每次 `draw_hole` 只计算一次 `active_skill_hud_layout`；有效布局仅使用
  `visual_bounds` 作为 blocker。
- `hole_bounds` 精确覆盖投影椭圆，静态标签和动态交互提示均携带相同
  source bounds、blocker 与 `screen_offset`。
- 仅当洞口 ready、玩家在范围内且 binding 非空时绘制动态
  `hole_interaction_prompt(..., descend)`；否则绘制静态标签，两个路径互斥。
- 守卫验证真实边界、完整 HUD 可视边界、镜头偏移、非空动态键、互斥分支与
  `draw_room` 的真实偏移传递；`HOLE_CODE` 也纳入死分支和未调用 lambda
  防护，提示调用数严格为动态/静态各一个。self-test 增加了洞口 blocker
  绕过、`draw_hole` 丢失 `screen_offset`、死分支、未调用 lambda 与重复静态
  提示的因果变异锚。
- `git diff --check` 已执行，exit 0；仅有 Git 的 LF/CRLF 未来转换警告。
- 独立只读审查最初发现上述洞口守卫强度不足（I）；已修复并以同一直接守卫
  额外复验一次，exit 0，输出同 GREEN。该额外复验是审查后测试守卫变更所需，
  未运行其他测试类别。

## 未运行

- 未运行长时 `hole_prompt_binding_source_self_test.cmake`。
- 未运行 CTest、`scripts/Test.ps1`、Release 构建、窗口/输入/音频/全屏或
  实机画面验收；后者仍需 Windows 本机视觉验收。
- 独立只读审查：除已修复的 I 外，无 C/M；确认真实椭圆 bounds、
  `visual_bounds`、真实 `screen_offset`、非空动态键与互斥提示路径正确。

## 提交

- `3e2204e fix(ui): keep hole prompts clear of active skills`

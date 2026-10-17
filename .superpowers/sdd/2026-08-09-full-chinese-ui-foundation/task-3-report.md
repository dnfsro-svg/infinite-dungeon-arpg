# Task 3：统一字形计划

## 范围

- 新增固定 `UiGlyphPlan`、严格 UTF-8 覆盖检查、图集字节计算和
  `SharedUiFont` 计划类型；不接入 raylib `Font`、renderer 或 host 生命周期。
- 固定加入 ASCII 32–126，随后仅通过 `visit_ui_text_catalog()` 收集、去重。
- `DeathOverlayFontPlan` 保留为 `UiGlyphPlan` 别名；
  `death_overlay_font_plan()` 与 `hud_font_plan()` 都只转发统一计划。
- 旧 death/HUD 中真实的玩家可见动态/格式化文本迁入
  `kUiDynamicGlyphCoverage`，这是没有稳定 lookup ID 的 visitor-only 静态目录
  条目；没有修改稳定 ID、枚举、玩法、存档或设置编码。

## TDD：RED

首先写入并注册 4 个字形计划行为 case。使用 MSVC 开发者环境：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

首次 RED 的关键输出：

```text
shared_ui_font_tests.cpp(15): error C2039: "UiGlyphPlan": 不是 "arpg::platform" 的成员
```

最小实现后首次 foundation 运行仍为预期 RED：

```text
[FAIL] shared_ui_font.complete catalog coverage: plan.codepoint_count > 384U
15 cases, 1 failures
```

这是目录只有 205 个条目、去重字形不足 385 的规格冲突；没有在
`shared_ui_font.cpp` 回填第二份手工语料。经主控授权，将旧 death/HUD 的实际
可见动态/格式化语料迁入唯一目录访问范围后，先补充已知文案断言并在迁移前确认：

```text
[FAIL] shared_ui_font.complete catalog coverage: platform::ui_glyph_plan_covers_text(plan, u8"最后一击")
15 cases, 1 failures
```

该 RED 证明删除迁移目录内容会丢失实际可见文案覆盖；原先的 `> 384` RED 同时
证明目录不足不能放宽为更小计划。

## GREEN 与 mutation

最终命令：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
& '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe'
$env:ARPG_CPLAY033_PASSIVE_CJK_ONLY='1'
& '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe'
```

输出：foundation `15 cases, 0 failures`；既有 HUD 兼容定向套件也为
`15 cases, 0 failures`。本机可用内存为构建前约 7785 MiB、结束约 7767 MiB，
构建始终为 `--parallel 2`。

在最终 GREEN 后实际执行并恢复三项可逆 mutation：

- 移除 `visit_ui_text_catalog()`：`coverage.covered` 失败。
- 移除去重：覆盖、唯一性与 `!capacity_exceeded` 三项失败。
- 忽略 `capacity_exceeded`：人工标记溢出计划仍覆盖“混沌之眼”的断言失败。

## 自审

- 当前统一计划码点数为 473，满足 `384 < count <= 640`；容量溢出明确标记，
  覆盖检查返回 false。
- 候选字体唯一为 `assets/fonts/NotoSansCJKsc-Medium.otf`；图集预算为 32 MiB，
  正式实例数为 1。
- 所有计划文本先经 `ui_utf8_is_valid()` 验证，再以严格 1–4 字节边界解码；
  固定 `std::array`、visitor 和栈值均不使用 JSON、运行时映射或动态容器。
- 旧 death/HUD cpp 已无中文手工字形语料；其接口形态保持可编译。
- `git diff --check` 已在最终验证前执行通过。

## 文件

- `src/platform/raylib/shared_ui_font.hpp`
- `src/platform/raylib/shared_ui_font.cpp`
- `src/platform/raylib/death_overlay_font.hpp`
- `src/platform/raylib/death_overlay_font.cpp`
- `src/platform/raylib/hud_font.cpp`
- `src/platform/raylib/ui_text_catalog.cpp`
- `src/platform/raylib/CMakeLists.txt`
- `tests/platform/shared_ui_font_tests.cpp`
- `tests/platform/ui_text_catalog_tests.cpp`
- `tests/platform/hud_font_tests.cpp`
- `tests/platform/CMakeLists.txt`
- `tests/platform/platform_test_main.cpp`

## 疑虑

- 无 Task 3 阻塞。真实 raylib 字体装载、单实例所有权和窗口生命周期按边界留给
  后续 Task 4；本任务不声称已完成 Windows 画面/字体渲染验收。

## Fix round 1：真实实例数与实际容量边界

审查发现 Task 3 曾把 `kUiFontAtlasInstanceCount` 提前写为 1，但当前
`HudRenderer`、`DeathOverlayRenderer`、`PauseMenuRenderer` 仍各自调用
`LoadFontEx`，真实运行有 3 份图集。用户确认阶段顺序：Task 3 必须如实保持 3，
Task 4 在接管唯一所有权、监控和关闭顺序的同一变更中原子切换为最终 1。

先将 Task 3 的阶段性文档与断言改为 3，在 `7ae6db5` 的生产常量仍为 1 时运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

得到精确 RED：

```text
shared_ui_font_tests.cpp(53): error C2607: 静态断言失败
```

随后先增加两类字形计划 RED 测试：前 95 项必须严格为 ASCII 32–126；通过新的
纯 `build_ui_glyph_plan(UiGlyphPlanCatalogVisitor)` 边界注入 545/546 个 UTF-8
CJK 码点，分别验证正好 640、以及第 641 个使 `capacity_exceeded=true`、计数不越界
且覆盖返回 false。实现 API 前重建，得到精确 RED：

```text
shared_ui_font_tests.cpp(88): error C2039: "build_ui_glyph_plan": 不是 "arpg::platform" 的成员
shared_ui_font_tests.cpp(94): error C3861: “build_ui_glyph_plan”: 找不到标识符
```

最小实现仅把原 `ui_glyph_plan()` 的 ASCII + catalog 收集逻辑抽到可注入 visitor 的
固定数组构建器；默认入口仍传入 `visit_ui_text_catalog()`。没有改 renderer、host 或
Task 4 生命周期。最终命令：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
& '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe'
$env:ARPG_CPLAY033_PASSIVE_CJK_ONLY='1'
& '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe'
```

输出：foundation `15 cases, 0 failures`，CPLAY-033 `15 cases, 0 failures`。
构建前可用内存约 7712 MiB、完成约 7652 MiB，持续使用 `--parallel 2`。计划文档
现明确 Task 3 阶段性实例数 3、Task 4 最终断言和真实监控实例数 1，消除了提前低报。

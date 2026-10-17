# Task 1：静态中文展示目录

## 实现内容

- 新增 `UiTextId`、所有要求的稳定 ID 中文展示访问器，以及零分配的
  `visit_ui_text_catalog()`。
- 目录使用编译期定长 `std::array<const char*>` 和 UTF-8 字面量；无 JSON、
  外部语言包、运行时映射容器或动态分配。
- 添加 5 个真实行为测试：固定中文文案、全部稳定 ID 覆盖、未知值回退、
  目录访问器覆盖、预热后 1000 次遍历的零分配。
- 添加 `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY` 定向入口；当前只注册本 Task
  已有的目录 suite，共 5 个 case。Task 2/3 的 suite 尚未提前加入。

## RED

先在 `scripts/Configure.ps1` 提供的 MSVC 19.44 / Windows SDK 10.0.26100.0
开发环境中运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

关键失败：

```text
ui_text_catalog_tests.cpp(4): fatal error C1083:
无法打开包括文件: “ui_text_catalog.hpp”: No such file or directory
```

这是预期 RED：测试先引用尚不存在的目录头文件。首次未加载开发环境的直接
构建在 CMake SDK 环境检查阶段失败，未作为 RED 接受。

## GREEN

构建：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

结果：通过；构建前可用内存约 7,917 MiB，保持 `--parallel 2`，没有实际内存压力。

定向测试：

```powershell
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
try {
  & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe'
  if ($LASTEXITCODE -ne 0) { exit $LASTEXITCODE }
} finally {
  Remove-Item Env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY -ErrorAction SilentlyContinue
}
```

结果：`5 cases, 0 failures`。

## 文件

- `src/platform/raylib/ui_text_catalog.hpp`
- `src/platform/raylib/ui_text_catalog.cpp`
- `src/platform/raylib/CMakeLists.txt`
- `tests/platform/ui_text_catalog_tests.cpp`
- `tests/platform/CMakeLists.txt`
- `tests/platform/platform_test_main.cpp`

## 自审

- 12 个公开访问器均存在，且有效范围和未知值回退都有测试。
- 访问器覆盖 205 条静态展示文本：32 UI 文案、18 底材、14 材料、6 槽位、
  3 品质、24 物品词缀、64 被动、8 怪物、12 怪物词缀全名和 12 个短名、
  10 设置动作、2 主动技能。
- 零分配测试使用真实 `allocation_count()` 探针，预热后连续 1000 次访问器
  遍历和查找的计数保持不变。
- 已执行 `git diff --cached --check`，提交为
  `07e8c3f feat: add static Chinese UI text catalog`。

## 疑虑

- 无 Task 1 阻塞。Task 2/3 尚未实现，所以未提前声称 UTF-8 验证、字体容量
  或图集验收已完成。

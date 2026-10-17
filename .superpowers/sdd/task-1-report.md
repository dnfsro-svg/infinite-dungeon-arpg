# Stage 12 / Task 1 报告：资源清单类型、纯校验、动画映射

## 实现

- 新增材质图集、精灵和帧的公开类型，以及非拥有式 `MaterialManifestDefinition`。
- 新增纯校验：拒绝零尺寸、负坐标、越界帧、越界锚点、图集维度超过 2048、总 RGBA 内存超过 64 MiB、重复图集/精灵 ID，以及帧引用不存在图集。
- 新增玩家选择器，覆盖 8 个 `PlayerState`、5 个有效 `AttackId`，并对 `AttackId::none` 返回 `player_idle`。
- 新增怪物选择器，覆盖 8 个 `MonsterId` 和全部 7 个 `MonsterAiPhase`；当前材质清单只有每个怪物的 idle 精灵，阶段不改变选择结果。
- 将实现源和两个测试套件接入 `arpg_raylib` / `arpg_platform_tests`。`platform_test_main.cpp` 是让新增测试实际由 `platform.units` 执行所必需的测试套件注册改动。

## 修改文件

- `src/platform/raylib/material_asset_types.hpp`
- `src/platform/raylib/material_manifest.hpp`
- `src/platform/raylib/material_asset_validation.hpp`
- `src/platform/raylib/material_asset_validation.cpp`
- `src/platform/raylib/material_animation.hpp`
- `src/platform/raylib/material_animation.cpp`
- `tests/platform/material_asset_validation_tests.cpp`
- `tests/platform/material_animation_tests.cpp`
- `src/platform/raylib/CMakeLists.txt`
- `tests/platform/CMakeLists.txt`
- `tests/platform/platform_test_main.cpp`（测试套件注册）

## TDD 记录

### RED

先仅添加两个测试文件和测试注册，尚未创建生产头/源文件。

指定命令：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests; ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure
```

该工作树既有构建目录先被所有权阻断，摘要：

```text
ninja: error: WriteFile(.ninja_lock): Unable to create file. Permission denied
CMake Error: Cannot open file for write: .../CMakeCache.txt.tmp... Permission denied
Stage 0 requires Windows SDK 10.0.26100.0; detected ''
```

在加载 `VsDevCmd.bat` 后，用可写的临时构建目录执行等价目标构建，得到预期的缺接口 RED：

```powershell
cmake --build E:\game\out\stage12-task1-red-2 --target arpg_platform_tests
```

摘要：

```text
fatal error C1083: 无法打开包括文件: “material_asset_validation.hpp”: No such file or directory
fatal error C1083: 无法打开包括文件: “material_animation.hpp”: No such file or directory
```

### GREEN

实现完成后：

```powershell
cmake --build E:\game\out\stage12-task1-red-2 --target arpg_platform_tests
ctest --test-dir E:\game\out\stage12-task1-red-2 -R platform.units --output-on-failure
```

结果：目标成功链接，`platform.units` 通过（1/1）。

完整相关验证：

```powershell
ctest --test-dir E:\game\out\stage12-task1-red-2 -R "platform.units|architecture.(core|combat|dungeon)_no_raylib" --output-on-failure
E:\game\out\stage12-task1-red-2\bin\arpg_platform_tests.exe
git diff --check
```

结果：4/4 CTest 通过；平台二进制输出 `285 cases, 0 failures`；`git diff --check` 无空白错误。

## 提交

未创建提交。尝试只暂存 Task 1 文件及必要的 `platform_test_main.cpp` 注册时，Git 不能写工作树索引：

```text
fatal: Unable to create 'E:/game/.git/worktrees/stage12-material-pack/index.lock': Permission denied
```

未修改全局 Git 配置，未添加 `safe.directory`。

## 已知问题 / 外部变更

- 指定工作树的 `out/build/windows-msvc-debug` 不可写，不能在该目录完成任务简报的原始构建命令；验证使用 `E:\game\out\stage12-task1-red-2` 临时构建目录，并已加载 MSVC x64 / Windows SDK 10.0.26100.0 环境。
- `docs/validation/evidence/stage11d/` 下的 PNG 和证据文本为 Task 1 无关的外部变更；未暂存、未修改其内容，也不会纳入本任务提交。

## 复审修复（Task 1）

### 新增修改

- `validate_material_frame` 现在在任何帧/锚点几何比较前，以 `std::isfinite` 检查 `source.x/y/width/height` 和 `foot_anchor.x/y`；NaN、正 Infinity 与负 Infinity 都返回无效帧。
- 扩展材质校验测试：负 Y、零高度、负锚点、六个浮点字段的 NaN/正负 Infinity、重复 atlas ID、帧引用缺失图集。
- 将玩家五个攻击 ID 的三种攻击状态改为精确 sprite 期望断言；将八个怪物 ID 的所有 AI 阶段改为精确 idle sprite 期望断言。
- 新增一个测试用例，平台测试总数由 285 更新为 286。

### 复审 TDD 记录

先仅添加上述测试断言，未修改生产实现，执行：

```powershell
cmake --build E:\game\out\stage12-task1-red-2 --target arpg_platform_tests
$env:ARPG_TEST_TRACE='1'; E:\game\out\stage12-task1-red-2\bin\arpg_platform_tests.exe
```

RED 摘要：构建成功；真实断言失败于 `material_asset_validation.rejects non-finite frame geometry and anchor`，输出为 `286 cases, 1 failures`，失败表达式是 `!arpg::platform::validate_material_frame(atlas, frame).valid`。

加入 `std::isfinite` 防护后，执行：

```powershell
cmake --build E:\game\out\stage12-task1-red-2 --target arpg_platform_tests
ctest --test-dir E:\game\out\stage12-task1-red-2 -R platform.units --output-on-failure
E:\game\out\stage12-task1-red-2\bin\arpg_platform_tests.exe
git diff --check
```

GREEN 摘要：构建成功；`platform.units` 为 1/1 通过；完整平台二进制输出 `286 cases, 0 failures`，其中包含 `material_asset_validation` 和 `material_animation` 套件；`git diff --check` 无空白错误。

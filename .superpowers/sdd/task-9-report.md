# Task 9 报告：Stage 3 dungeon presentation rules

## 状态

已完成并在 `task/m03-raylib-host` 工作树验证。实现仅提供纯平台视图决策和启动参数解析；未接线 `DungeonRuntime`、`SaveStore` Host 或真实渲染。

## TDD 记录

### RED

1. 先把 `dungeon_view_math` 从 5 扩展到 12 cases，并新增 `host_launch_options` 5 cases，将平台总数从 12 更新为 24。
2. 执行：

   ```powershell
   .\scripts\Build.ps1 -Preset windows-msvc-debug -Fresh
   ```

   结果：预期失败。`host_launch_options.hpp` 不存在，且 `DoorTheme`、`HoleVisualMode`、`Rgba8`、`SaveIndicator` 与新增视图函数均未声明。
3. 在 GREEN 审查中发现 `--save-dir` 的值槽可能为 null。先将该输入纳入既有“缺值”case；运行平台二进制复现访问违例：`platform_exit=-1073741819`。

### GREEN

1. 新增纯 `dungeon_view_math` API：门主题、生态 tint、洞状态、XY 平方距离范围、深渊 pulse、保存提示和恢复请求门控；`committing` 门保持 open，`faulted` 门为 closed。
2. 新增无容器扫描的 `parse_host_arguments`：支持十进制和 `0x`/`0X` 十六进制种子，拒绝重复、未知、缺失、负数、溢出和尾随字符；相对 `--save-dir` 在解析时立即转为绝对路径。filesystem 调用在 `noexcept` 内捕获。
3. 为 null 值槽添加最小检查，返回 `missing_value`，避免传入 `std::filesystem::path`。

## 验证命令与输出摘要

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
```

输出：`100% tests passed, 0 tests failed out of 13`。

```powershell
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

输出：`24 cases, 0 failures`，退出码 `0`。

```powershell
git diff --check
```

输出：无空白错误。

## 修改文件

- `src/platform/raylib/host_launch_options.hpp`
- `src/platform/raylib/host_launch_options.cpp`
- `src/platform/raylib/dungeon_view_math.hpp`
- `src/platform/raylib/dungeon_view_math.cpp`
- `src/platform/raylib/CMakeLists.txt`
- `tests/platform/host_launch_options_tests.cpp`
- `tests/platform/dungeon_view_math_tests.cpp`
- `tests/platform/platform_test_main.cpp`
- `tests/platform/CMakeLists.txt`

## 自审

- 门框固定值为 FIRE `{236,92,54,255}`、WATER `{64,156,236,255}`、LIGHTNING `{236,218,72,255}`、CHAOS `{154,76,210,255}`。
- `player_in_hole_range` 仅使用 X/Y 差值平方，未调用 `sqrt`。
- 新解析器没有 raylib 类型或 runtime/persistence 接线；`--save-dir` 先 `filesystem::absolute` 再由 Host 以后可能发生的 `ChangeDirectory` 使用。
- 所有生产修改在简报的 9 个文件内；本报告按任务指令置于 `.superpowers/sdd`，不纳入功能提交。

## Concerns

`std::filesystem::path` 与 `std::filesystem::absolute` 的标准库内部是否分配内存取决于实现；解析逻辑本身未使用动态容器或临时字符串，且必需的输出类型就是 owning `std::filesystem::path`。其余无已知 concerns。

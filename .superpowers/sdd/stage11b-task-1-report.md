# Stage 11-B Task 1 实现报告

## 范围

- 仅实现稳定设置类型、默认值、验证、按键查询和冲突交换。
- 未实现 codec、store、raylib host、暂停 UI 或运行时应用。
- 实现提交：`19b04af1fcd309284d000103b37b934165b1c57c`（`feat: define stage 11b settings schema`）。

## RED

先创建 settings 测试目标和黄金测试，生产头文件与实现尚不存在。

当前普通 PowerShell 没有开发环境变量，第一次直接执行：

```powershell
cmake --preset windows-msvc-debug
```

原始结果为 exit 1，提前被仓库工具链门禁拦截：

```text
Stage 0 requires Windows SDK 10.0.26100.0; detected ''
```

该结果未计为有效 RED。随后通过仓库固定的 MSVC x64 / Windows SDK 10.0.26100.0 环境运行同一配置命令，配置 exit 0：

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug
```

再运行 brief 指定目标：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_settings_tests
```

有效 RED 原始结果摘要：exit 1；MSVC 在 `settings_types_tests.cpp(3)` 报 C1083：

```text
fatal error C1083: 无法打开包括文件: “platform/settings/settings_types.hpp”: No such file or directory
ninja: build stopped: subcommand failed.
```

失败原因正是 Task 1 类型接口尚不存在，而不是测试拼写或环境错误。

## 实现

- 固定十项动作枚举和完整稳定键枚举，不保存 raylib 常量。
- `default_settings()` 使用固定数组产生 `W/S/A/D/J/K/L/E/I/P`，并设置 100%、窗口化、VSync 开、revision 0。
- `validate_settings()` 依次检查音量范围、5% 步长、窗口模式、键范围和重复键；重复检查为固定十项 O(10²) 扫描，无动态分配。
- `assign_or_swap()` 对未占用键直接赋值，对已占用键把目标动作旧键交换给原 owner；不修改 revision。
- `binding_for()` 和 `action_label()` 提供无异常只读查询。

## GREEN 与回归

```powershell
cmake --build --preset windows-msvc-debug --target arpg_settings_tests
```

原始结果摘要：exit 0；首次 GREEN 编译并链接 `arpg_settings.lib` 与 `arpg_settings_tests.exe`，提交前复跑为 `ninja: no work to do.`。

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^settings.units$" --output-on-failure
```

原始结果摘要：exit 0，`1/1` passed，0 failed。直接运行测试程序为：

```text
9 cases, 0 failures
```

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(core.units|combat.units|platform.module_boundary)$" --output-on-failure
```

原始结果摘要：exit 0，`3/3` passed，0 failed（`core.units`、`combat.units`、`platform.module_boundary`）。

`git diff --cached --check` exit 0，无空白错误。

## 改动文件

- `CMakeLists.txt`
- `src/platform/settings/CMakeLists.txt`
- `src/platform/settings/settings_types.hpp`
- `src/platform/settings/settings_types.cpp`
- `tests/settings/CMakeLists.txt`
- `tests/settings/settings_test_main.cpp`
- `tests/settings/settings_types_tests.cpp`

## Self-review

- 接口、枚举顺序、默认绑定和错误枚举与 brief 逐项一致。
- 测试覆盖十项默认绑定、音量/窗口/VSync/revision、所有六种 validation 结果、未占用赋值、占用交换和 1,000 次交换唯一性。
- 新模块只包含标准库头，不链接 raylib、Combat、Dungeon、Items、Progression、Passives 或角色 Persistence。
- CMake 插入位置在 raylib 之前，测试目标位于 `BUILD_TESTING` 内；暂存和提交范围只有 Task 1 的七个文件。
- 无生产测试后门、动态分配、codec/store/host 接线或无关重构。

## 疑虑

- 无功能性疑虑。
- 当前桌面 PowerShell 未预载 `WindowsSDKVersion`；裸 `cmake --preset` 会被既有工具链门禁拦截，需使用仓库 `scripts/Configure.ps1` 或等价的 MSVC DevShell。有效 RED 和全部 GREEN 均在固定 MSVC 19.44 / SDK 26100 环境完成。

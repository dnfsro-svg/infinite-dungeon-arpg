# Task 2: UTF-8 安全固定缓冲工具报告

## 范围与文件

- 新增 `src/platform/raylib/ui_utf8.hpp`、`src/platform/raylib/ui_utf8.cpp`。
- 新增 `tests/platform/ui_utf8_tests.cpp`。
- 接入 `src/platform/raylib/CMakeLists.txt`、`tests/platform/CMakeLists.txt` 和 `tests/platform/platform_test_main.cpp`。
- 未迁移既有业务调用点，未触及玩法、存档、设置编码、稳定 ID、枚举或版本号。

## TDD 记录

先写并注册了 6 个真实 API case：严格合法/非法边界、每个 1/2/3/4 字节码点容量边界、逐字节畸形恢复、中文省略号、capacity 0/NUL（包含 `ui_utf8_format(nullptr, 1, ...)`）、格式化尾部修复与 10000 次零分配。

畸形输入消费规则经主控确认并写入接口注释和测试：每次只消费一个输入字节、输出一个 `?`；例如 `E8 28 A1` 变为 `?(?`，`E8 AE` 变为 `??`。

第一次直接执行 brief 指定命令时，CMake 因未继承 SDK 环境在重新配置阶段失败（`WindowsSDKVersion` 为空），未将其视为 RED。随后在 VS 2022 Build Tools 开发者环境重新执行：

```powershell
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
```

得到精确 RED：`ui_utf8_tests.cpp(4): fatal error C1083: 无法打开包括文件: “ui_utf8.hpp”`。

最小实现后使用同一开发者环境重建，退出码 0；定向 GREEN：

```powershell
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY }
```

关键输出：`11 cases, 0 failures`（目录 5 + UTF-8 6）。

提交前独立审查发现 `ui_utf8_format(nullptr, capacity > 0, ...)` 会把空指针交给 `vsnprintf`。先加入该回归断言，旧实现触发 UCRT `buffer_count == 0 || buffer != nullptr` 断言，形成第二次 RED；修复为将空目标的有效格式化容量归零，走安全的长度查询。修复后重新构建并重新得到 `11 cases, 0 failures`。接口注释同时明确：`ui_utf8_format()` 的 `input_was_valid` 只报告固定输出缓冲内实际物化、可观察的字节；printf 截断后未物化的尾部不能在不分配内存的约束下验证。

## 自审与资源观测

- 解码器拒绝过长编码、孤立续字节、代理区和大于 `U+10FFFF` 的四字节序列；非法输入逐字节替换。
- 所有可写缓冲在 `capacity > 0` 且目标非空时写入 NUL；拷贝只写完整码点，截断省略号只在 3 字节完整可用时写入。
- `ui_utf8_format()` 修复由 `vsnprintf` 造成的尾部半码点；10000 次格式化循环的全局分配计数不变。
- 构建前可用内存约 7836 MiB、构建/测试后约 7738 MiB；按约束始终使用 `--parallel 2`，未出现实际内存压力。
- `git diff --check` 已执行且无空白错误。

## 额外观察与疑虑

完整运行 `arpg_platform_tests.exe` 观察到既有全量套件 655 case 中 2 项失败：`ecology_material_coverage` 的 alpha bounds 断言，以及总 case 数 `actual 655 != expected 654`。本任务没有将新 suite 加入全量 suites 数组，故该计数偏差不是由本任务的 6 个 case 直接引入；但本轮未在修改前另建基线复现，保守标记为待主控确认的既有/并行问题。Task 2 要求的 foundation 定向 11 case 已通过。

## Fix round 1：原地与重叠缓冲

审查指出原实现会在 `ui_utf8_copy()` 和 `ui_utf8_fit_with_ellipsis()` 读取 `source` 前写入首个 NUL，导致 `destination == source` 时静默丢失文本；部分重叠也会覆盖尚未读取的源字节。

先在既有 6 个 UTF-8 case 中增加以下实际 API 覆盖：

- `complete code point copy boundaries`：原地 `ui_utf8_copy(buffer, sizeof(buffer), buffer)` 保持“设置”。
- `Chinese ellipsis truncation`：原地 `ui_utf8_fit_with_ellipsis(buffer, 7, buffer)` 产生“设…”。
- `empty and NUL-only destinations`：`destination == nullptr && capacity > 0`、`source == nullptr && capacity > 0` 的 copy/ellipsis 参数矩阵，以及 copy/ellipsis 的部分重叠拒绝。

在当前 `65f0a53` 上先重建并运行 foundation，得到精确 RED：

```text
[FAIL] ui_utf8.complete code point copy boundaries: std::strcmp(in_place, u8"设置") == 0
[FAIL] ui_utf8.Chinese ellipsis truncation: std::strcmp(in_place, u8"设…") == 0
[FAIL] ui_utf8.empty and NUL-only destinations: partial_overlap[1] == '\0'
11 cases, 3 failures
```

最小修复：完全同址别名被支持；任意其他重叠范围在写入前被检测并安全拒绝，`capacity > 0` 时目标变为空串、`bytes_written == 0`，源非空时 `truncated == true`。该契约写入 `ui_utf8.hpp`，并对 copy 和 ellipsis 都有执行性断言。修复后使用 VS 开发者环境执行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'
try { & '.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe' } finally { Remove-Item Env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY }
```

关键 GREEN 输出：`11 cases, 0 failures`。

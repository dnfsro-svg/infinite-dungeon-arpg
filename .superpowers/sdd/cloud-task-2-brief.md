# Cloud Task 2: CMake 平台边界与 Linux 核心 preset

工作目录：`E:\game\.worktrees\stage19-large-density-rooms`

## 目标

让根 CMake 在 Windows 保持现有严格门禁，同时允许 Linux 使用 GCC/Clang 构建无图形核心测试；新增完整的 `linux-gcc-core-debug` configure/build/test preset。

## 必须修改

- `CMakeLists.txt`
- `CMakePresets.json`

## CMake 平台规则

- `WIN32` 分支完整保留现有规则：必须 MSVC 19.44、x64、Windows SDK 10.0.26100.0。
- 非 Windows 只接受 `GNU` 或 `Clang` 编译器。
- 非 Windows 若 `ARPG_BUILD_GRAPHICS=ON`，必须清晰 `FATAL_ERROR`，提示设置为 OFF。
- `arpg_enable_project_warnings`：MSVC 使用 `/W4 /permissive-`；其他编译器使用 `-Wall -Wextra -Wpedantic`。
- 不改变源目录、测试目录、游戏功能和其他选项。

建议结构：

```cmake
if(WIN32)
    # existing MSVC/version/x64/SDK gates
else()
    if(NOT CMAKE_CXX_COMPILER_ID MATCHES "^(GNU|Clang)$")
        message(FATAL_ERROR "Linux core builds require GCC or Clang")
    endif()
    if(ARPG_BUILD_GRAPHICS)
        message(FATAL_ERROR
            "Non-Windows builds are core-only; set ARPG_BUILD_GRAPHICS=OFF")
    endif()
endif()
```

## Preset

新增名为 `linux-gcc-core-debug` 的：

- configure preset：Ninja；binaryDir 与现有规则一致；`BUILD_TESTING=ON`、`CMAKE_EXPORT_COMPILE_COMMANDS=ON`、`ARPG_BUILD_GRAPHICS=OFF`、`CMAKE_BUILD_TYPE=Debug`。
- build preset：引用同名 configure preset。
- test preset：引用同名 configure preset，开启 outputOnFailure 和 stopOnFailure。

## 验证

运行：

```powershell
python tests/cloud/cloud_readiness_test.py
```

预期仍为 FAIL，但 Linux configure/build/test preset 相关测试必须通过，仅剩云端脚本和 CI 文件缺失。

额外验证 `CMakePresets.json` 是有效 JSON，并运行 `cmake --list-presets`。本机不具备 Linux 容器，当前任务不要求实际 GCC 构建。

## 约束与交付

- 不修改 `.superpowers/sdd/task-5-report.md`。
- 完整报告写入 `.superpowers/sdd/cloud-task-2-report.md`，包含命令与关键输出。
- 初始实现只提交 `CMakeLists.txt`、`CMakePresets.json`。
- 提交信息：`build: add Linux core-only CMake preset`。

## 独立审查修复

- 非 Windows 必须同时验证 `CMAKE_C_COMPILER_ID` 和 `CMAKE_CXX_COMPILER_ID` 均为 `GNU` 或 `Clang`。
- Linux test preset 必须排除当前无条件调用 `powershell.exe` 的 Windows 专属测试：`stage14.audio_sources`、`stage15.audio_sources`、`stage15.audio_sources_self_test`。
- `tests/cloud/cloud_readiness_test.py` 精确覆盖 Linux test preset 的排除正则。
- 审查修复允许提交 `CMakeLists.txt`、`CMakePresets.json`、`tests/cloud/cloud_readiness_test.py`，创建新的修复提交。

### 架构复核结论

三轮审查证明：用 Python 手写部分 CMake 源码解析器来验证编译器门禁会持续产生注释、嵌套、条件语义方面的假阳性/假阴性，等价于维护不完整的 CMake 解释器。删除该源码解析测试。编译器门禁的权威回归验证由后续任务在 Ubuntu 中实际执行 `cmake --preset linux-gcc-core-debug --fresh` 完成；当前任务保留生产代码审查和 preset JSON 契约。不得保留脆弱的 CMake 源码结构解析辅助函数。

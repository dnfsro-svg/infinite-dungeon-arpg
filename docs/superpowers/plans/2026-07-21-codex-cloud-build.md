# Codex Cloud Build Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让 Codex 云端 Linux 容器承担平台无关核心编译和测试，同时保留 Windows/MSVC 完整 raylib 构建，并由 GitHub Actions 生成 Windows 构建产物。

**Architecture:** 根 CMake 只在 Windows 上执行现有 MSVC 19.44、x64、SDK 10.0.26100.0 门禁；Linux 仅允许 GNU/Clang 且强制 `ARPG_BUILD_GRAPHICS=OFF`。云端通过专用 preset 和 Bash 脚本配置、构建、测试，Windows CI 继续调用现有 PowerShell 脚本，不复制工具链逻辑。

**Tech Stack:** C++17、CMake 3.25+、Ninja、GCC/Clang、Python 3 标准库、PowerShell、GitHub Actions。

## Global Constraints

- Windows 正式游戏仍要求 MSVC 19.44、Windows SDK 10.0.26100.0、x64。
- Linux 云端阶段不构建 raylib 主机或游戏窗口，`ARPG_BUILD_GRAPHICS=OFF`。
- 云端必须运行所有已注册的平台无关 CTest；图形、音频、输入和正式截图验收留在 Windows。
- 不修改游戏规则、存档格式、输入绑定或资产内容。
- 不提交 `.superpowers/sdd/task-5-report.md` 等无关现有改动。

---

### Task 1: 云端就绪契约验证

**Files:**
- Create: `tests/cloud/cloud_readiness_test.py`

**Interfaces:**
- Consumes: `CMakePresets.json`、`CMakeLists.txt`、`scripts/cloud/*.sh`、`.github/workflows/build-and-test.yml`。
- Produces: 可在 Windows 和 Linux 上用 `python tests/cloud/cloud_readiness_test.py` 执行的零依赖验证。

- [ ] **Step 1: 写失败验证**

使用 `unittest` 和 `json` 检查：存在 `linux-gcc-core-debug` configure/build/test preset；该 preset 使用 Ninja、开启测试、关闭图形；云端脚本执行 configure/build/ctest；CI 同时包含 Ubuntu 核心测试和 Windows 完整构建任务。

- [ ] **Step 2: 确认 RED**

Run: `python tests/cloud/cloud_readiness_test.py`

Expected: FAIL，报告缺少 `linux-gcc-core-debug`、云端脚本和 CI 工作流。

- [ ] **Step 3: 保留失败证据后进入 Task 2**

失败必须来自功能尚不存在，而不是 Python 语法或路径错误。

### Task 2: CMake 平台边界与 Linux 核心 preset

**Files:**
- Modify: `CMakeLists.txt`
- Modify: `CMakePresets.json`

**Interfaces:**
- Consumes: 现有 `ARPG_BUILD_GRAPHICS` 和所有平台无关测试目标。
- Produces: `linux-gcc-core-debug` configure/build/test preset。

- [ ] **Step 1: 收窄 Windows 门禁**

把现有 MSVC、版本、x64、SDK 检查放入 `if(WIN32)`；非 Windows 只接受 `GNU|Clang`，并在图形开启时明确失败：

```cmake
if(WIN32)
    # existing MSVC/SDK gates
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

- [ ] **Step 2: 增加跨平台警告**

```cmake
if(MSVC)
    target_compile_options("${target_name}" PRIVATE /W4 /permissive-)
else()
    target_compile_options("${target_name}" PRIVATE -Wall -Wextra -Wpedantic)
endif()
```

- [ ] **Step 3: 增加 Linux preset**

新增 `linux-gcc-core-debug`，生成器为 Ninja，`BUILD_TESTING=ON`、`ARPG_BUILD_GRAPHICS=OFF`、`CMAKE_BUILD_TYPE=Debug`，并增加同名 build/test preset。

- [ ] **Step 4: 运行契约验证**

Run: `python tests/cloud/cloud_readiness_test.py`

Expected: 仍 FAIL，仅缺云端脚本和 CI。

### Task 3: Codex 云端 setup 与 maintenance

**Files:**
- Create: `scripts/cloud/setup.sh`
- Create: `scripts/cloud/maintenance.sh`

**Interfaces:**
- Consumes: `linux-gcc-core-debug` presets。
- Produces: Codex Environment 可直接配置的两个 Bash 入口。

- [ ] **Step 1: 写 setup 脚本**

脚本启用 `set -euo pipefail`，检查 `cmake`、`ninja`、`g++`，然后执行：

```bash
cmake --preset linux-gcc-core-debug --fresh
cmake --build --preset linux-gcc-core-debug
ctest --preset linux-gcc-core-debug --no-tests=error
```

- [ ] **Step 2: 写 maintenance 脚本**

maintenance 使用相同 preset 重新配置和构建，但不使用 `--fresh`，随后运行 CTest。

- [ ] **Step 3: 运行契约验证**

Run: `python tests/cloud/cloud_readiness_test.py`

Expected: 仍 FAIL，仅缺 CI 工作流。

### Task 4: GitHub Actions 双平台验证与 Windows 产物

**Files:**
- Create: `.github/workflows/build-and-test.yml`

**Interfaces:**
- Consumes: Linux preset、`scripts/Test.ps1`、`scripts/Build.ps1`。
- Produces: Ubuntu 核心测试结果和 Windows release `arpg_game.exe` 构建产物。

- [ ] **Step 1: 增加 Ubuntu job**

使用 `ubuntu-latest`，安装 `cmake ninja-build g++`，执行云端 setup 脚本。

- [ ] **Step 2: 增加 Windows job**

使用 `windows-latest`，调用：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\scripts\Build.ps1 -Preset windows-msvc-release
```

上传 `out/build/windows-msvc-release/bin/**`，artifact 名称固定为 `infinite-dungeon-arpg-windows-x64`。

- [ ] **Step 3: 确认 GREEN**

Run: `python tests/cloud/cloud_readiness_test.py`

Expected: PASS。

### Task 5: 使用说明和本地回归

**Files:**
- Modify: `README.md`

**Interfaces:**
- Consumes: 云端脚本和 CI 工作流。
- Produces: Codex Environment setup/maintenance 配置文本和平台职责说明。

- [ ] **Step 1: 更新 README**

记录 Codex 云端只承担核心构建测试，Windows 承担完整游戏；给出：

```bash
bash scripts/cloud/setup.sh
bash scripts/cloud/maintenance.sh
```

- [ ] **Step 2: 运行配置和 Windows 回归**

Run: `python tests/cloud/cloud_readiness_test.py`

Expected: PASS。

Run: `.\scripts\Test.ps1 -Preset windows-msvc-core-debug`

Expected: 全部 CTest 通过。

- [ ] **Step 3: 检查差异并提交**

只暂存本计划列出的文件；确认没有暂存既存 `.superpowers/sdd/task-5-report.md` 或美术设计草稿。

Commit: `build: prepare core tests for Codex cloud`

### Task 6: 推送与云端实际验证

**Files:** 无新增文件。

**Interfaces:**
- Consumes: 已提交分支。
- Produces: GitHub 上可供 Codex Cloud 检出的分支和双平台 CI 结果。

- [ ] **Step 1: 推送当前分支并设置 upstream**

Run: `git push -u origin codex/stage19-large-density-rooms`

Expected: GitHub 返回成功并建立 upstream。

- [ ] **Step 2: 检查 CI**

确认 Ubuntu core-tests 与 Windows build-and-test 均成功；失败时以 CI 日志作为下一轮 RED 证据修复。

- [ ] **Step 3: 配置 Codex Environment**

Setup script: `bash scripts/cloud/setup.sh`

Maintenance script: `bash scripts/cloud/maintenance.sh`

在云端新任务中选择本分支并再次执行 `ctest --preset linux-gcc-core-debug --no-tests=error`。

## Self-Review

- Spec coverage: Linux 核心、Windows 完整构建、云端脚本、CI 产物、文档、推送和实际云端验证均有对应任务。
- Placeholder scan: 无 TBD、TODO 或未定义实现步骤。
- Type consistency: 三处均使用同一 preset 名 `linux-gcc-core-debug`，CI artifact 名固定。

# Task 1 云端就绪契约验证报告

## 变更文件

- `tests/cloud/cloud_readiness_test.py`

该测试只使用 Python 标准库的 `unittest`、`json`、`pathlib` 与 `shlex`，并从测试文件自身的位置推导仓库根目录。

## 测试命令

```powershell
python tests/cloud/cloud_readiness_test.py
```

## 关键输出

首次运行返回退出码 `1`，共执行 4 项测试且均为功能性失败：

- `Missing configurePresets preset: linux-gcc-core-debug`
- `Missing buildPresets preset: linux-gcc-core-debug`
- `Missing required cloud file: scripts/cloud/setup.sh`
- `Missing required cloud file: .github/workflows/build-and-test.yml`

## 自检结论

测试文件可正常加载和执行；失败由目标云端配置、脚本与工作流尚未实现引起，非 Python 语法错误或路径推导错误。已运行暂存区空白检查，且提交中仅包含契约测试文件。既存 `.superpowers/sdd/task-5-report.md` 未被修改。

## 提交

`fb8a9f4dc8ebd5144451d36d8c20c9040b9469dd` — `test: define Codex cloud readiness contract`

## 审查修复

已更新 `tests/cloud/cloud_readiness_test.py`：

- Shell 脚本仅检查去除空行及首字符为 `#` 的注释行后的可执行行。
- CI 工作流按 `jobs:` 下二空格 job 块解析，并在各自的 `runs-on` job 块内验证 Ubuntu 核心测试及 Windows 测试/完整构建证据。
- 为 `JSONDecodeError` 提供文件、错误说明、行号和列号。
- 补齐 PEP 8 所需空行。

### 验证命令与关键输出

```powershell
python tests/cloud/cloud_readiness_test.py
```

退出码仍为 `1`，`Ran 4 tests`，`FAILED (failures=4)`。失败均为功能缺失：缺少 `.github/workflows/build-and-test.yml`、`scripts/cloud/setup.sh`、Linux configure/build preset；未出现 Python 语法或路径推导错误。

### 审查修复提交

`ad496d9a3bd5c73746268d10ad9453b314222b17` — `test: strengthen cloud readiness contract`

## 第二次复审结构修复

- Bash 脚本改用 `shlex.split(..., comments=True, posix=True)` 解析每个非空命令行，并以精确 token 序列验证 setup 与 maintenance 的不同 configure 参数。
- CI 仅在 `jobs:` 下的二空格 job 块中，读取真实四空格 `runs-on:` 键；从 inline `run:` 和 `run: |`/`run: >` 块提取实际命令，排除空行及纯注释。
- Ubuntu 验证精确 `bash scripts/cloud/setup.sh`；Windows 验证规范化大小写和路径分隔符后的测试、Release 构建 PowerShell 命令。
- YAML 轻量解析辅助函数已用 docstring 说明支持的子集，文件内容保留原文，仅在比较时小写化。

### 验证命令与关键输出

```powershell
python tests/cloud/cloud_readiness_test.py
```

退出码为 `1`，输出 `Ran 4 tests` 和 `FAILED (failures=4)`。失败明确为缺少 CI 工作流、`scripts/cloud/setup.sh`、Linux configure/build preset；未出现 Python 语法或路径推导错误。

### 第二次复审修复提交

`ac9f82b222be73d80c00e0d871c2dbd0f814623a` — `test: parse cloud readiness commands structurally`

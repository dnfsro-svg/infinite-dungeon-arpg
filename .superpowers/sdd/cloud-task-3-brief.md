# Cloud Task 3: Codex Cloud setup 与 maintenance

工作目录：`E:\game\.worktrees\stage19-large-density-rooms`

## 目标

提供可直接粘贴到 Codex Cloud Environment 设置中的两个 Bash 入口，使用现有 `linux-gcc-core-debug` preset 完成 Linux 核心配置、构建和测试。

## 文件

- 创建 `scripts/cloud/setup.sh`
- 创建 `scripts/cloud/maintenance.sh`

## setup.sh 精确要求

- `#!/usr/bin/env bash`
- `set -euo pipefail`
- 用 `command -v` 检查 `cmake`、`ninja`、`g++`，缺失时输出清楚错误并非零退出。
- 从脚本自身位置解析仓库根目录并 `cd`，不依赖调用者当前目录。
- 实际执行以下精确命令，每条一行：

```bash
cmake --preset linux-gcc-core-debug --fresh
cmake --build --preset linux-gcc-core-debug
ctest --preset linux-gcc-core-debug --no-tests=error
```

## maintenance.sh 精确要求

- 同样使用 Bash strict mode、工具检查和仓库根目录定位。
- 实际执行以下精确命令，每条一行；configure 不使用 `--fresh`：

```bash
cmake --preset linux-gcc-core-debug
cmake --build --preset linux-gcc-core-debug
ctest --preset linux-gcc-core-debug --no-tests=error
```

## 验证

```powershell
python tests/cloud/cloud_readiness_test.py
```

预期脚本测试转绿，总体仅剩 CI workflow 一项失败。由于本机无 Linux 容器，不在 Windows 上执行 Bash 构建；使用 Python 契约和 Git Bash `bash -n` 做语法检查。

## 约束与交付

- 不安装依赖，不修改 CMake、游戏代码、资产或其他脚本。
- 不修改 `.superpowers/sdd/task-5-report.md`。
- 完整报告写入 `.superpowers/sdd/cloud-task-3-report.md`。
- 只提交两个新 Bash 文件。
- 提交信息：`build: add Codex cloud environment scripts`。

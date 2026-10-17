# Task 1: 云端就绪契约验证

工作目录：`E:\game\.worktrees\stage19-large-density-rooms`

## 目标

创建 `tests/cloud/cloud_readiness_test.py`，作为零依赖的云端配置契约测试，可在 Windows 和 Linux 上执行。

## 必须验证

- `CMakePresets.json` 存在 `linux-gcc-core-debug` configure/build/test preset。
- configure preset 使用 Ninja，`BUILD_TESTING=ON`、`ARPG_BUILD_GRAPHICS=OFF`、`CMAKE_BUILD_TYPE=Debug`。
- `scripts/cloud/setup.sh` 与 `scripts/cloud/maintenance.sh` 执行 configure、build、CTest。
- `.github/workflows/build-and-test.yml` 同时包含 Ubuntu 核心测试和 Windows 完整构建任务。

## 实现约束

- 只使用 Python 标准库 `unittest`、`json` 和路径处理功能。
- 路径必须从测试文件位置推导仓库根目录，不能依赖当前工作目录。
- 当前功能尚不存在，所以首次运行必须是功能性 FAIL，而不能是 Python 语法错误或路径推导错误。
- 不修改任何其他项目文件。
- 不触碰既存 `.superpowers/sdd/task-5-report.md`。

## 验证

运行：

```powershell
python tests/cloud/cloud_readiness_test.py
```

预期：FAIL，并明确指出缺少 Linux preset、云端脚本或 CI 工作流。

## 交付

- 将完整实施报告写入 `.superpowers/sdd/cloud-task-1-report.md`。
- 报告包括：变更文件、测试命令、关键输出、自检结论、提交 SHA。
- 只提交 `tests/cloud/cloud_readiness_test.py`，提交信息：`test: define Codex cloud readiness contract`。

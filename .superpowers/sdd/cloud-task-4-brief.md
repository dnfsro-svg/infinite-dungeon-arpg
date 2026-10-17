# Cloud Task 4: GitHub Actions 双平台验证与 Windows 产物

工作目录：`E:\game\.worktrees\stage19-large-density-rooms`

## 目标

创建 `.github/workflows/build-and-test.yml`，在 GitHub Actions 中执行 Ubuntu Linux 核心测试和 Windows MSVC 完整 raylib 构建测试，并上传可下载的 Windows 产物。

## Ubuntu job

- `runs-on: ubuntu-latest`
- checkout 当前提交。
- 安装或确保具备 `cmake`、`ninja-build`、`g++`。
- 通过真实 `run` 步骤精确执行：

```bash
bash scripts/cloud/setup.sh
```

该入口必须完成 `linux-gcc-core-debug` 的 fresh configure、build 和 CTest。

## Windows job

- `runs-on: windows-2022`，固定使用 VS 2022 17.14 / MSVC 19.44 / Windows SDK 10.0.26100.0 镜像，不使用已经迁移到 VS 2026 的 `windows-latest`。
- checkout 当前提交。
- 用 PowerShell 真实执行：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\scripts\Build.ps1 -Preset windows-msvc-release
```

- 用 `actions/upload-artifact@v4` 上传 `out/build/windows-msvc-release/bin/**`。
- artifact 名固定为 `infinite-dungeon-arpg-windows-x64`。
- 构建或测试失败时不得上传伪成功产物。

## 独立审查修复

- 修改 `scripts/Configure.ps1` 的 `vswhere` 查询：继续限制 VS `[17.0,18.0)` 并要求 x64 C++ 工具和 Windows 11 SDK 26100，但 `-products` 必须接受 `*`，使本机 Build Tools 和 GitHub 托管的 VS 2022 Enterprise 都能被发现。错误信息不得声称只支持 Build Tools。
- workflow 顶层设置最小权限：

```yaml
permissions:
  contents: read
```

- 固定官方 actions 到完整 SHA：
  - `actions/checkout@11bd71901bbe5b1630ceea73d27597364c9af683`（v4.2.2）
  - `actions/upload-artifact@ea165f8d65b6e75b540449e92b4886f43607fa02`（v4.6.2）
- 扩展 `tests/cloud/cloud_readiness_test.py`，先确认当前 `windows-latest`、BuildTools-only 查询、未固定权限/actions 为 RED；再验证 `windows-2022`、Enterprise/BuildTools 通用查询、最小权限和完整 SHA。
- 审查修复允许修改 workflow、`scripts/Configure.ps1` 和云端契约测试。

## 通用要求

- 支持 `push`、`pull_request`、`workflow_dispatch`。
- 使用 concurrency 对同一 ref 取消旧运行。
- 两个 job 均设置合理 timeout。
- 不写入任何凭证，不开启额外权限。

## 验证

```powershell
python tests/cloud/cloud_readiness_test.py
```

预期全部 PASS。

额外做 YAML 文本自检、`git diff --check`；本地不伪装执行 GitHub runner。实际 Linux/Windows 运行归 Task 6 推送后验证。

## 约束与交付

- 初始实现只创建 `.github/workflows/build-and-test.yml`；独立审查修复允许上述三个文件。
- 不修改游戏、CMake、脚本、资产、`task-5-report.md`。
- 完整报告写 `.superpowers/sdd/cloud-task-4-report.md`。
- 提交信息：`ci: build core tests and Windows game in cloud`。

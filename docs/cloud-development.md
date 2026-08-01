# 云端开发说明

本项目采用“云端开发与构建、本机只做最终图形验收”的工作流。

## 职责划分

- Codex Cloud：阅读和修改代码、静态检查、文档与资源批处理。
- GitHub Actions Windows：使用 MSVC 编译 raylib 游戏、运行 CTest、生成可下载的游戏产物。
- 本机 Windows：只验证真实窗口、键盘输入、音频、全屏表现和手感。

## Codex Cloud 环境

在 Codex 的 Cloud environment 设置中连接本仓库，并填写：

```bash
bash scripts/cloud/setup.sh
```

Maintenance script 填写：

```bash
bash scripts/cloud/maintenance.sh
```

代理阶段保持默认禁网即可。若某项任务确实需要下载新依赖，再为该任务临时开放受限网络。

## 云端日常流程

1. 从 `main` 创建 `codex/<任务名>` 分支。
2. 在 Codex Cloud 选择该分支并提交单一、明确的任务。
3. Codex 完成修改后创建 PR。
4. `Cloud CI` 自动执行 Linux 仓库检查和 Windows 编译测试。
5. CI 通过后合并；需要试玩时下载 `arpg-windows-<commit>` 产物到本机验收。

## 手动触发 Windows 云构建

在 GitHub 仓库的 Actions 页面选择 `Cloud CI`，点击 `Run workflow`。构建完成后，从该次运行的 Artifacts 区域下载游戏。

## 本地等价命令

本机命令执行器恢复正常后，可用以下命令复现云端 Windows 构建：

```powershell
./scripts/cloud/build-windows.ps1 -Configuration Release
```

不要在 Codex Cloud 的 Linux 容器中启动 raylib 窗口。无显示器环境只负责代码检查；真实输入与画面验收仍在 Windows 完成。

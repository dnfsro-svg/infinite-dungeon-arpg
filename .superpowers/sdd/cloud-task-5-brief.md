# Cloud Task 5: 使用说明和本地回归

工作目录：`E:\game\.worktrees\stage19-large-density-rooms`

## 目标

更新 `README.md`，让后续开发者可以直接配置 Codex Cloud、理解 Linux/Windows 职责边界并取得 Windows 游戏产物；随后运行配置契约与 Windows 核心回归。

## README 必须包含

- 新增清晰的“Codex Cloud 开发”章节，与现有构建说明风格一致。
- 说明 Codex Cloud Linux 仅配置、构建、测试 `ARPG_BUILD_GRAPHICS=OFF` 的平台无关核心；不启动 raylib 窗口、不做输入/音频/全屏验收。
- 说明 GitHub Actions `windows-2022` 使用 VS 2022 / MSVC 19.44 / SDK 10.0.26100.0 完整构建 Windows raylib 游戏并上传产物 `infinite-dungeon-arpg-windows-x64`。
- Codex Environment Setup script 精确写为：

```bash
bash scripts/cloud/setup.sh
```

- Maintenance script 精确写为：

```bash
bash scripts/cloud/maintenance.sh
```

- 说明日常流程：云端选 `codex/<任务>` 分支、修改/测试、创建 PR、等待 Ubuntu/Windows jobs、下载 artifact，本机只做最终图形和手感验收。
- 链接仓库内 `.github/workflows/build-and-test.yml`、两个云端脚本；使用相对 Markdown 链接。
- 不声称实际 GitHub CI 已通过，Task 6 前只能写“推送后自动运行”。

## 验证

```powershell
python tests/cloud/cloud_readiness_test.py
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
git diff --check
```

- 契约必须 7/7 PASS。
- Windows core CTest 必须全部通过；若耗时较长，保留完整最终摘要，不因超时误报成功。
- 不运行完整 raylib 窗口。

## 约束与交付

- 只修改 `README.md`。
- 不修改游戏、构建配置、脚本、workflow、资产、`.superpowers/sdd/task-5-report.md`。
- 完整报告写 `.superpowers/sdd/cloud-task-5-report.md`。
- 提交信息：`docs: document Codex cloud development workflow`。

# Square hundredfold room evidence

该目录只允许保存实际执行产生的证据，不包含伪造截图或占位性能数字。

Windows Release 渲染准备门禁已在 `release-20260802T201000Z/` 通过。仓库收录 compact comparison、六份 per-run summary 与全量 SHA-256 manifest；逐帧 CSV 和 revision patch 约 123 MB，保留在生成它们的本机目录而不纳入 Git。1920×1080 真实全屏画面仍待本机人工验收。

Release 渲染准备证据由 `scripts/Measure-LargeRoomRelease.ps1` 生成。每次运行写入新的 UTC 时间目录，不覆盖旧证据；目录内包含每轮 `samples.csv`、每个探针的 `summary.json`、跨 revision 的 `comparison-summary.json`、`current-revision.patch` 和全目录 SHA-256 清单。脚本只接受干净的当前 worktree；失败时保留原始数据，禁止删除失败轮次后只提交通过结果。

# Square hundredfold room evidence

该目录当前只保留证据口径说明，不包含伪造截图或性能数字。

待 Windows 本机完成 1920×1080 全屏验收与 Release 10,000 帧 P99 测量后，将原始日志、截图和指标文件放入本目录，并在 `docs/validation/square-hundredfold-room.md` 中记录生成命令、机器环境和结论。

Release 渲染准备证据由 `scripts/Measure-LargeRoomRelease.ps1` 生成。每次运行写入新的 UTC 时间目录，不覆盖旧证据；目录内包含每轮 `samples.csv`、每个探针的 `summary.json`、跨 revision 的 `comparison-summary.json`、`current-revision.patch` 和全目录 SHA-256 清单。脚本只接受干净的当前 worktree；失败时保留原始数据，禁止删除失败轮次后只提交通过结果。

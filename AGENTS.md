# 项目执行约定

## 技术基线

- 使用 C++17 与 raylib 6.0。
- 只实现当前任务明确要求的内容，不擅自推进下一里程碑或改写已确认规则。
- 保留用户已有改动；不要删除已有文件，不要重置 Git，不要覆盖无关改动。

## Codex 云端

- 云端 Linux 环境只构建和测试平台无关核心，使用 `linux-gcc-core-debug` preset；不要尝试启动 raylib 窗口。
- 新容器初始化运行 `bash scripts/cloud/setup.sh`。
- 缓存环境恢复后运行 `bash scripts/cloud/maintenance.sh`。
- 修改后先运行相关目标测试，再运行 `ctest --preset linux-gcc-core-debug --no-tests=error`。
- 云端不能完成 Windows 输入、音频、全屏、画面和操作手感验收；这些结果必须明确标为等待 Windows 本机验收，不能声称已通过。

## Windows 交付

- 完整游戏构建使用 `scripts/Build.ps1 -Preset windows-msvc-release`。
- 完整自动化验证使用 `scripts/Test.ps1 -Preset windows-msvc-debug`。
- 提交前报告实际执行过的命令及结果；无法执行的验证要说明原因。

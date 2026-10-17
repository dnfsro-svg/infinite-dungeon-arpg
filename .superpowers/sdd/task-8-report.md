# Task 8 Report

## Status

完成自动化端到端回归、保存故障恢复验证、Debug/Release 全量验证和验收记录初稿。未改动生产系统或 Stage 8 范围。

## RED / GREEN

- RED：新增的星盘端到端用例 sentinel 按预期失败；第一次链接还发现遗留的本工作树测试进程占用了 `arpg_dungeon_tests.exe`（LNK1168），停止该测试进程后重新执行。
- GREEN：替换为真实 1000 房双 session trace 后，Debug `dungeon.units` 80/80、`persistence.units` 38/38、`platform.view_math` 52/52 通过。

## Verification

- Debug：`cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug && ctest --preset windows-msvc-debug`，27/27 通过。
- Release：`cmake --preset windows-msvc-release && cmake --build --preset windows-msvc-release && ctest --preset windows-msvc-release`，27/27 通过。
- `git diff --check` 无空白错误。

## Concern

窗口的 P 键、鼠标分配/退款、进房保持和重启观察由主代理的 Windows 自动化执行；`docs/stage7/2026-07-14-result.md` 已保留对应的待补实测栏，未将其写成已执行。

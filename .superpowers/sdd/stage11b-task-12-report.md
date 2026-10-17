# Stage 11-B Task 12 执行报告

## 验证竞态记录

首次为了取得 CTest 汇总误启动两条 Debug `ctest --preset windows-msvc-debug --output-on-failure`。
两条进程并发写入 `out/build/windows-msvc-debug/bin/stage11-death-formal-validation`，其中一条
`stage11.death_formal.five_paths` 失败，原始 `LastTest.log` 第 524–545 行显示：
`Missing .../01-normal-death.png`。这是同一正式证据目录的测试并发竞态，不是代码、构建或
正式验证逻辑失败。

两条并发进程均退出后，已改为单进程独占重跑完整 Debug CTest；最终 Debug/Release 汇总和
审计结果将在该报告追加。

## 最终门禁

- 修复后 Debug：`scripts/Configure.ps1 -Preset windows-msvc-debug -Fresh`，
  `cmake --build --preset windows-msvc-debug --clean-first`（263 个目标），
  `ctest --preset windows-msvc-debug --output-on-failure`（61/61）。
- Release：`scripts/Configure.ps1 -Preset windows-msvc-release -Fresh`，
  `cmake --build --preset windows-msvc-release --clean-first`（263 个目标），
  `ctest --preset windows-msvc-release --output-on-failure`（61/61）。
- Release 的 `stage11b.settings_formal` 重新生成七张 1280×720 证据 PNG，时间为
  2026-07-18 09:37:04–09:37:08；证据摘要 `result=pass`，旧 J=0、新 U=1，暂停 tick=0/0，
  角色存档 hash 前后一致。
- 最终差异/产物审计：`git diff --check 918f2e9..HEAD` 通过；相对基线的已提交差异不含
  build、设置槽、角色槽或日志。仓库中已有的 Stage8/9 PNG/log 是基线历史验收材料，未由
  本阶段新增。

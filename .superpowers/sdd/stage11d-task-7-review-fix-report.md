# Stage 11-D Task 7 审查修复报告

## 起点与范围

- 修复起点：`a03f0a8f28f084b55d403f69a66217e6dd9665f0`。
- 只修复 Task 7 审查发现的拾取回执、表现 observer、HUD 生命周期、fault 投影、测试与报告归档问题。
- 没有 amend `a03f0a8`，没有推进 Task 8+。

## TDD RED

先新增/强化测试，再修改生产代码：

- error/recovery 后 generation 高水位不下降，旧/同/倒退不发，严格更高 generation 发一次；
- 首次直接附着历史回执只 baseline，正常空帧后的第一个真实回执发布；
- fault 帧携带新回执也不发布；
- valid=false 只有所有字段严格默认才算空回执，非法 rarity/source 是 malformed；
- 真实 runtime 保存失败、解除 fault hook、再次 fixed_tick 成功后 HUD 只发布一次；
- 回执与 room transition 同一 presented frame 时通知不被清除；
- wrong ordinal 与 same-ordinal replacement 先建立旧成功回执，再验证全部旧字段保持；
- priority=60，低于 save/recovery/abyss confirmation，高于/稳定排序于普通奖励；
- abyss receipt 到 feedback、HudNotice、Context、ContextPanelPlan 的紫色标记端到端。

API RED 命令：

```powershell
cmd.exe /d /s /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out/build/windows-msvc-debug --target arpg_platform_tests -- -j1"
```

结果：退出 1，`DungeonRenderStatus::faulted` 不存在。

最小增加只读 fault 投影后，行为 RED：

```powershell
cmd.exe /d /s /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out/build/windows-msvc-debug --target arpg_platform_tests -- -j1 && out\build\windows-msvc-debug\bin\arpg_platform_tests.exe"
```

结果：`277 cases, 6 failures`。六个失败精确命中：

1. 保存失败后重试的 HUD pickup 被 baseline 吞掉；
2. pickup 通知在跨房时被 `clear_room_context` 删除；
3. error/recovery 后高水位与 attachment 语义错误；
4. fault 帧仍发布新回执；
5. 非法 enum 的 valid=false 回执被误判为空；
6. abyss 跨房紫色链路通知丢失。

## 最小修复

- `DungeonRenderStatus` 增加只读 `faulted`，`render_status()` 只调用一次 `state()` 并缓存结果，同时投影 recovery/fault，避免每帧第二次完整 snapshot。
- `LootPickupFeedbackState` 分离单调 generation 高水位与 attachment：
  - 高水位永不因 error/recovery/fault/旧回执下降；
  - blocked 帧不发布，但吸收更高的已确认回执，避免恢复后重放；
  - blocked 后旧/同/倒退不破坏 live attachment，严格更高 generation 发布一次；
  - 普通 malformed/backward 清 attachment；下一 valid 仅重建 baseline；
  - 正常严格空回执幂等建立 live observation，确保首次真实拾取可见。
- 严格空回执要求 validity、generation、item/base/level、rarity、source 全部等于默认。
- `HudNoticeKind::loot_pickup` 被定义为跨房间保留的三秒事务通知；save/recovery 仍是更高优先级持久通知。
- runtime fault/replacement 测试比较回执的 valid、generation、item/base/level、rarity、source 全字段。

## 报告修复

- 用 apply_patch 将 `.superpowers/sdd/task-7-report.md` 精确恢复为 `417f58a` 中的 Stage 10 历史报告。
- Stage 11-D 初始 Task 7 报告迁移到 `.superpowers/sdd/stage11d-task-7-report.md`。
- 本报告使用 `.superpowers/sdd/stage11d-task-7-review-fix-report.md`；历史 `.superpowers/sdd/task-7-review-fix-report.md` 同样保持不变。
- `task-7-brief.md` 与 `progress.md` 已更新为新报告路径，不再覆盖历史证据。

## GREEN 与验证

完整平台：

```powershell
cmd.exe /d /s /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out/build/windows-msvc-debug --target arpg_platform_tests arpg_stage11c_hud_stress -- -j1 && out\build\windows-msvc-debug\bin\arpg_platform_tests.exe"
```

结果：`277 cases, 0 failures`。

相关 CTest：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|stage11c\.hud_stress\.zero_alloc_100k|stage11c\.architecture\.hud_boundaries(_self_test)?|stage11d\.renderer_integration_guard(_self_test)?)$' --output-on-failure
```

结果：`6/6 passed`，包括平台完整测试、HUD 100k 零分配、Stage 11-C HUD guard/self-test、Stage 11-D renderer guard/self-test。

- feedback 独立 unchanged observer 100,000 次零分配 case 包含在 `platform.units`。
- `git diff --check`：无 whitespace error。
- 历史报告校验：`git diff 417f58a -- .superpowers/sdd/task-7-report.md` 无输出。

## 剩余风险

- 正式 raylib 截图和内容验证仍属于 Task 9。
- 本修复未增加 Task 8 架构守卫，按任务边界保留给下一任务。

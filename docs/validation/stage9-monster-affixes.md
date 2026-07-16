# Stage 9 怪物词缀验收

构建基线：`80bce30`。本验收只新增测试与测试专用入口；`src/persistence` 未修改。

## 自动化验收

压力 Trace 先以失败方式接入。首次独立运行在第一个新增 case 以
`0xC00000FD` 退出；定位为测试记录把 1,000 个完整固定房间数组放在栈上。
记录改为堆上的固定容量数组后，逐字段相等比较保持不变，复跑通过：

```text
ARPG_STAGE9_AFFIX_STRESS_ONLY=1 ARPG_TEST_TRACE=1 build-release/bin/arpg_dungeon_tests.exe
[RUN] dungeon_affix_stress.1000 room affix trace is deterministic
[RUN] dungeon_affix_stress.saturated high risk affix world allocates nothing
2 cases, 0 failures
EXIT=0
```

- PASS：两个独立 session 使用同一根种子连续 1,000 房。
- PASS：Trace 显式保存并逐字段比较房间 seed、怪物 ID/位置、词缀 ID/等级/危险分、累计经验、掉落 ordinal 与物品 ID/等级/稀有度、领取位和最终物品摘要。
- PASS：每 37 房使用 V4 `encode_checkpoint/decode_checkpoint` 重新构造 session 后继续。
- PASS：96 怪、384 弹体、96 区域、三高危组合连续 600 tick，`allocation_count` 增量为零。

fixture 命令：

```text
build-release/bin/arpg_stage9_validation_fixture.exe
```

fixture 输出：

```text
shallow_seed=10829911648371452629 depth=1 waves=1 budget=8
deep40_seed=10829911648371452629 depth=40 waves=2 budget=15
  wave=0 spawn=0 affixes=CHAIN-M2,BURN-M3 score=15
  wave=1 spawn=2 affixes=CHAIN-M3,DEATH-M3,SHD-M2 score=22
high_risk=MULTI-M3,BURN-M3,CHAIN-M3 score=27
rewards score=27 drop_bp=4150 item_level=49 experience=148
v4_reload_consistent=1
```

## 窗口验收

已启动 `arpg_stage9_validation_game.exe` 和正式 `arpg_game.exe`；测试专用窗口提供 12 个固定词缀、M1/M2/M3 和危险分，不读取或写入正式存档，也不改变正式地下城规则。

| 项目 | 结果 | 证据 |
| --- | --- | --- |
| 测试专用固定 12 词缀/M1-M3 窗口启动 | PASS | 窗口标题 `Infinite Dungeon - Stage 9 Fixed Affix Validation`、`Responding=True`；`docs/validation/evidence/stage9/01-fixed-affix-room-render.png` |
| 正式游戏窗口启动 | PASS | `docs/validation/evidence/stage9/02-formal-game-initial.png` |
| Stage 9 提交后第 60 帧 GPU framebuffer | PASS | `stage9.validation_game.capture_after_present` 以 `--capture-at-frame-60-and-exit` 运行，通过后导出 1280x720 图像；左上角为 RGB `13,17,27`，并检测到 152 个白色 UI 采样像素。证据图为 `docs/validation/evidence/stage9/01-fixed-affix-room-render.png` |
| 浅层 M1、深 40 M2/M3、四种高危预警、L 上挑、高危掉落、重载领取与极端池 | 不适用（本测试专用固定词缀窗口） | 这些是规则/数据契约，不由固定展示窗口人工驱动；本页上方的压力 Trace 与 fixture 输出覆盖相应数据。 |

窗口证据入口先前在 `EndDrawing()` 前调用 `TakeScreenshot(绝对路径)`。raylib 会将
`TakeScreenshot` 参数再次拼接到其工作目录，导致绝对路径无效；该入口现在和正式
host 一样在提交帧后使用 `LoadImageFromScreen/ExportImage`，并在 `InitWindow` 前设置
`FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE`。新增的图形验收用例固定验证提交后的第 60 帧，
避免首帧假阳性。

## 双配置与范围检查

已运行 Release 目标构建：

```text
cmake --build build-release --target arpg_dungeon_tests arpg_stage9_validation_fixture arpg_stage9_validation_game
ctest --test-dir build-release -R '^stage9\.validation_game\.capture_after_present$' --output-on-failure
```

完整 Debug/Release `ctest` 和最终 `git diff --exit-code 7b54370 -- src/persistence` 需在本机 raylib framebuffer 可用的图形会话中继续执行；本次未将它们标记为通过。

范围扫描要求：

```text
rg -n "abyss_affix|summon_affix|aura_affix|Stage 10" src tests
```

结果应为空；本 Task 未实现深渊、召唤或光环词缀，也未进入 Stage 10。

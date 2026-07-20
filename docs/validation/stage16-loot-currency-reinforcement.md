# Stage 16 材料、货币与强化验收

Stage 16 增加 18 个稳定装备基底、14 种独立材料、九种货币改造、三合一、强化石与四档强化券。材料不占装备背包格，也不受地面装备过滤影响；材料计数、已发现位、已领取位和强化等级随 V7 存档持久化，V1–V6 迁移初始化为零。

## 规则冻结

- 普通材料掉率为 `min(3500, 800 + 危险度 * 100)` bp；正常权重、深渊权重、深度门槛与券门槛均由 `material_loot.cpp` 的固定表消费。普通材料与强化券使用不同 RNG domain；券可与材料和装备同掉。
- 清房将所有地面材料在同一次 `room_clear` 保存中吸附。未提交的保存不会领取，重载不会重复生成已领取材料；死亡会清空地面材料而保留已提交材料。
- 九种货币只在有效改造和精确提交后扣除；三合一要求三件未装备、同基底、同稀有度，产物为平均物品等级、强化归零。
- 强化概率为 +1–3 100%、+4/+5/+6 90/80/70%、+7–12 70%、+13 60% 后每级 ×0.95，最低 0.1%。失败按当前等级保持、回 +6、归零或销毁；强化仅投影对应底材属性，词缀不被放大。

## 自动验收

`stage16.loot_reinforcement.simulation` 使用每档 **1,000,000** 个确定性 room seed 抽样，按二项分布 `6σ + 12` 容差校验材料掉率；当前观测如下。

| 危险度 | 期望 bp | 命中数 / 1,000,000 | 容差 |
| ---: | ---: | ---: | ---: |
| 0 | 800 | 79,410 | 1,639 |
| 10 | 1,800 | 179,548 | 2,317 |
| 27 | 3,500 | 349,659 | 2,873 |

同一模拟还覆盖四张券的临界深度/危险度、九种货币、强化概率与失败分段、四张券面、材料拾取、清房真空、死前未拾取损失、V7 状态重载、极限强化饱和，以及 2,000 次“材料流采样前后装备生成完全相同”的随机回放。

`stage16.loot_reinforcement.real_raylib` 真正初始化 raylib 6.0 / GLFW / OpenGL 并在 **1280×720** 生成：

- `out/build/windows-msvc-debug/tests/platform/stage16 loot reinforcement evidence/stage16-loot-reinforcement-1280x720.png`
- `out/build/windows-msvc-debug/tests/platform/stage16 loot reinforcement evidence/stage16-loot-reinforcement-state.txt`

该场景依次执行并渲染已提交的真实核心事务：材料拾取、材料袋选择、Chaos 改造、+12 强化券、+12→+13 销毁确认与失败销毁。状态文本记录每一步的持久化结果，截图不使用预制图片或终端替代。

执行命令：

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -L stage16 --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
.\scripts\Build.ps1 -Preset windows-msvc-release
```

2026-07-20 的最终新鲜结果：`ctest -L stage16` 为 **4/4 通过、0 失败**（材料 12 项、交易 6 项、百万次模拟、真实 Raylib）；`dungeon.units` 为 **272 项、0 失败**，耗时 192.93 秒。`windows-msvc-release` 构建以 exit 0 完成，`out/build/windows-msvc-release/bin/arpg_game.exe` 存在（1,579,520 字节）。

Stage 9 的 `stage9.formal_game.capture_after_present` 图形栈溢出是早于 Stage 16 的已知基线例外；它未被 Stage16 标签套件调用，也不能被写作 Stage16 通过的证据。Stage16 仅以本页列出的新鲜命令结果为准。

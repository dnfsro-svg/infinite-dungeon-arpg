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

同一模拟还覆盖正常与深渊两套十种材料权重、普通与深渊怪的四档券概率/优先级（深渊三倍概率）、14 种材料可得性、四张券的精确/深度减一/危险度减一边界、九种货币、四段强化失败后果与 75,000 次/段的真实 Session 强化长跑、材料拾取、清房真空、死前未拾取损失、V7 `checkpoint_codec` 编码/解码后重建 Session、极限强化饱和。

既有 Task1 房间、怪物、装备、深渊装备奖励流以完整字段指纹 `15972885342817959559` 冻结；在材料 RNG 调用插入前后必须一致。

`stage16.loot_reinforcement.real_raylib` 真正初始化 raylib 6.0 / GLFW / OpenGL 并在 **1280×720** 生成：

- `out/build/windows-msvc-debug/tests/platform/stage16 loot reinforcement evidence/stage16-run/stage16-ground-material-1280x720.png`
- `out/build/windows-msvc-debug/tests/platform/stage16 loot reinforcement evidence/stage16-run/stage16-confirmation-1280x720.png`
- `out/build/windows-msvc-debug/tests/platform/stage16 loot reinforcement evidence/stage16-run/stage16-complete-1280x720.png`
- `out/build/windows-msvc-debug/tests/platform/stage16 loot reinforcement evidence/stage16-run/stage16-loot-reinforcement-state.txt`

该场景以真实 `SaveStore` 写入 V7 存档，再由生产 `DungeonRuntime` 读取；它执行实际战斗生成地面材料、靠近拾取、材料袋选择、Chaos 改造、+12 强化券、对 +12 装备使用强化石的确认与失败销毁，最后重启运行时核验装备已消失、已拾取材料仍在，并核验三种已使用材料均为预期余量（若地面掉落恰好同种，则保留那一份拾取物）。每次运行先删除受控 `stage16-run` 子目录并重新创建截图；验证 PNG 尺寸、内容、文件大小、修改时间和状态文本。

执行命令：

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -L stage16 --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R '^dungeon\.units$' --output-on-failure
.\scripts\Build.ps1 -Preset windows-msvc-release
```

本轮最终执行结果（2026-07-20，EDT）：

- `ctest --test-dir out/build/windows-msvc-debug -L stage16 --output-on-failure`：**4/4 通过，0 失败，46.63 秒**。其中 `stage16.material_loot.units` 0.09 秒、`stage16.crafting_transaction.units` 0.01 秒、`stage16.loot_reinforcement.simulation` 45.83 秒、`stage16.loot_reinforcement.real_raylib` 0.80 秒。
- `ctest --test-dir out/build/windows-msvc-debug -R '^dungeon\.units$' --output-on-failure`：**exit 0，272/272 cases，0 failures，196.40 秒**。
- `./scripts/Build.ps1 -Preset windows-msvc-release`：**exit 0**，输出 `ninja: no work to do.`；发布文件 `out/build/windows-msvc-release/bin/arpg_game.exe` 存在，大小 **1,579,520 bytes**，最后写入时间为 **2026-07-20 20:09:20 EDT**。

证据目录只保存最近一次真实运行的产物。

Stage 9 的 `stage9.formal_game.capture_after_present` 图形栈溢出是早于 Stage 16 的已知基线例外；它未被 Stage16 标签套件调用，也不能被写作 Stage16 通过的证据。Stage16 仅以本页列出的新鲜命令结果为准。

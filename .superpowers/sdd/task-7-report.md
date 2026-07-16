# Stage 10 Task 7 报告：固定容量深渊环境规则

## 状态与范围

- 基线 HEAD：`9f4e0764e58cc7e09f01f5bb07432f7d0b72889e`。
- 只实现 Thunderstorm、Hunting Flames、Chaos Expansion 的 combat runtime、统一 HazardPool、伤害语义、snapshot 和 clear API。
- 未接 dungeon clear lifecycle，未实现 UI、renderer、奖励或怪物设计。
- runtime 所有数值均消费 `CombatEncounterConfig::abyss.environment`；按 rule ID 只选择 renderer-facing `HazardKind`，没有重写 cycle、bp、warning、duration、interval 或 radius。

## TDD：RED → GREEN

### RED 1：tick 0 环境不存在

- 先只注册 `chaos region exists at tick zero`。
- 新构建成功后输出：`151 cases, 1 failures`。
- 精确失败：`state.hazard_count == 1U`，证明测试命中尚未存在的环境 runtime，而非旧二进制或拼写错误。

### GREEN 1：最小固定池基础

- 加入 `HazardSource`、三种 `HazardKind`、固定 `AbyssEnvironmentRuntime`、环境专用 pool spawn 和 tick 0 chaos 槽。
- 输出：`151 cases, 0 failures`。

### RED 2：完整规则行为

- 再加入周期、伤害、snapshot、饱和、清理、无敌帧和 allocation 测试。
- 校准 actual max HP 夹具后输出：`160 cases, 8 failures`。
- 失败分别命中 chaos 扩张、thunder warning、hunting warning/首次伤害、config 消费、pool saturation 和 clear API；不是编译错误。

### GREEN 2：三规则与统一伤害路径

- 实现 config-driven 周期调度、环境 telegraph 零点激活、chaos 原槽扩张、environment owner 合法旁路、source-aware owner 清理和 clear API。
- 输出：`160 cases, 0 failures`。
- 自审修正 chaos 初始 spawn 失败时同一 tick 可能重试的问题：用固定 `expansion_stage` sentinel 记录 tick 0 已尝试，只在下一正常 expansion stage 重试。

### RED/GREEN 3：换波不能误删环境

- 自审新增 `wave reload keeps environment`。
- RED：`161 cases, 1 failures`，失败于 `load_wave()` 后 environment count 从 1 变 0。
- GREEN：`load_wave()` 改为只清 `HazardSource::monster`；输出 `161 cases, 0 failures`。

## 固定 tick 时序

CombatWorld 在处理逻辑 tick `T` 后把 snapshot 的 `tick` 增为 `T+1`。下表的“逻辑 tick”是事件和规则定义使用的 tick；warning snapshot 是该 tick 处理后的可绘制状态。

| 规则 | 逻辑 tick | 处理 | 处理后 snapshot / 伤害 |
|---|---:|---|---|
| Thunderstorm | 0..179 | 无环境槽 | environment hazard count 0 |
| Thunderstorm | 180 | 锁定本 tick 玩家位置并尝试占一个 pool 槽 | source=environment，kind=thunderstorm，radius=0.8，telegraph=45，active_ticks=1 |
| Thunderstorm | 181..224 | warning 倒计时 | tick 224 处理后 telegraph=1 |
| Thunderstorm | 225 | telegraph 到 0，当 tick 结算一次 ground/environment lightning | 15% actual max HP；命中或落空后区域结束；成功命中事件 tick=225 |
| Thunderstorm | 360 | 下一正常周期 | 若 t180 池满、此时有槽，则在这里重新生成；中间不重试 |
| Hunting Flames | 0..239 | 无环境槽 | environment hazard count 0 |
| Hunting Flames | 240 | 锁点并生成 warning | radius=1.0，telegraph=45，active_ticks=180，interval=60 |
| Hunting Flames | 285 | 区域进入 active 的第一 tick | 第一次 10% fire 尝试；测试锁定成功事件 tick=285 |
| Hunting Flames | 345 / 405 | 每 60 tick 再尝试 | 成功事件分别为 345、405；每次仍经过统一 i-frame |
| Hunting Flames | 464 | 第 180 个 active tick 结束 | 当 tick 后销毁；不会在 465 再伤害 |
| Hunting Flames | 480 | 下一正常 cycle | 重新锁定玩家并 warning |
| Chaos Expansion | 0 | 挑战初始化即占槽，中心为房间中心，立即 active | radius=1.0；第一次 8% chaos 尝试事件 tick=0 |
| Chaos Expansion | 60 / 120 | 每 60 tick 伤害 | 仍为 radius=1.0 |
| Chaos Expansion | 180 | 先把同一槽 radius 更新为 2.3，再结算本 tick 伤害 | 没有新 pool 分配 |
| Chaos Expansion | 360 / 540 / 720 | 同一槽依次更新 | radius=3.6 / 4.9 / 6.2 |
| Chaos Expansion | >720 | stage 上限保持 | radius 永久保持 6.2，直到显式 clear/reset |

环境 warning 采用 pool 内部 `warning + 1` 的初始计数，并在生成当 tick 先经过一次 hazard simulation；因此 renderer-facing snapshot 精确看到配置的 45，而激活仍精确发生在锁点后第 45 tick。monster-source telegraph 保持既有“到 0 后下一 tick 激活”语义，旧词缀测试未改变。

## HazardPool、source 与饱和

- `HazardPool` 仍是唯一 `std::array<HazardRuntime, 96>` 固定池；没有环境旁路容器。
- `HazardSource::monster` 要求有效 monster owner；`HazardSource::abyss_environment` 使用 `{index=0xFFFF,generation=0}` 的无效 owner，此 owner 对环境 source 合法。
- `remove_owned_hazards()` 和换波清理只删除 monster source；怪物死亡、显式 destroy 和 `load_wave()` 不删除 environment source。
- `clear_abyss_rule_preserving_resources()` 只删除 environment source、复位固定 runtime、清空 abyss config，并阻止后续再生。
- pool 满的 t180：旧 96 槽 generation/source 不变，environment spawn 返回失败，`hazard_saturation_count` 只加 1，没有 `player_hit`/`player_hurt_started`。
- t181..359 不逐 tick 重试；释放一个槽后，t360 下一正常周期成功生成，saturation 仍为 1。
- chaos 初始失败只在下一 expansion stage 边界重试，不会在 tick 0 的初始化和首次 `tick()` 双重计数。

## 伤害、减伤、闪避与无敌帧

- raw damage 统一调用 Task 1 `percent_of_actual_max_hp(player_.max_hp, damage_bp)`，即 `max(1, ceil(actual_max_hp * bp / 10000))`。
- 100 actual max HP 测试锁定：thunder=15、hunting=10、chaos=8；自定义 333bp 锁定 ceil 为 4，证明 runtime 没有按 rule ID 重写数值。
- `DamagePacket` 只在 config 指定元素槽写值：catalog 三规则分别严格为 lightning/fire/chaos，其余四槽为 0。
- 环境统一调用 `apply_player_damage(packet, ground_or_environment, center, heavy)`，因此 10000bp evasion 仍命中，但元素减伤照常生效；15 lightning + 50% reduction 实际扣 8。
- i-frame 与直接命中共用同一状态。测试在 hunting t285 前先造成直接伤害，t285 环境尝试被 30 tick invulnerability 拦截；该 hazard 仍进入下一 60 tick interval，t345 才成功扣 10。测试没有把“每 60 tick 尝试”误写成“每 60 tick 必扣血”。
- 环境 path 不读取 monster affix、不调用 `apply_monster_direct_hit`、chain、corrosion 或 death trigger；thunder 命中后 corrosion 仍为 0。

## Snapshot 与零分配

- `HazardSnapshot` 固定携带：active、generation、owner、source、kind、center、radius、telegraph_ticks、active_ticks、lifetime_ticks、damage_interval_ticks、latch/persistence 和 DamagePacket。
- Task 11 可以只读 snapshot 区分 warning/active、monster/environment 与三种环境表现。
- chaos lifetime/active_ticks 使用固定 `uint16_t` 最大值作为持续区域展示值，simulation 对该 source/kind 不递减，最终半径保持 6.2。
- `AbyssEnvironmentRuntime` 只有 enum、整数、Vec3 和 bool；static_assert 为 trivially copyable。
- 600 tick 循环同时执行 `tick()` 与 `snapshot()`，allocation probe 前后 delta 为 0。
- 新增与修改的 runtime/helper 全部 `noexcept`，不使用 vector、map、string、function 或 heap allocation。

## 修改文件

- 生产：`src/combat/combat_types.hpp`、`combat_world.hpp/.cpp`、`abyss_environment.cpp`、`monster_pool.hpp/.cpp`、`combat_snapshot.cpp`、`CMakeLists.txt`。
- 测试：`tests/combat/abyss_environment_tests.cpp`、`combat_test_support.hpp`、`monster_affix_trigger_tests.cpp`、`combat_test_main.cpp`、`CMakeLists.txt`。
- 报告：`.superpowers/sdd/task-7-report.md`。

## 测试覆盖

- chaos tick 0 snapshot、元素 packet、owner/source、固定 lifetime/interval。
- chaos t0/60 伤害与 180/360/540/720 半径、最终 6.2 保持。
- thunder t180 锁点、45 warning、t225 命中结束、100% evasion bypass、lightning reduction。
- hunting t240 warning、t285 首击、t345/t405 interval、t464 duration 结束、共享 i-frame。
- 自定义 environment config cycle/warning/radius/bp/damage_type 原样消费。
- pool 满不覆盖、单次 saturation、无伤害、释放后下一正常周期恢复。
- monster destroy 与 wave reload 均保留 environment；clear API 删除并停止 environment。
- 600 tick allocation delta 0；既有 monster hazard snapshot equality 增加 source 字段。

## 最终验证

- Combat：`arpg_combat_tests.exe`，`161 cases, 0 failures`。
- Dungeon：完整 executable 回归含 1000-room stress，`161 cases, 0 failures`。
- Architecture：`ctest -L architecture`，`20/20` passed，0 failures。
- 上述三组均在最终工作树状态重新构建/执行并得到新鲜 exit 0；提交前另执行 `git diff --check`。

## 疑虑与边界

- 无已知 Task 7 功能缺口。
- 本任务没有把 clear API 接到 dungeon clear lifecycle；这是 Task 8 范围。
- renderer/UI 尚未消费新增 snapshot 字段；这是 Task 11 范围。

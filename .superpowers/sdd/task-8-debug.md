# Task 8 清房夹具根因调查（未改生产逻辑）

运行：`windows-msvc-core-debug/arpg_dungeon_tests.exe`，临时测试侧状态跟踪。

## 证据

- lifecycle 工作样本：room 0、seed 默认，计划是 `chaos_chaser, lightning_shooter, chaos_chaser`（ID `6,4,6`）。相同 launcher 输入从 tick 1 清至 tick 615：`phase=cleared`、`wave=0/1`、`targets=0`、四门 `1111`，玩家 HP 920。
- maximum-index navigation：起点计划 `6,1,6`；tick 4096 时仍 `phase=combat`、`targets=2`、门 `0000`、玩家 HP=1。charger 已击败，但两个 chaser 分别 HP 260/222。
- transaction：起点计划全是四个 chaser (`6,6,6,6`)；tick 4096 仍 combat、targets=4、门/洞未开、玩家 HP=1，四个怪仍 HP 222/146/184/184。
- stress：相同 helper 输入在一组 `6,4,6` 计划可在 tick 616 到 awaiting_exit；但另一个 room 0 的 `6,4,6` 布局在 tick 8192 后仍 targets=3、玩家 HP=1、所有怪满血。后续含 bulwark 的 room 1 在 8192 tick 也未清。
- 所有失败样本都只有一波（`wave=0/1`），没有进入 `wave_delay`；因此失败发生在波次生命周期之前，不能归因于门/洞或存档事务。

## 对比与单一假设

清房夹具只按“最近存活怪物 + 固定单次 launcher”驱动，未根据玩家受击/无敌窗口与多怪站位形成可持续、可命中的输入序列。某些 encounter 布局把玩家压到 HP=1，之后输入虽仍循环，但不能稳定造成伤害；同一怪物组成在不同出生布局下结果不同。

最小可测试修复假设：将 dungeon 测试夹具改为确定性的近距离连段控制器（只使用正常 `queue_action`/移动），在攻击取消窗口补入 light 连段输入，并在玩家受击期间不投递会过期的输入；保持最近存活 active monster 选择。先为一个四 chaser 固定 seed 写 RED：应在上限 tick 内到 `awaiting_exit`，且不触及生产 combat/session 逻辑。

## 连段假设结果

新增四 chaser 回归后，旧固定 launcher 夹具在 4096 tick 超时（RED）。改为未受击时在 J1/J2 取消窗口补 light 的测试夹具后，四 chaser 在 tick 841 清空并进入 `cleared`，下一 tick 到 `awaiting_exit`；不涉及生产文件。

但同一次全 dungeon 运行仍显示 `6,4,6` 的 lifecycle 布局在 4096 tick 超时，且队列中继随后断言溢出。因此单一夹具修复仅证明四 chaser 路径，未使整个回归转绿；按约束停止，等待下一轮根因分析，未叠加修复。

## 第二轮并列调查（无代码修改）

可清的固定 launcher 轨迹：同一 lifecycle `6,4,6` 在 tick 615 清房；终态玩家位于 `(7.08,-0.78)`、HP 920，三个目标 HP 都为 0。新 light 控制器的同一布局在 tick 4097 超时：玩家 `(0.24,-1.62)`、HP 1；两个 chaser 仍为 232/260 HP，shooter 240 HP。第一个已知分叉是 light 路径只造成一记 28 伤害后被多怪受击压制，而 launcher 路径保持单发攻击直至清房。

静态数据流检查：light 控制器只在当前“最近存活目标”仍处于 1.75 x / 0.60 y 的 lane 时，才在 J1=8 或 J2=9 tick 投递连段输入；但取消判定本身只需要 input buffer 中有 light。shooter 每步维持 preferred range 并会重定位，最近目标也会随 chaser 移动切换。因此 hit 后的取消窗口若目标离开 lane，控制器不会投递输入；现有输出没有逐 tick `queue_action` 返回值或 hit event 序列，不能诚实地指认精确分叉 tick。

单一根因假设：测试夹具错误地把“继续连段的 buffer 投递”绑定到取消窗口时的实时攻击 lane；该条件比 combat 本身的取消条件更强，遇到 shooter/移动 chaser 时会中断连段，随后玩家被压至 HP=1。后续最小 RED 应只验证：一次已确认命中的 J1 后，即使最近目标在 J1 取消窗口暂时离开 lane，夹具仍在未 hurt 状态缓冲 J2；不改变生产逻辑。

## 第二假设结果

按假设移除 J1/J2 取消 tick 的瞬时 lane 重查后，固定四 chaser 仍可在 tick 841 清房，但 lifecycle `6,4,6` 仍在 tick 4097 超时（目标 HP 232/240/208，玩家 HP=1）。因此“取消窗口 lane 重查”不是充分根因；按单一修复约束停止，未运行后续 navigation/transaction/stress 作为成功验证，也未修改生产逻辑。

## 第三轮角色与伤害归因（只读）

已测 launcher 的 `6,4,6`：两个 chaser 各累计 260 伤害、shooter 累计 240 伤害，全部击败；终态 tick 615、玩家 HP 920。当前 light 控制器的同布局在 tick 4097：chaser 分别累计 28/52 伤害，shooter 累计 0；没有击败事件，玩家 HP 1。light J1/J2/J3 单次伤害为 28/34/52，shooter HP 240、preferred range 4.5，且 AI 每个 move tick 调整到 preferred range；chaser preferred range 0.9 并持续接触攻击。

出生坐标：失败 `6,4,6` 为 chaser `(-0.11,-2.40)`、shooter `(6.89,-0.59)`、chaser `(3.94,0.38)`；四 chaser 成功样本为 `(-5.96,1.92)`、`(0.37,2.70)`、`(4.41,-1.02)`、`(3.74,1.31)`。首 200 tick 的精确位置、facing、queue_action 返回值及 hit event 次数没有被现有非侵入日志记录；本轮禁止改代码，故不编造这些数值。

角色级单一假设：断点是 shooter 的 4.5 preferred-range 风筝路径叠加两个 chaser 的 0.9 近战压力；light 控制器先锁最近 chaser，未能有效转换到 shooter，受击后少量 J1/J2 命中后停滞。launcher 的更独立单发命中/位移使其可穿过该压力。要验证此假设需要后续一轮专门的、可丢弃的测试侧事件采样，或一个只含 shooter+chaser 的最小 RED；本轮未修改夹具或生产代码。

## 最后一个混合控制器假设结果

新增 `6,4,6` 与四 chaser 两个回归后，旧控制器如预期在 `6,4,6` 超时（RED）。混合控制器成功优先击败 shooter（ID 4 HP=0），但 tick 4097 仍剩两个满血 chaser（各 HP=260），玩家 HP=1，因此 `6,4,6` 回归仍失败。四 chaser 回归继续成功。

这已否定“先清 shooter 再用 light 清 melee”这一最后允许夹具策略；需要架构讨论（测试控制器是否应承担真实多怪战斗清关，或是否需一个受控 combat fixture），不得继续添加第四种夹具策略。

## force-defeat 后 relay overflow 定位

`rg` 显示已迁移的通用 `drive_until_cleared` 和 stress `drive_clear` 都使用 `force_defeat_current_wave`。仍遗留的自动战斗循环有：`dungeon_wave_tests.cpp:42-62` 的本地 `clear_current_wave`、`dungeon_stress_tests.cpp:360-374` 的双 session 确定性循环、Release-only `dungeon_navigation_tests.cpp:114-137`，以及 `dungeon_transaction_tests.cpp:265-280` 的队列溢出专用循环。

本次 core-debug 断言不是常规清房路径（它们已 force defeat）；最可疑且唯一在 Debug 常规运行仍持续投递 action 的位置是 `dungeon_lifecycle_tests.cpp:210-216` 的 `combat_events_relay_in_source_order`。该测试每个 idle tick 都 queue light 并只跑 128 tick，目的是验证 relay 顺序，不是清房 helper；同时 transaction 的 265-280 是刻意压力循环。需要下一步先对这两个测试的 drain/队列容量意图做独立检查，不能把它们盲目改成 force defeat。

## 四个保留循环的 drain 模式复核

- lifecycle relay-order（210-216）：每 tick 后 `drain_all_events`，理论上不应累积跨 tick relay；但 real monsters 使一次 tick 的 event burst 大于旧 dummy 模式，仍可能在 session 内部 relay 前压满 64。
- stress 双 session（350-390）：`tick_equal` 在每个 tick 后逐个 drain dungeon 与 combat relay，非跨 tick 积压。
- Release navigation：每 tick 只 drain combat relay、刻意不 drain dungeon events；仅 `#if NDEBUG` 编译，不能解释 core-debug 断言。
- Release transaction queue-overflow：同样仅 `#if NDEBUG`，且故意要求 overflow/fault；不能解释 core-debug 断言。

最小修复假设（尚未实现）：core-debug 来源只能是 lifecycle relay-order 内单个 real-combat tick 的事件 burst，或某个未列入扫描的 TestAccess force-defeat 一次性为 64+ 活动 slot 产生事件。先决验证应在 Debug 单独隔离 lifecycle relay-order 与 force-defeat stress，不改变队列或测试逻辑；现有正常房间最大 4 怪，force-defeat 正常每波最多 4，故 lifecycle single-tick real AI burst 更值得优先隔离。

## 最小隔离实验可行性与事件上界

测试框架没有 filter/单测选择入口；完整 suite 直接运行会在断言终止前吞掉后续定位。静态事件上界排除了 A：正常 director wave 最多 4 spawn，`force_defeat_current_wave` 对每个仍活跃怪仅调用一次既有 defeat 分支，产生恰好一个 `defeated` combat event，因此一次最多 4，远小于 `kCombatRelayCapacity=64`。它在下一 `DungeonSession::tick` 被 relay，通用 helper 紧接着 drain。

生命周期 relay-order 则维持真实 AI 并持续 128 tick；每 idle tick 投递 light，AI 可同时发射 projectile/hazard、player_hit/hurt、attack hit/summary 等事件。虽然测试每 tick 后 drain，但 session 的 relay queue 在单 tick 内先完整转发 CombatWorld event queue，故 B（单 tick burst）仍成立。C（正常 force-defeat波次的固定队列不足）被上述 4-event 上界否定。最小修复范围应仅是 lifecycle relay-order 的测试布置：使用受控一怪 encounter 或每 tick 限制其真实事件源；不能改生产队列容量。

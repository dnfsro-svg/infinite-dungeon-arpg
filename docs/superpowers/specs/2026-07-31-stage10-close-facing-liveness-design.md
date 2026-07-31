# Stage 10 近身朝向活性修复设计

日期：2026-07-31

## 1. 根因

人口缩放预算的首轮直接运行在 48,914 tick 时停在 486/600。终态不是普通的
“预算耗尽”：玩家位于 `(-32.9425, 81.0109)`、锁定怪物位于
`(-32.4962, 81.2404)`，双方很近，但玩家面向左、怪物在右侧，且驱动同时满足：

- `close_for_light=true`；
- `geometry_light=false`；
- 攻击、技能、受击、停帧和输入队列均为空；
- `sweep_escape=false`。

夹具 `movement_toward()` 只有 `|dx| > 0.45` 才产生水平输入，而 light lane 在
`|dx| > 0.2` 时要求玩家面向目标。因此 `0.2 < |dx| <= 0.45`、
`|dy| <= 0.25` 且背向目标时，移动恒为零、轻击恒不可发、close 模式又不会进入
stale-geometry 或 movement-stall 恢复，形成可无限持续的确定性死锁。

锁计数也吻合：300 次取得锁，293 次目标消失释放、6 次移动停滞释放，恰好剩当前
这一把活锁。

## 2. 方案比较

1. **继续放大预算**：死锁可无限持续，不能解决根因，不采用。
2. **修改生产移动/攻击判定**：会改变游戏手感和玩法规则，超出测试恢复范围，不采用。
3. **夹具通过公开移动输入转向一帧（采用）**：目标已在 light 距离/纵深内、仅朝向
   不符且玩家可控时，发送一个朝目标的水平输入 tick；下一 tick 走原有 light 判定。

## 3. 精确合同

仅在现有 close 锁存在时检查以下全部条件：

- `movement_toward(player, target)` 返回 X/Y 均为 0；
- 玩家处于 `controllable_movement_snapshot()`；
- `|dx| <= 1.70` 且 `|dy| <= 0.55`；
- `|dx| > 0.2`，且当前 facing 与 `dx` 方向相反；
- 当前目标不满足既有 `in_attack_lane()`。

先保留 Storm→Draw→light 的既有动作优先级。只有 action ready 且本 tick 没有动作或
技能被接受时，才设置 `movement.x = sign(dx)`；不改位置、不改 facing、不调用私有
接口，实际转向和移动仍由生产 `session.tick(movement)` 完成。

每把锁最多尝试一次转向：新增 per-lock `close_facing_turn_attempted`，在 acquire 和
release 时重置。转向 tick 不计作普通 close 导航进度检查，避免怪物/障碍阻挡实际
位移时被错误判为新的 movement stall。下一公开 snapshot：

- 同一目标进入 light lane：走原 light 路径；
- 目标消失：走原 absent handoff；
- 同一目标仍落入上述零移动、背向死区：释放并排除该目标，进入既有
  `recover_until_light_lane` 扫图，不允许第二次反向微移。

增加 `close_facing_corrections` 失败诊断计数，除此之外不改变锁、扫图、恢复或技能
优先级。

## 4. 范围与验证

- 保留当前人口预算公式、60 秒 CTest 超时、durable clear 分叉和所有既有改动。
- 不改变任何生产文件、阈值、玩法、人口、伤害、奖励或保存语义。
- 保持每个 `drive_clear()` 迭代恰好一次 `session.tick(movement)`。
- 串行构建后只运行一次直接门禁。
- 若直接退出 0，再运行精确 Stage 10 transaction CTest 与私有注入护栏。
- 若仍退出 8，但失败终态不再满足上述零输入朝向死锁且击杀持续推进，则停止本卡，
  用新完整证据另行标定预算；本卡不得顺手扩大预算或超时。

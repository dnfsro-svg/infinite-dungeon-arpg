# Stage 10 障碍安全扫图设计

日期：2026-07-31

## 1. 已确认的问题边界

`24000 + 96N` 在固定 600 怪房间运行 81,600 tick 后仍稳定停在
599/600。新增约 12,000 个 sweep tick 完成了名义上的一圈路点，却没有发现
最后目标，因此不得继续增加预算。

五条中心行的理论 streaming 覆盖为 `0-4 / 4-8 / 8-12 / 12-16 /
16-19`，横向理论覆盖全部 20 列。缺口来自实际移动：生产碰撞会拒绝整个玩家
步进，而 fixture 目前把任意一帧 `position_changed == false` 直接解释成当前
waypoint 已完成。确定性障碍因此会让每一圈都跳过同一段实际路径。

## 2. 公开接口诊断合同

先保持输入、动作和失败结果不变，仅在 fixture 内记录固定容量诊断：

- 用实际房间描述和公开 `build_room_monster_plan()` 重建蓝图，并以怪物数量、
  generator version 和 blueprint hash 验证一致性；
- 从每 tick 的公开 `CombatSnapshot` 记录 resident ordinal 和最后位置；
- 从公开 `CombatEventKind::defeated` 事件记录已击败 ordinal；
- 用生产运行时同款公开 `make_room_streaming_region()` 记录 400 个 home cell 的
  访问次数、首次 tick 和末次 tick；
- 记录 sweep 非零输入后玩家未移动的 waypoint 次数；
- 只在失败路径输出未击败 ordinal 的 id、home cell、行列、初始位置、最后
  resident 位置和 home-cell 覆盖次数。

不得访问 field 私有状态、使用测试注入、改变 session 结果或让诊断数据参与输入
决策。直接运行一次即停止，根据结果选择覆盖修复或 residency 调查，不能猜测。

## 3. 覆盖修复合同

若诊断证明缺失怪物的 home cell 未被实际覆盖，并同时记录到障碍停步，则只修改
fixture 的公开 `MovementInput` 扫图路线：

- 将五条横向巡航线移到 cell 边界 `2 / 6 / 10 / 14 / 18`；对应 streaming
  行带严格覆盖 `0-3 / 4-7 / 8-11 / 12-15 / 16-19`；
- 左右端点使用房间 `min_x / max_x`，形成蛇形网格边界路线；
- 使用小于环境障碍最小 cell inset `0.30` 的 sweep 专属到达容差；
- 初次进入 sweep 时先用单轴公开移动贴近最近 cell 边界；若该轴被阻挡，尝试
  另一轴或另一侧边界；进入边界走廊后只沿网格边界移动；
- 非零输入但未移动只切换入网探针，绝不再推进 waypoint；只有实际达到路点才
  推进。

环境生成保证障碍 AABB 至少缩进 cell 边界 0.30，且每格至多一个障碍，因此上述
边界走廊不会穿过 intact 障碍。全部操作仍经过真实 player collision 和 session
tick；不修改生产代码、玩法或怪物状态。

## 4. 最终预算与验收

覆盖修复后撤销无效的完整周期储备，恢复有限公式：

```text
budget(N) = 12000 + 96 * N
```

CTest timeout 保持 75 秒。串行构建后，先要求 standalone exit 0；之后才运行
精确 transaction CTest 和 evidence guard，各 1/1 通过。若诊断显示 home cell
已多次覆盖但 ordinal 从未 resident，则停止本设计的路线修复，转查 field 身份链。

# Stage 10 奖励网格导航设计

日期：2026-07-31

## 1. 新暴露的 RED

障碍安全扫图在 `12000 + 96N` 下已清除 600/600，standalone 不再以
combat exit 8 结束。它随后首次到达奖励领取路径，并在旧 `claim_reward()` 的
500-tick 上限以 exit 10 失败：phase 4、fault 0、三个深渊奖励仍未领取。

这不是战斗回归。奖励固定生成在房间中心 `(-0.75,0) / (0,0) / (0.75,0)`，但
玩家现在可在边长约 162.48 的任意位置结束战斗。500 tick 以验证移速每 tick
0.099 仅能走 49.5 单位；从房间边缘或角落到中心本就可能超出，并且原直线输入
仍可能被完整房间的 intact 障碍拒绝。

## 2. 公开导航合同

复用已验证的网格边界导航基础，不访问障碍或 session 私有状态：

- 抽出 `grid_route_movement(player, target, phase, boundary_column)`；
- `need_join` 先沿 X 单轴贴最近竖向 cell 边界；近边受阻后改走当前 cell 另一
  竖边；
- 入网后先沿竖边单轴移动到 target.y，再沿该横边单轴移动到 target.x；
- 非零输入未产生位移时，route 回到 `need_join`，join-near 切换 join-far，
  join-far 只记录不变量失败；任何阻挡都不得伪造到达；
- sweep 和 reward claim 各自持有独立导航状态；奖励导航不改变战斗驱动状态；
- 请求拾取、pending save、实际位置半径验证和所有后续断言保持原生产路径。

奖励 y=0 位于第 10 行 cell 边界，中心区域由环境生成器保证无障碍；三个 x 坐标
也都在中心开放区域。因此入网后的 `Y -> X` 路线可达。

## 3. 有限预算

最坏公开路线保守上界：入网探针不超过两个 cell width，之后到 y=0 和 x=0 各
不超过 room half extent：

```text
2 * 8.1241 + 81.2404 + 81.2404 < 179 units
179 / 0.099 < 1810 ticks
```

领取上限设为 2,400 tick，额外约 590 tick 留给离散容差、一次错误探针和存档
提交。不得增加 combat 预算或 CTest 75 秒 timeout。失败诊断必须打印玩家、目标、
导航 phase/boundary 和不变量失败计数。

## 4. 验收

串行构建 Stage 10 fixture，standalone 必须 exit 0；随后精确 transaction CTest
和 evidence guard 各 1/1 通过。只允许 fixture 改动，不修改生产玩法、奖励位置、
拾取半径、存档或物品规则。

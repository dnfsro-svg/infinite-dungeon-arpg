# Task 9 实施与验证报告

## 结果

Task 9 已实现确定性的世界空间房间渲染：背景按相机可见范围生成固定容量世界块计划，门、洞、环境物件和障碍统一经相机投影；环境查询显式区分非法查询与容量硬故障，并保持 sealed blueprint、世界 AABB、稳定 ordinal 和 RNG 状态不变。

## 主要实现

- 现有 `2560x1440` 房间背景 atlas 被切分为 `10x10` 世界块；每帧只发布相机矩形加一块边距内的稳定行优先计划，固定容量 `25`，不新增 RenderTexture、framebuffer 或纹理驻留类。
- `MaterialPack::draw_frame_to` 支持目标矩形、旋转与 tint，生产房间渲染不再把整张背景拉伸到屏幕。
- 环境查询上限保持 `7x5x3=105`，并以 `static_assert(105<=128)` 固化证明；非法 span、offset、cell count 或输出容量均返回 canonical empty hard fault，不截断、不退化为全房间扫描。
- 环境布局拒绝不一致数量，携带 sealed obstacle world AABB、稳定 ordinal、intact/broken 视觉状态和四分之一转旋转。
- `DungeonRenderSnapshot` 显式携带 `exits_unlocked`、`full_clear` 与环境查询结果；25% 解锁与 100% 清理装饰不再混用。
- 门继续使用非红色元素材质；洞的中心与轮廓半径都随相机投影缩放。

## 回归修复

完整回归暴露并修复了 Task 7 之后遗留的测试契约：

- 怪物掉落测试现在使用 source home-cell 一格游走范围内的合法位置，保留固定 home-cell bucket 与 `331` 候选容量证明；未改为按死亡位置迁移 bucket。
- 1000 房装备轨迹使用 sealed blueprint 的初始位置，稳定通过自动拾取与重载。
- V9 权威装备容量已为 `1152`，深渊奖励饱和夹具不再只填旧的 `192` 槽；该夹具明确是 raw-capacity 防御场景。
- 1000 房新确定性黄金值为 inventory validation `967`；`unexpected_hot_path_allocations=0`，总分配分解仍成立。
- 健康药 mutation-gate 测试的 7 个大型 `DungeonSession` 改为堆对象，消除已由 WER 确认的 `0xC00000FD` / 4 MiB 测试栈溢出，未提高 `/STACK`。

## TDD 与验证

初始 RED / GREEN 证据：

- Dungeon focused：14 项中 1 失败（hard-fault seam），扩展查询后 16 项中 2 失败，session seam 后 18 项中 2 失败，最终 `19/19`。
- Platform focused：12 项中 2 失败（背景计划为空），布局阶段 53 项中 3 失败，最终 `57/57`。
- 完整回归首轮暴露旧掉落夹具、深渊容量基线和测试栈问题；逐项最小复现并修复。

最终实际执行结果：

- `ARPG_TASK9_ENVIRONMENT_RENDER_ONLY=1 arpg_dungeon_tests.exe`：`19/19`。
- `ARPG_HEALTH_POTION_ONLY=1 arpg_dungeon_tests.exe`：`19/19`。
- `ARPG_STAGE8_EQUIPMENT_STRESS_ONLY=1 arpg_dungeon_tests.exe`：`2/2`，含 1000 房确定性轨迹。
- `ARPG_STAGE10_ABYSS_REWARD_ONLY=1 arpg_dungeon_tests.exe`：`32/32`。
- `arpg_dungeon_tests.exe`：`359/359`，`0` 失败。
- `arpg_platform_tests.exe`：`574/574`，`0` 失败。
- `git diff --check`：通过；仅报告仓库既有的 Windows 行尾提示。

## 资源与边界

- 构建与测试始终串行 `-j1`；一次 detached 基线编译的单个 `cl.exe` 私有内存达到约 `12.9 GB`，已立即终止，系统可用内存从 `0.56 GB` 恢复到约 `5.7 GB`，未再次运行该高成本基线。
- 本任务未修改玩法规则、掉落 bucket 容量模型、V9 wire 格式或 main 工作树的用户未提交内容。
- 自动测试证明数据与渲染计划契约；最终全屏画面与操作手感仍需 Windows 游戏窗口验收。

## 独立审查修复

首轮独立审查报告 `P0=0 / P1=3 / P2=0`，三项均已按 RED/GREEN 闭环：

- visible query 现在校验当前 span 与前一 span 连续、首 offset、record home cell 与 ordinal；任何损坏都发布 canonical empty `environment_capacity` hard fault，不再静默漏记录。
- 背景块不再把透视四角包成相互重叠的轴对齐矩形；相邻块共享同一组投影顶点，通过 `MaterialScreenQuad` 进入原材质 shader。
- `quarter_turns` 保持为密封的世界布局/碰撞朝向；直立 2.5D billboard 不再执行屏幕平面 90/180/270 度旋转。

修复前定向测试稳定复现 `19` 项中 `1` 失败、`57` 项中 `2` 失败；修复后为 dungeon `19/19`、platform `57/57`。

最终复审追加发现 quad API 会接受零面积、凹或自交几何；按真实提交顺序增加了平移不变、随边长缩放容差的严格凸四边形一致 winding 校验，并由 `MaterialPack` 与背景投影共同使用。测试先以零面积反例复现 `57` 项中 `1` 失败，再加入 `0.001 x 0.001` 亚像素合法正例、凹与 bow-tie 反例；最终 `57/57`。独立终审结论为 `APPROVED`，`P0=0 / P1=0 / P2=0`。

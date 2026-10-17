# Task 10：真实输入 1000 房路径调查

## 目标与边界

在扩大真实 `queue_action` + `MovementInput` 压力前，先验证最小、可重复、合法的
Stage 4 遭遇能否由实际 CombatWorld 输入完成。调查期间不调用 `force_defeat_current_wave`，
不修改生产战斗/AI，不把受控夹具作为替代结论。

## 最小合法固定样本

- 规则：`base_budget=2`、`max_budget=2`、`two_wave_threshold=2`，其余
  `EncounterDirectorConfig` 默认值。
- 验证：`validate_encounter_director_config` 接受该配置；预算 2 不拆波。
- 目录：唯一成本 2 的直接目标是 `chaos_chaser`，导演的确定后备/最低成本选择形成
  单波 1 只 chaser。
- 根 seed：`0x5245414C494E5055`。
- 路线：room0 选择 up 进入 room1；room1 descriptor 也由同一稳定状态通过
  `make_door_transition` 单独重建，用于对照。
- 每个 tick 排空 dungeon 与 combat event，因此没有把 relay/event 队列积压误判为
  输入失败。

## 复现证据（Debug core executable）

### room0：light 机器人成功

- 初始：player `(0.00, 0.00)`，chaser `(-2.81, -3.43)`，HP 260。
- `queue_action(light)`：10/10 accepted，0 rejected。
- swing 10、active 采样 55、hit 10、defeated 1、player hit/hurt 0/0。
- 无挥空分叉，随后真实门移动与提交成功。

### room1：相同 light 机器人失败

- 初始：player `(0.00, 2.75)`，chaser `(0.50, 1.09)`，HP 260。
- `queue_action(light)`：7/7 accepted，0 rejected；说明不是 queue/input API 失败。
- swing 7、active 采样 39、hit 6、defeated 0；首次挥空在 tick 450，
  `dx=-0.30`、`dy=-0.51`、facing right、monster `recovery/idle`。
- player hit/hurt 为 52/52；最终 tick 4380 仍 combat，target HP 92，所有
  dungeon/relay/combat/input overflow 均为 0。

### room1 launcher 对照：成功

- 从同一 room1 稳定描述重建，移动与事件排空条件不变，只把动作改为 launcher。
- `queue_action(launcher)`：10/10 accepted，0 rejected。
- swing 10、active 采样 67、hit 7、defeated 1、player hit/hurt 0/0。
- 包含一次挥空（tick 108）但仍可完成清房。

## 单一根因假设

当前真实压力机器人只发轻击。room1 的入口出生把 chaser 放在近距离攻击节奏内；轻击
伤害/有效帧不足以在首次接触前结束战斗，之后 chaser 的周期命中反复施加玩家 hurt lock。
机器人为避免在 hurt lock 中积压输入，明确只在 `hurt_ticks==0` 发起动作，因而只能得到
很短的可攻击窗口并最终停留在 6 命中、92 HP 的活怪状态。

launcher 在完全相同的世界状态和输入通道上可击败该怪，排除了“DungeonSession 无法用
真实动作清房”“事件排空缺失”“AI/战斗接口不可用”三个假设。下一步允许的唯一策略调整是
将真实压力机器人在该最小 chaser 配置下的攻击动作改为 launcher；不更改生产数值、AI、
容量或会话规则。先重跑 10 房，再决定是否扩展 1000 房。

## 唯一 launcher 调整后的结果

### 10 房：通过

同一 seed、规则和路线下，launcher-only 真实机器人完成 10/10 committed rooms。每房
`queue_action(launcher)` 均 accepted；典型房间为 7 次命中、1 次 defeated、0 次 player
hit/hurt。该结果确认 launcher 能覆盖 room1 的近身 hurt-lock 情况。

### 1000 房：在 room38 停止

按相同 launcher-only 策略扩展后，room38 在 4096 tick 上限内失败，立即停止，未尝试任何
第二策略或生产改动。

- room38 初始 player `(-6.50, 0.00)`、facing `right`。
- chaser 初始 `(-7.05, -1.72)`，即目标位于玩家左后方，但 `abs(dx)=0.55`。
- 现有 `movement_toward` 的横向死区是 `1.00`，因此机器人不产生横向移动，也不会更新
  facing。
- `in_light_attack_lane`（名称保留自旧测试）要求目标方向与 facing 一致；目标在左、facing
  仍为 right，所以 launcher 永远不入队。
- 轨迹：queue/swing/active/hit/defeated 全为 0，player hit/hurt 为 53/53；四类 overflow
  全为 0。

## 根因、单一修复与最终状态

单一根因是压力测试机器人的 steering/facing deadband：它不能处理“目标在反向且距离小于
1.00”的入口几何。该根因位于测试机器人，不是 CombatWorld AI、真实动作 API、事件排空、
导演合法性或会话保存事务。

经批准实施一项测试控制器最小修复：仅当 nearest target 位于反向、`abs(dx) <= 1.00` 且
facing 不朝向目标时，输出一 tick 对应 X 方向 movement，以更新 facing。其他 movement、
launcher 入队条件、AI、事件排空、门移动、pending transition 与 save receipt 流程均未变。

### 回归序列

1. **RED room38 几何回归**：旧控制器稳定复现 `queue=0`、`swing=0`、`hit=0`，并在 4096
   tick 失败。
2. **GREEN room38**：修复后至少一次 launcher attack/hit 并完成清房。
3. **同 seed 10 房**：10/10 committed rooms 通过。
4. **同 seed 1000 房**：1000/1000 committed rooms 通过，输出
   `allocation-delta=0 overflow=0/0/0/0`。

真实输入耐久路径现已覆盖反向近距入口、真实 `queue_action(launcher)`、CombatWorld AI、事件
排空、wave 清房、门移动、pending transition 和逐房 save receipt；不依赖 `force_defeat`。

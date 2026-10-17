# Stage 10 Task 10 实现报告

## 结果

实现了深渊奖励 claim、离房 abandon 和普通 room transition 三类事务的原子边界，并增加不入存档的二次确认状态。重载 cleared abyss 现在构造零怪、无深渊环境的导航 `CombatWorld`，玩家可移动、自动领取并正常走门/洞离场，不会重开战斗或重复结算。

## 事务表

| 事务 | 准备态校验 | next state | receipt 成功后发布 | 失败行为 |
|---|---|---|---|---|
| 普通 `loot_pickup` | 原有距离、掉落、ownership、ID、generation 校验 | ownership 加物品、普通 claimed drop bit；普通 sequence 语义不变 | 删除对应普通 ground | 保持原有 rollback/indeterminate 语义 |
| `abyss_reward_materialized` | cleared、slot 未 generated/abandoned、确定性 ground、ID 唯一、revision/generation 可递增 | generated bit、reward revision +1 | 发布确定性 abyss ground | not committed/mismatch/indeterminate fail closed |
| `abyss_reward_claim` | cleared abyss、导航 `CombatWorld` 存在、玩家在 pickup 半径内、source/ground ordinal/reward ordinal、generated=1、claimed=0、abandoned=0、完整确定性 ground、ID 唯一、ownership 合法、容量、revision/generation | ownership 复制后追加物品、claimed bit、reward revision +1；`next_item_sequence` 不变 | exact receipt 后才删除 abyss ground | 距离不足为 rejected；not committed、indeterminate、receipt/cache mismatch 均 fault 且 ground/masks 保持；容量满或 allocation failure 为 rejected，可重试 |
| 普通 `transition` | 原有 door/descent builder；若离开 cleared abyss 且无 remaining，同事务生成 resolution | 目标房、清空普通 claimed drop bits、目标房 abyss checkpoint、覆盖 `LastAbyssResolution` | 清 ground/combat，发布 transition committed + room destroyed | 原有普通 transition 语义 |
| `abyss_abandon` | 第二次同 transition/direction/revision 确认；builder 与旧 abyss checkpoint 合法 | 未 generated 且未 abandoned 的位在旧 checkpoint 结算视图中加入 abandoned；目标房 checkpoint 不泄漏旧状态；同事务写稳定 resolution | exact receipt 后按普通 transition 发布并销毁未拾取 ground | not committed/mismatch fault，留在原房；磁盘由 SaveStore 保证只能 old/new |

`LastAbyssResolution` 使用有效 reward mask 的 popcount 写 `generated/claimed/abandoned`，`total` 固定为旧房 reward total；generated 但未领取的物品不写 abandoned，离房时随 ground 销毁，可由 `generated - claimed` 推导 unpicked。

## 确认状态机

瞬态结构：`armed + transition + direction + door_input_released + reward_revision`，不进入 checkpoint/codec。release latch 放在 `AbyssExitConfirmation` 原有对齐空隙中，不扩大 `DungeonSession`。

| 当前状态 | 输入/条件 | 下一个状态/动作 |
|---|---|---|
| unarmed | 无 pending/unpicked | 直接创建普通 transition，并在同事务写 resolution |
| unarmed | 有 remaining，第一次 door/hole 触发 | 预留并发布 `abyss_exit_warning`（含 pending/unpicked），成功后 armed；不创建 save |
| unarmed | warning 队列已满 | 不 armed，进入 `event_overflow` fault |
| armed door，未释放 | 持续按同方向任意 tick | 保持 armed，不创建 save |
| armed door，未释放 | 仍在门范围且出现一次无 outward request 的 tick | 设置 transient release latch，仍不创建 save |
| armed door，已释放 | 再次同 direction + revision 触发 | 清 armed，创建单个 `abyss_abandon` 原子 save |
| armed descent | 第二次显式 E 且仍在范围 | 清 armed，创建单个 `abyss_abandon` 原子 save；不要求 release edge |
| armed | 换方向/换 transition | 清 armed，不离房 |
| armed | reward materialize/claim 准备或 revision 改变 | 清 armed |
| armed door | 每 tick 离开对应边界门宽 | 清 armed |
| armed descent | 每 tick 离开固定洞中心 `(0, 3.5)` 半径 `2.0` | 清 armed，无需再次按 E |
| 任意 | reset/fault/transition commit | 清 transient |

快照发布 `abyss_pending_rewards`（未 generated 且未 abandoned）、`abyss_unpicked_rewards`（`valid & generated & ~claimed` 的 popcount，不依赖 ground 是否已重建）以及 confirmation armed/transition/direction。重载构造新 session，天然 unarmed。

## SaveStore fault matrix

real `SaveStore` 对 claim 和 abandon 各执行以下 7 个 fault point：

| fault point | 重载允许状态 |
|---|---|
| `before_temp_write` | old |
| `after_temp_write` | old |
| `after_temp_validation` | old |
| `before_publish` | old |
| `after_publish` | new |
| `final_scan_a` | new |
| `final_scan_b` | new |

每格均以完整 `same_run_state` 比较。claim 额外断言 item count、claimed mask 和不变的 `next_item_sequence=9001`；abandon 额外断言目标房/`LastAbyssResolution` 一致，未出现复制物品或 eligibility 半丢。

## RED -> GREEN 证据

1. RED：先把 Task 9 的“abyss pickup reserved/rejected”改为 claim 期望；专项运行 `16 cases, 1 failures`，失败点为 `request_pickup(...) == accepted`。
2. GREEN：实现 claim 最小事务后专项 `16 cases, 0 failures`。
3. RED：加入 confirmation/snapshot/warning API 测试，编译明确失败于缺失 `abyss_pending_rewards`、confirmation 字段和 `abyss_exit_warning`。
4. GREEN：实现状态机、finalize helper、零怪 reload navigation world；新增 Task 10 综合专项全部通过。旧 Task 9 测试随后按新增自动 claim 时序适配。
5. RED：增加非法 source/确定性位置测试，专项 `24 cases, 1 failures`，非法 source 被旧路径误当普通掉落。
6. GREEN：显式拒绝未知 `GroundItemSource` 并要求 abyss claim 当前房为 abyss；专项 `24 cases, 0 failures`。
7. 完整 dungeon 初跑暴露 5 个旧压力驱动未服务 claim/二次确认；按根因更新共享驱动后 `194 cases, 0 failures`。
8. 评审修复 RED：专项 `26 cases, 3 failures`，分别证明持续按门会错误离房、generated 但未重建 ground 时 unpicked 错为 0、远距离 abyss claim 被接受。
9. 评审修复 GREEN：增加门 release edge、mask-only unpicked 和统一 pickup 距离校验后专项 `26 cases, 0 failures`；标准 1MB 栈完整 dungeon 为 `196 cases, 0 failures`。
10. 栈临界回归：初版 release 检查在 `tick` 中物化第二个大型 `CombatSnapshot`，launcher stress 以 `0xC00000FD` 暴露；最终把 confirmation range/release、nearby pickup 和 requested exit 合并到同一个 snapshot，标准栈千房回归通过。当前实测 `sizeof(DungeonSession)=95336`、`sizeof(DungeonSnapshot)=49640`。
11. 复审覆盖补强：增加独立 session 级门退出用例，构造 high danger `total=3/generated_mask=1` 并保留对应 ground、填满其余 ground 槽，验证首次 warning 为 pending=2/unpicked=1，neutral release 后第二次同门产生 `generated=1/claimed=0/abandoned=2`，exact receipt 后离房且已生成未领取奖励不计 abandoned。以临时错误 mutation `~claimed_mask` 运行 RED 时新断言准确失败；恢复正确 `~generated_mask` 后专项 `27 cases, 0 failures`。生产实现未改动。

## 最终验证

- fresh build：`arpg_dungeon_tests arpg_persistence_tests arpg_platform_tests`，成功。
- `ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure`：2/2，0 failures；dungeon 149.19s，persistence 0.24s。
- `ctest --preset windows-msvc-debug -R "platform.units|^architecture\\." --output-on-failure`：19/19，0 failures；包含 platform 1 项和 architecture 18 项，总计 65.16s。
- 深渊奖励专项：27 cases，0 failures。
- persistence 全套：71 cases，0 failures。
- 千房压力：unexpected allocation bucket 为 0，dungeon/relay/combat/input overflow 均为 0。
- `git diff --check`：通过（仅 Git 的 LF -> CRLF 工作树提示，无 whitespace error）。

## 疑虑/边界

- 按任务约束未实现 UI 渲染；warning 已通过明确事件和快照计数提供给后续 UI。
- `request_descent(true)` 第一次警告返回 `false`，语义为“未发生 descent”；是否已 armed 由 warning 事件和 snapshot 明确表达。
- 门确认要求“同一门范围内至少一个无 outward request 的 tick”；持续按住不会触发离房。release latch 为纯 transient，方向/range/revision/reset/fault 均清除。
- 重载 cleared abyss 的运行时 HP/barrier 由已结算 player build 重建为可导航资源状态；checkpoint 本身不持久化逐帧 HP/barrier，因此没有额外运行时资源可恢复。

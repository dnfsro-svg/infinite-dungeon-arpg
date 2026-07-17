# Stage 11-A 死亡、退层与继续验证记录

## 验证范围

Stage 11-A 在 Stage 10 基线 `a06eddbdf557b7209146f12ec1a6bc75a1aa75b3` 上完成统一的普通/深渊死亡入口、死亡回顾、退回上一层和按 `E` 继续闭环。目标房在第一次事务中完整保存；重试或重启只能复用该目标，不能重骰。

本阶段明确不实现设置或按键重绑定、物品过滤器、完整 HUD、macOS 构建，以及 Stage 11-B。首领、召唤、光环和新装备/词缀/怪物/深渊内容也不在本阶段范围内。

## 单向模块边界

数据和控制方向固定为：

```text
Combat -> Dungeon -> Persistence
                  -> platform/raylib
```

- `combat` 只维护 300 tick 固定容量承伤历史、稳定致死来源和冻结的战斗死亡快照，不读取房间、存档或 raylib。
- `dungeon` 把战斗快照与房间/深渊/永久角色状态组合成稳定死亡检查点，生成确定性退层目标并编排事务。
- `persistence` 只编码稳定 checkpoint 类型、CRC、版本迁移和双槽恢复，不反向链接 Dungeon 实现或 Combat/raylib 目录。
- `platform/raylib` 只读取公开 `DungeonSnapshot`、执行输入门禁和绘制覆盖层；它不能直接改稳定状态、构造死亡检查点或注入死亡。

`stage11.architecture.death_boundaries`、`architecture.*_no_raylib`、`platform.module_boundary` 与 Stage 11 evidence guard 共同验证这些边界。

## 两次原子提交

1. `death_retreat`：生命归零后立即冻结 Combat；事务一次写入递增的 `death_sequence`、完整死亡回顾和完整目标房。普通死亡保留历史深渊结算；深渊死亡在同一事务中写入 failed lifecycle 与 `LastAbyssResolution=failed`。`not_committed` 允许按相同输入确定性重试；`indeterminate` 或 receipt 的 kind、代数、旧/新状态不匹配一律 fault。
2. `death_continue`：只在稳定状态为 `pending_continue` 时接受 `E`。事务把已保存目标发布为当前房并清空死亡检查点，不再次增加死亡序号。`not_committed` 保持原回顾供重试；`indeterminate` 和 receipt 不匹配一律 fault。

死亡与同 tick 清房冲突时死亡优先，当前房经验、掉落与奖励不得结算。固定 tick 的顺序为 Session tick 后立即服务 pending save；死亡保存期间只允许 F12/V、F1、Esc，提交完成后才额外允许 E。

## V6 存档布局与 V5 迁移

V6 magic 为 8 字节 `ARPGSV6\0`，format 为 6。V5 的 204-byte base payload 后新增 `death_sequence` 和 224-byte death block；V6 base payload 为 428 bytes、无物品时完整编码为 460 bytes，40-byte item records 从绝对偏移 460 开始。

| 绝对偏移 | 大小 | 内容 |
| ---: | ---: | --- |
| 236 | 8 | `death_sequence` |
| 244 | 8 | lifecycle、data version、来源、伤害类型、怪物 ID、reserved、detail ID |
| 252 | 4 | 深渊标记、死亡生态、reserved |
| 256 | 16 | 死亡深度、层内房号 |
| 272 | 32 | raw、护盾损失、生命损失、final damage |
| 304 | 40 | 最近 5 秒物理/火/水/电/混沌实际承伤 |
| 344 | 40 | HP/护盾、护甲/闪避值、减伤率/闪避率 |
| 384 | 32 | 四元素减伤率与上限 |
| 416 | 36 | 目标房 index/seed/depth/floor、入口/生态/洞/深渊 |
| 452 | 8 | reserved zero |

编码器只写 V6；解码器接受 V1–V6。真实 V5 fixture 迁移后固定为 `death_sequence=0` 和规范 `DeathLifecycle::none`，其余 V5 房间、深渊、角色、星盘、物品和摘要语义不变。结构非法、截断、尾随、CRC、magic/version、reserved 和长度错误均拒绝；需要房间规则和目录的语义校验留在 Dungeon 构造边界。

## 正式五路径与证据

`stage11.death_formal.five_paths` 启动真实 `run_raylib_host`，在 1280x720 窗口经生产输入/战斗/存档路径生成并校验五张 fresh 截图。deep/floor-one 的自动继续只把 `FrameKeyState.e` 置位，随后与玩家输入共用 `death_input_gate`、唯一的 continue request 分支和 `runtime.fixed_tick` 提交链：

| 路径 | 证据 | 验证点 |
| --- | --- | --- |
| 普通死亡回顾 | `01-normal-death.png` | 正常伤害触发死亡并显示完整回顾 |
| 重启恢复同一回顾 | `02-restarted-death.png` | panel hash、目标 seed 和死亡 hash 与重启前完全相同 |
| 深层死亡后继续 | `03-deep-continued.png` | 第 2 层死亡的保存目标与最终房均为第 1 层 |
| 第 1 层死亡后继续 | `04-floor-one-continued.png` | 目标和最终深度均夹紧为 1 |
| 深渊死亡回顾 | `05-abyss-death.png` | 深渊标记和 failed resolution 同一事务持久化 |

构建目录中的证据目录为：

```text
out/build/windows-msvc-debug/bin/stage11-death-formal-validation/
out/build/windows-msvc-release/bin/stage11-death-formal-validation/
```

每个目录还包含独立存档目录和 `formal-path-summary.txt`。PowerShell validator 检查截图新鲜度、尺寸、可见内容、死亡 panel、普通/重启 panel hash 一致性，以及五条 summary marker。`stage11.death_evidence.production_paths` 和其余四项负向/变异 guard 防止私有注入、公开死亡注入、Present 前截图或字段校验被移除。

## 1000 次死亡压力

`stage11.death_stress.determinism_zero_alloc` 的 direct/restart 两条 trace 都从同一个稳定状态开始，各自在同一个持续演进的状态链上执行 1000 次死亡与继续；`death_sequence` 必须逐次从 1 增加到 1000。restart trace 分别按 17、31、43 的间隔执行 codec round-trip 或重建 Session。测试要求两条 trace 的每个死亡检查点、退层 seed、累计 hash、最终完整稳定状态与非默认永久角色字段一致。继续 prepare 的 allocation counter 只包围 `request_death_continue()`，随后通过 `pending_save_view()` 检查结果，避免测试副本污染计数；死亡 prepare、两次 commit 与继续 prepare 的受测路径均要求零堆分配。普通掉落随机流保持 Stage 10 golden 不变。

## 需求到测试名映射

| Stage 11-A 要求 | 主要 CTest / suite case |
| --- | --- |
| 300 tick 五类承伤、饱和、零分配 | `combat.units`: `300 tick inclusive window`, `history saturates`, `history allocates nothing` |
| 所有真实致死来源、最后一击、冻结、同 tick 死亡优先 | `combat.units`: `player_death_snapshot` suite 的 `projectile real source`、`native hazard real source`、`burning ground real source`、`abyss real source`、`first lethal hit wins`、`death freezes combat`、`same tick mutual defeat`、`lethal tick zero allocations` |
| 确定性退层、深度夹紧、独立随机流和 overflow | `dungeon.units`: `retreat clamps depth and uses named stream`, `retreat replay and ordinary streams`, `retreat overflow faults are explicit` |
| 普通/深渊 `death_retreat`、receipt、重试和同 tick 事件 | `dungeon.units`: `ordinary death prepares atomic retreat`, `abyss death prepares one atomic retreat`, `ordinary death receipt fault matrix`, `death wins same tick and events exactly once` |
| `death_continue` 复用目标、深度 1、重试和 receipt | `dungeon.units`: `death continue prepares exact target`, `depth one continue constructs saved room`, `death continue retry is exact`, `death continue receipt fault matrix` |
| V6 固定布局、完整 round-trip、V5 迁移、非法输入拒绝 | `persistence.units`: `v6 layout and full round trip`, `v5 fixture migrates to canonical none`, `v6 death enum boolean reserved and state errors`, `v6 lengths crc and magic are rejected` |
| 跨真实 SaveStore 重启与固定 tick 提交顺序 | `platform.units`: `fixed tick commits death before returning snapshot`, `runtime continue is narrow and fixed tick commits it`, `v6 pending death load preserves generation and target`；`stage11.death_fixture.transactions` |
| 死亡输入仅 E/F12/V/F1/Esc | `platform.units`: `saving allows only global controls`, `pending allows continue and global controls`, `saving wins when both flags are set` |
| 完整回顾、中文来源、fallback 和窗口边界 | `platform.units`: `hidden and complete recap mapping`, `all source ids have Chinese names`, `ASCII fallback maps complete recap`, `layouts fit supported windows`, `worst case values fit compact columns` |
| 1000 次确定性/重启/零分配 | `stage11.death_stress.determinism_zero_alloc` |
| 真实 raylib 五路径和生产证据 | `stage11.death_formal.five_paths`, `stage11.death_evidence.production_paths`, `stage11.death_evidence.rejects_private_injection`, `stage11.death_evidence.rejects_pre_present_capture`, `stage11.death_evidence.rejects_public_death_injection`, `stage11.death_evidence.mutation_self_test` |
| Combat→Dungeon→Persistence→raylib 单向边界 | `stage11.architecture.death_boundaries`, `architecture.core_no_raylib`, `architecture.combat_no_raylib`, `architecture.dungeon_no_raylib`, `architecture.persistence_no_raylib`, `platform.module_boundary` |

## 全量验证结果

2026-07-17 在 MSVC 19.44.35228.0、Windows SDK 10.0.26100.0 环境中按下一节命令从头验证。下表是最终审查修复前、文档收口提交 `a0c7395` 的完整基线：

| 配置 | clean-first 构建 | 完整 CTest | 总耗时 | Stage 11 stress | Stage 11 五路径 |
| --- | --- | --- | ---: | ---: | ---: |
| Debug | 237/237 | 52/52，0 失败 | 739.92 秒 | 123.05 秒 | 6.69 秒 |
| Release | 237/237 | 52/52，0 失败 | 282.57 秒 | 35.67 秒 | 6.62 秒 |

Release fresh 源码头声明 raylib 6.0.0，配置和 host 编译期断言均验证该版本；最终 Ninja 链接边包含静态 `lib/raylib.lib`，`dumpbin /dependents` 不含 raylib DLL。Release 正式测试重新生成了五张截图和 `formal-path-summary.txt`，五条 marker 全部为 `PASS`。

最终审查增强验证后，Debug 定向复验结果为：演进状态版 1000 次 stress 通过（150.19 秒），真实 raylib 五路径 1/1 通过（7.06 秒），Stage 11 evidence guards 5/5 通过，Platform 3/3 通过，Architecture 21/21 通过。stress 相比收口基线的 123.05 秒增加 27.14 秒（约 22%），原因是每条 trace 不再重建 1000 个独立根状态，而是运行真实连续房间链、消费公共事件并比较最终完整状态。

## 从头复现

在仓库根目录使用固定的 MSVC/SDK 环境运行：

```powershell
cmake --preset windows-msvc-debug --fresh
cmake --build --preset windows-msvc-debug --clean-first
ctest --preset windows-msvc-debug --output-on-failure

cmake --preset windows-msvc-release --fresh
cmake --build --preset windows-msvc-release --clean-first
ctest --preset windows-msvc-release --output-on-failure
```

配置阶段固定下载并校验 raylib 6.0.0；正式 host 还使用编译期版本断言。完整 CTest 包含 headless unit、架构、压力、真实图形、formal 和 evidence guard，不需要手工排除或追加测试。

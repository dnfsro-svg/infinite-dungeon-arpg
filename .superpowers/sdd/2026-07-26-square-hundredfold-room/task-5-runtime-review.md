# Task 5 运行时接线只读审查

- 审查快照：2026-07-27，基线 `c6ecd4f090d47fdede97f4a601c9909eb61624cd`
- 范围：`DungeonSession` / `DungeonRuntime` / `raylib_host` 的 V9 capture、load、commit、background、shutdown、input rearm 接线及相关当前测试
- 约束：只读源码；未修改生产代码；未构建、未运行测试
- 结论：**当前不可提交，存在 9 项 P0、6 项 P1、2 项 P2。** 实现仍在变化，下列只记录当前代码中可直接证明的问题。

## P0

### P0-1：异步 completion 没有独立的 per-frame pump，inventory 暂停固定步时事务可永久停在 `committing`

当前 completion 只从 `fixed_tick()`、`service_pending_save()` 路径轮询。Inventory 点击后只调用一次 `service_pending_save()`（例如 `src/platform/raylib/inventory_renderer.cpp:633-646,699-716,740-782`）；inventory 打开时 host 不推进 fixed step（`src/platform/raylib/raylib_host.cpp:3153-3165`）。若 worker 在那次即时 poll 之后完成，session 会一直保持 `committing`，直到玩家关闭 inventory 或再次触发某条显式 service 路径。

当前 `fixed_tick()` 已不再等待 worker，这是正确方向；但必须把 completion pump 接入每个 presented frame，包括 pause/inventory/closing，异步闭环才完整。

### P0-2：clean shutdown exact transaction 和 close latch 完全未接入

`DungeonRuntime` 没有 shutdown request/closing API；`raylib_host.cpp` 仍把窗口关闭、死亡界面退出、暂停菜单 Quit、验证退出直接映射为 `exit_requested=true`（`src/platform/raylib/raylib_host.cpp:2921-2966,3003-3047,3133-3138,3748-3772`）。析构函数只 `stop_and_join()` 已存在的 worker（`src/platform/raylib/dungeon_runtime.cpp:94-99`），不会捕获并 exact commit 最新 authority revision。

因此第一次 close 没有创建 shutdown intent，loop 不会留在 closing/present/poll；也没有 `not_committed` 取消 closing、`indeterminate` fault，以及 raylib close flag 持续为 true 时的 first-request/cancel latch。当前关闭最多等待已有 background job，可能丢失最近 5 秒状态。

### P0-3：没有 host 级物理释放重武装，exact 前也没有清除已缓冲 gameplay input

`raylib_host.cpp` 当前无 rearm 状态机。movement/action/skill/descent 在 exact 前照常提交（`src/platform/raylib/raylib_host.cpp:3170-3234,3262-3280`）；`DungeonRuntime` 进入 exact 时不清 `CombatWorld::InputBuffer`。`InputBuffer` 只在 V9 reload 的 `CombatWorld::restore_room_checkpoint()` 中清理（`src/combat/room_combat_checkpoint.cpp:935-939`）。

结果包括：提交前已排队的攻击可在事务后触发；I/O 期间按住的移动/攻击/技能/E 可在恢复首 tick 立即生效；同一 rendered frame 有多个 fixed steps 时，后续 step 可复用事务前的 held movement。应覆盖 movement、普通攻击、主动技能、E/下降、拾取相关 gameplay control，并保持 UI/recovery/close 输入可用；完成后必须等所有 gameplay keyboard/mouse controls 同帧 physical-up 才重武装。

### P0-4：exact job 是“pending durable state + mutation 前 combat state”的混合快照

`capture_save_checkpoint()` 可以用 `pending->next_state` 作为 durable override（`src/dungeon/dungeon_session.cpp:475-493`），但始终从当前 live `combat_` 捕获 combat（`src/dungeon/dungeon_session.cpp:529-533`），而真正 mutation 要到 completion 后才发布（`src/dungeon/dungeon_transition.cpp:1851-1858,1934-1957`）。这不是事务提交后的 immutable state：

- 装备/卸装/强化已改变 durable build，但 checkpoint 仍含旧 `max_hp/armor/evasion/...`；reload 构建新 build 后，`restore_room_checkpoint()` 会因派生属性不相等拒绝（`src/combat/room_combat_checkpoint.cpp:545-556`）。
- health-potion claim 已从 durable/ground 中消费，但 checkpoint HP 仍是治疗前值；崩溃重启会“药消失、血没回”。
- abyss start/fail 只要 room index/seed 相同就被当成 same room；`same_room` 未比较 `is_abyss` 等 descriptor 字段（`src/dungeon/dungeon_session.cpp:486-493`），可能把旧生态 combat 写进新 abyss durable state。

需要在 heap job slot 中构造完整的 **post-mutation** checkpoint，不能仅替换 `DungeonRunState`。

### P0-5：normal full-clear 并不总是 exact；走 exact 时落盘 flags 也错误

普通房没有 ground material 且没有可喝药时，`prepare_room_clear()` 直接结算并 `publish_room_clear()`（`src/dungeon/dungeon_session.cpp:2026-2033`），完全绕过 exact transaction，违反任务卡“Full clear ... are exact transactional requests”。

需要事务的 normal room-clear 把 `resume_phase` 设为 `combat`（`src/dungeon/dungeon_session.cpp:2085-2093`），但 capture 只在 `resume_phase` 为 `cleared/awaiting_exit` 时记录 `exits_unlocked`（`src/dungeon/dungeon_session.cpp:513-519`）。因此磁盘上会出现 durable progression 已结算、`full_clear=true`，却 `exits_unlocked=false/reward_committed=false` 的状态。reload 后 phase 回到 combat，下一 tick 可再次调用 room-clear 结算，造成重复 clear XP/错误出口状态。

### P0-6：`recover_with_new_run()` 仍走 legacy 同步 store，恢复后没有 worker

recovery-required 路径不会启动 V9 worker；`recover_with_new_run()` 仍调用 `store_.archive_invalid_and_create()` 后直接发布 session（`src/platform/raylib/dungeon_runtime.cpp:694-720`），没有初始化 `authority_revision_ / durable_revision_ / progress_dirty_`，也没有 `start_worker()`。

恢复成功后的游戏看似 running，但 background 永远无法提交，下一次 exact pending 也会因 `save_worker_ == nullptr` 永远停住。

### P0-7：secondary ordinal namespace 仍会让 material 覆盖 potion，reload 静默丢物

capture 直接用 material 原 ordinal；同一循环中 potion 使用 `2*spawn+1`（`src/dungeon/dungeon_session.cpp:555-588`）。当 material ordinal 恰为某个 odd potion claim ordinal 时，material 分支先 `continue`，活跃 potion 不会写入 checkpoint。结构校验只能看到单条有序 record，无法发现被漏掉的 live potion。

必须落实已定 canonical namespace：material/equipment secondary 的偶数映射、potion 的奇数映射、abyss 保留区，并在 claim bits adapter、capture、restore 三处一致。

### P0-8：`not_committed` 对部分 exact 仍会 fault，而不是取消 mutation 并继续游戏

`commit_pending_save()` 对 abyss start/fail/clear/reward/claim/abandon 的 `not_committed`（除一个特例）直接进入 `save_receipt_mismatch` fault（`src/dungeon/dungeon_transition.cpp:1676-1717`）。任务卡明确要求 `not_committed` 取消 requested mutation（以及 shutdown closing）并保持游戏运行。当前 runtime 虽正确映射成 `SaveDisposition::not_committed`，session 随后仍把它升级成 fault。

### P0-9：V9 load 未做到 main-thread 全语义验证后原子 publish

runtime 先 `session_.emplace(...)`，再对该已发布 optional 调 `restore_room_progress_checkpoint()`（`src/platform/raylib/dungeon_runtime.cpp:231-297`）。session restore 又先调用会改写 combat 的 `restore_room_checkpoint()`，之后才校验/写 ground source、ordinal 和 potion mapping（`src/dungeon/dungeon_session.cpp:607-675`）。结构 validator 并不校验所有这些后置条件（例如 equipment/material source 与 odd potion mapping），所以存在 combat 已改写后才返回 false 的实际路径。

应在 heap scratch owner 中依次完成 structural decode、monster/environment rebuild+hash、全部 bitset/record/cross invariant、collision、combat restore/transient canonicalization；全部成功后一次性把 owner publish 给 runtime。任何失败前不得触碰 live `session_`。

## P1

### P1-1：background cadence 用 `% 300 == 0`，busy 时会错过整个 5 秒窗口

`submit_background_if_due()` 只在 `fixed_tick_count_ % 300 == 0` 时尝试（`src/platform/raylib/dungeon_runtime.cpp:662-687`）。若该 tick 有 active background/exact/pending，条件失败后要等到 tick 600，而不是 slot 可用后立即 coalesce 最新 revision。需要 `background_due`/`next_due_tick` latch，并在每帧 pump 中重试最新 revision。

### P1-2：background 在无可持久化变化的 phase 也每 300 ticks 重写

runtime 每次 `session_->tick()` 后无条件增加 authority revision 并置 dirty（`src/platform/raylib/dungeon_runtime.cpp:505-521`）。例如 `death_pending` 的 `DungeonSession::tick()` 只增加未进入 checkpoint 的 `session_tick_`（`src/dungeon/dungeon_session.cpp:312-316`），V9 durable/room payload不变，runtime 仍会每 5 秒仅为新 persistence revision 写盘。应由可持久化 authority mutation 显式 bump revision/dirty，避免死亡界面等 idle phase 的无意义 I/O。

### P1-3：旧 completion 没有严格的“只消费当前 flight”规则

exact completion mismatch 被转换成 `indeterminate` 后仍调用 `session_->resolve_pending_save()`（`src/platform/raylib/dungeon_runtime.cpp:591-659`），没有 pending 时会把 session fault；这不是“旧 completion 不应用”。background completion则完全不比对 runtime 记录的 token/epoch/expected revision，只按 kind 更新 durable revision/status。runtime 应保存 background token/epoch，并对所有 completion 先做单消费者 receipt 匹配；stale receipt 只能丢弃/记录诊断，不能改 durable、session 或 fault state。

### P1-4：durable death/abyss 与 room-combat 副本没有显式交叉校验

`valid_room_progress_checkpoint_structural()` 只要求 lifecycle 与 `has_death_snapshot` 对应；runtime/session restore 没有把 combat death snapshot 与 durable `DeathCheckpoint` 做 exact cross-check，也没有把 room lifecycle/full-clear 与 durable abyss lifecycle/reward masks 做完整合法组合校验。当前 combat 内部只对 encounter config 的局部 abyss runtime 做校验，不能替代跨 owner invariant。

### P1-5：当前测试没有覆盖任务卡要求的 schedule/host 语义，并临时移除了绝大多数 platform suites

当前 runtime 测试的 `settle_runtime_save()` 是主动 yield/poll 到完成，无法证明真实 host 在 1/7/31 presentation frames 后仍正确；没有 clean-close/not-committed latch、physical rearm、inventory/pause per-frame pump、background due latch、stale completion injection 测试。`tests/platform/platform_test_main.cpp` 当前只注册 `dungeon_runtime_suite()`，把原 488-case registry 临时缩成 42 cases；若保持到提交，会让 host/input/UI 回归失去覆盖。

### P1-6：pending-death 启动续关使用无超时的主线程忙等

当前 `continue_pending_death_on_initialize()` 已不再依赖一次即时 poll 的竞速，但改为在 `exact_flight_.active` 上无限 `yield()`，直到 worker 完成（`src/platform/raylib/dungeon_runtime.cpp:138-163`）。这发生在进入 host loop 前，因而不是 P0 的“提交期间停止 presentation”，但 I/O/worker 若挂起会让启动永久卡死，且没有 cancel/timeout/fault transition。应使用明确的 initialization state/有界等待策略，或让 host 先进入可呈现的 loading/committing loop。

## P2

### P2-1：runtime 仍保留两套 persistence 路径和未使用旧 helper

`DungeonRuntime` 同时持有 legacy `SaveStore store_` 与 V9 `SaveCommitStorage/Worker`；`sync_load_status()`、`sync_commit_status()`、`to_session_result()` 仍存在，recovery 路径实际使用旧 store。收敛成单一路径可避免同一进程出现不同 codec/revision/故障语义。

### P2-2：生产路径残留无条件 `fprintf` 调试输出

initialize/capture 的成功与失败路径有多处 `v9 ...` 的无条件 stdout/stderr 输出（例如 `src/platform/raylib/dungeon_runtime.cpp:196-226,332-359,371-413`，`src/dungeon/dungeon_session.cpp:529-603`）。完成诊断后应改为现有诊断状态/受控日志，避免正式版噪声和测试时序污染。

## 已核实的正向点

- `DungeonRuntime::~DungeonRuntime()` 显式先 `stop_and_join()`；成员声明为 storage 后 worker，反向析构也保证 worker 先于 storage（`src/platform/raylib/dungeon_runtime.hpp:102-106`）。
- checkpoint/job 与两块 8 MiB buffer 均嵌入 heap-owned `SaveCommitStorage`，`DungeonRuntime` 只持 `unique_ptr`；当前 runtime/host 没有把这些巨型数组放到自动对象中。
- exact completion 已携带 revision/intent/token/epoch 并尝试匹配；问题在 mismatch/stale 的处理策略和 background receipt 未对称跟踪，而不是完全没有代际字段。

## 建议修复顺序

1. 先定义统一 runtime `pump_persistence_frame()`、exact/closing/background flight 状态与 receipt 规则；保持 fixed tick 无等待。
2. 做 post-mutation heap checkpoint builder，修复 full-clear、heal、equipment、abyss transaction 的完整快照。
3. 增加 scratch session/owner 的 V9 semantic validate + atomic publish。
4. 接 host close latch 与 GameplayRearmGate，并让每个 presented frame无条件 pump。
5. 修 secondary namespace、recovery path、pending-death startup 和 `not_committed` 语义。
6. 恢复完整 platform registry，再补 schedule injection、close/rearm/background/stale tests。

## 第二轮复核（2026-07-27，最新工作树）

- 复核方式：继续只读生产源码与当前测试注册表；本审查者未构建、未运行测试。
- 外部验证状态：实现者回报 MSVC 构建成功，`persistence` 为 `100/100`、完整 `platform` registry 为 `495/495`；这些数字仅作为实现者提供的验证证据记录。
- 当前结论：**剩余 P0 = 0、P1 = 1、P2 = 0；仍不批准。** 生产路径中原 9 项 P0、除测试覆盖外的 5 项 P1、2 项 P2 均已静态关闭，但任务卡明确要求的 completion 调度注入与 10,000 tick 连续性证据尚未实现。

### 原 17 项逐项状态

| 原问题 | 第二轮状态 | 复核结论 |
|---|---|---|
| P0-1 per-frame pump | 已关闭 | `raylib_host` 每个 presented frame 顶部调用 `pump_persistence_frame()`；pump 不依赖 fixed tick，pause/inventory/closing 时 completion 仍可被主线程应用。 |
| P0-2 clean shutdown / close latch | 已关闭 | runtime 已有 `request_clean_shutdown()` 与 closing/canceled/ready/faulted 状态；shutdown exact 留在 host loop 内轮询，`not_committed` 取消 closing，indeterminate/fault 统一进入 fault；raylib 6.0 的 GLFW close flag 会在 `PollInputEvents()` 后清除，host 只复位自身 edge latch。 |
| P0-3 physical rearm / buffered input | 已关闭 | exact capture 清 combat buffered input；runtime gate 阻止 authority request/fixed tick；host 要求键盘、技能键、鼠标同帧全部 physical-up，并通过 `gameplay_rearm_was_required` 保证释放帧本身也不转发 gameplay。多 fixed-step 同帧在每步进入 runtime 后再次受 rearm/exact gate。 |
| P0-4 post-mutation checkpoint | 已关闭 | capture 对 pending durable state 同时应用 staged player build、HP potion 治疗、abyss clear runtime 清理、claim/ground 可见性与 full-clear flags，不再生成 durable/combat 混合快照。 |
| P0-5 normal/abyss full clear exact | 已关闭 | 普通与深渊 full clear 均创建 exact pending；capture 把 pending full clear 写成 `full_clear + exits_unlocked + reward_committed`，重载恢复 `awaiting_exit`，避免重复结算。 |
| P0-6 recovery worker | 已关闭 | recovery 归档后启动同一 V9 worker，以 asynchronous recovery-initial exact 提交；完成前保持 recovery presentation，成功后才进入 running，并要求 physical rearm。 |
| P0-7 secondary namespace | 已关闭 | 普通 material 使用偶数、potion 使用奇数、abyss material 使用保留区；capture、restore、claim adapter 一致。 |
| P0-8 `not_committed` | 已关闭 | session 对所有 pending kind 统一取消未发布 mutation、恢复 resume phase 并保留 running；只有 indeterminate/receipt corruption fault。 |
| P0-9 scratch atomic load | 已关闭 | runtime 在 heap-owned candidate `DungeonSession` 中完成结构、owner、blueprint/hash、combat/ground restore；全部成功后才一次 publish 到 `session_`，失败 candidate 直接销毁。 |
| P1-1 background due latch | 已关闭 | `background_due_` 与 `next_background_tick_` 保留 busy 时到期意图，并由每帧 pump 在 slot 可用时重试最新 authority revision。 |
| P1-2 idle death dirty writes | 已关闭 | `death_pending` 的非持久化 session tick 不再 bump authority revision/dirty；真实 active authority tick 仍按设计进入 background cadence。 |
| P1-3 stale receipts | 已关闭（代码） | exact/background 均匹配 token、epoch、revision，exact 另匹配 intent；不匹配 completion 直接丢弃。worker stop/restart 会清 queued/completion 并提升 epoch，旧 lease/receipt 不跨 restart 应用。 |
| P1-4 death/abyss cross invariants | 已关闭（代码） | death room combat snapshot 与 durable death source、伤害、防御、房间 owner exact 对照；abyss lifecycle/full-clear/environment/reward masks 与可见 chest mask 交叉校验。Abyss claim 继续以 `claimed_mask` 为独立 durable owner，capture 仅按 reward ordinal/mask 排除旧 live chest，不改变 ground slot 或 item ID。 |
| P1-5 schedule/host/registry tests | **仍开放** | registry 已恢复完整 495，但当前只新增 clean-shutdown、rearm helper、post-heal、full-clear 等直接用例。未见任务卡要求的 completion before/after capture、1/7/31 presentation frames、active background 后排队、pause/inventory 零 fixed-tick pump、stale completion 注入，以及保存/重载后至少 10,000 authority ticks 的逐 tick hash/RNG/damage-history/obstacle/event 等价测试。现有 `settle_runtime_save()` 主动轮询到完成，不能替代这些 schedule 证明。 |
| P1-6 pending-death bounded wait | 已关闭 | startup auto-continue 使用 10 秒 deadline 与 1 ms sleep；超时明确 fault，不再无限 yield。V9 death room 可携带 combat/death 副本重载，内部启动续关不受外部 rearm gate 误拒绝。 |
| P2-1 双 persistence 路径 | 已关闭 | runtime 不再持有或调用 legacy `SaveStore` transaction path，recovery 也统一走 V9 storage/worker；旧 transition helper 已移除。 |
| P2-2 临时调试输出 | 已关闭 | 生产 `DungeonRuntime` / `DungeonSession` 中原 `v9 ...` 无条件 stdout/stderr 输出已移除；保留的格式化 UI 文本与受控 host `TraceLog` 不属于该问题。 |

### 第二轮中发现并已关闭的附加风险

- clean shutdown 排在既有 exact 后时，capture/submit/indeterminate 等错误现在都通过统一 `fault_persistence_runtime()` 推进 clean state 到 faulted，不再停在 closing 挂死。
- recovery initial commit 改为异步 flight；host 在 completion 前继续呈现 recovery screen，而不是在恢复按钮路径同步阻塞写盘。
- `fixed_tick()` 在每次 pump 后重新检查 exact/rearm，因此同一 rendered frame 的后续 catch-up step 不会在快速 completion 后越过 rearm。
- V9 death reload 保留合法 death-room combat owner；startup auto-continue 直接调用受内部状态校验的 session request，随后 exact 提交。`build_death_continue_next()` 与 pending death cache 均允许且核对该 restored combat，commit 后再清 transient。
- Abyss reward claim 的 V9 capture 以 durable `claimed_mask` 排除尚未从 live ground 数组发布删除的 chest；没有借用普通 monster claim ledger，也没有改变后续奖励复用 ground slot 与确定性 item ID。

### 剩余批准条件

只有补齐 `docs/superpowers/plans/2026-07-26-square-hundredfold-room.md` Task 5 的强制调度与连续性矩阵，并让新增 cases 纳入严格 platform registry、在完整 MSVC 回归中通过，才能把最后一项 P1 关闭并给出 `APPROVED`。在此之前，当前 verdict 为 **不批准**。

## 第三轮终审（2026-07-27，最新工作树）

- 终审范围仍限于 `DungeonSession` / `DungeonRuntime` / host 的 V9 capture、load、commit、background、shutdown、input rearm 接线及 Task 5 强制调度/连续性证据。
- 本审查者未修改生产代码或测试代码，未执行构建；在确认 preset 二进制时间晚于相关源文件后，独立执行了两个现有严格测试二进制。
- Fresh 验证：`arpg_platform_tests.exe` 为 **495 cases, 0 failures**（17.2 s）；`arpg_persistence_tests.exe` 为 **100 cases, 0 failures**（85.7 s）。
- 最终结论：**runtime 范围剩余 P0 = 0、P1 = 0、P2 = 0；APPROVED。** 该结论仅关闭本 runtime 审查报告的原 17 项与其强制证据条件，不覆盖独立 worker/storage 审查的结论。

### 最后一项 P1 关闭证据

1. `tests/platform/dungeon_runtime_tests.cpp` 现在执行 9 组确定性 schedule：真实 background completion 在新 exact capture 前由 worker 完成并由 `pump_persistence_frame()` 应用；新 exact completion 在 capture 后 0/1/7/31 个 presentation frames 完成；覆盖 active background 后排队、capture 前/后 stale receipt，以及 pause/inventory 等价的零 fixed-tick per-frame pump。
2. 所有 schedule 逐组比较 durable V9 bytes、下一 authority tick 的 V9 bytes、Dungeon/Combat event trace、导出 RNG state、装备交易结果、commit generation、authority revision 和 combat tick；不再仅依赖 poll-until-complete 的单一时序。
3. `tests/persistence/checkpoint_v9_tests.cpp` 从 quiescent boundary 执行真实 V9 encode/decode/restore，然后连续推进 10,000 authority ticks。每 tick 比较完整 V9 bytes/CRC、room progress、导出 evasion RNG state、damage-history totals、obstacle/fire-crate state 与完整 Dungeon/Combat event 序列。
4. 版本化瞬态清除仍由独立 `room checkpoint transients` 及 active-skill normalization 用例覆盖：buffered input、projectile、hazard、hit-stop、presentation event queue 和 active skill 均不被 10,000 tick 用例偶然忽略。

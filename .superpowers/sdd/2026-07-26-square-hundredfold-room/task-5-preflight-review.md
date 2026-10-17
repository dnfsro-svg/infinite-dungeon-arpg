# Task 5 架构预检（基线 `c6ecd4f`）

## 结论

Task 5 当前存在 7 项 P0。前四项会直接导致接口无法编译、后台存档产生冲突槽，或无法满足无分配/完整恢复要求；后三项是规格内部缺失或互相冲突，若不先定案，不同模块会实现出互不兼容的 V9。

本报告为只读预检。除本报告外未修改源码；按要求未构建、未运行测试。

## P0（实施前必须定案）

### P0-1：巨型、不可复制 checkpoint 与现有按值 `DungeonRunState` 架构冲突

规格同时要求：

- `state.room_progress` 持有固定容量的大数组；
- checkpoint/job 禁止 copy/move，且不得按值返回；
- 大数组只能位于 `SaveCommitStorage` 堆槽，不能嵌入 host/runtime/worker 自动对象。

但当前 `DungeonRunState` 是小型可复制值类型（`src/dungeon/dungeon_checkpoint.hpp:288`），被以下路径大量按值持有或返回：

- `PendingTransition`、`PendingSave`、`PendingSaveResult`（`src/dungeon/dungeon_types.hpp:266-315`）；
- `DungeonSession::stable_state_`、`ReusablePendingSave::value_`、`death_validation_scratch_`（`src/dungeon/dungeon_session.hpp:35-78,281-283`）；
- `RunStateBuildResult` 和 `make_*transition()`（`src/dungeon/dungeon_progression.hpp:17-28`、`src/dungeon/dungeon_progression.cpp:13-18,169,230`）；
- `DecodeResult`、`SaveLoadResult`、`SaveCommitResult`（`src/persistence/checkpoint_codec.hpp:64-68`、`src/persistence/save_store.hpp:60-73`）；
- `DungeonRuntime::initialize()` 中的多次局部 move/copy（`src/platform/raylib/dungeon_runtime.cpp:189-284`）。

把不可复制 `RoomProgressCheckpoint` 直接嵌入 `DungeonRunState` 会使上述 API 失效；保留复制则会让 `DungeonSession`/`DungeonRuntime` 间接嵌入多份巨型数组，并产生大栈返回值，违反 heap-only 和 stack guard。

建议接口：

- 在 `SaveCommitStorage` 中定义两个不可复制 `SaveCommitJobSlot`，每槽组合“小型 durable run state + RoomProgressCheckpoint + revision/intent”；
- live `DungeonRunState` 不拥有巨型数组；若必须保留源码表达式 `state.room_progress`，它只能是由 caller-owned slot 提供的非拥有 view，且不得跨 slot 生命周期保存；
- `DungeonSession::capture_checkpoint(SaveCommitJobSlot&) const noexcept` 直接写目标槽；结果只返回小型状态码/revision；
- 删除大状态在 `PendingSave`、`PendingSaveResult`、`SaveLoadResult`、`SaveCommitResult` 中的按值传播，改为 slot index/稳定指针 + 小型 receipt；
- `DungeonRuntime` 初始化也应 decode 到 storage slot，再由主线程 restore，不得先构造局部巨型 `DungeonRunState checkpoint`。

任务文件清单至少还需覆盖 `src/dungeon/dungeon_types.hpp`、`src/persistence/save_store.hpp`、`src/persistence/save_store_detail.hpp` 和 `src/persistence/save_recovery.cpp`；否则无法完成上述迁移。

### P0-2：后台 revision 与 A/B 槽仲裁键冲突

后台普通击杀/掉落只增加 authority revision，不应伪装成现有事务 `commit_generation`。但当前槽选择只比较 `DungeonRunState::commit_generation`；同 generation 且内容不同会被判为冲突：

- `highest_slot()`：`src/persistence/save_slot.cpp:272-294`；
- 启动恢复直接返回 `conflicting_slots`：`src/persistence/save_recovery.cpp:157-164`；
- 发布后 final scan 也把同 generation 不同内容判为 `indeterminate`：`src/persistence/save_recovery.cpp:231-240`。

因此连续两个五秒后台 V9（同一 gameplay transaction generation、不同 room progress）会让下一次启动进入恢复错误。

建议接口：

- V9 头加入独立、持久化、非零、单调的 `save_revision`/`slot_sequence`，A/B 选择先比较它；payload 中继续保存原有 `commit_generation`；
- 定义 revision 溢出行为及 A/B 相等时“V9 bytes/decoded state 必须完全相同”的规则；
- worker completion 的 `revision` 必须就是该持久化仲裁键，不能只存在内存中；
- 修改 `SlotInfo`、`highest_slot()`、`load_recovered()`、`verify_published_transaction()`，并覆盖“连续后台保存、后台后事务、旧后台晚完成”的启动恢复测试。

不能用“每次击杀都递增 `commit_generation`”规避：该字段当前参与死亡目标校验、制作/强化 RNG、事务 receipt 和大量 `+1` 不变量（例如 `src/dungeon/death_checkpoint.cpp:111-128`、`src/dungeon/dungeon_transition.cpp:811-823,948-951,1009-1012`）。

### P0-3：完整 affix/effect 恢复没有可用接口，且任务文件清单遗漏所有者模块

Monster 与 obstacle 权威状态都包含 `modifiers::EffectSet`：

- `MonsterPersistentState::effects`：`src/combat/monster_persistent_state.hpp:43-45`；
- `MonsterRuntime::effects`：`src/combat/monster_pool.hpp:51-53`；
- `RoomObstacleState::effects`：`src/combat/room_obstacle_runtime.hpp:24-34`。

`EffectSet` 的 active-effect 数组、待执行 command 队列、head/count 和 diagnostics 全为 private；公开 API只能查询 count/特定 id/比较，无法枚举并无损恢复定义、剩余 tick、stack、on-expire command 或队列顺序（`src/modifiers/effect_set.hpp:63-107`）。只改 `combat_world.*` 无法满足“恢复 monster affix effects”“invalid effects 必须拒绝”和 10,000 tick 确定性。

建议新增固定容量 `modifiers::EffectSetCheckpoint` 及：

```cpp
bool EffectSet::capture_checkpoint(EffectSetCheckpoint&) const noexcept;
bool EffectSet::restore_checkpoint(const EffectSetCheckpoint&) noexcept;
bool valid_effect_checkpoint(const EffectSetCheckpoint&) noexcept;
```

字段必须包括 occupied records、稳定 effect id/规则/strength/modifier/on-expire、剩余 tick/stacks，以及有序 command queue；diagnostics 若定义为 presentation diagnostic 则明确清零，不能默默遗漏。任务文件清单需加入 `src/modifiers/effect_set.hpp/.cpp` 与对应 tests/CMake。

### P0-4：预分配 codec/无后续分配与现有 persistence API 不兼容

当前 codec 和保存路径必然动态分配并返回大对象：

- `EncodedCheckpoint = std::vector<uint8_t>`，`encode_checkpoint()` 返回 `optional<vector>`，`decode_checkpoint()` 返回含 state 的 `DecodeResult`（`src/persistence/checkpoint_codec.hpp:64-75`）；
- `encode_checkpoint()` 每次 `resize()`（`src/persistence/checkpoint_codec.cpp:352-360`）；
- `read_slot()` 用 `ifstream + istreambuf_iterator` 构造 vector，并把 decoded state 复制入 `SlotInfo`（`src/persistence/save_slot.cpp:224-254`）；
- `SaveStore::commit()` 会重新 load/scan/decode 两槽，再创建 encoded vector（`src/persistence/save_store.cpp:31-66`）。

仅增加两个 8 MiB buffer 不会让这些路径自动改为无分配，也无法通过大自动变量 source guard。

建议提供 worker 专用的 in-place API：

```cpp
EncodeResult encode_checkpoint_into(
    const SaveCommitJobSlot&, uint8_t* out, size_t capacity) noexcept;
StructuralDecodeResult decode_checkpoint_into(
    const uint8_t* data, size_t size, SaveCommitJobSlot& out) noexcept;
ExactCompareResult decode_and_compare_checkpoint(
    const uint8_t* data, size_t size,
    const SaveCommitJobSlot& expected) noexcept;
```

worker 的文件 I/O 也必须直接读写预分配 buffer；不能内部回落到现有 `SaveStore::commit/read_slot`。若“no post-initialization allocation”只限定 persistence/capture/worker，需在规格与 allocator test 中明确作用域，因为现有 gameplay inventory vector 本身仍会增长。

### P0-5：V8 “保留 ownership”与“当前房间 fresh”互相冲突

V8 的 room-local claim bits 当前存放在 `ItemOwnershipState`：

- `claimed_drop_bits[3]`、`material_claimed_drop_bits[7]`：`src/items/item_types.hpp:79-80`；
- 当前构房/掉落/药水逻辑直接读取这些位；药水还复用 secondary claim ordinal（例如 `src/dungeon/dungeon_session.cpp:1652-1663,1681-1718`）。

如果 V8 migration 原样“preserves durable inventory/ownership”再令 `room_progress={}`，旧低位 claims 仍会压制当前房间掉落，房间并不 fresh。

建议把规格钉死为：保留 `items/equipment/material counts/discovery/next_item_sequence`，但把两组 legacy room-local claim bits 明确分类为非 durable ownership，并在 V8→V9 时清零；V9 active room 再由 canonical `room_progress` 镜像低 3/7 words。增加测试证明 migration 后同 seed 当前房间按零 claim 开始，背包与装备不变。

### P0-6：V9 semantic decode 需要 `DungeonRules`，现有接口没有上下文

蓝图重建 API都要求 rules：

- `build_room_monster_plan(room, rules, version, out)`：`src/dungeon/room_monster_plan_builder.hpp:16-21`；
- `build_room_environment(room, rules, version, monsters, out)`：`src/dungeon/room_environment.hpp:18-25`。

但 `decode_checkpoint(bytes,size)`、`SaveStore::load()` 都没有 `DungeonRules`；`DungeonRuntimeConfig.rules` 可不是 default rules。codec 若偷用默认 rules，会拒绝合法的自定义规则存档或接受错误蓝图。

建议把主线程 V9 load/restore API改为显式 `CheckpointValidationContext{const DungeonRules&}`，并贯穿 runtime 初始化。结构 decoder 不得自行选择 default rules。

### P0-7：worker 回读 decode 与“RNG 只在主线程”冲突

全局约束要求 authority、RNG、checkpoint result application 在主线程；worker 不能选择 RNG。但如果“worker readback decode”也承担 P0-6 的蓝图 hash/collision semantic validation，它必须在 worker 调用基于 `DeterministicRng` 的 monster/environment generator。

建议明确分两层：

1. worker：长度/CRC/字段/枚举/容量/顺序的结构 decode，并逐字段与 immutable expected job 比较；不得生成蓝图；
2. main thread：初始化 load 和 apply completion 前，使用 runtime rules 重建 monster blueprint，再基于它重建 environment，校验 version/hash/collision，全部通过后才 restore/publish。

capture 入队前主线程也执行同一 semantic validator，保证 worker 保存的 job 已合法。这样同时满足 worker exact readback 与 RNG 线程边界。

## P1（高风险，实施时必须显式编码/测试）

### P1-1：`RoomProgressCheckpoint` lifecycle/flags 缺少合法状态表

规格要求拒绝 malformed lifecycle、required-count mismatch、unlock below threshold、full-clear/reward 组合，但没有定义 lifecycle 枚举和值组合。必须在 `room_progress_checkpoint.hpp/.cpp` 固定：

- canonical empty（V8 migration/新房尚未 materialize）哪些字段必须为零；
- active、full-clear、death-pending/其他已存在边界如何映射；
- `required_kills=(generated+3)/4`，generated=0 的唯一合法情形；
- `exits_unlocked/full_clear/reward` 与 defeated/generated/abyss lifecycle 的组合；
- room index/seed 何时必须与 `DungeonRunState.current_room` 相等。

codec 只做 field encoding；唯一 validator 应由 dungeon 层提供，避免 codec/session 两套规则漂移。

### P1-2：secondary ground record 的 canonical key 未定义

现有 material 使用 `2*spawn`/`2*spawn+1`，potion 使用同一个 `2*spawn+1`，二者靠 coupon `else if potion` 保持互斥（`src/dungeon/health_potion_loot.hpp:18-21`、`src/dungeon/dungeon_session.cpp:1683-1718`）；另有 abyss ordinal 从 384 开始（`src/dungeon/material_loot.hpp:15`）。

V9 必须固定 record tag、ordinal domain 和严格排序键。建议 `(ordinal, tag)` 编码，但同时验证同一 secondary ordinal 最多一条，且 monster-derived 与 16 个保留 ordinal 不碰撞；否则“strictly increasing ordinal”和 tagged records 会产生多种合法 bytes。

### P1-3：恢复字段集合没有覆盖现有全部权威字段

`PlayerRuntime` 还有 `state/combo_stage/air_attack_available/hurt/invulnerability`；`MonsterRuntime/PersistentState` 还有 reaction/AI ticks、attack serial、contact latch、shield recharge、owner transient、burning/blink、warning 等（`src/combat/combat_world.hpp:72-100`、`src/combat/monster_persistent_state.hpp:10-45`）。

应逐字段列出 checkpoint schema、宽度、范围和“保存/派生/显式清除”三选一。不能只按 brief 中的概括字段猜测。特别是 ordinary attack 的 `startup/recovery` 应校验为由 attack catalog + player build 推导的值，还是直接持久化，需唯一化。

### P1-4：清除 active skill 后的 player state 未规范化

当前 active skill 开始会把 player 设为 `attack_startup`（`src/combat/combat_world.cpp:592-603`），`clear_active_skill()` 只清 snapshot/latch，不重置 player state（`src/combat/active_skill_runtime.cpp:157-160`）。V9 要清 executing skill，却又保存 player state；直接组合会得到 `attack_startup + no ordinary attack + no active skill` 的非 canonical 状态。

建议在版本化 transient-clear 规则中规定恢复后的 player state：地面为 idle，空中按 z/vertical velocity 映射 jump rise/fall（保留 landing 仅在其本来合法时），并清横向 skill-only velocity 还是保留也要明确。测试 active skill 每个 phase。

### P1-5：damage history 的 tick/溢出合法性口径不完整

`PlayerDamageHistory::begin_tick()` 在 tick 回退或跨度 ≥300 时清空全部 bucket（`src/combat/player_damage_history.cpp:17-39`）；`record()` 与 `totals()` 使用饱和加法，`UINT64_MAX` 可由合法运行产生（`src/combat/player_damage_history.cpp:41-56`）。

因此“reject invalid active/history ticks、overflowed totals”必须说明：

- initialized=false 时要求 active_tick=0 且 buckets 全零；
- initialized=true 时 active_tick 与 `CombatWorld::tick_` 的允许差值（死亡 tick 分支会停止 begin_tick）；
- uint64 tick wrap 是否 canonical reset；
- 已合法饱和的 bucket/total 是否允许。不能把 runtime 可产生的饱和值当 corruption。

### P1-6：durable death/abyss 与 room-combat 副本需交叉校验

V9 同时保存 `DungeonRunState::death/abyss` 与 `RoomCombatCheckpoint::death_snapshot/AbyssEnvironmentRuntime`。必须验证 present/lifecycle、room/death tick、source/damage totals、防御快照、abyss rule 相互一致；否则 codec 可接受两个各自合法但互相矛盾的权威副本，恢复路径会因先读哪一个而不同。

### P1-7：两个 job slots 在“in-flight + pending”时没有明显的 readback decode 目标

in-flight background 占一槽、pending transaction 占另一槽时，worker 回读不能覆盖 expected slot，也不能覆盖 pending slot。实现需选定以下一种固定策略并计入 resident bytes：

- `decode_and_compare` 流式逐字段比较，不 materialize 第三 checkpoint；或
- 增加第三个专用 verification checkpoint（会影响 32 MiB cap）。

仅有第二个 8 MiB byte buffer 不能代替 decoded object，除非明确采用流式比较。

### P1-8：32 MiB “exact resident bytes”不能只用 `sizeof(SaveCommitStorage)`

两块 codec buffer 已占 16 MiB；现有 inventory 是 vector，两个预留 job 的 item backing storage 不计入 `sizeof`。线程栈、同步对象内部资源也不计入。应记录公式，至少包括：

- 两个 job 固定数组；
- 两个 8 MiB buffers；
- 两个 job 的 inventory 最大预留 backing bytes；
- 若存在 verification state 的存储。

同时用 compile-time hard cap 覆盖内联部分，用 runtime allocation accounting 覆盖外部 backing storage。0.50 ms P99 还需固定测试数据规模、build 类型、采样数和参考机器；否则不是可重复 gate。

### P1-9：shutdown/异常析构与 close-cancel 路径必须显式

当前 host 遇到 `WindowShouldClose()` 可直接设 `exit_requested`，随后跳出 loop 并 `CloseWindow()`（`src/platform/raylib/raylib_host.cpp:2921-2966,3767-3772`）。V9 必须：

- 第一次 close 只创建 exact shutdown job 并进入 closing；
- loop 继续 present/poll completion，不再 tick authority；
- committed 才退出；not_committed 清 closing 并保持游戏运行；indeterminate 走 fault；
- `DungeonRuntime::~DungeonRuntime()` 无条件先 `worker.stop_and_join()`，成员声明顺序保证 worker 先于 `storage_` 析构，初始化半失败也成立。

raylib close flag 可能持续为 true；not_committed 后需显式清 close request/latch，避免每帧自动重提 shutdown。

### P1-10：输入“物理释放后重武装”需有 host 级状态机

当前 host 每帧先映射输入，再按 UI/death/pause gate 决定 forwarding（`src/platform/raylib/raylib_host.cpp:2924-2942,3115-3181`）。仅把 `RoomPhase` 设为 committing 不会防止持有 movement 在恢复首 tick 立即生效，也不会处理在 I/O 期间按下再持有的键。

建议 host 维护 `GameplayRearmGate { blocked, saw_all_released }`：进入任何 exact commit 时清 session `InputBuffer`；commit 期间丢弃所有 gameplay edges/down；完成后直到所有 gameplay keyboard/mouse controls 同帧 physical-up 才允许 movement/actions/skill/descent/pickup。UI/recovery/close 控制不应被误屏蔽。schedule injection tests 要覆盖提交前已持有、提交中按下、完成帧仍持有三种情况。

### P1-11：恢复必须先构造临时完整对象，再一次性发布

当前初始化直接把 decoded state move 进 `DungeonSession`，构房失败会把 session 带入 fault（`src/platform/raylib/dungeon_runtime.cpp:242-251`、`src/dungeon/dungeon_session.cpp:473-568`）。V9 要满足“hash/碰撞全部通过前不 mutate live session”，应在 storage scratch/临时 heap owner 中按以下顺序：

1. 结构 decode；
2. main-thread monster blueprint rebuild + version/hash；
3. environment rebuild against that exact monster plan + version/hash；
4. room progress bitsets/records及交叉 invariant；
5. obstacle state/effects；
6. player/monster position与 intact obstacle collision；
7. combat state restore + transient canonicalization；
8. 成功后一次性把临时 owner 发布给 session。

任何失败都不得部分改写 `stable_state_`、`combat_`、ground arrays 或 active slot status。

## P2（建议补强）

### P2-1：8 MiB 上限应有静态最大编码公式

除了 decode 时 `size <= 8 MiB`，应按每个 record 的固定编码宽度计算 `kV9MaximumEncodedBytes` 并 `static_assert(kV9MaximumEncodedBytes <= 8_MiB)`。公式要包括最多 65,535 durable item records、1,152 monster/equipment records、2,320 secondary records、300×damage-type buckets 和最多 environment records。否则未来字段增加可能让合法满容量状态无法编码。

### P2-2：adapter 的 canonical mismatch 规则要测试

V9 是 canonical room representation，而低 3/7 words 仍镜像在 legacy ownership。decode 遇到两边不一致时应拒绝，不应静默选择一边；encode 前也应 assert/canonicalize 一次。高 words 在 Task 5 必须全零并验证，不要只依赖默认初始化。

### P2-3：completion API 需要单消费者、一次性 receipt

建议 `try_take_completion()` move 出小型 `{revision, intent, exact_result, slot_index}` 并清 ready flag；重复 poll 不得重复 apply。旧 background completion 若 `revision <= durable_revision` 只回收槽，不改变 indicator/dirty；transaction completion 必须 intent + expected revision 都匹配当前 committing request。

## 建议的最小实施顺序（不涉及后续任务）

1. 先定案 P0-1/P0-2/P0-5/P0-6/P0-7 的类型、revision、迁移和两层 validation contract，并补齐允许修改的文件。
2. 先实现 modifiers/core 的固定 checkpoint export/import，再定义完整 V9 schema 和最大字节公式。
3. 实现 heap storage + in-place codec + worker，先用合成 job 验证队列、回读和 shutdown；此时不接 live session。
4. 把 `DungeonSession` capture/restore 改成 caller-owned slot，完成原子恢复与 dirty revision。
5. 最后接 `DungeonRuntime`/host exact commit、close loop 和 physical-release gate，再执行 brief 的完整测试矩阵。

## 预检证据边界

- worktree `HEAD` 与指定 BASE 均为 `c6ecd4f090d47fdede97f4a601c9909eb61624cd`。
- 预检开始时 `git status --short` 无输出。
- 已通读 Task 5 brief，以及当前 persistence codec/store/slot/recovery/transaction、dungeon checkpoint/progression/session/snapshot/transition、combat world/monster/obstacle/effect/damage-history、raylib runtime/host 保存与输入路径。
- 按任务约束未编辑源码、未构建、未测试。

# Task 6 实现前审计（HEAD `30fe24b`）

## 1) Task 5 已提供、应复用的接口/状态

- V9 已持久化 `RoomProgressCheckpoint::{generated_monsters, defeated_monsters, required_kills, exits_unlocked, full_clear, reward_committed}`，并在 codec 中编码、解码和迁移；见 `src/dungeon/room_progress_checkpoint.hpp:46-66`、`src/persistence/room_progress_codec.cpp:792-797,840-845,1124-1129`。
- V9 结构校验已经规定四分之一阈值 `required_kills == (generated_monsters + 3)/4`，且 `exits_unlocked` 不得早于阈值、`full_clear` 必须等于全击杀；见 `src/dungeon/room_progress_checkpoint.cpp:620-654`。直接提取为 `required_kills(uint32_t)`，不要复制公式。
- `DungeonSession` 已有可复用的精确保存事务骨架：`PendingSave`（含 `resume_phase`）、`resolve_pending_save()` 和 `commit_pending_save()`；见 `src/dungeon/dungeon_types.hpp:273-313`、`src/dungeon/dungeon_transition.cpp:1640-1741`。提交会严格校验 generation 和 verified state，提交后统一发布状态；见 `:1866-1943`。
- 击杀账本和未结算怪物 XP 已在 `relay_combat_defeats()` 写入 `room_progress_`/`pending_room_experience_`；见 `src/dungeon/dungeon_session.cpp:1511-1561`。完整清理奖励/材料真空已封装于 `prepare_room_clear()`；见 `:2168-2302`。
- 现有 V9 capture/restore 路径已保存击杀 bits 和场景对象；见 `src/dungeon/dungeon_session.cpp:510-540,658-784`。过渡提交会清空临时地面物、战斗、`room_progress_`；见 `src/dungeon/dungeon_transition.cpp:1929-1943`。

## 2) 尚缺行为（对应需求 Step 1-5）

### Step 1：阈值及保存 RED 测试

- `RoomProgressState` 仍只有 initial/defeated/bits，未含 required、`exits_unlocked`、`full_clear`；`src/dungeon/dungeon_types.hpp:110-115`。需要在 `tests/dungeon/dungeon_exit_unlock_tests.cpp` 先覆盖 300→75、301→76、1125→282、阈值前拒绝、阈值提交前关闭、提交后 combat 继续、全清奖励仅一次。
- 无 `PendingSaveKind::room_unlock`；`src/dungeon/dungeon_types.hpp:273-293`。须新增并使提交分支能将 unlock 状态发布且只发一次 `DungeonEventKind::exits_opened`（事件枚举已存在，`:49-68`）。

### Step 2：出口与全清状态拆分

- 快照仍从 `RoomPhase::{cleared,awaiting_exit}` 推导 `exits_open`；`src/dungeon/dungeon_snapshot.cpp:82-90`。应改为已提交的 `room_progress_.exits_unlocked`（pending unlock 时仍 false）。
- 击杀循环只在 remaining=0 时触发 full clear，尚没有阈值 unlock 入口；`src/dungeon/dungeon_session.cpp:327-337,1527-1561`。
- capture 仍从 phase 推导 `room.exits_unlocked`；restore 因此把任何 unlocked V9 存档还原为 `awaiting_exit`；`src/dungeon/dungeon_session.cpp:524-535,781-783`。Task 6 后阈值存档必须恢复为 `combat`，full-clear 才恢复为现有 exit 状态。
- `prepare_room_clear()` 当前发布 `room_cleared` 后也发布 `exits_opened`；`src/dungeon/dungeon_session.cpp:2304-2310`。Task 6 后该事件只能在 unlock 提交时发布，100% 仅发布 `room_cleared`。

### Step 3：早退奖励语义

- `attempt_exit()`/`request_descent()` 仅接受 `awaiting_exit`；`src/dungeon/dungeon_transition.cpp:140-156,176-189`。须改为 `combat && committed exits_unlocked`，同时保留原有 cleared/awaiting 兼容路径。
- 普通早退当前 `prepare_transition()` 直接复制 `room_progression_`，未结算 `pending_room_experience_`，也未显式区分“无 clear bonus、无 vacuum”；`src/dungeon/dungeon_transition.cpp:355-401`。要在同一 transition 保存中结算已赚怪物 XP、保留已领取物品、放弃在地物和活怪，并清 V9 room progress。
- 早退深渊尚未实现：`finalize_abyss_exit()` 仅处理 `AbyssLifecycle::cleared`；`:326-352`。已开始但未全清的深渊会错误走普通 transition，不能原子写入 `failed`、失败/abandoned `LastAbyssResolution`、零 clear reward 和下一房间。

### Step 4：宿主显示与交互

- 门视觉已接收 `exits_open`，只需让快照提供正确提交 flag；`src/platform/raylib/dungeon_view_math.cpp:19-30`。
- 洞仍按 phase 在 combat 时 sealed、且 `can_prompt_descent()` 据此拒绝；`src/platform/raylib/dungeon_view_math.cpp:216-255`。需改为 `exits_unlocked`，保持 `kHoleInteractionRadius=3.25F` 不变（`dungeon_view_math.hpp:99-100`）。
- E 键已通过 `can_prompt_descent()` 传入 range；`src/platform/raylib/raylib_host.cpp:3570-3575`。不要改输入绑定。HUD 提示还只认 `awaiting_exit`；`hud_notice_state.cpp:265-274`，HUD 目标文本也仅按 phase 显示出口开放；`hud_view_model.cpp:89-104`。

### Step 5：注册和全套回归

- 新 dungeon suite 需要加入 `tests/dungeon/CMakeLists.txt:1-34` 和 `tests/dungeon/dungeon_test_main.cpp:10-39,185-215`；V9 save/reload 覆盖补入现有 `tests/persistence/checkpoint_v9_tests.cpp`（该 suite 已在 `tests/persistence/persistence_test_main.cpp:8-40` 注册）。

## 3) 事务、回滚、indeterminate、abyss early-exit 高风险点

- Unlock 事务必须在 crossing threshold 后冻结为 `committing`，并以 pending 的 **next** checkpoint 记录 `exits_unlocked=true`；`not_committed` 恢复 combat+未解锁，并可由同一 defeat state 重试，不能预先改 live flag/发事件。`indeterminate` 必须沿用 `save_commit_indeterminate` fault；`dungeon_transition.cpp:1686-1732`。
- 新 unlock 事务只预留/发布一个事件；full-clear 事务改为只预留/发布 `room_cleared`。当前 clear 强制预留两个事件（`:2227-2231`），并在 commit 后成对发布（`:1919-1922,2304-2310`）；否则会重复 `exits_opened` 或错误 event-overflow 边界。
- 早退 `not_committed` 的恢复 phase 必须是原房 `combat`（不是 `awaiting_exit`）；新门/洞 guard 仍由 committed unlock flag 允许再次尝试。普通 transition 当前固定 `resume_phase=awaiting_exit`；`:385-394`。
- 不要把深渊早退复用为当前 `abyss_abandon` 而不改回滚策略：所有 `abyss_abandon` 的 `not_committed` 当前被视为 protected abyss commit 并 fault；`:1690-1708`，与需求“留在原房”冲突。必须为早退场景提供可回滚事务语义，同时 committed 分支原子设置 failed/resolution 并清旧 progress。
- `prepare_room_clear()` 有保留的诊断捷径，可在仍有存活怪时同步清理；`:2208-2220`。这与“仅 `defeated==initial` 才全清”的硬约束冲突，不能让 unlock 路径复用它；需决定删除/隔离该测试 hook。
- `tick()` 目前只在 `phase==combat` 检查 full clear；`:327-337`。提前 unlock 后必须一直保持 combat，否则攻击、技能、AI 和后续死亡账本会被停掉。

## 4) 依赖“全清后 awaiting_exit”的现有测试，不能误改

- 正常门/洞全清路径：`tests/dungeon/dungeon_navigation_tests.cpp:51-126`（`clear_and_await`/`clear_without_dungeon_drain`）、`dungeon_transaction_tests.cpp:303-367`、`dungeon_save_integration_tests.cpp:286-295`。它们应继续在 **100% clear** 后进入 awaiting_exit。
- 全清的移动与事件：`tests/dungeon/dungeon_lifecycle_tests.cpp:680-752`（`real_combat_clears_once_without_respawn`）、`dungeon_wave_tests.cpp:628-642`、`dungeon_stress_tests.cpp:658-673,1082-1125`。不能把这些 full-clear 断言改成“25% 即 awaiting_exit”。
- cleared abyss 的可导航/奖励确认：`tests/dungeon/dungeon_abyss_reward_tests.cpp:779-1147`，以及 `dungeon_lifecycle_tests.cpp:138-254`。这些是已全清深渊的 `awaiting_exit` 语义，不可混入 started-abyss 的早退语义。
- 已全清后的 build/物品事务回归：`dungeon_passive_tree_tests.cpp:27-184`、`dungeon_item_transaction_tests.cpp:68-407`、`dungeon_health_potion_tests.cpp:407-1188`。仍需保持 full-clear 后的 awaiting_exit；新增早退不要扩大这些 API 的可用阶段。

## 5) 最小 TDD 顺序与覆盖命令

1. 写 `required_kills()` 和阈值前/临界/提交后 RED 测试；增加 `RoomProgressState` flag 与 `room_unlock` pending，先只让 normal combat unlock 通过。
2. 写 `not_committed` 重试、`indeterminate` fault、单次 `exits_opened`/100% 单次 `room_cleared` RED 测试；随后修正 snapshot、capture/restore 和 event reservation。
3. 写普通早退事务 RED 测试（XP 已结算、无 clear bonus/vacuum、未领取地面物和 room progress 被丢弃、失败留原房）。
4. 写 started-abyss 25% 早退 RED 测试（failed + zero-clear resolution + next room + rollback/indeterminate）；只在它们通过后修改 abyss transaction 分支。
5. 写 host math/HUD 单测：combat+unlocked 的四门和 hole E prompt 可用，洞半径仍 3.25；最后跑全套。

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon|persistence|platform)\.units$' --output-on-failure
```

## 6) 计划与当前代码冲突

1. **明确冲突：**需求要求 `RoomProgressState { initial, defeated, required, exits_unlocked, full_clear }`，当前仅三项字段；`src/dungeon/dungeon_types.hpp:110-115`。
2. **明确冲突：**需求要求已提交 V9 flag 独立于 `RoomPhase`，当前 snapshot/capture/restore 全由 phase 推导且 unlocked reload 直接 awaiting_exit；`dungeon_snapshot.cpp:82-90`、`dungeon_session.cpp:524-535,781-783`。
3. **明确冲突：**需求要求 25% 后仍 combat 并可 exit，当前 transition guard 和 hole UI 只接受 awaiting_exit/full-clear；`dungeon_transition.cpp:140-189`、`dungeon_view_math.cpp:216-255`。
4. **明确冲突：**需求要求 `exits_opened` 只在阈值 commit 一次，当前只随 full-clear 的 `room_cleared` 一起发布；`dungeon_session.cpp:2227-2231,2304-2310`。
5. **明确冲突：**需求要求 `prepare_room_clear()` 仅在全击杀，当前诊断兼容分支允许 live targets；`dungeon_session.cpp:2208-2220`。
6. **明确冲突：**需求要求 abyss early-exit 可 rollback 且 committed 标失败，当前 abyss 仅清理后才结算退出，且 `abyss_abandon` 的 not_committed 会 fault；`dungeon_transition.cpp:326-352,1690-1708`。

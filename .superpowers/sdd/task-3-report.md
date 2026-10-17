# Stage 10 Task 3 报告

## 结果

- 新增固定四向 `preview_abyss_doors`，槽位严格为 up/down/left/right（0/1/2/3）。
- `DungeonSnapshot::abyss_doors` 只公开 `std::array<bool, 4>`，不公开门后的规则、危险等级或奖励。
- 初始房与 descent 目标显式 `is_abyss=false` 且 `AbyssCheckpoint={}`。
- door 目标显式使用预告结果；命中后 pending next state 写入 `available` 和由目标 seed/depth 选择的 rule/danger/rules_version。
- `current.index == UINT64_MAX` 时预告全 false；正式 transition 仍由既有 room index overflow 路径拒绝。
- 未接入 `started` 原子门禁；该行为保留给 Task 4。

## RED

1. 先只修改测试，新增 10 个用例并把旧 Stage 3 abyss sample 断言改为“仅诊断、不授予资格”。
2. 直接运行 brief 的 build 命令时，当前 PowerShell 未加载 MSVC 标准库环境，首先得到 `cmath` 缺失；该失败不作为 RED 证据。
3. 加载 `vcvars64.bat` 后重跑，确认编译稳定失败于：
   - 缺少 `preview_abyss_doors`；
   - `DungeonSnapshot` 缺少 `abyss_doors`。
4. 首轮 GREEN 后完整 dungeon 回归出现既有 `fixed seed room trace matches baseline golden` 失败。根因是 generic generation 已不授予资格，但纯 `make_door_transition` 尚未把门预告显式传给目标房。以该既有失败为回归测试，补上 door builder 的显式 preview 传递后，原 golden trace 恢复。

## GREEN / Refactor

- `generate_room_descriptor` 继续生成原 ecology/hole/abyss 三个诊断 sample，但 descriptor 默认显式普通房。
- `preview_abyss_doors` 只使用 `derive_door_room_seed(current.seed, current.index + 1, direction)` 和 `abyss::is_abyss_roll`。
- `make_initial_run_state`、`make_descent_transition` 显式普通房；`make_door_transition` 显式传入对应 preview bit。
- `DungeonSession::prepare_transition` 清除上一房 abyss checkpoint；普通/descent 保持 none，命中 door 时建立 available checkpoint。
- snapshot 直接从当前稳定 room descriptor 计算四向 bool map。
- 没有共享顺序 RNG、缓存候选房或附加 heap 容器。

## 新增测试（10）

1. 四方向枚举槽位与各自目标 seed 判定一致。
2. 固定 golden seeds 覆盖 0、1、2、3、4 扇深渊门。
3. room index 溢出预告全 false。
4. legacy roll 命中的初始房仍强制普通且 checkpoint none。
5. ecology bias 变化不改变门预告。
6. snapshot 门预告字段严格为 `std::array<bool, 4>` 且等于纯函数结果。
7. 普通门预告与 pending target/checkpoint 一致。
8. 深渊门预告与 pending target、selection rule/danger/version 一致。
9. 固定 seed 证明深渊目标与普通洞可共存。
10. legacy roll 命中的 descent 目标仍强制普通且 checkpoint none。

旧测试继续锁定 ecology/hole/legacy abyss samples：`83/7295/8629`、hole 边界 `999/1000`、legacy abyss sample 边界 `99/100`，以及 hole=210 与 legacy abyss=47 共存。

## 随机域证明

- 四门只从当前 seed、`index + 1`、固定方向 ID 派生目标 seed，再进入 Task 1 的固定 abyss 命名域。
- 预告函数无可变状态，不调用 ecology、hole、encounter、affix 或 drop 流。
- generic generation 原有 ecology/hole/legacy abyss 三个命名域和调用次序未改变；只是旧 abyss sample 不再直接赋值给正式资格。
- door 资格由 preview bit 显式写入，initial/descent 不读取 abyss sample，因此 bias、深度和历史状态不影响 1% 门资格。

## 文件

- 修改：`src/dungeon/room_generation.hpp/.cpp`。
- 修改：`src/dungeon/dungeon_types.hpp`、`dungeon_snapshot.cpp`、`dungeon_transition.cpp`。
- 修改：`src/dungeon/dungeon_progression.cpp`，用于把 preview 显式传入纯 door builder，并对 initial/descent 显式清零；这是修复既有 room trace 回归所必需的最小边界修改。
- 修改：`tests/dungeon/room_generation_tests.cpp`、`dungeon_navigation_tests.cpp`、`dungeon_test_main.cpp`。

## 验证

- `arpg_dungeon_tests.exe`：139 cases，0 failures。
- `ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure`：2/2 通过；dungeon 150.00s，persistence 0.23s。
- `ctest --preset windows-msvc-debug -R "architecture.dungeon_no_raylib|architecture.dungeon_no_persistence" --output-on-failure`：2/2 通过。
- `git diff --check`：通过（仅 Git 的 LF→CRLF 工作区提示，无 whitespace error）。

## 自审与疑虑

- 自审确认 preview/commit 使用同一方向槽与同一目标 seed；overflow 不做 `index + 1`；checkpoint 在新房发布前完整写入。
- 自审确认 snapshot 的门预告没有 rule/danger/reward 字段，旧 ecology/hole golden 和 1000 房 room trace 均未漂移。
- 自审确认没有实现 Task 4 的 available→started 原子门禁、战斗副作用或失败生命周期。
- `DungeonSession::prepare_transition` 与纯 door builder 都计算同一 preview：前者负责 checkpoint，后者负责所有纯 builder 消费者的 room flag。这是无状态重复计算，不消费顺序 RNG；后续若抽取统一 transition helper，必须保持当前文件依赖边界。
- 无阻塞疑虑。

## 提交后审查修复

- 初始实现提交：`c3549d2840edf8bfee2d8856681a07da6b9c7e55`。
- 审查修复提交：`64b7b6369356f6e945bbd206f7f292bed34c90dd`。
- 只读审查发现 1 个 Important：`same_run_state` 尚未比较 Task 2 新增的 `abyss` 和 `last_abyss_resolution`，因此仅篡改深渊 checkpoint 的 verified receipt 可能被错误接受。
- RED：在既有“abyss door target persists selected checkpoint”用例中分别改动 `abyss.rules_version` 和 `last_abyss_resolution.valid`，确认失败于 `!same_run_state(...)`。
- GREEN：`same_run_state` 补齐 `AbyssCheckpoint` 9 个字段和 `LastAbyssResolution` 7 个字段；事务测试再提交仅 last-resolution 错配的 verified state，确认进入 `faulted/save_receipt_mismatch`。
- 修复后重新运行指定回归：dungeon/persistence 2/2 通过（150.21s/0.23s）；两个 dungeon architecture 门禁 2/2 通过；`git diff --check` 通过。
- 审查 Critical=0、Minor=0；唯一 Important 已修复，无剩余疑虑。

## 独立审查第二轮修复

- 修复提交：`8e5c06765a3cc6a8252f9fd031fa3e0aa781df8c`。

### RED

- Important 1：新增两个纯 `make_door_transition` 测试。上一房携带非默认 cleared checkpoint 时，普通门测试失败于 lifecycle 未清零；命中门测试失败于 lifecycle 未成为 available，证明公开 builder 未产出完整 next state。
- Important 2：新增合法 door/entry 但目标 seed 不命中新 `is_abyss_roll` 的迁移测试，失败于迁移结果仍被授予 available；合法迁移 fixture 改用固定命中 seed `0x11E9` 并显式断言新域命中。
- Minor：新增 16 项表驱动 `same_run_state` mutation，逐一覆盖 `AbyssCheckpoint` 9 字段与 `LastAbyssResolution` 7 字段；该用例按预期直接通过，因为第一轮审查修复已补齐比较。

### GREEN / Refactor

- `make_door_transition` 成为 door next state 的唯一构建源：先 `next.abyss={}`，再使用 preview bit；普通门保持 none，深渊门用目标 room seed + depth 写 available/danger/rule/rules_version。
- `DungeonSession::prepare_transition` 删除重复 preview 与 selection，只消费 builder 返回的完整 state。
- legacy migration 的资格收紧为旧 flag、合法 door、正确对侧入口以及 `abyss::is_abyss_roll(current_room.seed)` 四项同时成立；新域未命中时仅清除 is_abyss/checkpoint/resolution，其他房间、进度、被动和物品字段保持。
- 公开事务回归分别提交代表性的 `abyss.rules_version` 错配与 `last_abyss_resolution.valid` 错配，两者都进入 faulted/save_receipt_mismatch。
- 表驱动测试整理为具名 mutation 表，不通过 test-only API，也不修改产品接口。

### 验证证据

- RED persistence：`68 cases, 1 failures`，失败用例为 `legacy door abyss requires new domain roll`。
- RED dungeon：`ordinary door builder clears previous abyss checkpoint` 与 `abyss door builder produces complete selection` 两项按预期失败。
- GREEN 局部 persistence：`68 cases, 0 failures`。
- GREEN 指定回归：`ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure`，2/2 通过；dungeon 149.97s，persistence 0.23s。
- GREEN architecture：`architecture.dungeon_no_raylib` 与 `architecture.dungeon_no_persistence`，2/2 通过，11.16s。
- `git diff --check` 通过；只有 Git 的 LF→CRLF 工作区提示，无 whitespace error。
- 提交前最终新鲜验证：重新构建 dungeon/persistence 后，指定回归再次 2/2 通过（dungeon 151.61s、persistence 0.24s）；architecture 再次 2/2 通过（11.70s）。

### 自审

- builder 与 session 不再各自计算 selection；所有调用公开 builder 的消费者得到相同完整 checkpoint。
- migration 不再把旧 Stage 3 legacy sample/flag 当作资格，仅认可目标 seed 的 Task 1 固定 1% 域。
- 16 个 stable abyss 字段均有独立 mutation；代表性的 checkpoint 与 resolution receipt mismatch 均经过公开 transaction 路径验证。
- 未实现 Task 4 started 门禁或任何战斗副作用。无剩余疑虑。

# Stage 10 Task 9 实现报告

## 范围

仅实现确定性深渊宝箱奖励的派生、原子物化和 cleared 重载重建。未实现 claim、abandon、离房确认或 UI。`abyss_chest` 在 Task 10 前由显式拾取和附近自动拾取路径直接拒绝，物品保留在地面。

## 确定性命名域

奖励内容只依赖 `room_seed`、`abyss::kAbyssRulesVersion`、`reward_ordinal`、danger 和 depth 导出的最终 item level：

1. `ABYSSRW1`：深渊奖励总命名域。
2. `ABYSSRVR1 ^ rules_version`：规则版本独立子域。
3. `ABYSORD1 ^ reward_ordinal`：奖励位独立子域。
4. `ABYSSLT1`（常量 `0x41425953534C5431`）：部位 roll。
5. `ABYSRRT1`（常量 `0x4142595352525431`）：稀有度 roll。
6. `ABYSSED1`（常量 `0x4142595353454431`）：item seed。
7. `ABYSITD1`（常量 `0x4142595349544431`）：item id。

每个输出从 ordinal root 的独立 stream 派生。时间、session tick、build、重试次数、commit generation 和 `next_item_sequence` 均不参与。item id 为 0 时固定映射为 1；任何 ownership 或 ground 冲突直接进入 `abyss_reward_collision`，不重投。

最终等级先把 `uint64_t depth` 安全 cap 到 100，再应用 Task 1 profile，结果等价于 `min(depth + bonus, 100)`。稀有度先取 `items::rarity_weights(final_ilvl)`，再做 Task 1 danger shift，最后独立 weighted roll。最终 item 通过 `generate_item(..., forced_rarity)` 生成。

## 物化事务表

| 阶段 | stable mask/revision | pending cache | ground 可见性 | 结果 |
|---|---|---|---|---|
| 扫描 | 不变 | 无 | 不变 | 最多扫描 3 个 reward ordinal，选最小未 generated 且未 abandoned |
| 资格准备 | 不变 | 本地 ground index + reward ordinal + 完整 item | 不变 | 先找 free ground，再 derive/generate/检查 ID、revision、generation |
| `abyss_reward_materialized` pending | 不变 | 有 | 不变 | next state 仅推进 commit generation、generated bit、reward revision +1 |
| exact committed receipt | 采用 next | 再校验后清空 | 写入预留 ground slot | 发布物品并恢复原 phase |
| not committed | 不变 | 保留在 faulted session 内但不可见 | 不变 | `save_receipt_mismatch` |
| indeterminate | 不变 | 不可见 | 不变 | `save_commit_indeterminate` |
| generation/state/cache mismatch | 不变 | 不可见 | 不变 | `save_receipt_mismatch` |
| checkpoint copy OOM | 不变 | 清空 | 不变 | 不 fault，后续 tick 可按相同内容重试 |

`committing` 时 `tick()` 冻结，因此 ground pool 不变；exact receipt 前还会验证预留槽仍空、cached item 与纯派生结果逐字段相同，并重新扫描 ownership/ground ID。

## cleared 重载算法

1. 校验当前房是合法 cleared abyss，selection/danger/rule/version、reward_total 和三位 mask 合法。
2. 不创建 `CombatWorld`、encounter 或怪物；phase 设为 `cleared`，保留 room descriptor、hole 和开门语义。
3. 对最多 3 个 ordinal，处理 `generated && !claimed`：
   - 若同 ordinal 已在 ground，逐字段验证；
   - 否则重新执行同一纯派生和 forced-rarity item generation；
   - ownership、另一 ground ID 或重复 ordinal 冲突立即 fault；
   - 固定池无空位时停止，stable mask/revision 不变，后续 tick 再重建；
   - 重建不创建保存请求，不推进 generated mask 或 revision。
4. 重建后，正常 tick 对未 generated 项走物化事务。live clear 为保持既有 phase 契约，首 tick 先稳定进入 `awaiting_exit`，后续 tick 开始物化；无 combat 的 reload 可在首 tick 继续物化。

## GroundItem 兼容性

`GroundItem` 和轻量 snapshot 增加 `source` 与 `abyss_reward_ordinal`。普通 monster drop 显式写入 `monster_drop/0xFF`，其 drop seed、概率、affix score、item sequence 和 claimed bits 逻辑未改。深渊奖励的 `drop_ordinal` 始终是实际 fixed ground array 索引，奖励位单独存为 0..2。

## TDD 证据

### RED 1

先加入 cleared abyss 首 tick 应产生 `abyss_reward_materialized` pending 的测试；当前实现实际没有 pending，断言按预期失败。

### GREEN 1

加入纯派生模块、cleared 无战斗构造、pending cache、保存后发布和拾取拒绝；最小集成测试通过。

### RED 2 / GREEN 2

加入三档/cap/shift、独立 ordinal/重复部位、部分/满池、释放续跑、三类失败 receipt、exact receipt、reload 逐字段、collision/revision 和 pickup 保留测试；14 项专用 suite 通过。

额外 allocation RED 证明 ownership 非空时 checkpoint copy OOM 会错误进入 fault；修复为隐藏且可重试后通过。

全回归首轮暴露 Task 8 phase 兼容问题：live clear 首 tick 被提前改为 committing，导致 lifecycle/千房 stress 失败。修复为首 tick保留 awaiting_exit，并让 stress save driver 识别 materialized kind 后，全回归通过。

## 验证

- Task 9 专用 suite：14 cases，0 failures。
- `ctest --preset windows-msvc-debug -R "dungeon.units|items.units|persistence.units" --output-on-failure`：3/3，0 failures；包含 184 个 dungeon cases 和 Stage 9 千房/普通掉落回归。
- `ctest --preset windows-msvc-debug -R "^architecture\." --output-on-failure`：18/18，0 failures。
- `git diff --check`：无 whitespace error（仅 Git 的 LF/CRLF 提示）。

## 疑虑与 Task 10 接口

- Task 9 故意拒绝 `abyss_chest` pickup；Task 10 必须用 `abyss_reward_claim` 原子更新 ownership + claimed mask，并在 exact receipt 后移除 ground item。
- Task 10 必须在离房前处理未 generated/未 claimed/未 abandoned 位；本任务不永久决定 abandon 或离房策略。
- 保存 API 的 `DungeonRunState` 含 vector，资格准备在 ownership 非空时可能分配；已保证 OOM 不发布、不推进并可重试，exact receipt 发布路径经 allocation probe 证明不分配。

## 复审 Important 修复（追加）

### Important 1：cache position 完整语义

- 新增公开纯函数 `abyss_reward_position(reward_ordinal)`，三个中心偏移只在 `abyss_reward.cpp` 的单一常量表中定义；ordinal 3 及以上返回空。
- 新增纯 `derive_abyss_ground_item(...)`。prepare、cleared reload 和 exact receipt cache consistency 全部调用它，统一产生 `active`、实际 ground index、`abyss_chest` source、reward ordinal、position 和最终 item。
- 新增 `same_ground_item`，exact receipt 前逐字段比较 active/index/source/reward ordinal、position 三个 float，以及完整 `ItemInstance`。任何 cache 漂移都在采用 pending next state 之前 fail closed。
- RED：测试访问器同时篡改 cached position 的 x/y/z；旧实现仍提交，测试失败。GREEN：统一 helper 后返回 `save_receipt_mismatch`，stable generated mask/revision 保持 0，ground 不发布。

### Important 2：统一 ID 冲突门禁

- 新增统一 `item_id_in_use(ownership, ground_items, item_id, ignored_ground_index)`：扫描全部 ownership 和全部 active ground，可在 pickup/reload 验证时忽略当前槽。
- normal monster drop、recipe、abyss prepare、abyss reload、abyss receipt cache 和 pickup 发布边界均使用该 helper。普通掉落 ID 派生、概率、seed 和 golden 行为未变。
- recipe 在删除材料、递增 sequence 或创建 pending 之前检查全部 active ground，包括 `monster_drop` 和 `abyss_chest`；冲突进入既有 `item_id_collision`，不重投。
- RED：先纯调用 `generate_recipe_item` 得到预期 product ID，再安装同 ID 的未拾取 `abyss_chest` ground。旧实现接受 recipe；GREEN 后请求 fault，三个材料、sequence、ground 和 stable ownership 原样保留。

### 复审验证

- Task 9 专用：16 cases，0 failures。
- Fresh `dungeon.units|items.units|persistence.units`：3/3，0 failures；`dungeon.units` 186 cases，含千房 stress 和普通掉落 golden。
- Fresh architecture：18/18，0 failures。

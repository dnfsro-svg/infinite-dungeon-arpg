# Task 7 实现报告

## 结果

- 实现每怪独立确定性 1% 装备掉落，ordinal 固定为 `wave * 96 + target_index`。
- 使用 192 项固定地面池和 192 位 room-local rolled bits；claimed 位持久化，rolled 位不持久化。
- chance、slot、item-content、item-id 使用独立命名 RNG 域；物品等级截断到 100，ID 为零固定映射到 1。
- snapshot 按 ordinal 升序紧凑导出 active ground 项，其余项保持零值。
- 实现显式与自动 1.5 X/Y 半径拾取；每 tick 最多创建一个 `loot_pickup` pending save，且先于同 tick 出口请求。
- pickup 保存覆盖 committed、not_committed、indeterminate、回执 ordinal/地面内容错配、OOM 原子回滚和无分配 committed 发布。
- transition pending 清空 claimed bits；只有 committed 才立即清地面池与 rolled bits，not_committed 保留旧房状态。
- stress 驱动机械支持 loot pending，并把分配严格拆成 save boundary、room load、unexpected hot path 三类；普通 hot path 分配保持为 0。

## TDD 证据

1. 首组 RED：`snapshot.ground_item_count == expected_count` 失败，证明掉落池尚未实现；最小实现后 1/1 GREEN。
2. 自动拾取 RED：tick 后未进入 `committing`；实现统一 tick 尾部拾取后 2/2 GREEN。
3. transition RED：pending next state 的 `claimed_drop_bits` 未清零；修复后 13/13 GREEN。
4. 回执加固 RED：同 ID 但完整 item bytes 被替换时未 fault；完整 ItemInstance 比较后 14/14 GREEN。
5. depth RED：加载 depth=0 状态未 fault；构造入口校验后 16/16 GREEN。
6. 完整集成初次 RED：5 个 stress 用例因旧驱动不服务 loot pending 而冻结；泛化 pending 服务后只剩两个过时的零分配断言。
7. 分配分类 RED：定位约每房一次的非空背包 room-load ownership scratch；按裁决单列 room-load boundary 后 stress+loot 26/26 GREEN。

## 验证

- focused loot：16/16 cases，0 failures。
- focused stress + loot：26/26 cases，0 failures。
  - launcher 1000 房：total 6871 = save 5896 + room-load 975 + unexpected 0；overflow 全 0。
  - generic 1000 房：total 6810 = save 5842 + room-load 968 + unexpected 0；overflow 全 0。
- full dungeon：116/116 cases；`ctest -R dungeon.units` 1/1 passed，388.59s。
- deterministic snapshot comparator 定点：1/1 passed（含 ground packed view、pending kind、inventory/equipment）。
- 完整 Release 全目标编译通过，包含 `arpg_game.exe`、platform、persistence 与 raylib consumers。
- `items.units` 1/1 passed；`combat.units` 1/1 passed。
- `git diff --check` 通过；仅现有 Windows LF/CRLF 提示，无空白错误。

## 范围

- 生产代码仅修改 brief 授权的 dungeon 文件。
- 测试新增 loot suite、注册与 friend test support；经主控批准，最小联动 `dungeon_stress_tests.cpp` 以服务新 pending kind 并保持压力/确定性覆盖。
- 未修改 persistence、platform 或 raylib 源码。

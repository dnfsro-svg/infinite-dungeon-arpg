## Task 10: 原子领取、背包满和离房二次确认放弃

**Files:**

- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `tests/dungeon/dungeon_item_transaction_tests.cpp`
- Modify: `tests/dungeon/dungeon_navigation_tests.cpp`
- Modify: `tests/dungeon/dungeon_abyss_reward_tests.cpp`
- Modify: `tests/persistence/save_store_fault_tests.cpp`

**Produces:** 领取掩码、背包满留地、同门双触确认、放弃摘要、防复制故障矩阵。

**Consumes:** 普通 pickup 事务、`requested_exit` 和 `LastAbyssResolution`。

- [ ] 增加 transient 确认状态，不写存档：

```cpp
struct AbyssExitConfirmation final {
    bool armed{};
    TransitionKind transition{TransitionKind::none};
    ExitDirection direction{ExitDirection::none};
    std::uint32_t reward_revision{};
};
```

快照增加 `abyss_pending_rewards`、`abyss_unpicked_rewards`、`abyss_exit_confirmation_armed` 和确认方向。

- [ ] 增加测试：背包满时物品保持 ground；任意顺序领取都正确更新 claimed mask；领取提交失败不移除地面物品；第一触同门取消并提示计数；离开门范围、换方向、奖励状态变化使确认失效；revision 溢出进入 fault；第二次同门创建 `abyss_abandon`；同房洞口第一次 E 警告、离开洞范围失效、第二次 E 原子放弃并下层；成功后仅未生成槽位进入 abandoned mask，已生成未领取物品随普通离房销毁，摘要的 generated/claimed/abandoned 计数正确；放弃保存失败 fault 且仍在原房；下一次结算覆盖旧摘要。

- [ ] `request_pickup` 对 monster_drop 保持原路径；abyss_chest 的 `next_state` 同时加入物品、设置 claimed bit 并增加 reward revision，但不推进普通掉落使用的 `next_item_sequence`。receipt 成功才移除 ground item。

- [ ] `attempt_exit` 在 `cleared` 深渊存在 pending/unpicked 时：第一次只 arm 并 emit 警告事件；第二次且方向和 revision 相同才把所有未生成位写入 abandoned mask，并在同一个离房存档事务中提交。成功 receipt 后按普通门转换；已生成未领取物品按普通离房规则丢失但不写入 abandoned mask；不存在剩余奖励时直接转换。

- [ ] `request_descent` 复用同一确认状态，以 `TransitionKind::descent + ExitDirection::none` 作为确认键；第一次 E 只提示，玩家离开洞口范围后失效，第二次 E 才把 abandoned 和 descent 放进同一原子事务。奖励已全部领取时仍保持原来的一次 E 下层。

- [ ] `LastAbyssResolution` 只在离房前生成稳定摘要，保存 room seed、rule、total 和四个计数；下一次深渊 resolution 覆盖。

- [ ] 离房 next state 必须把当前房深渊 checkpoint 重置为 `none`，但保留刚写入的 `LastAbyssResolution`；普通门和 descent 都走同一清理逻辑，不能把 cleared/reward masks 泄漏到新房。

- [ ] 运行事务与故障测试：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

预期：所有 fault point 下无重复物品、无凭空丢失 eligibility，放弃必须有明确已提交状态。

- [ ] 提交：

```powershell
git add src/dungeon tests/dungeon tests/persistence
git commit -m "feat: protect abyss reward claims and abandonment"
```

---


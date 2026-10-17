## Task 8: 深渊清场原子结算与资源还原

**Files:**

- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `tests/dungeon/dungeon_lifecycle_tests.cpp`
- Modify: `tests/dungeon/dungeon_transaction_tests.cpp`

**Produces:** 最后一只怪死亡后先保存 cleared，再开放门/洞、移除规则和开始宝箱结算。

**Consumes:** `abyss_clear` pending save、Task 6 的资源比例映射。

- [ ] 增加测试：最后一怪死亡后 phase 立即变 committing；提交前门和洞封闭、规则仍在；commit 成功后 lifecycle cleared、规则关闭、生命比例映回普通最大生命、当前护盾只 clamp 不补满、门和洞开放；三类失败均 fault；普通房清场路径不改变。

- [ ] 把现有清场直接 `phase_=cleared` 分成 `prepare_room_clear()` 与 `publish_room_clear()`。普通房仍同步发布；started 深渊房构造 `abyss_clear` 保存请求，`next_state.abyss.reward_total` 由危险档固定为 1/2/3。

- [ ] 清场保存成功后才调用 `combat_->clear_abyss_rule_preserving_resources()`，随后发 `room_cleared` 和 `exits_opened`。任何 receipt 不一致不得发这两个事件。

- [ ] 运行：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

预期：清场故障矩阵通过；洞与深渊同房时洞在成功清场后为 ready。

- [ ] 提交：

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: atomically settle cleared abyss rooms"
```

---


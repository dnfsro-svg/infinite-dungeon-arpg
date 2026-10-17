## Task 2: 升级 V5 检查点并迁移旧存档

**Files:**

- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Create: `src/dungeon/abyss_checkpoint_migration.hpp`
- Create: `src/dungeon/abyss_checkpoint_migration.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/save_store.hpp`
- Modify: `src/persistence/save_store_detail.hpp`
- Modify: `src/persistence/save_slot.cpp`
- Modify: `src/persistence/save_recovery.cpp`
- Modify: `tests/persistence/checkpoint_codec_tests.cpp`
- Modify: `tests/persistence/save_store_tests.cpp`
- Modify: `tests/persistence/save_store_fault_tests.cpp`
- Modify: `tests/persistence/persistence_test_main.cpp`

**Produces:** V5 的当前深渊状态、三位奖励掩码和最近一次稳定结算摘要；V1～V4 确定性迁移。

**Consumes:** Task 1 稳定枚举；存档层不得包含战斗运行时类型。

- [ ] 先增加 15 个测试：V5 全字段往返、每个非法枚举/布尔/掩码拒绝、CRC 拒绝、V1～V4 迁移、非法初始/下层深渊清除、`started` 载入可被上层识别、摘要往返、A/B 发布故障不伪造提交。

在 `DungeonRunState` 增加：

```cpp
struct AbyssCheckpoint final {
    abyss::AbyssLifecycle lifecycle{abyss::AbyssLifecycle::none};
    abyss::AbyssDanger danger{abyss::AbyssDanger::low};
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint32_t rules_version{};
    std::uint8_t reward_total{};
    std::uint8_t generated_mask{};
    std::uint8_t claimed_mask{};
    std::uint8_t abandoned_mask{};
    std::uint32_t reward_revision{};
};

struct LastAbyssResolution final {
    bool valid{};
    std::uint64_t room_seed{};
    abyss::AbyssRuleId rule{abyss::AbyssRuleId::none};
    std::uint8_t total{};
    std::uint8_t generated{};
    std::uint8_t claimed{};
    std::uint8_t abandoned{};
};
```

`generated_mask | claimed_mask | abandoned_mask` 只能使用低三位和 `reward_total` 对应的有效位；`claimed_mask` 必须是 `generated_mask` 的子集；`abandoned_mask` 只能标记从未生成的奖励，因此必须与 `generated_mask` 不相交。终态摘要满足 `popcount(generated_mask) + popcount(abandoned_mask) == reward_total`，其中已生成未领取数由 `generated - claimed` 表示。

状态校验还必须保证：`available/started/cleared` 对应 `current_room.is_abyss=true`；`none/failed` 不允许正在运行的深渊规则；非 none 状态的 `rules_version` 必须等于 `kAbyssRulesVersion`，且 rule/danger 必须与 room seed + depth 的确定性重建结果一致。

`DecodeResult` 和 `SaveLoadResult` 各增加 `bool migrated`，并由 slot scan/recovery 原样传播。legacy codec 只保留旧字段并标记 migrated，不调用深渊生成逻辑；`dungeon::migrate_legacy_abyss_checkpoint` 再把旧状态转换为合法 V5 值。这样 persistence 仍只依赖稳定 checkpoint 值类型，runtime 也能区分“本来就是 V5 普通房”和“需要发布迁移结果的旧房”。

- [ ] 运行新增测试并确认先红：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests
& .\out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe
```

预期：V5 版本号、负载长度和深渊字段断言失败。

- [ ] 将 `kCheckpointFormatVersion` 提升到 5，保留 V4 magic/长度常量并添加 V5 magic/长度；编码严格按固定字段顺序写入，解码先验证长度、版本、CRC，再验证状态不变量。

- [ ] V1～V4 codec 解码后只设置 `migrated=true`。随后由 `dungeon::migrate_legacy_abyss_checkpoint` 处理：非深渊房为 `none`；合法四向门目标深渊根据 room seed + depth 派生并写入 rule/danger/version，状态为 `available`；旧 `current_room.is_abyss` 且 `entry == initial` 或 `last_transition == descent` 时清除 `is_abyss` 并置 `none`，保留 seed、索引、生态、洞口、偏向、进度和物品。runtime 必须先把这一合法 V5 迁移结果原子提交成功，之后才创建 session。

- [ ] 运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests
ctest --preset windows-msvc-debug -R "persistence.units" --output-on-failure
```

预期：persistence 局部测试全部通过，V1～V4 golden bytes 仍可读取。

- [ ] 提交：

```powershell
git add src/dungeon/dungeon_checkpoint.hpp src/persistence tests/persistence
git commit -m "feat: persist abyss lifecycle in checkpoint v5"
```

---


### Task 4: 把星盘位图纳入检查点格式 3 并保留旧存档迁移

**Files:**
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/CMakeLists.txt`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `tests/persistence/CMakeLists.txt`
- Create: `tests/persistence/passive_tree_checkpoint_tests.cpp`
- Modify: `tests/persistence/persistence_test_main.cpp`
- Modify: `tests/persistence/checkpoint_codec_tests.cpp`

**Interfaces:**
- Consumes: Task 2 的 `PassiveTreeState` 与 `valid_passive_tree_state()`。
- Produces: `DungeonRunState::passive_tree`，格式 3 `kEncodedCheckpointSize == 120U`，并能从格式 1/2 返回一个仅起点的合法状态。

- [ ] **Step 1: 写格式 3 的 RED 测试**

增加测试，指定明确的 wire contract：

```cpp
arpg::test::Failure passive_bits_round_trip_at_little_endian_offset_106() noexcept {
    auto state = make_fixture();
    state.progression = {10U, 0U, 9U, 6U};
    state.passive_tree.allocated_bits = (1ULL << 0U) | (1ULL << 8U)
        | (1ULL << 9U) | (1ULL << 10U);
    std::array<std::uint8_t, persistence::kEncodedCheckpointSize> bytes{};
    ARPG_REQUIRE(persistence::encode_checkpoint(state, bytes));
    ARPG_REQUIRE(bytes.size() == 120U);
    ARPG_REQUIRE(bytes[106U] == 0x01U && bytes[107U] == 0x07U);
    const auto decoded = persistence::decode_checkpoint(bytes.data(), bytes.size());
    ARPG_REQUIRE(decoded.error == persistence::CodecError::none);
    ARPG_REQUIRE(decoded.state.passive_tree.allocated_bits == state.passive_tree.allocated_bits);
    return {};
}
```

同时增加格式 2 fixture 迁移为 `allocated_bits == 1ULL`、bit 0 清除/断链/超点被拒绝、CRC 覆盖新增 8 字节和保存槽故障恢复保持位图的测试。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests
out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe
```

Expected: 编译失败，因为当前 checkpoint 大小为 112、没有 `passive_tree` 字段且没有格式 3 解码分支。

- [ ] **Step 3: 实现格式 3 编码、解码和迁移**

在 `DungeonRunState` 加入：

```cpp
passives::PassiveTreeState passive_tree{};
```

格式常量改为：当前版本 3、当前魔数 `IARPGS04`、payload 88、encoded 120；保留格式 1/2 的常量与旧魔数 `IARPGS03`。新版本在 offset 106 写入 `allocated_bits` little-endian；CRC 最大覆盖大小改为 108。解码根据魔数、format、payload size 三元组选择分支：格式 1 沿用现有 progress 迁移，格式 2 保留已有 progress，二者均设置 `PassiveTreeState{1ULL}`；格式 3 读取 106–113 并调用同一状态验证。

`valid_state()` 必须同时调用 progression 与 passives 的验证。更新 `arpg_persistence` 与 `arpg_dungeon` CMake 链接到 `arpg_passives`，但保持 persistence 不可达 combat/dungeon 的现有边界。

- [ ] **Step 4: 验证存档迁移和双槽恢复**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "persistence.units|architecture.persistence" --output-on-failure
git diff --check
```

Expected: 所有旧 checkpoint fixture 仍可迁移，格式 3 round-trip 与新故障恢复测试通过，`architecture.persistence_no_raylib`、`no_dungeon`、`no_combat` 全部通过。

- [ ] **Step 5: 提交 Task 4**

```powershell
git add src/dungeon/dungeon_checkpoint.hpp src/persistence src/dungeon/CMakeLists.txt tests/persistence
git commit -m "feat: persist passive tree allocations"
```


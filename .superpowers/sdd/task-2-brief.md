### Task 2: 新建无图形的 64 节点星盘目录、连通规则与投影

**Files:**
- Create: `src/passives/CMakeLists.txt`
- Create: `src/passives/passive_tree_types.hpp`
- Create: `src/passives/passive_tree_catalog.hpp`
- Create: `src/passives/passive_tree_catalog.cpp`
- Create: `src/passives/passive_tree_rules.hpp`
- Create: `src/passives/passive_tree_rules.cpp`
- Modify: `CMakeLists.txt`
- Create: `tests/passives/CMakeLists.txt`
- Create: `tests/passives/passive_test_main.cpp`
- Create: `tests/passives/passive_tree_catalog_tests.cpp`
- Create: `tests/passives/passive_tree_rules_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Consumes: Task 1 的 `Modifier`、`PlayerModifierValues` 与 `ProgressionState`。
- Produces: `PassiveTreeState`、`PassiveNode`、`PassiveNodeType`、`PassiveTreeError`、`allocate_node()`、`refund_node()`、`valid_passive_tree_state()`、`evaluate_passive_tree()`；dungeon 和 persistence 以后不得重新实现连通搜索。

- [ ] **Step 1: 写目录、BFS 和点数守恒的 RED 测试**

在 `passive_tree_catalog_tests.cpp` 中断言固定目录，在 `passive_tree_rules_tests.cpp` 中断言分配与退款：

```cpp
arpg::test::Failure catalog_has_exact_stable_shape() noexcept {
    const auto nodes = arpg::passives::passive_nodes();
    ARPG_REQUIRE(nodes.size() == 64U);
    ARPG_REQUIRE(nodes[0].id == 0U && nodes[0].type == PassiveNodeType::start);
    ARPG_REQUIRE(nodes[8].type == PassiveNodeType::connector);
    ARPG_REQUIRE(nodes[21].type == PassiveNodeType::keystone);
    ARPG_REQUIRE(nodes[63].type == PassiveNodeType::keystone);
    ARPG_REQUIRE(arpg::passives::catalog_is_valid());
    return {};
}

arpg::test::Failure allocation_requires_adjacency_and_refund_keeps_connectivity() noexcept {
    ProgressionState progress{10U, 0U, 9U, 9U};
    PassiveTreeState state{};
    ARPG_REQUIRE(!allocate_node(state, progress, 21U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 8U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 9U).changed);
    ARPG_REQUIRE(allocate_node(state, progress, 10U).changed);
    ARPG_REQUIRE(!refund_node(state, progress, 8U).changed);
    ARPG_REQUIRE(refund_node(state, progress, 10U).changed);
    ARPG_REQUIRE(progress.unspent_passive_points == 7U);
    return {};
}
```

为两个 suite 的总 case 数设置为 9 组：目录组除形状外逐项黄金断言 64 个节点的类型、名称、所有邻接和所有 Modifier；规则组 5 个，包含重复节点、无点、未知 ID、断链、相同位图的确定性投影。`catalog_is_valid()` 还必须拒绝自环、重复邻居、非对称边和超出邻接容量的目录。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug --target arpg_passive_tests'
```

Expected: CMake 失败，因为 `src/passives/CMakeLists.txt` 和 target `arpg_passives` 尚不存在。

- [ ] **Step 3: 实现固定目录和规则 API**

建立以下公开类型。邻接数组固定为 12 条边，`neighbor_count` 指定有效前缀；容量 12 足以编码起点的 11 条直接边，同时仍是固定容量栈数组。Modifier 数组固定容量为 4，以容纳 stormstep 的四个独立效果，仍不得使用堆内存：

```cpp
inline constexpr std::size_t kPassiveNodeCount = 64U;
using PassiveNodeId = std::uint8_t;
struct PassiveTreeState final { std::uint64_t allocated_bits{1U}; };
enum class PassiveNodeType : std::uint8_t { start, small, notable, keystone, connector };
enum class PassiveTreeError : std::uint8_t {
    none, unknown_node, already_allocated, not_allocated, no_points,
    not_adjacent, disconnects_tree, invalid_state
};
struct PassiveTreeResult final {
    PassiveTreeState state{};
    progression::ProgressionState progression{};
    PassiveTreeError error{PassiveTreeError::none};
    bool changed{};
};
struct PassiveNode final {
    PassiveNodeId id{};
    PassiveNodeType type{};
    const char* name{};
    std::int16_t x{};
    std::int16_t y{};
    std::array<PassiveNodeId, 12> neighbors{};
    std::uint8_t neighbor_count{};
    std::array<modifiers::Modifier, 4> modifiers{};
    std::uint8_t modifier_count{};
};
```

在 `passive_tree_catalog.cpp` 按已批准规格建立：中央 ID 0–7，路线入口 8/22/36/50，每条路线 `B+0..B+13`，关键节点 21/35/49/63。以 [已批准设计第 3.1–3.2 节](../specs/2026-07-14-stage7-passive-star-chart-design.md) 的两张节点表为唯一数据来源：0 同时连接 1–7 和四个路线入口；每条路线严格采用 `B+1` 的三叉、`B+2→3→4→10→13`、`B+5→6→7→11`、`B+8→9→12` 的无环路径，中央及路线数值、关键节点收益和代价逐项照表编码，不作改名或调参。每条边在两端同时出现，所有 Modifier ID 由 `1000U + node_id * 3U + local_index` 生成并唯一。

`passive_tree_rules.cpp` 用 `std::array<PassiveNodeId, 64>` 与 `std::array<bool, 64>` 实现 BFS，不使用 `vector`。`valid_passive_tree_state()` 检查 bit 0、没有超范围位、节点定义与已分配集合连通、以及 `popcount(bits & ~1ULL) + unspent == earned`。`evaluate_passive_tree()` 按节点 ID 顺序写入固定 `std::array<Modifier, 128>`，再调用 Task 1 的 `evaluate_player_modifiers()`；目录测试必须逐项锁定 64 个节点和每条边，避免“看起来正确”但数值漂移。

增加根 CMake 的 `add_subdirectory(src/passives)`，在 `src/modifiers` 后、`src/dungeon` 前；添加 `tests/passives`，并在平台架构 CMake 增加 `architecture.passives_no_raylib` 和 passives 不可达 combat/dungeon/persistence 的边界断言。

- [ ] **Step 4: 验证星盘核心和边界**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_passive_tests
ctest --test-dir out/build/windows-msvc-debug -R "passives.units|architecture.passives" --output-on-failure
git diff --check
```

Expected: `arpg_passive_tests` 为 9 cases, 0 failures；passives 只链接 `arpg_modifiers` 和 `arpg_progression`，没有 raylib/combat/dungeon/persistence 可达路径。

- [ ] **Step 5: 提交 Task 2**

```powershell
git add CMakeLists.txt src/passives tests/passives tests/platform/CMakeLists.txt
git commit -m "feat: add passive tree catalog and allocation rules"
```


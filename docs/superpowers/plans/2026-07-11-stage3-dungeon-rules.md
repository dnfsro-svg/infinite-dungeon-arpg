# Stage 3 地下城正式规则实施计划

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (- [ ]) syntax for tracking.

**Goal:** 在 Stage 2 单向房间循环上实现四元素偏向、10% 普通洞口、固定 1% 深渊、无限深度、事务式切换、双槽防重骰存档和对应 raylib 表现。

**Architecture:** dungeon 只产生确定性稳定状态和待提交事务，不访问文件系统；arpg_persistence 只包含无 Combat 依赖的 dungeon_checkpoint.hpp，负责 96 字节版本化检查点、CRC32 和双槽发布；raylib Host 负责把持久化三态结果回传 DungeonSession。门、洞、生态、深渊和存档表现都从已保存快照读取，不在渲染层重新投掷。

**Transitional model boundary:** Task 2–3 place the stable wire DTOs in `arpg::dungeon::checkpoint` so the existing Stage 2 runtime `arpg::dungeon::RoomDescriptor::combat` API remains buildable. Task 3 keeps the legacy `make_initial_room`/`make_next_room` wrappers while adding pure checkpoint generation/progression. Task 5 performs the runtime migration and then exposes the stable types through the public `arpg::dungeon` aliases used by persistence and session code.

**Tech Stack:** C++17、MSVC 19.44 x64、Windows SDK 10.0.26100.0、raylib 6.0.0 静态库、CMake 3.25、Ninja、CTest、PowerShell。

## Global Constraints

- main 在用户选择最终集成动作前保持 bb74bd48b42435d798b2b6ce9968d1d2d3f8a9da，不直接开发或提交功能。
- 集成工作树固定为 E:\game\.worktrees\m03-dungeon-rules，分支 milestone/m03-dungeon-rules。
- 每个任务分支只修改本节列出的文件；不得删除已有文件、重置 Git 或覆盖无关改动。
- persistence、dungeon、combat、core 都不得包含或链接 raylib。
- combat 不得包含或链接 dungeon 或 persistence；dungeon 不得包含或链接 persistence。
- persistence 只能包含 dungeon/dungeon_checkpoint.hpp，不得链接 arpg_dungeon 或传递引入 arpg_combat。
- 当前同时只模拟一个房间；不生成四门候选房，不保存旧房历史。
- 四门 ID 固定 up=0、down=1、left=2、right=3、none=255；元素 ID 固定 fire=0、water=1、lightning=2、chaos=3。
- 元素基础权重固定 100，每点偏向增加 25；普通洞口阈值固定 1000/10000，深渊阈值固定 100/10000。
- 洞口、深渊和生态使用独立域；不得使用 std::uniform_int_distribution。
- 初始稳定存档提交代数固定从 1 开始；全局房间序号从 0 开始；玩家看到的深度和本层房间号从 1 开始。
- 存档或规则错误必须区分 committed、not_committed、indeterminate；indeterminate 绝不能回旧房重选。
- Combat Update、DungeonSession tick、房间生成和事件热路径保持零堆分配。
- Stage 3 不实现正式怪物、奖励、经验、装备、掉落、背包、刷怪导演或深渊专属内容。
- 每个功能改动必须先写 RED 测试、观察预期失败、最小实现、观察 GREEN，再提交。
- 每个分支合并前必须完成规格评审和代码质量评审；Critical 与 Important 全部解决。
- 长时间执行时每 60 秒以内发送一次“没有卡住”进度更新。

## 文件与所有权

### 纯地下城模型工作树

工作树：E:\game\.worktrees\m03-room-rules

分支：task/m03-room-rules

- Modify: src/core/deterministic_rng.hpp
- Modify: src/core/deterministic_rng.cpp
- Modify: tests/core/deterministic_rng_tests.cpp
- Modify: tests/core/test_main.cpp
- Create: src/dungeon/dungeon_checkpoint.hpp
- Create: src/dungeon/dungeon_rules.hpp
- Create: src/dungeon/dungeon_rules.cpp
- Create: src/dungeon/room_combat_template.hpp
- Create: src/dungeon/room_combat_template.cpp
- Create: src/dungeon/dungeon_progression.hpp
- Create: src/dungeon/dungeon_progression.cpp
- Modify: src/dungeon/dungeon_types.hpp
- Modify: src/dungeon/room_generation.hpp
- Modify: src/dungeon/room_generation.cpp
- Modify: src/dungeon/CMakeLists.txt
- Create: tests/dungeon/dungeon_rules_tests.cpp
- Create: tests/dungeon/dungeon_progression_tests.cpp
- Modify: tests/dungeon/room_generation_tests.cpp
- Modify: tests/dungeon/dungeon_test_main.cpp
- Modify: tests/dungeon/CMakeLists.txt

### 事务会话工作树

工作树：E:\game\.worktrees\m03-dungeon-session

分支：task/m03-dungeon-session

- Modify: src/dungeon/dungeon_types.hpp
- Modify: src/dungeon/dungeon_session.hpp
- Modify: src/dungeon/dungeon_session.cpp
- Modify: src/dungeon/room_navigation.hpp
- Modify: src/dungeon/room_navigation.cpp
- Create: tests/dungeon/dungeon_transaction_tests.cpp
- Modify: tests/dungeon/dungeon_lifecycle_tests.cpp
- Modify: tests/dungeon/dungeon_navigation_tests.cpp
- Modify: tests/dungeon/dungeon_stress_tests.cpp
- Modify: tests/dungeon/dungeon_test_support.hpp
- Modify: tests/dungeon/dungeon_test_main.cpp
- Modify: tests/dungeon/CMakeLists.txt

### 持久化工作树

工作树：E:\game\.worktrees\m03-persistence

分支：task/m03-persistence

- Modify: CMakeLists.txt
- Create: src/persistence/CMakeLists.txt
- Create: src/persistence/crc32.hpp
- Create: src/persistence/crc32.cpp
- Create: src/persistence/checkpoint_codec.hpp
- Create: src/persistence/checkpoint_codec.cpp
- Create: src/persistence/save_paths.hpp
- Create: src/persistence/save_paths.cpp
- Create: src/persistence/save_store.hpp
- Create: src/persistence/save_store.cpp
- Create: tests/persistence/CMakeLists.txt
- Create: tests/persistence/persistence_test_main.cpp
- Create: tests/persistence/checkpoint_codec_tests.cpp
- Create: tests/persistence/save_store_tests.cpp
- Create: tests/persistence/save_store_fault_tests.cpp
- Modify: tests/platform/CMakeLists.txt
- Modify: tests/platform/module_boundary_test.cmake
- Create: tests/platform/persistence_boundary_test.cmake

### 存档集成工作树

工作树：E:\game\.worktrees\m03-save-integration

分支：task/m03-save-integration

- Create: tests/persistence/dungeon_save_integration_tests.cpp
- Modify: tests/persistence/persistence_test_main.cpp
- Modify: tests/persistence/CMakeLists.txt

### raylib 工作树

工作树：E:\game\.worktrees\m03-raylib-host

分支：task/m03-raylib-host

- Create: src/platform/raylib/host_launch_options.hpp
- Create: src/platform/raylib/host_launch_options.cpp
- Create: src/platform/raylib/dungeon_runtime.hpp
- Create: src/platform/raylib/dungeon_runtime.cpp
- Modify: src/platform/raylib/dungeon_view_math.hpp
- Modify: src/platform/raylib/dungeon_view_math.cpp
- Modify: src/platform/raylib/raylib_host.hpp
- Modify: src/platform/raylib/raylib_host.cpp
- Modify: src/platform/raylib/combat_renderer.hpp
- Modify: src/platform/raylib/combat_renderer.cpp
- Modify: src/platform/raylib/CMakeLists.txt
- Modify: src/app/main.cpp
- Create: tests/platform/host_launch_options_tests.cpp
- Create: tests/platform/dungeon_runtime_tests.cpp
- Modify: tests/platform/dungeon_view_math_tests.cpp
- Modify: tests/platform/platform_test_main.cpp
- Modify: tests/platform/CMakeLists.txt

---

### Task 1: 为 DeterministicRng 增加固定拒绝采样

**Worktree:** E:\game\.worktrees\m03-room-rules

**Files:**
- Modify: src/core/deterministic_rng.hpp
- Modify: src/core/deterministic_rng.cpp
- Modify: tests/core/deterministic_rng_tests.cpp
- Modify: tests/core/test_main.cpp

**Interfaces:**
- Consumes: 已有 DeterministicRng::next_u64()。
- Produces:

    [[nodiscard]] std::optional<std::uint64_t>
    next_bounded(std::uint64_t bound) noexcept;

- bound=0 返回 nullopt 且不消费 RNG；其他 bound 使用规则版本 1 的固定拒绝采样。

- [ ] **Step 1: 创建规则工作树并验证基线**

    git -C E:\game\.worktrees\m03-dungeon-rules worktree add E:\game\.worktrees\m03-room-rules -b task/m03-room-rules milestone/m03-dungeon-rules
    Set-Location E:\game\.worktrees\m03-room-rules
    .\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh

Expected: 7/7 CTest 通过；Core 22 cases、Dungeon 20 cases、0 failures。

- [ ] **Step 2: 添加两个 RED 用例**

在 deterministic_rng_tests.cpp 中加入：

    arpg::test::Failure bounded_zero_and_golden_samples() noexcept {
        DeterministicRng zero{1};
        ARPG_REQUIRE(!zero.next_bounded(0).has_value());
        ARPG_REQUIRE(zero.next_u64() == 0xB3F2AF6D0FC710C5ULL);

        DeterministicRng samples{1};
        ARPG_REQUIRE(samples.next_bounded(10000).value() == 9557U);
        ARPG_REQUIRE(samples.next_bounded(425).value() == 97U);
        ARPG_REQUIRE(samples.next_bounded(1).value() == 0U);
        return {};
    }

    arpg::test::Failure bounded_rejection_path_is_fixed() noexcept {
        DeterministicRng rng{1};
        static_cast<void>(rng.next_u64());
        static_cast<void>(rng.next_u64());
        static_cast<void>(rng.next_u64());
        ARPG_REQUIRE(
            rng.next_bounded(0x8000000000000001ULL).value()
            == 0x327A48E29A233672ULL);
        return {};
    }

把这两个用例加入 kCases，Core 守卫从 22 改为 24。

- [ ] **Step 3: 运行测试并观察 RED**

    .\scripts\Build.ps1 -Preset windows-msvc-core-debug
    .\out\build\windows-msvc-core-debug\bin\arpg_core_tests.exe

Expected: 编译失败，提示 DeterministicRng 没有 next_bounded。

- [ ] **Step 4: 实现固定拒绝采样**

头文件加入 optional。实现必须等价于：

    std::optional<std::uint64_t>
    DeterministicRng::next_bounded(std::uint64_t bound) noexcept {
        if (bound == 0U) {
            return std::nullopt;
        }
        const std::uint64_t threshold =
            (std::uint64_t{0} - bound) % bound;
        for (;;) {
            const std::uint64_t value = next_u64();
            if (value >= threshold) {
                return value % bound;
            }
        }
    }

- [ ] **Step 5: 运行 GREEN**

    .\scripts\Build.ps1 -Preset windows-msvc-core-debug
    .\out\build\windows-msvc-core-debug\bin\arpg_core_tests.exe

Expected: 24 cases, 0 failures。

- [ ] **Step 6: 提交**

    git diff --check
    git add src/core/deterministic_rng.hpp src/core/deterministic_rng.cpp tests/core/deterministic_rng_tests.cpp
    git commit -m "feat: add deterministic bounded sampling"

---

### Task 2: 建立无 Combat 依赖的稳定检查点与规则配置

**Worktree:** E:\game\.worktrees\m03-room-rules

**Files:**
- Create: src/dungeon/dungeon_checkpoint.hpp
- Create: src/dungeon/dungeon_rules.hpp
- Create: src/dungeon/dungeon_rules.cpp
- Modify: src/dungeon/dungeon_types.hpp
- Modify: src/dungeon/CMakeLists.txt
- Create: tests/dungeon/dungeon_rules_tests.cpp
- Modify: tests/dungeon/dungeon_test_main.cpp
- Modify: tests/dungeon/CMakeLists.txt

**Interfaces:**
- dungeon_checkpoint.hpp 只能包含 array 与 cstdint，并在 `arpg::dungeon::checkpoint` 命名空间产生：

    enum class ExitDirection : std::uint8_t {
        up = 0, down = 1, left = 2, right = 3, none = 0xFF
    };

    enum class EntrySide : std::uint8_t {
        initial = 0, top = 1, bottom = 2, left = 3, right = 4
    };

    enum class DungeonElement : std::uint8_t {
        fire = 0, water = 1, lightning = 2, chaos = 3
    };

    enum class TransitionKind : std::uint8_t {
        door = 0, descent = 1, none = 0xFF
    };

    struct RoomDescriptor final {
        std::uint64_t index{};
        std::uint64_t seed{};
        std::uint64_t depth{1};
        std::uint64_t floor_room_index{1};
        EntrySide entry{EntrySide::initial};
        DungeonElement ecology{DungeonElement::fire};
        bool has_hole{};
        bool is_abyss{};
    };

    struct DungeonRunState final {
        std::uint64_t root_seed{};
        std::uint64_t commit_generation{1};
        std::array<std::uint32_t, 4> biases{};
        RoomDescriptor current_room{};
        TransitionKind last_transition{TransitionKind::none};
        ExitDirection last_direction{ExitDirection::none};
    };

- dungeon_rules.hpp 产生：

    inline constexpr std::uint32_t kProbabilityScale = 10000U;

    enum class DungeonFault : std::uint8_t {
        none,
        invalid_rules,
        invalid_direction,
        commit_generation_overflow,
        room_index_overflow,
        depth_overflow,
        floor_room_overflow,
        bias_overflow,
        weight_overflow,
        event_overflow,
        combat_relay_overflow,
        save_commit_indeterminate,
        save_receipt_mismatch
    };

    struct DungeonRules final {
        std::array<std::uint32_t, 4> base_weights{{100, 100, 100, 100}};
        std::uint32_t bias_weight_increment{25};
        std::uint32_t hole_threshold{1000};
        std::uint32_t abyss_threshold{100};
        std::uint32_t rules_version{1};
    };

    [[nodiscard]] std::optional<checkpoint::DungeonElement> element_for_exit(
        checkpoint::ExitDirection direction) noexcept;
    [[nodiscard]] DungeonFault validate_rules(
        const DungeonRules& rules) noexcept;
    [[nodiscard]] DungeonFault compute_ecology_weights(
        const DungeonRules& rules,
        const std::array<std::uint32_t, 4>& biases,
        std::array<std::uint64_t, 4>& weights,
        std::uint64_t& total) noexcept;

- [ ] **Step 1: 写五个规则 RED 用例**

dungeon_rules_tests.cpp 必须分别覆盖：

    static_assert(static_cast<std::uint8_t>(ExitDirection::up) == 0U);
    static_assert(static_cast<std::uint8_t>(ExitDirection::none) == 0xFFU);
    static_assert(static_cast<std::uint8_t>(TransitionKind::descent) == 1U);

    ARPG_REQUIRE(element_for_exit(ExitDirection::up).value()
        == DungeonElement::fire);
    ARPG_REQUIRE(element_for_exit(ExitDirection::down).value()
        == DungeonElement::water);
    ARPG_REQUIRE(element_for_exit(ExitDirection::left).value()
        == DungeonElement::lightning);
    ARPG_REQUIRE(element_for_exit(ExitDirection::right).value()
        == DungeonElement::chaos);
    ARPG_REQUIRE(!element_for_exit(ExitDirection::none).has_value());

    DungeonRules defaults;
    ARPG_REQUIRE(validate_rules(defaults) == DungeonFault::none);
    ARPG_REQUIRE(defaults.base_weights[0] == 100U);
    ARPG_REQUIRE(defaults.bias_weight_increment == 25U);
    ARPG_REQUIRE(defaults.hole_threshold == 1000U);
    ARPG_REQUIRE(defaults.abyss_threshold == 100U);

    std::array<std::uint32_t, 4> biases{{5, 0, 0, 0}};
    std::array<std::uint64_t, 4> weights{};
    std::uint64_t total = 0;
    ARPG_REQUIRE(compute_ecology_weights(
        defaults, biases, weights, total) == DungeonFault::none);
    ARPG_REQUIRE(weights[0] == 225U && total == 525U);

另外两个用例固定：阈值 0 与 10000 合法、10001 非法；基础权重 0 非法；四个 bias 全为 UINT32_MAX 且增量为 UINT32_MAX 时，单项仍可表示但累计总权重返回 weight_overflow，输出 weights 与 total 保持全零。

注册 dungeon_rules_suite，并把阶段性 Dungeon 守卫从 20 改为 25。

规则测试可在函数体内使用 `using namespace checkpoint;` 简化断言书写，但生产接口仍使用显式的 `checkpoint::` 类型。

- [ ] **Step 2: 运行测试并观察 RED**

    .\scripts\Build.ps1 -Preset windows-msvc-core-debug
    .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe

Expected: 编译失败，缺少 dungeon_checkpoint.hpp 与 dungeon_rules.hpp。

- [ ] **Step 3: 实现稳定头和 checked 权重**

实现要求：

- dungeon_types.hpp 改为包含 dungeon_checkpoint.hpp，并将 ExitDirection、EntrySide 别名到 checkpoint 命名空间；为保持 Stage 2 可独立编译，旧的运行时 RoomDescriptor（含 combat）暂时保留到 Task 5。
- `checkpoint::RoomDescriptor` 不得包含 CombatLabConfig、optional、string、filesystem 或 raylib 类型；Task 2 不修改 room_generation/session 的旧运行时接口。
- validate_rules 拒绝规则版本非 1、任一基础权重为 0、阈值大于 10000。
- compute_ecology_weights 对乘法、逐项加法和总和都先检查再计算，失败时不得留下部分有效总和。
- compute_ecology_weights 先写局部 weights/total，全部成功后才复制到输出；任何 fault 时输出保持调用前的全零状态。
- element_for_exit 对 ExitDirection::none 返回 nullopt，不伪造元素。

每项权重必须使用同一检查形状：

    const std::uint64_t base = rules.base_weights[index];
    const std::uint64_t bias = biases[index];
    const std::uint64_t increment = rules.bias_weight_increment;
    if (increment != 0U
            && bias > ((std::numeric_limits<std::uint64_t>::max)()
                - base) / increment) {
        return DungeonFault::weight_overflow;
    }
    const std::uint64_t weight = base + bias * increment;
    if (total > (std::numeric_limits<std::uint64_t>::max)() - weight) {
        return DungeonFault::weight_overflow;
    }
    total += weight;

- [ ] **Step 4: 运行 GREEN**

    .\scripts\Build.ps1 -Preset windows-msvc-core-debug
    .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe

Expected: 25 cases, 0 failures。

- [ ] **Step 5: 提交**

    git diff --check
    git add src/dungeon tests/dungeon
    git commit -m "feat: define stage 3 dungeon rules"

---

### Task 3: 实现房间三流随机、战斗模板派生和纯状态推进

**Worktree:** E:\game\.worktrees\m03-room-rules

**Files:**
- Create: src/dungeon/room_combat_template.hpp
- Create: src/dungeon/room_combat_template.cpp
- Create: src/dungeon/dungeon_progression.hpp
- Create: src/dungeon/dungeon_progression.cpp
- Modify: src/dungeon/room_generation.hpp
- Modify: src/dungeon/room_generation.cpp
- Modify: src/dungeon/CMakeLists.txt
- Modify: tests/dungeon/room_generation_tests.cpp
- Create: tests/dungeon/dungeon_progression_tests.cpp
- Modify: tests/dungeon/dungeon_test_main.cpp
- Modify: tests/dungeon/CMakeLists.txt

**Interfaces:**
- 本任务新增的 `RoomDescriptor`、`RoomGenerationResult` 和 `DungeonRunState` 均指 `arpg::dungeon::checkpoint` 中的稳定 DTO；现有 `make_initial_room`/`make_next_room` 继续返回旧运行时 RoomDescriptor，直到 Task 5 迁移完成。
- 本节代码块中的 `ExitDirection`、`EntrySide`、`RoomDescriptor` 和 `DungeonRunState` 均为 `checkpoint::` 类型的简写。
- room_generation.hpp 产生：

    struct RoomRandomSamples final {
        std::uint64_t ecology{};
        std::uint32_t hole{};
        std::uint32_t abyss{};
    };

    struct RoomGenerationResult final {
        DungeonFault fault{DungeonFault::none};
        RoomDescriptor room{};
        RoomRandomSamples samples{};
    };

RoomRandomSamples 只用于生成单元测试和当次生成诊断，不进入 RoomDescriptor 或 wire 存档；加载后的 F1 只显示已保存的 ecology、has_hole、is_abyss 结果。

    [[nodiscard]] std::uint64_t derive_initial_room_seed(
        std::uint64_t root, std::uint64_t serial) noexcept;
    [[nodiscard]] std::uint64_t derive_door_room_seed(
        std::uint64_t current, std::uint64_t next_serial,
        ExitDirection direction) noexcept;
    [[nodiscard]] std::uint64_t derive_descent_room_seed(
        std::uint64_t current, std::uint64_t next_serial) noexcept;
    [[nodiscard]] RoomGenerationResult generate_room_descriptor(
        std::uint64_t seed,
        std::uint64_t global_index,
        std::uint64_t depth,
        std::uint64_t floor_room_index,
        EntrySide entry,
        const std::array<std::uint32_t, 4>& biases,
        const DungeonRules& rules) noexcept;

- room_combat_template.hpp 产生：

    [[nodiscard]] std::optional<combat::CombatLabConfig>
    make_combat_lab_config(
        EntrySide entry, std::uint32_t rules_version) noexcept;

- dungeon_progression.hpp 产生：

    struct RunStateBuildResult final {
        DungeonFault fault{DungeonFault::none};
        DungeonRunState state{};
        RoomRandomSamples samples{};
    };

    [[nodiscard]] RunStateBuildResult make_initial_run_state(
        std::uint64_t root_seed, const DungeonRules& rules) noexcept;
    [[nodiscard]] RunStateBuildResult make_door_transition(
        const DungeonRunState& current,
        ExitDirection direction,
        const DungeonRules& rules) noexcept;
    [[nodiscard]] RunStateBuildResult make_descent_transition(
        const DungeonRunState& current,
        const DungeonRules& rules) noexcept;
    [[nodiscard]] bool same_run_state(
        const DungeonRunState& lhs,
        const DungeonRunState& rhs) noexcept;

- [ ] **Step 1: 重写八个房间生成 RED 用例**

room_generation_tests.cpp 的八个用例固定：

1. 初始、门、下坠完整黄金种子链。
2. 对 0xCF92F9DC3E32DA47 与权重 125/100/100/100 得到 ecology=83、hole=7295、abyss=8629，生态为 fire。
3. 洞口边界：seed 0xAA3 得 999 为真；seed 0x2D80 得 1000 为假。
4. 深渊边界：seed 0x11E9 得 99 为真；seed 0x38 得 100 为假。
5. 共存：seed 0x747 得 hole=210、abyss=47，两者均真。
6. 同 seed 改 bias 只能改变 ecology 权重与可能的生态，hole/abyss 样本完全相同。
7. 五种进入侧重建 Stage 2 原始玩家、朝向和三个木桩模板。
8. rules_version 非 1 时 make_combat_lab_config 返回 nullopt。

黄金链断言：

    constexpr std::uint64_t root = 0x0123456789ABCDEFULL;
    const auto initial = derive_initial_room_seed(root, 0U);
    ARPG_REQUIRE(initial == 0xCA5A07A71C3153C4ULL);
    const auto up = derive_door_room_seed(
        initial, 1U, ExitDirection::up);
    ARPG_REQUIRE(up == 0xCF92F9DC3E32DA47ULL);
    const auto right = derive_door_room_seed(
        up, 2U, ExitDirection::right);
    ARPG_REQUIRE(right == 0xF71A3E545FA8D5CCULL);
    ARPG_REQUIRE(derive_descent_room_seed(right, 3U)
        == 0x21DD351FA20839E8ULL);

- [ ] **Step 2: 写七个纯推进 RED 用例**

dungeon_progression_tests.cpp 的七个用例固定：

1. 初始状态 generation=1、index=0、depth=1、floor=1、bias 全 0，三流只投掷一次。
2. 四门分别增加正确 bias，generation/index/floor 各加 1，depth 不变。
3. 门偏向先增加再生成下一房生态。
4. 下坠 generation/index/depth 加 1、floor=1、bias 清零、entry=initial、last_transition=descent、last_direction=none。
5. generation、index、depth、floor、被选 bias 五种上限分别返回对应 fault，输入 state 字段完全不变。
6. 同根种子和相同门/洞脚本得到 same_run_state。
7. 不调用的三个门不会派生或改变任何样本。

注册 dungeon_progression_suite；房间生成保持 8 cases，规则 5 cases，推进 7 cases，加原有生命周期 5、导航 6、压力 4，阶段性 Dungeon 守卫设为 35。

- [ ] **Step 3: 运行测试并观察 RED**

    .\scripts\Build.ps1 -Preset windows-msvc-core-debug
    .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe

Expected: 编译失败，缺少新生成与推进接口。

- [ ] **Step 4: 实现固定域和三流判定**

room_generation.cpp 固定域：

    constexpr std::uint64_t kInitialRoomDomain =
        0x524F4F4D5F494E49ULL;
    constexpr std::uint64_t kDoorRoomDomain =
        0x524F4F4D5F4E4558ULL;
    constexpr std::uint64_t kDescentRoomDomain =
        0x44455343454E5431ULL;
    constexpr std::uint64_t kEcologyDomain =
        0x45434F4C4F475931ULL;
    constexpr std::uint64_t kHoleDomain =
        0x484F4C455F563031ULL;
    constexpr std::uint64_t kAbyssDomain =
        0x41425953535F3031ULL;

多输入派生必须逐层取第一个值：

    std::uint64_t first(
        std::uint64_t seed, std::uint64_t domain) noexcept {
        auto stream = core::DeterministicRng::derive_stream(seed, domain);
        return stream.next_u64();
    }

    std::uint64_t derive_door_room_seed(
        std::uint64_t current,
        std::uint64_t next_serial,
        ExitDirection direction) noexcept {
        return first(
            first(first(current, kDoorRoomDomain), next_serial),
            static_cast<std::uint64_t>(direction));
    }

    std::uint64_t derive_descent_room_seed(
        std::uint64_t current,
        std::uint64_t next_serial) noexcept {
        return first(
            first(first(current, kDescentRoomDomain), next_serial),
            static_cast<std::uint64_t>(ExitDirection::none));
    }

生态按 fire、water、lightning、chaos 的累计左闭右开区间选取。hole 与 abyss 各自创建 derive_stream(seed, domain)，各调用一次 next_bounded(10000)。生成失败不得留下半有效 RoomDescriptor。

- [ ] **Step 5: 移出 Combat 模板并实现状态推进**

- 把原 room_generation.cpp 的五种 CombatLabConfig 模板逐字段原样迁移到 room_combat_template.cpp。
- initial generation 固定为 1。
- 门切换按 generation、index、floor、bias 的顺序 checked +1，再生成唯一下一房。
- 下坠按 generation、index、depth checked +1，floor=1、bias 清零，再用 descent 域生成。
- make_door_transition 对 none 返回 invalid_direction，不派生种子。
- same_run_state 逐字段比较，不使用 memcmp。

- [ ] **Step 6: 运行 GREEN 和完整规则门**

    .\scripts\Build.ps1 -Preset windows-msvc-core-debug
    .\out\build\windows-msvc-core-debug\bin\arpg_core_tests.exe
    .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
    ctest.exe --test-dir .\out\build\windows-msvc-core-debug -R "^(core\.units|dungeon\.units|architecture\.)$" --output-on-failure

Expected: Core 24 cases、Dungeon 35 cases、所有选择的 CTest 通过。

- [ ] **Step 7: 提交**

    git diff --check
    git add src/dungeon tests/dungeon
    git commit -m "feat: add deterministic dungeon progression"

---

### Task 4: 评审并合并稳定模型，派生会话与持久化工作树

**Integration worktree:** E:\game\.worktrees\m03-dungeon-rules

**Consumes:** task/m03-room-rules 的三个提交。

**Produces:** 经评审的 milestone/m03-dungeon-rules，以及从同一提交派生的 task/m03-dungeon-session 和 task/m03-persistence。

- [ ] **Step 1: 在规则分支执行完整基线**

    git -C E:\game\.worktrees\m03-room-rules status --short --branch
    E:\game\.worktrees\m03-room-rules\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
    E:\game\.worktrees\m03-room-rules\out\build\windows-msvc-core-debug\bin\arpg_core_tests.exe
    E:\game\.worktrees\m03-room-rules\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe

Expected: 工作树干净；Core 24、Dungeon 35，0 failures；Core-only 7/7。

- [ ] **Step 2: 执行两阶段评审**

规格评审逐条核对设计第 2、5、6 节，重点检查：

- 初始 generation=1、room index=0、depth/floor=1。
- 门偏向先增加，再生成唯一下一房。
- 洞口和深渊阈值边界与独立流。
- 下坠清零偏向且 entry=initial。
- `checkpoint::RoomDescriptor` 不包含 CombatLabConfig。
- persistence 可单独包含 dungeon_checkpoint.hpp 而不引入 combat。

代码质量评审重点检查：

- next_bounded 不消费 bound=0。
- 拒绝采样没有除零或有偏映射。
- checked 加法/乘法无静默回绕。
- 没有 memcmp、标准库随机分布、候选房数组或堆分配。

任何 Critical/Important 必须增加 RED 回归后修复，并重跑 Step 1。

- [ ] **Step 3: 合并规则分支**

    git -C E:\game\.worktrees\m03-dungeon-rules merge --no-ff task/m03-room-rules -m "merge: integrate stage 3 dungeon model"
    E:\game\.worktrees\m03-dungeon-rules\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh

- [ ] **Step 4: 从同一集成提交创建两个工作树**

    git -C E:\game\.worktrees\m03-dungeon-rules worktree add E:\game\.worktrees\m03-dungeon-session -b task/m03-dungeon-session milestone/m03-dungeon-rules
    git -C E:\game\.worktrees\m03-dungeon-rules worktree add E:\game\.worktrees\m03-persistence -b task/m03-persistence milestone/m03-dungeon-rules

- [ ] **Step 5: 验证两个新工作树**

    E:\game\.worktrees\m03-dungeon-session\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
    E:\game\.worktrees\m03-persistence\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh

Expected: 两边均 7/7，且起点提交完全相同。

---

### Task 5: 把 DungeonSession 改为显式持久化事务

**Worktree:** E:\game\.worktrees\m03-dungeon-session

**Files:**
- Modify: src/dungeon/dungeon_types.hpp
- Modify: src/dungeon/dungeon_session.hpp
- Modify: src/dungeon/dungeon_session.cpp
- Modify: src/dungeon/room_navigation.hpp
- Modify: src/dungeon/room_navigation.cpp
- Create: tests/dungeon/dungeon_transaction_tests.cpp
- Modify: tests/dungeon/dungeon_lifecycle_tests.cpp
- Modify: tests/dungeon/dungeon_navigation_tests.cpp
- Modify: tests/dungeon/dungeon_stress_tests.cpp
- Modify: tests/dungeon/dungeon_test_support.hpp
- Modify: tests/dungeon/dungeon_test_main.cpp
- Modify: tests/dungeon/CMakeLists.txt

**Interfaces:**

    enum class RoomPhase : std::uint8_t {
        locked,
        combat,
        cleared,
        awaiting_exit,
        committing,
        transitioning,
        faulted
    };

    enum class SaveDisposition : std::uint8_t {
        committed,
        not_committed,
        indeterminate
    };

    struct PendingTransition final {
        TransitionKind kind{TransitionKind::none};
        ExitDirection direction{ExitDirection::none};
        std::uint64_t expected_generation{};
        DungeonRunState next_state{};
    };

    struct TransitionSaveResult final {
        SaveDisposition disposition{SaveDisposition::indeterminate};
        std::uint64_t generation{};
        DungeonRunState verified_state{};
    };

    explicit DungeonSession(
        DungeonRules rules,
        DungeonRunState stable_state) noexcept;
    [[nodiscard]] bool request_descent(
        bool player_in_range) noexcept;
    [[nodiscard]] std::optional<PendingTransition>
    pending_transition() const noexcept;
    void resolve_pending_transition(
        const TransitionSaveResult& result) noexcept;

DungeonEvent 增加 transition_requested、transition_committed、save_failed，并固定完整字段：

    struct DungeonEvent final {
        DungeonEventKind kind{};
        std::uint64_t session_tick{};
        std::uint64_t room_index{};
        std::uint64_t room_seed{};
        std::uint64_t destination_room_index{};
        std::uint64_t destination_room_seed{};
        TransitionKind transition{TransitionKind::none};
        ExitDirection direction{ExitDirection::none};
    };

room_index/room_seed 始终是事件 subject；transition_committed 的 subject 是旧房、destination 是新房；room_destroyed 的 subject 是旧房且 destination 保持新房，便于表现层清理。

DungeonSnapshot 固定完整字段：

    struct DungeonSnapshot final {
        std::uint64_t session_tick{};
        std::uint64_t root_seed{};
        std::uint64_t commit_generation{};
        std::uint64_t room_index{};
        std::uint64_t room_seed{};
        std::uint64_t depth{1};
        std::uint64_t floor_room_index{1};
        std::array<std::uint32_t, 4> biases{};
        RoomPhase phase{RoomPhase::locked};
        bool has_active_room{};
        std::array<bool, 4> exits_open{};
        std::uint8_t remaining_targets{};
        EntrySide entry_side{EntrySide::initial};
        ExitDirection last_exit{ExitDirection::none};
        TransitionKind last_transition{TransitionKind::none};
        DungeonElement ecology{DungeonElement::fire};
        bool has_hole{};
        bool is_abyss{};
        bool has_pending_transition{};
        std::optional<combat::CombatSnapshot> combat{};
        DungeonDiagnostics diagnostics{};
    };

DungeonDiagnostics 保留 event_overflow_count、combat_relay_overflow_count、rejected_exit_count，并新增 save_failure_count 与 DungeonFault fault。

- [ ] **Step 1: 写九个事务 RED 用例**

dungeon_transaction_tests.cpp 必须固定以下九例：

1. 构造器使用已保存 RoomDescriptor，不调用 generate_room_descriptor 重骰。
2. 门请求进入 committing，旧 CombatWorld 仍存在且冻结，pending 只含唯一下一房。
3. exact committed 回执才采用 verified_state、销毁旧房并进入 transitioning。
4. not_committed 丢弃 pending、回 awaiting_exit，重试得到字段完全相同的 pending。
5. indeterminate 进入 faulted，不允许移动、攻击、R 或重新选择。
6. generation 不匹配进入 save_receipt_mismatch。
7. verified_state 任一字段不匹配进入 save_receipt_mismatch。
8. 洞口请求只有房已清、has_hole、player_in_range、E 按下沿四项同时满足才产生 descent pending。
9. 事件队列或 combat relay 溢出在 Release 语义也进入 faulted，不继续传播。

核心断言形状：

    const auto before = session.snapshot();
    drive_to_open_door(session, ExitDirection::up);
    const auto pending = session.pending_transition();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->expected_generation
        == pending->next_state.commit_generation);
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::committing);
    ARPG_REQUIRE(session.snapshot().combat.has_value());
    ARPG_REQUIRE(session.snapshot().room_index == before.room_index);

    session.resolve_pending_transition({
        SaveDisposition::committed,
        pending->next_state.commit_generation,
        pending->next_state,
    });
    ARPG_REQUIRE(session.snapshot().phase == RoomPhase::transitioning);
    ARPG_REQUIRE(!session.snapshot().combat.has_value());

- [ ] **Step 2: 运行测试并观察 RED**

    .\scripts\Build.ps1 -Preset windows-msvc-core-debug
    .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe

Expected: 编译失败，缺少 committing、PendingTransition 和新 Session API。

- [ ] **Step 3: 实现事务状态机**

实现顺序必须固定：

1. 门或洞请求调用纯 progression 函数生成 pending，并把 expected_generation 精确设为 next_state.commit_generation。
2. phase 设为 committing；旧 CombatWorld 保留且不再 tick。
3. committed 回执先逐字段 same_run_state，再把 stable_state 设为 verified_state。
4. 一旦磁盘状态被接受，旧 combat 必须销毁，即使随后事件队列溢出也不能回滚。
5. transition_committed 和 room_destroyed 使用显式捕获的旧/新房字段。
6. 下一固定 Tick 才从新 descriptor 的 entry 派生 CombatLabConfig 并进入 locked。
7. not_committed 才能回 awaiting_exit。
8. indeterminate 或回执不匹配进入 faulted。

emit 失败时先设置 diagnostics.fault 与 phase=faulted，再在 Debug 触发 assert。Release 不得继续。

回执处理的骨架必须保持以下分支：

    void DungeonSession::resolve_pending_transition(
        const TransitionSaveResult& result) noexcept {
        if (phase_ != RoomPhase::committing
                || !pending_.has_value()) {
            enter_fault(DungeonFault::save_receipt_mismatch);
            return;
        }
        if (result.disposition == SaveDisposition::indeterminate) {
            enter_fault(DungeonFault::save_commit_indeterminate);
            return;
        }
        if (result.disposition == SaveDisposition::not_committed) {
            pending_.reset();
            phase_ = RoomPhase::awaiting_exit;
            emit_save_failed();
            return;
        }
        if (result.generation
                != pending_->expected_generation
                || pending_->expected_generation
                    != pending_->next_state.commit_generation
                || !same_run_state(
                    result.verified_state, pending_->next_state)) {
            enter_fault(DungeonFault::save_receipt_mismatch);
            return;
        }
        const DungeonRunState previous = stable_state_;
        stable_state_ = result.verified_state;
        combat_.reset();
        phase_ = RoomPhase::transitioning;
        emit_committed(previous, stable_state_);
        pending_.reset();
    }

- [ ] **Step 4: 迁移旧生命周期、导航和 helper**

在 dungeon_test_support.hpp 新增：

    inline bool commit_pending(
        dungeon::DungeonSession& session) noexcept {
        const auto pending = session.pending_transition();
        if (!pending.has_value()) {
            return false;
        }
        session.resolve_pending_transition({
            dungeon::SaveDisposition::committed,
            pending->next_state.commit_generation,
            pending->next_state,
        });
        return true;
    }

旧 drive_exit 在观察到 committing 后必须显式 commit_pending，再检查 transitioning。删除“请求出口立即销毁”的旧断言，替换为“请求后冻结、提交后销毁”。R 在 committing/faulted 时无效；在正常房间只重建 combat，不改变所有稳定字段。

- [ ] **Step 5: 更新 1000 房压力脚本**

保持四个压力用例数量不变。每个房清理后：

- 当前房有洞且 global index 能被 7 整除时调用 request_descent(true)。
- 其他房按 up、right、down、left 循环走门。
- 每个 pending 都显式回 committed。
- 两个相同会话逐字段比较 snapshot、pending、DungeonEvent 与 CombatEvent。

测量区继续要求 allocation delta=0、全部 overflow=0、最终 phase=combat。Dungeon 最终守卫设为 44 cases：

- room generation 8
- dungeon rules 5
- progression 7
- transaction 9
- lifecycle 5
- navigation 6
- stress 4

- [ ] **Step 6: 运行 GREEN**

    .\scripts\Build.ps1 -Preset windows-msvc-core-debug
    .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
    ctest.exe --test-dir .\out\build\windows-msvc-core-debug -R "^(dungeon\.units|architecture\.)$" --output-on-failure

Expected: Dungeon 44 cases、0 failures；选择的 CTest 全绿。

- [ ] **Step 7: 提交**

    git diff --check
    git add src/dungeon tests/dungeon
    git commit -m "feat: make dungeon transitions transactional"

---

### Task 6: 实现 96 字节版本化检查点编解码

**Worktree:** E:\game\.worktrees\m03-persistence

**Files:**
- Modify: CMakeLists.txt
- Create: src/persistence/CMakeLists.txt
- Create: src/persistence/crc32.hpp
- Create: src/persistence/crc32.cpp
- Create: src/persistence/checkpoint_codec.hpp
- Create: src/persistence/checkpoint_codec.cpp
- Create: tests/persistence/CMakeLists.txt
- Create: tests/persistence/persistence_test_main.cpp
- Create: tests/persistence/checkpoint_codec_tests.cpp
- Modify: tests/platform/CMakeLists.txt
- Modify: tests/platform/module_boundary_test.cmake
- Create: tests/platform/persistence_boundary_test.cmake

**Interfaces:**

    inline constexpr std::size_t kCheckpointHeaderSize = 32U;
    inline constexpr std::size_t kCheckpointPayloadSize = 64U;
    inline constexpr std::size_t kEncodedCheckpointSize = 96U;
    inline constexpr std::uint32_t kCheckpointFormatVersion = 1U;

    enum class CodecError : std::uint8_t {
        none,
        wrong_size,
        bad_magic,
        unsupported_format,
        unsupported_rules,
        bad_payload_length,
        bad_crc,
        invalid_enum,
        invalid_boolean,
        invalid_state
    };

    struct DecodeResult final {
        CodecError error{CodecError::none};
        dungeon::DungeonRunState state{};
    };

    [[nodiscard]] std::uint32_t crc32(
        const std::uint8_t* bytes, std::size_t size) noexcept;
    [[nodiscard]] bool encode_checkpoint(
        const dungeon::DungeonRunState& state,
        std::array<std::uint8_t, kEncodedCheckpointSize>& out) noexcept;
    [[nodiscard]] DecodeResult decode_checkpoint(
        const std::uint8_t* bytes, std::size_t size) noexcept;

arpg_persistence 的 PUBLIC include 是项目 src；不得 target_link_libraries 到 arpg_dungeon。它通过无 Combat 的 dungeon_checkpoint.hpp 共享 wire DTO。

- [ ] **Step 1: 写八个 codec RED 用例**

八例固定为：

1. 所有字段非零的往返。
2. 总长度 96、header 32、payload 64，以及小端 generation 字节。
3. 字符串 123456789 的 CRC 必须为 0xCBF43926；任一 CRC 覆盖字节翻转后 bad_crc。
4. 错 magic。
5. 格式版本和规则版本分别不支持。
6. payload length 不是 64。
7. entry、element、transition、direction 越界。
8. bool 不是 0/1、depth/floor/generation 为 0。

测试 fixture 必须手工逐字段构造 DungeonRunState，不调用 dungeon progression，证明 persistence 不链接 arpg_dungeon。

- [ ] **Step 2: 运行测试并观察 RED**

    .\scripts\Build.ps1 -Preset windows-msvc-core-debug -Fresh

Expected: 配置或编译失败，缺少 src/persistence 与 codec 接口。

- [ ] **Step 3: 实现 wire layout**

Header 固定：

| Offset | Size | Field |
| --- | --- | --- |
| 0 | 8 | magic IARPGS03 |
| 8 | 4 | format version |
| 12 | 4 | rules version |
| 16 | 8 | commit generation |
| 24 | 4 | payload length=64 |
| 28 | 4 | CRC32 |

Payload 固定：

| Offset | Size | Field |
| --- | --- | --- |
| 32 | 8 | root seed |
| 40 | 16 | four uint32 biases |
| 56 | 8 | room index |
| 64 | 8 | room seed |
| 72 | 8 | depth |
| 80 | 8 | floor room index |
| 88 | 1 | entry |
| 89 | 1 | ecology |
| 90 | 1 | has hole |
| 91 | 1 | is abyss |
| 92 | 1 | last transition |
| 93 | 1 | last direction |
| 94 | 2 | reserved zero |

CRC32 覆盖 bytes 8..27 与 32..95，跳过 magic 和 CRC 字段。所有多字节整数显式 little-endian，不 reinterpret_cast 结构体。

CRC 变体固定为 CRC-32/ISO-HDLC：反射多项式 0xEDB88320、初始值 0xFFFFFFFF、逐字节反射处理、最终异或 0xFFFFFFFF。实现核心：

    std::uint32_t crc = 0xFFFFFFFFU;
    for (std::size_t index = 0; index < size; ++index) {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                0U - static_cast<std::uint32_t>(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return crc ^ 0xFFFFFFFFU;

- [ ] **Step 4: 泛化模块边界检查**

module_boundary_test.cmake 改为接收 FORBIDDEN_MODULE 与 FORBIDDEN_LABEL，保留原 combat_no_dungeon 行为，并新增：

- architecture.persistence_no_raylib
- architecture.dungeon_no_persistence
- architecture.combat_no_persistence

CMake configure 时增加目标闭包断言：

    arpg_assert_target_dependency_boundary(arpg_persistence)
    arpg_assert_target_not_reachable(arpg_persistence arpg_dungeon)
    arpg_assert_target_not_reachable(arpg_persistence arpg_combat)
    arpg_assert_target_not_reachable(arpg_dungeon arpg_persistence)
    arpg_assert_target_not_reachable(arpg_combat arpg_persistence)

新增 architecture.persistence_checkpoint_only，使用 persistence_boundary_test.cmake 扫描 src/persistence：允许精确的 dungeon/dungeon_checkpoint.hpp，拒绝任何其他 dungeon 或 combat 头。Core-only CTest 数量从 7 增至 12。

- [ ] **Step 5: 运行 GREEN**

    .\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
    .\out\build\windows-msvc-core-debug\bin\arpg_persistence_tests.exe

Expected: Core-only 12/12；Persistence 8 cases、0 failures。

- [ ] **Step 6: 提交**

    git diff --check
    git add CMakeLists.txt src/persistence tests/persistence tests/platform
    git commit -m "feat: add versioned dungeon checkpoint codec"

---

### Task 7: 实现双槽发布、恢复和故障注入

**Worktree:** E:\game\.worktrees\m03-persistence

**Files:**
- Create: src/persistence/save_paths.hpp
- Create: src/persistence/save_paths.cpp
- Create: src/persistence/save_store.hpp
- Create: src/persistence/save_store.cpp
- Modify: src/persistence/CMakeLists.txt
- Create: tests/persistence/save_store_tests.cpp
- Create: tests/persistence/save_store_fault_tests.cpp
- Modify: tests/persistence/persistence_test_main.cpp
- Modify: tests/persistence/CMakeLists.txt

**Interfaces:**

    enum class SaveSlot : std::uint8_t { none, a, b };
    enum class SaveCommitState : std::uint8_t {
        committed, not_committed, indeterminate
    };
    enum class SaveLoadState : std::uint8_t {
        ready, empty, recovery_required, blocked
    };
    enum class SaveError : std::uint8_t {
        none,
        directory_unavailable,
        read_failed,
        write_failed,
        flush_failed,
        publish_failed,
        final_scan_failed,
        conflicting_slots,
        archive_failed,
        invalid_checkpoint
    };
    enum class SaveFaultPoint : std::uint8_t {
        before_temp_write,
        after_temp_write,
        after_temp_validation,
        before_publish,
        after_publish,
        final_scan_a,
        final_scan_b,
        before_archive
    };

    using SaveFaultHook = bool (*)(
        SaveFaultPoint point, void* context) noexcept;
    using UtcStampProvider = std::uint64_t (*)(
        void* context) noexcept;

    struct SaveStoreConfig final {
        std::filesystem::path directory{};
        SaveFaultHook fault_hook{};
        void* fault_context{};
        UtcStampProvider stamp_provider{};
        void* stamp_context{};
    };

    struct SaveLoadResult final {
        SaveLoadState state{SaveLoadState::blocked};
        SaveError error{SaveError::none};
        SaveSlot active_slot{SaveSlot::none};
        bool recovered{};
        dungeon::DungeonRunState checkpoint{};
    };

    struct SaveCommitResult final {
        SaveCommitState state{SaveCommitState::indeterminate};
        SaveError error{SaveError::none};
        SaveSlot active_slot{SaveSlot::none};
        dungeon::DungeonRunState verified_state{};
    };

    class SaveStore final {
    public:
        explicit SaveStore(SaveStoreConfig config);
        [[nodiscard]] SaveLoadResult load() noexcept;
        [[nodiscard]] SaveCommitResult commit(
            const dungeon::DungeonRunState& expected) noexcept;
        [[nodiscard]] SaveLoadResult archive_invalid_and_create(
            const dungeon::DungeonRunState& initial) noexcept;
    };

    [[nodiscard]] std::optional<std::filesystem::path>
    default_save_directory() noexcept;
    [[nodiscard]] std::optional<std::uint64_t>
    system_root_seed() noexcept;

- [ ] **Step 1: 写七个正常双槽 RED 用例**

save_store_tests.cpp 固定：

1. 空目录 load=empty，commit generation=1 写 A，重载 ready。
2. generation 2 写 B、generation 3 写 A，始终选最高代数。
3. A/B 同代数同载荷时固定 A 为 active，下一提交覆盖 B。
4. 一有效一缺失时创建缺失槽。
5. 一有效一损坏时先把损坏槽归档，再连续完成两次交替保存。
6. A/B 同代数但载荷不同，load 必须 recovery_required。
7. 有一个有效槽并残留任意 temp 时，temp 不参与最高代数选择，load 仍返回有效槽。

每个测试使用独立临时目录，结束前确认只有 run_a.sav、run_b.sav、允许的归档文件和零个临时文件。

- [ ] **Step 2: 写六个故障 RED 用例**

save_store_fault_tests.cpp 固定：

1. temp 写入前失败返回 not_committed，旧槽字节不变。
2. temp 校验后、publish 前失败返回 not_committed，旧槽仍唯一有效。
3. publish 后 final scan 故障：若重扫能证明 expected 有效则 committed，否则 indeterminate；绝不 not_committed。
4. 只有截断 temp、没有有效槽时 load=recovery_required，不得当成 empty 新建。
5. 两槽都坏且两个 temp 残留时 load=recovery_required；archive_invalid_and_create 必须同时归档四个文件，再创建 generation=1 新局。
6. archive 失败时 blocked，所有原损坏文件逐字节保留。

- [ ] **Step 3: 运行测试并观察 RED**

    .\scripts\Build.ps1 -Preset windows-msvc-core-debug
    .\out\build\windows-msvc-core-debug\bin\arpg_persistence_tests.exe

Expected: 编译失败，缺少 SaveStore。

- [ ] **Step 4: 实现路径、扫描和目标槽选择**

- default_save_directory 读取 LOCALAPPDATA，追加 InfiniteDungeon\save；环境变量缺失返回 nullopt。
- system_root_seed 使用 std::random_device 组合两个 32 位值；捕获异常并返回 nullopt。
- 文件名只允许 run_a.sav、run_b.sav、run_a.tmp、run_b.tmp 与带 UTC 数字后缀的 corrupt 归档。
- 两有效不同代数选高者；同代数同载荷选 A；同代数不同载荷 recovery_required。
- 单有效加损坏必须先归档损坏再进入 ready；归档失败 blocked。

- [ ] **Step 5: 实现发布三态**

顺序固定：

1. 根据 active/target 选择非当前槽。
2. 写 target.tmp，flush、close。
3. 回读 temp 并 decode。
4. 删除旧 target，但保留 active。
5. rename temp 到 target，这是 publish 边界。
6. 重新从磁盘扫描 A/B。
7. 扫描得到 expected 完整 state 且为最高代数，返回 committed 与 verified_state。
8. publish 前失败或能证明 target 无效且旧 active 唯一有效，返回 not_committed。
9. publish 后无法证明磁盘真值，返回 indeterminate。

fault hook 只能在列出的八个边界触发；生产 config 的 hook 为空。任何异常都在 noexcept 边界内转换为 SaveError。

- [ ] **Step 6: 运行 GREEN**

    .\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
    .\out\build\windows-msvc-core-debug\bin\arpg_persistence_tests.exe

Expected: Core-only 12/12；Persistence 21 cases、0 failures。

- [ ] **Step 7: 提交**

    git diff --check
    git add src/persistence tests/persistence
    git commit -m "feat: add atomic dual-slot dungeon saves"

---

### Task 8: 评审、合并会话与持久化，并增加端到端存档测试

**Integration worktree:** E:\game\.worktrees\m03-dungeon-rules

**Files for the integration test commit:**
- Create: tests/persistence/dungeon_save_integration_tests.cpp
- Modify: tests/persistence/persistence_test_main.cpp
- Modify: tests/persistence/CMakeLists.txt

**Consumes:**
- task/m03-dungeon-session
- task/m03-persistence

**Produces:** milestone/m03-dungeon-rules 上同时具备事务 Session 与双槽 Store，并由 6 个端到端用例锁定。

- [ ] **Step 1: 分别验证两个分支**

    E:\game\.worktrees\m03-dungeon-session\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
    E:\game\.worktrees\m03-persistence\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh

Expected:

- Session：Core 24、Dungeon 44、0 failures。
- Persistence：Core 24、Dungeon 35、Persistence 21、0 failures，Core-only 12/12。

- [ ] **Step 2: 对两个分支分别执行规格与代码质量评审**

Session 评审重点：

- committing 保留并冻结旧 combat。
- 只有 committed exact state 才销毁。
- not_committed 可重试且 pending 字段相同。
- indeterminate、错误 generation、错误 state 均 faulted。
- 已落盘后事件溢出也不会恢复旧房。
- R/load 不调用 room generation。

Persistence 评审重点：

- persistence 不链接 dungeon/combat。
- 96 字节 wire 与 CRC 覆盖范围精确。
- active 槽在 publish 前始终保留。
- publish 后不确定绝不返回 not_committed。
- 单损坏槽归档后可以继续交替保存。
- 所有异常在 noexcept API 内转为明确错误。

解决所有 Critical/Important，并在各自分支重跑 Step 1。

- [ ] **Step 3: 合并两个分支**

    git -C E:\game\.worktrees\m03-dungeon-rules merge --no-ff task/m03-dungeon-session -m "merge: integrate stage 3 transactional session"
    git -C E:\game\.worktrees\m03-dungeon-rules merge --no-ff task/m03-persistence -m "merge: integrate stage 3 persistence"
    E:\game\.worktrees\m03-dungeon-rules\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh

Expected: Core-only 12/12；Core 24、Combat 32、Dungeon 44、Persistence 21，全部 0 failures。

- [ ] **Step 4: 创建存档集成测试工作树**

    git -C E:\game\.worktrees\m03-dungeon-rules worktree add E:\game\.worktrees\m03-save-integration -b task/m03-save-integration milestone/m03-dungeon-rules

- [ ] **Step 5: 写六个端到端用例**

dungeon_save_integration_tests.cpp 使用真实临时目录、SaveStore 和 DungeonSession，固定六例：

1. generation=1 初始状态先成功保存，再构造 Session；加载 descriptor 字段完全相同。
2. 门 pending 经 Store committed 后回传 verified_state，Session 销毁旧房；重启直接加载下一房。
3. publish 前 fault 返回 not_committed，Session 回 awaiting_exit，磁盘与内存都仍是旧房。
4. publish 后回执丢失：当前 Session 进入 faulted；新进程 scan 到新房，不能回旧房。
5. 洞口 descent 保存后重启，depth+1、floor=1、bias 全 0，洞口/深渊结果不重骰。
6. R、关闭重开、重复 load 都保持同一 room seed、ecology、hole、abyss 和 generation。

桥接代码在测试和 Host 中必须采用同一映射：

    dungeon::TransitionSaveResult to_session_result(
        const persistence::SaveCommitResult& saved) noexcept {
        using persistence::SaveCommitState;
        dungeon::SaveDisposition disposition =
            dungeon::SaveDisposition::indeterminate;
        if (saved.state == SaveCommitState::committed) {
            disposition = dungeon::SaveDisposition::committed;
        } else if (saved.state == SaveCommitState::not_committed) {
            disposition = dungeon::SaveDisposition::not_committed;
        }
        return {
            disposition,
            saved.verified_state.commit_generation,
            saved.verified_state,
        };
    }

Persistence 守卫从 21 改为 27。

- [ ] **Step 6: 运行集成 GREEN**

    .\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
    .\out\build\windows-msvc-core-debug\bin\arpg_persistence_tests.exe

Expected: Core-only 12/12；Persistence 27 cases、0 failures。

- [ ] **Step 7: 提交、评审并合并**

    git diff --check
    git add tests/persistence
    git commit -m "test: verify transactional dungeon saves"

对六个端到端用例做规格评审；解决所有 Critical/Important 后：

    git -C E:\game\.worktrees\m03-dungeon-rules merge --no-ff task/m03-save-integration -m "merge: verify stage 3 save integration"
    E:\game\.worktrees\m03-dungeon-rules\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh

- [ ] **Step 8: 创建 raylib 工作树**

    git -C E:\game\.worktrees\m03-dungeon-rules worktree add E:\game\.worktrees\m03-raylib-host -b task/m03-raylib-host milestone/m03-dungeon-rules

---

### Task 9: 实现纯平台视图决策与启动参数

**Worktree:** E:\game\.worktrees\m03-raylib-host

**Files:**
- Create: src/platform/raylib/host_launch_options.hpp
- Create: src/platform/raylib/host_launch_options.cpp
- Modify: src/platform/raylib/dungeon_view_math.hpp
- Modify: src/platform/raylib/dungeon_view_math.cpp
- Modify: src/platform/raylib/CMakeLists.txt
- Create: tests/platform/host_launch_options_tests.cpp
- Modify: tests/platform/dungeon_view_math_tests.cpp
- Modify: tests/platform/platform_test_main.cpp
- Modify: tests/platform/CMakeLists.txt

**Interfaces:**

    struct Rgba8 final {
        std::uint8_t r{}, g{}, b{}, a{255};
    };

    struct DoorTheme final {
        dungeon::DungeonElement element{};
        const char* label{};
        const char* arrow{};
        Rgba8 frame{};
    };

    enum class HoleVisualMode : std::uint8_t {
        hidden, sealed, ready, busy, faulted
    };

    enum class SaveIndicator : std::uint8_t {
        none, saving, saved, recovered, error
    };

    [[nodiscard]] DoorTheme door_theme(
        dungeon::ExitDirection direction) noexcept;
    [[nodiscard]] HoleVisualMode hole_visual_mode(
        const dungeon::DungeonSnapshot& snapshot) noexcept;
    [[nodiscard]] bool player_in_hole_range(
        combat::Vec3 position,
        combat::Vec3 center,
        float radius) noexcept;
    [[nodiscard]] Rgba8 ecosystem_tint(
        dungeon::DungeonElement element) noexcept;
    [[nodiscard]] float abyss_pulse_alpha(
        float elapsed_seconds) noexcept;
    [[nodiscard]] const char* save_indicator_label(
        SaveIndicator indicator) noexcept;
    [[nodiscard]] bool recovery_requested(
        bool runtime_recovery_required,
        bool n_pressed) noexcept;

    struct HostLaunchOptions final {
        std::optional<std::filesystem::path> save_directory{};
        std::optional<std::uint64_t> new_run_seed{};
    };

    enum class HostArgumentError : std::uint8_t {
        none, unknown_option, missing_value, invalid_seed, duplicate_option
    };

    struct HostArgumentResult final {
        HostArgumentError error{HostArgumentError::none};
        HostLaunchOptions options{};
    };

    [[nodiscard]] HostArgumentResult parse_host_arguments(
        int argc, const char* const* argv) noexcept;

- [ ] **Step 1: 把 dungeon view suite 扩展到 12 cases**

保留原五例并增加六例：

1. 四门分别得到 FIRE/WATER/LIGHTNING/CHAOS、正确箭头和不同主题色。
2. 四生态 tint 稳定。
3. abyss pulse 对负时间、周期边界和超大时间保持 0..1。
4. HoleVisualMode：无洞 hidden；locked/combat sealed；cleared/awaiting ready；committing busy；faulted faulted。
5. player_in_hole_range 在中心、1.20 边界为真，边界外为假，负半径为假。
6. SaveIndicator 五个标签稳定；RoomPhase committing/faulted 标签稳定。
7. recovery_requested 只有 recovery_required 与 N 按下沿同时为真。

door_visual_mode 在 committing 保持 open 但禁止输入，在 faulted 使用 closed 错误样式；transitioning 或无活动房仍 hidden。

- [ ] **Step 2: 写五个启动参数 RED 用例**

host_launch_options_tests.cpp 固定：

1. 无参数得到两个 nullopt。
2. --seed 8 与 --seed 0x0000000000000008 都得到 8。
3. --save-dir 接受含空格 Windows 路径，并在 ChangeDirectory 之前转换为绝对路径。
4. 重复 --seed、重复 --save-dir、未知参数分别 duplicate/unknown。
5. 缺值、负数、溢出、尾随字符分别 invalid 或 missing。

Platform 阶段性守卫设为 24：

- combat view 3
- feedback 3
- key bindings 1
- dungeon view 12
- host options 5

- [ ] **Step 3: 运行测试并观察 RED**

    .\scripts\Build.ps1 -Preset windows-msvc-debug -Fresh
    .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe

Expected: 编译失败，缺少新 view 与 argument API。

- [ ] **Step 4: 实现纯视图函数**

颜色默认值固定为：

- FIRE frame {236, 92, 54, 255}
- WATER frame {64, 156, 236, 255}
- LIGHTNING frame {236, 218, 72, 255}
- CHAOS frame {154, 76, 210, 255}

ecosystem tint 使用同色系低饱和背景；abyss 只叠加紫红，不覆盖 ecology。player_in_hole_range 只计算 X/Y 平方距离，不调用 sqrt。

- [ ] **Step 5: 实现无分配参数解析**

- 使用 std::from_chars 解析十进制或 0x 十六进制种子。
- --seed 只表示“目标目录无有效存档时的新局种子”；解析层不覆盖任何存档。
- --save-dir 使用 std::filesystem::absolute 立即冻结调用方当前目录语义；Host 后续切换到应用目录不得改变存档位置。
- 不接受缩写、等号形式或位置参数。
- 捕获 filesystem::path 构造异常并返回 missing/invalid，不从 noexcept 逃逸。

- [ ] **Step 6: 运行 GREEN**

    .\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
    .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe

Expected: Full Debug 13/13 CTest；Platform 24 cases、0 failures。

- [ ] **Step 7: 提交**

    git diff --check
    git add src/platform/raylib tests/platform
    git commit -m "feat: add stage 3 dungeon presentation rules"

---

### Task 10: 接入 DungeonRuntime、双槽 Host 和 Stage 3 画面

**Worktree:** E:\game\.worktrees\m03-raylib-host

**Files:**
- Create: src/platform/raylib/dungeon_runtime.hpp
- Create: src/platform/raylib/dungeon_runtime.cpp
- Modify: src/platform/raylib/raylib_host.hpp
- Modify: src/platform/raylib/raylib_host.cpp
- Modify: src/platform/raylib/combat_renderer.hpp
- Modify: src/platform/raylib/combat_renderer.cpp
- Modify: src/platform/raylib/CMakeLists.txt
- Modify: src/app/main.cpp
- Create: tests/platform/dungeon_runtime_tests.cpp
- Modify: tests/platform/platform_test_main.cpp
- Modify: tests/platform/CMakeLists.txt

**Interfaces:**

    enum class DungeonRuntimeState : std::uint8_t {
        uninitialized,
        running,
        recovery_required,
        faulted
    };

    using RootSeedProvider = std::optional<std::uint64_t> (*)(
        void* context) noexcept;

    struct DungeonRuntimeConfig final {
        dungeon::DungeonRules rules{};
        persistence::SaveStoreConfig save{};
        std::optional<std::uint64_t> new_run_seed{};
        RootSeedProvider seed_provider{};
        void* seed_context{};
    };

    struct DungeonRenderStatus final {
        SaveIndicator indicator{SaveIndicator::none};
        persistence::SaveSlot active_slot{
            persistence::SaveSlot::none};
        persistence::SaveError error{
            persistence::SaveError::none};
    };

    class DungeonRuntime final {
    public:
        explicit DungeonRuntime(DungeonRuntimeConfig config);
        [[nodiscard]] bool initialize() noexcept;
        [[nodiscard]] DungeonRuntimeState state() const noexcept;
        [[nodiscard]] dungeon::DungeonSession* session() noexcept;
        [[nodiscard]] const dungeon::DungeonSession* session() const noexcept;
        [[nodiscard]] DungeonRenderStatus render_status() const noexcept;
        void service_pending_transition() noexcept;
        [[nodiscard]] bool recover_with_new_run() noexcept;
    };

CombatRenderer::draw 的最终签名固定为：

    void draw(
        const dungeon::DungeonSnapshot& previous,
        const dungeon::DungeonSnapshot& current,
        const DungeonRenderStatus& runtime_status,
        float interpolation_alpha,
        bool draw_debug,
        const CombatFeedback& feedback,
        bool audio_ready) noexcept;

DungeonRuntime 使用 optional<DungeonSession>::emplace 在启动时原地构造；不得返回、移动或复制含 BoundedQueue 的 DungeonSession。

- [ ] **Step 1: 写八个 DungeonRuntime RED 用例**

1. 空目录使用注入 seed=8，先持久化 generation=1，再暴露 running Session。
2. 已有有效存档时忽略 new_run_seed 覆盖值。
3. pending 经 committed Store 后 Session 进入 transitioning，indicator=saved。
4. publish 前 not_committed 后 Session 回 awaiting_exit，indicator=error，但可重试。
5. indeterminate 后 Session 和 Runtime 都 faulted，不可重新选择。
6. 双槽损坏 initialize=recovery_required；recover_with_new_run 归档后创建新局并 running。
7. 单槽损坏 initialize 后直接 running，render_status.indicator=recovered，且后续两次保存正常交替。
8. rules_version 非 1、阈值 10001、基础权重 0 分别 initialize=faulted；目录中不创建存档且 session=null。

Platform 最终守卫从 24 改为 32。

- [ ] **Step 2: 运行测试并观察 RED**

    .\scripts\Build.ps1 -Preset windows-msvc-debug
    .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe

Expected: 编译失败，缺少 DungeonRuntime。

- [ ] **Step 3: 实现 Runtime 启动与三态桥接**

initialize 顺序：

1. SaveStore::load。
2. ready：直接 emplace Session，忽略 --seed。
3. empty：使用 --seed，否则 seed_provider，否则 system_root_seed。
4. make_initial_run_state 后先 Store::commit；只有 committed 才 emplace Session。
5. recovery_required：不创建 Session。
6. blocked 或种子失败：faulted。

service_pending_transition 取 pending、显示 saving、同步 Store::commit，再把 committed/not_committed/indeterminate 和 verified_state 回传 Session。indeterminate 不得自动重试。

recover_with_new_run 使用与 initialize 相同的种子优先级，调用 make_initial_run_state，再调用 archive_invalid_and_create；只有返回 ready 且 generation=1 才 emplace 新 Session。DungeonRenderStatus 每次 load/commit/recover 后同步更新。若 Session 自身进入 RoomPhase::faulted，DungeonRuntime::state 也必须返回 faulted。

- [ ] **Step 4: 把 Runtime 接入 Host**

RaylibHostConfig 改为：

    enum class HostExitCode : int {
        success = 0,
        window_initialization_failed = 1,
        invalid_arguments = 2,
        save_initialization_failed = 3
    };

    struct RaylibHostConfig final {
        int window_width{1280};
        int window_height{720};
        const char* window_title{
            "Infinite Dungeon - Stage 3 Dungeon Rules"};
        std::optional<std::filesystem::path> save_directory{};
        std::optional<std::uint64_t> new_run_seed{};
    };

main 改为 main(int argc, char** argv)，解析失败打印明确错误并返回 HostExitCode::invalid_arguments。Host 未指定 save directory 时调用 default_save_directory。run_raylib_host 的 noexcept 边界必须捕获 DungeonRuntime、filesystem 和字符串构造异常并返回 save_initialization_failed，不允许异常越过 C ABI 入口。

窗口启动后：

- recovery_required 时只绘制错误说明和 N 确认，不创建游戏 Session。
- 每帧用 recovery_requested(runtime.state()==recovery_required, IsKeyPressed(KEY_N)) 判定；为真时必须调用 runtime.recover_with_new_run()，成功后立即初始化 previous/current snapshot，失败则保持恢复画面。
- running 时保持固定步；每个 session.tick 后调用 runtime.service_pending_transition，再刷新 snapshot 并再次 drain 事件。
- E 按下沿读取当前玩家位置，用中心 {0,2,0} 与半径 1.20 计算 in range，再调用 request_descent。
- R 在 committing/faulted 时由 Session 拒绝。
- Esc 和窗口关闭都正常 shutdown audio/window。

- [ ] **Step 5: 绘制 Stage 3 房间**

combat_renderer.cpp 按固定顺序绘制：

1. ecology 低饱和远景、地面和网格。
2. abyss 紫红脉冲与边缘暗化。
3. 四元素门框、标签和箭头。
4. 有洞房的洞口；sealed 暗色，ready 发光，busy 显示 SAVING。
5. combat actors、effects 和 debug volumes。
6. HUD 与 transition overlay。

普通 HUD：

- Depth、Floor Room、Global Room。
- Ecology。
- F/W/L/C bias。
- ABYSS。
- Hole NONE/SEALED/READY。
- Autosave SAVING/SAVED/RECOVERED/ERROR。
- 操作行加入 E Descend。

Host 每帧把 runtime.render_status() 作为第三个参数传给 CombatRenderer::draw。F1 追加 root seed、room seed、已保存的 ecology/hole/abyss 结果、generation、runtime_status.active_slot、runtime_status.error 与 DungeonFault。F12 文件名改为 stage3-dungeon-rules.png，并且只在 EndDrawing 完成后调用 TakeScreenshot。

- [ ] **Step 6: 运行 GREEN 与静态产物检查**

    .\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
    .\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
    .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
    .\out\build\windows-msvc-release\bin\arpg_platform_tests.exe
    Get-Item .\out\build\windows-msvc-release\bin\arpg_game.exe
    @(Get-ChildItem .\out\build\windows-msvc-release -Recurse -Filter "raylib*.dll" -File).Count

Expected: Debug/Release 各 13/13；Platform 各 32 cases、0 failures；EXE 存在；raylib DLL 数量 0。

- [ ] **Step 7: 可见冒烟**

使用全新隔离目录：

    .\out\build\windows-msvc-debug\bin\arpg_game.exe --seed 0x8 --save-dir .\out\stage3-smoke

验证首房为 Depth 1 / Floor Room 1、WATER ecology、四偏向 0、四元素门关闭；清房后门开启。关闭并重开同命令，必须加载同一房间而不是用 --seed 覆盖已有存档。Esc 后确认：

    @(Get-Process arpg_game -ErrorAction SilentlyContinue).Count

Expected: 0。

- [ ] **Step 8: 提交**

    git diff --check
    git add src/platform/raylib src/app tests/platform
    git commit -m "feat: present persistent stage 3 dungeon rules"

---

### Task 11: 合并、全量验证、固定路线可视验收与交付

**Integration worktree:** E:\game\.worktrees\m03-dungeon-rules

- [ ] **Step 1: 评审 raylib 分支**

规格评审覆盖：

- 四门颜色、标签、方向与偏向。
- 洞口进房可见、清房前 sealed、清房后 E。
- abyss 叠加但不遮挡生态与演员。
- Host 先保存初始房再构造 Session。
- 已有存档忽略 --seed。
- 单槽恢复和双槽 N 流程。
- committing/faulted 不接受游戏输入。

代码质量评审覆盖：

- Host 没有复制/移动 DungeonSession。
- persistence 没有通过 raylib target 反向污染 dungeon/combat。
- 渲染帧不消费任何地下城 RNG。
- screenshot 在 EndDrawing 后执行。
- 所有 TraceLog/TextFormat 参数类型正确。
- 画面与存档路径异常不逃出 noexcept。

解决所有 Critical/Important 后重跑 Task 10 Step 6。

- [ ] **Step 2: 合并 Host**

    git -C E:\game\.worktrees\m03-dungeon-rules merge --no-ff task/m03-raylib-host -m "merge: integrate stage 3 raylib host"

- [ ] **Step 3: 运行全部 Fresh 门**

    Set-Location E:\game\.worktrees\m03-dungeon-rules
    .\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
    .\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
    .\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
    ctest.exe --test-dir .\out\build\windows-msvc-core-debug -N
    ctest.exe --test-dir .\out\build\windows-msvc-debug -N
    ctest.exe --test-dir .\out\build\windows-msvc-release -N

Expected:

- Core-only 12/12。
- Debug 13/13。
- Release 13/13。

- [ ] **Step 4: 运行所有直接测试程序**

Debug 与 Release 都运行：

    .\out\build\windows-msvc-debug\bin\arpg_core_tests.exe
    .\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
    .\out\build\windows-msvc-debug\bin\arpg_dungeon_tests.exe
    .\out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe
    .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
    .\out\build\windows-msvc-release\bin\arpg_core_tests.exe
    .\out\build\windows-msvc-release\bin\arpg_combat_tests.exe
    .\out\build\windows-msvc-release\bin\arpg_dungeon_tests.exe
    .\out\build\windows-msvc-release\bin\arpg_persistence_tests.exe
    .\out\build\windows-msvc-release\bin\arpg_platform_tests.exe

Expected case guards：

- Core 24。
- Combat 32。
- Dungeon 44。
- Persistence 27。
- Platform 32。
- 全部 0 failures。

- [ ] **Step 5: 静态架构与产物检查**

    function Assert-NoRgMatch {
        param([string[]]$RgArgs)
        $output = & rg.exe @RgArgs
        $code = $LASTEXITCODE
        if ($code -eq 0) {
            throw "Forbidden dependency match: $($output -join '; ')"
        }
        if ($code -gt 1) {
            throw "rg failed with exit code $code"
        }
    }
    Assert-NoRgMatch -RgArgs @(
        '-n', '-i',
        '#\s*include\s*[<"](raylib|raymath|rlgl|raylib-cpp)',
        'src/core', 'src/combat', 'src/dungeon', 'src/persistence')
    Assert-NoRgMatch -RgArgs @(
        '-n', '#\s*include\s*[<"](dungeon|persistence)/',
        'src/combat')
    Assert-NoRgMatch -RgArgs @(
        '-n', '#\s*include\s*[<"]persistence/',
        'src/dungeon')
    Assert-NoRgMatch -RgArgs @(
        '-n', '-P',
        '#\s*include\s*[<"]dungeon/(?!dungeon_checkpoint\.hpp)',
        'src/persistence')
    Get-Item .\out\build\windows-msvc-release\bin\arpg_game.exe
    Get-Item .\out\build\windows-msvc-release\lib\raylib.lib
    @(Get-ChildItem .\out\build\windows-msvc-release -Recurse -Filter "raylib*.dll" -File).Count
    git diff --check
    git status --short --branch

Expected: 四个 source scan 无非法匹配；EXE 与 raylib.lib 存在；DLL 数量 0；工作树干净。

- [ ] **Step 6: 使用固定种子完成十房可视矩阵**

使用空目录 E:\game\.worktrees\m03-dungeon-rules\out\acceptance\stage3-seed-8，启动：

    .\out\build\windows-msvc-release\bin\arpg_game.exe --seed 0x0000000000000008 --save-dir .\out\acceptance\stage3-seed-8

严格按以下路线清房并提交：

| 可见房号 | Global index | Depth/Floor | Bias F/W/L/C | Ecology | 特殊 | 提交 |
| --- | --- | --- | --- | --- | --- | --- |
| 1 | 0 | 1/1 | 0/0/0/0 | WATER | 无 | 上门 |
| 2 | 1 | 1/2 | 1/0/0/0 | FIRE | 无 | 下门 |
| 3 | 2 | 1/3 | 1/1/0/0 | FIRE | ABYSS + HOLE | E 下坠 |
| 4 | 3 | 2/1 | 0/0/0/0 | LIGHTNING | 无 | 右门 |
| 5 | 4 | 2/2 | 0/0/0/1 | FIRE | 无 | 上门 |
| 6 | 5 | 2/3 | 1/0/0/1 | CHAOS | 无 | 下门 |
| 7 | 6 | 2/4 | 1/1/0/1 | LIGHTNING | HOLE | 左门 |
| 8 | 7 | 2/5 | 1/1/1/1 | CHAOS | 无 | 右门 |
| 9 | 8 | 2/6 | 1/1/1/2 | FIRE | 无 | 上门 |
| 10 | 9 | 2/7 | 2/1/1/2 | WATER | ABYSS | 停留验收 |

每房核对 F1 seed、ecology/hole/abyss 结果、generation 和 overflow。第 3 房验证洞口在战斗中 sealed、清房后 ready，E 后四 bias 清零。第 7 房验证存在洞时仍可选择元素门。第 10 房按 F12，确认生成 stage3-dungeon-rules.png。

- [ ] **Step 7: 验证防重骰与恢复**

在第 4 房关闭游戏并重启同一命令：

- --seed 必须被已有存档忽略。
- 恢复同一 room seed、ecology、hole、abyss、depth、floor、bias。
- R 后上述字段全部不变。

关闭进程后，用 PowerShell 验证路径并复制两个隔离副本：

    $acceptanceRoot = (Resolve-Path -LiteralPath .\out\acceptance).Path
    $source = (Resolve-Path -LiteralPath .\out\acceptance\stage3-seed-8).Path
    $single = [IO.Path]::GetFullPath(
        (Join-Path $acceptanceRoot "single-corrupt"))
    $double = [IO.Path]::GetFullPath(
        (Join-Path $acceptanceRoot "double-corrupt"))
    foreach ($target in @($single, $double)) {
        if (-not $target.StartsWith(
                $acceptanceRoot + [IO.Path]::DirectorySeparatorChar,
                [StringComparison]::OrdinalIgnoreCase)) {
            throw "Unsafe fixture path: $target"
        }
        if (Test-Path -LiteralPath $target) {
            throw "Fixture already exists: $target"
        }
        Copy-Item -LiteralPath $source -Destination $target -Recurse
    }

只破坏副本中的固定槽：

    $bad = [byte[]](0x42, 0x41, 0x44, 0x53, 0x41, 0x56, 0x45)
    [IO.File]::WriteAllBytes(
        (Join-Path $single "run_b.sav"), $bad)
    [IO.File]::WriteAllBytes(
        (Join-Path $double "run_a.sav"), $bad)
    [IO.File]::WriteAllBytes(
        (Join-Path $double "run_b.sav"), $bad)

启动 single-corrupt：

    .\out\build\windows-msvc-release\bin\arpg_game.exe --seed 0x8 --save-dir .\out\acceptance\single-corrupt

Expected: 显示 RECOVERED；随后完成两次出口，F1 显示 A/B 继续交替且 generation 连续增加。

启动 double-corrupt：

    .\out\build\windows-msvc-release\bin\arpg_game.exe --seed 0x8 --save-dir .\out\acceptance\double-corrupt

Expected: 进入阻塞恢复画面；按 N 后 generation=1 新局可运行，并且：

    @(Get-ChildItem -LiteralPath .\out\acceptance\double-corrupt -Filter "*.corrupt.*" -File).Count

Expected: 至少 2。

不得操作 %LOCALAPPDATA% 中的正式存档。

- [ ] **Step 8: 验证退出路径和进程**

分别用 Esc 与窗口关闭按钮退出一次：

    @(Get-Process arpg_game -ErrorAction SilentlyContinue).Count

Expected: 两次均为 0。

- [ ] **Step 9: 最终跨分支评审与交付**

评审从设计提交 0e5fcf692037e69238294b8930da393ff0ce54c0 到里程碑 HEAD 的所有变更。任何代码修复必须增加 RED 回归，并重跑 Steps 3–8。

最终确认：

    git -C E:\game rev-parse main
    git rev-parse milestone/m03-dungeon-rules
    git log --oneline --decorate --graph --max-count 30

Expected: main 仍为 bb74bd48b42435d798b2b6ce9968d1d2d3f8a9da；M03 工作树干净。保留所有 M03 工作树，等待用户选择本地合并、保留分支或其他集成动作；不得自行 merge main、push、建 PR 或开始 Stage 4。

## Plan Self-Review Checklist

- [ ] 设计第 1–13 节每项要求都映射到一个任务和一个可执行断言。
- [ ] persistence 仅包含 dungeon_checkpoint.hpp，且不链接 arpg_dungeon/arpg_combat。
- [ ] 初始 generation=1 在 progression、codec、store、runtime 和验收中一致。
- [ ] DungeonRunState、RoomDescriptor、PendingTransition、TransitionSaveResult 的字段名在所有任务一致。
- [ ] committed/not_committed/indeterminate 的 publish 边界和 Session 行为一致。
- [ ] 两槽同代数同载荷固定 A active/B target；同代数不同载荷阻塞。
- [ ] RoomDescriptor 不含 CombatLabConfig；加载与 R 不调用 room generation。
- [ ] RNG 域、组合顺序、黄金向量、万分阈值和验收 seed=8 全部一致。
- [ ] Core 24、Combat 32、Dungeon 44、Persistence 27、Platform 32 的守卫与文件中新增 suite 数量一致。
- [ ] Core-only 12、Full 13 的 CTest 数量与 CMake 中测试项一致。
- [ ] 1000 房脚本显式确认每个 pending，保持零分配和零溢出。
- [ ] 没有正式怪物、奖励、经验、装备或其他 Stage 4+ 内容。
- [ ] 使用由相邻字符串拼接生成的禁止词列表扫描计划；预期无未完成式指令。
- [ ] 运行 git diff --check；预期无输出。

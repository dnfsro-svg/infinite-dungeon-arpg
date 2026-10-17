# Stage 16 掉落物、功能货币与强化系统 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use `superpowers:subagent-driven-development` or `superpowers:executing-plans` to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在 raylib 6.0 无限地下城中交付十八种装备底材、十四种材料、可持久化掉落和材料袋、九种改造货币、三合一、无限强化与强化券。

**Architecture:** 新的 `items` 纯逻辑层负责多固有属性、材料目录、装备改造和强化数学；`dungeon` 层只负责确定性掉落、所有权和原子事务；`platform/raylib` 层只负责地面可视化、材料袋和输入。存档升级为V7，V6读入时所有装备强化为零、材料袋为空。

**Tech Stack:** C++17、raylib 6.0、CMake/Ninja/MSVC、既有固定步模拟与双槽二进制存档。

## Global Constraints

- 保持单人、低资源、固定容量和无每帧动态分配；材料地面池固定为400，装备地面池仍为192。
- 不增加金币、交易、鉴定、Boss、装备槽位、插槽或锁词缀功能。
- 旧底材ID `1..6` 保持稳定；新增底材ID `7..18` 与材料ID `1..14` 终身稳定。
- 所有掉落和改造使用新的确定性随机域，不能改变既有房间、怪物、装备和深渊装备奖励随机序列。
- 除普通强化失败外，材料操作不销毁装备；当前强化等级`>=12`的普通强化失败才销毁装备。
- 每个任务先写失败测试，再做最小实现，再运行任务相关测试并提交。
- 既有 `stage9.formal_game.capture_after_present` 在未改代码的V6基线中稳定栈溢出；记录为基线异常，不能把它当作Stage 16回归。

---

## 文件结构

| 路径 | 职责 |
|---|---|
| `src/items/item_types.hpp` | `ItemInstance`强化等级、材料数量和稳定枚举 |
| `src/items/item_catalog.*` | 18种底材、多固有效果、目录校验 |
| `src/items/item_generation.*` | 三底材选取和三合一输出 |
| `src/items/item_crafting.*` | 九种货币的纯函数改造、强化概率和强化券 |
| `src/items/material_catalog.*` | 十四种材料的名称、层数门槛和权重 |
| `src/dungeon/material_loot.*` | 材料/券掉落、深渊奖励、固定材料地面池操作 |
| `src/dungeon/dungeon_session.*` | 材料拾取、清房吸取、改造/三合一/强化事务 |
| `src/dungeon/dungeon_types.hpp` | 材料快照、请求和待保存类型 |
| `src/persistence/checkpoint_codec.*` | V7编码、V6迁移、材料与强化字段 |
| `src/platform/raylib/material_loot_view.*` | 材料标签、颜色、光柱和合并拾取提示 |
| `src/platform/raylib/material_bag_renderer.*` | 材料袋、选择、合法目标高亮和危险确认 |
| `tests/items/*` | 目录、改造、三合一和强化纯逻辑测试 |
| `tests/dungeon/*` | 掉落、事务、清房、死亡和确定性测试 |
| `tests/persistence/*` | V7往返、V6迁移和中断原子性测试 |
| `tests/platform/*` | 材料袋布局、点击、地面表现和HUD提示测试 |

## Task 1: 18种底材与多固有效果

**Files:**
- Modify: `src/items/item_types.hpp`
- Modify: `src/items/item_catalog.hpp`
- Modify: `src/items/item_catalog.cpp`
- Modify: `src/items/item_modifiers.cpp`
- Modify: `src/items/item_generation.cpp`
- Modify: `tests/items/item_catalog_tests.cpp`
- Modify: `tests/items/item_generation_tests.cpp`

**Interfaces:**
- Produce `BaseEffect`、`BaseDefinition::effects`和`base_ids_for_slot(ItemSlot)`。
- Existing `item_modifiers_for_equipment()` must apply every active base effect exactly once.

- [ ] **Step 1: Write failing catalog tests**

```cpp
ARPG_REQUIRE(items::base_ids_for_slot(ItemSlot::weapon).size() == 3U);
ARPG_REQUIRE(items::base_definition(7U)->slot == ItemSlot::weapon);
ARPG_REQUIRE(items::base_definition(18U)->slot == ItemSlot::accessory);
ARPG_REQUIRE(items::validate_catalog());
```

- [ ] **Step 2: Run the isolated failure**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^items.units$' --output-on-failure`

Expected: compile failure because `base_ids_for_slot` and multi-effect members do not exist.

- [ ] **Step 3: Implement fixed-size base effects and all 18 stable definitions**

```cpp
struct BaseEffect final {
    ItemEffectKind effect{};
    modifiers::StatId stat{modifiers::StatId::count};
    modifiers::ModifierOperation operation{modifiers::ModifierOperation::flat};
    std::array<std::int32_t, 8> values{};
};

struct BaseDefinition final {
    std::uint8_t id{};
    std::string_view name{};
    ItemSlot slot{};
    std::array<BaseEffect, 3> effects{};
    std::uint8_t effect_count{};
};

[[nodiscard]] std::array<std::uint8_t, 3> base_ids_for_slot(ItemSlot slot) noexcept;
```

Populate values exactly as `2026-07-20-stage16-loot-currency-reinforcement-design.md` sections 3.2–3.4 specify. Replace the old one-base-per-slot validation with exactly three bases per slot, nonzero effect counts, valid sentinels and stable ID checks. Derive the selected base from a new item-base random domain after slot selection.

- [ ] **Step 4: Run catalog and generation tests**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^(items.units|modifier.units)$' --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/items tests/items
git commit -m "feat: add eighteen equipment bases"
```

## Task 2: 强化字段、数学与材料目录纯逻辑

**Files:**
- Modify: `src/items/item_types.hpp`
- Create: `src/items/material_catalog.hpp`
- Create: `src/items/material_catalog.cpp`
- Create: `src/items/item_crafting.hpp`
- Create: `src/items/item_crafting.cpp`
- Modify: `src/items/CMakeLists.txt`
- Create: `tests/items/item_crafting_tests.cpp`
- Modify: `tests/items/CMakeLists.txt`

**Interfaces:**
- Produce `enum class MaterialId : uint8_t` with values `transmute..coupon_15` and `count`.
- Produce `CraftResult craft_item(CraftRequest)`, `ReinforcementPreview reinforcement_preview(...)`, and `apply_coupon(...)`.

- [ ] **Step 1: Write failing pure-logic tests**

```cpp
ARPG_REQUIRE(reinforcement_success_chance_bp(13U) == 6000U);
ARPG_REQUIRE(reinforcement_success_chance_bp(14U) == 5700U);
ARPG_REQUIRE(reinforcement_failure(11U) == ReinforcementFailure::reset_zero);
ARPG_REQUIRE(reinforcement_failure(12U) == ReinforcementFailure::destroy);
ARPG_REQUIRE(apply_coupon(item_plus_8, MaterialId::coupon_12)->reinforcement == 12U);
```

- [ ] **Step 2: Run the isolated failure**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^items.units$' --output-on-failure`

Expected: compile failure for missing material and crafting interfaces.

- [ ] **Step 3: Implement materials and exact reinforcement formulas**

```cpp
enum class MaterialId : std::uint8_t {
    transmute, augment, regal, chaos, exalt, annul, divine, scour, directed,
    reinforcement_stone, coupon_6, coupon_9, coupon_12, coupon_15, count,
};

[[nodiscard]] std::uint16_t reinforcement_success_chance_bp(std::uint32_t target) noexcept;
[[nodiscard]] ReinforcementFailure reinforcement_failure(std::uint32_t current) noexcept;
[[nodiscard]] std::int64_t reinforced_base_value(std::int64_t base, std::uint32_t level) noexcept;
[[nodiscard]] std::int32_t reinforced_accessory_bonus_bp(std::uint32_t level) noexcept;
```

Use integer fixed-point rounding (nearest half up), minimum 10 basis points for success, and saturating arithmetic. Add `uint32_t reinforcement{}` to `ItemInstance`, keep it trivially copyable, and update its static layout tests.

- [ ] **Step 4: Implement nine deterministic craft operations**

`CraftRequest` must carry root seed, operation nonce, item and optional directed category. It must reject illegal targets without consuming anything. Implement normal-to-magic, magic augment, magic-to-rare, rare full reroll, rare add, annulling with rarity downgrade, divine value reroll, scour and directed replacement exactly per design sections 6.1–6.2.

- [ ] **Step 5: Run pure item tests**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^items.units$' --output-on-failure`

Expected: PASS, including deterministic seed replay and overflow saturation cases.

- [ ] **Step 6: Commit**

```powershell
git add src/items tests/items
git commit -m "feat: add crafting materials and reinforcement rules"
```

## Task 3: V7存档与旧档迁移

**Files:**
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/save_slot.cpp`
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `tests/persistence/checkpoint_codec_tests.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`
- Modify: `tests/persistence/v5_golden_fixture.hpp`

**Interfaces:**
- Produce V7 checkpoint constants and `DecodeResult::migrated` behavior for V1–V6.
- Persist `ItemInstance::reinforcement`, fourteen `uint64_t` material counts and a fourteen-bit discovery mask.

- [ ] **Step 1: Write V7 round-trip and V6 migration tests**

```cpp
ARPG_REQUIRE(decoded.state.item_ownership.items[0].reinforcement == 15U);
ARPG_REQUIRE(decoded.state.item_ownership.materials[material_index(MaterialId::chaos)] == 42U);
ARPG_REQUIRE(v6_decoded.migrated);
ARPG_REQUIRE(v6_decoded.state.item_ownership.items[0].reinforcement == 0U);
```

- [ ] **Step 2: Run the persistence failure**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^persistence.units$' --output-on-failure`

Expected: compile or assertion failure because V7 fields are absent.

- [ ] **Step 3: Implement versioned encoding without rewriting old decoders**

Add explicit V7 payload/record constants rather than changing V6 byte counts. Decode V6 using its old record size, initialize reinforcement/materials/discovery to zero, then publish a valid V7 in-memory state. Validate counters and all material IDs before accepting V7 input.

- [ ] **Step 4: Run persistence tests**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^(persistence.units|settings.units)$' --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/persistence src/dungeon/dungeon_checkpoint.hpp tests/persistence
git commit -m "feat: persist materials and reinforcement in v7 saves"
```

## Task 4: 材料地面池、确定性掉落和清房吸取

**Files:**
- Create: `src/dungeon/material_loot.hpp`
- Create: `src/dungeon/material_loot.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_loot_drop_tests.cpp`
- Create: `tests/dungeon/dungeon_material_loot_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`

**Interfaces:**
- Produce `kGroundMaterialCapacity = 400`, `GroundMaterial`, `GroundMaterialSnapshot`, `material_drop_chance_bp`, `roll_material_drop` and `roll_coupon_drop`.
- Extend `DungeonSnapshot` with fixed material snapshots/count and material pickup receipt.

- [ ] **Step 1: Write deterministic drop tests**

```cpp
ARPG_REQUIRE(material_drop_chance_bp(0U) == 800U);
ARPG_REQUIRE(material_drop_chance_bp(17U) == 2500U);
ARPG_REQUIRE(material_drop_chance_bp(99U) == 3500U);
ARPG_REQUIRE(coupon_eligible(MaterialId::coupon_12, 60U, 14U));
```

- [ ] **Step 2: Run the dungeon failure**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^dungeon.units$' --output-on-failure`

Expected: compile failure for material symbols.

- [ ] **Step 3: Implement independent material pool and drop domains**

Use separate RNG domain constants for material chance, material type, coupon tier and abyss material rewards. A monster can emit at most one common material and one coupon. Keep item `claimed_drop_bits` untouched; introduce material claim/rolled bits sized for the fixed pool. Generate low/medium/high abyss material counts 1/2/3 with the exact normal and abyss weights from the design.

- [ ] **Step 4: Implement pickup and room-clear vacuum as atomic item-state saves**

```cpp
[[nodiscard]] RequestResult request_material_pickup(std::uint16_t ordinal) noexcept;
void vacuum_room_materials() noexcept;
```

Only nearby materials are auto-picked during combat. `prepare_room_clear()` must append the remaining material counts to the next state before its clearing save transaction. Death clears transient ground material state but never already committed counts.

- [ ] **Step 5: Run dungeon material tests**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^(dungeon.units|stage10.validation_fixture.real_abyss_transactions)$' --output-on-failure`

Expected: PASS.

- [ ] **Step 6: Commit**

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: add deterministic material drops and vacuum pickup"
```

## Task 5: 材料袋、标签与拾取反馈

**Files:**
- Create: `src/platform/raylib/material_loot_view.hpp`
- Create: `src/platform/raylib/material_loot_view.cpp`
- Create: `src/platform/raylib/material_bag_renderer.hpp`
- Create: `src/platform/raylib/material_bag_renderer.cpp`
- Modify: `src/platform/raylib/inventory_renderer.*`
- Modify: `src/platform/raylib/inventory_view_math.*`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Create: `tests/platform/material_loot_view_tests.cpp`
- Create: `tests/platform/material_bag_renderer_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Produce `material_color(MaterialId)`, `material_label(MaterialId)`, `MaterialBagRenderer` and `selected_material()`.
- Existing loot filter must not be consulted for materials.

- [ ] **Step 1: Write renderer/view-math tests**

```cpp
ARPG_REQUIRE(material_label(MaterialId::coupon_12) == "+12强化券");
ARPG_REQUIRE(material_is_emphasized(MaterialId::directed));
ARPG_REQUIRE(material_bag_layout(1280, 720).contains_all_slots());
```

- [ ] **Step 2: Run the platform failure**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^platform.units$' --output-on-failure`

Expected: compile failure for material view symbols.

- [ ] **Step 3: Draw ground materials and material bag**

Render material labels independently from `GroundItem` filtering. Show coupon face values, stronger beam/color for exalt/directed/+12/+15, and aggregate repeated pickup notifications by material ID and count. Add a 14-slot bag to the paused inventory without changing equipment-grid capacity.

- [ ] **Step 4: Run platform tests**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^platform.units$' --output-on-failure`

Expected: PASS.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render material drops and material bag"
```

## Task 6: 改造货币与三合一事务

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `src/platform/raylib/material_bag_renderer.cpp`
- Modify: `src/platform/raylib/inventory_renderer.cpp`
- Modify: `tests/dungeon/dungeon_item_transaction_tests.cpp`
- Create: `tests/dungeon/dungeon_crafting_transaction_tests.cpp`

**Interfaces:**
- Produce `request_craft(MaterialId, uint64_t item_id, DirectedCategory)` and an extended `request_recipe` that rejects equipped inputs and resets output reinforcement.

- [ ] **Step 1: Write transaction tests**

```cpp
ARPG_REQUIRE(session.request_craft(MaterialId::chaos, rare_id) == RequestResult::accepted);
ARPG_REQUIRE(pending->next_state.item_ownership.materials[chaos_index] == before - 1U);
ARPG_REQUIRE(session.request_craft(MaterialId::augment, rare_id) == RequestResult::rejected);
ARPG_REQUIRE(recipe_output.reinforcement == 0U);
```

- [ ] **Step 2: Run the failure**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^dungeon.units$' --output-on-failure`

Expected: compile failure for `request_craft` or failed new assertions.

- [ ] **Step 3: Implement atomic request preparation**

Copy run state through the existing reusable pending-save path, call the pure `items::craft_item`, decrement one material only after it succeeds, recompute the equipped build if the target is equipped, and submit `PendingSaveKind::craft`. Invalid target/category/insufficient count returns `rejected` with no mutation. Preserve item IDs for craft operations.

- [ ] **Step 4: Extend three-to-one contract**

Require all inputs to be un-equipped and match exact `base_id` plus rarity. Use the existing deterministic recipe seed ordered by IDs, calculate floor average level, generate output with reinforcement zero, and display a warning if any input has reinforcement above zero.

- [ ] **Step 5: Run dungeon and item tests**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^(items.units|dungeon.units)$' --output-on-failure`

Expected: PASS.

- [ ] **Step 6: Commit**

```powershell
git add src/items src/dungeon src/platform/raylib tests/items tests/dungeon
git commit -m "feat: add item currency crafting and recipe safeguards"
```

## Task 7: 强化石、强化券与销毁风险

**Files:**
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_session.hpp`
- Modify: `src/dungeon/dungeon_session.cpp`
- Modify: `src/platform/raylib/material_bag_renderer.*`
- Modify: `src/platform/raylib/inventory_renderer.*`
- Modify: `src/platform/raylib/inventory_view_math.*`
- Modify: `tests/dungeon/dungeon_crafting_transaction_tests.cpp`
- Modify: `tests/platform/material_bag_renderer_tests.cpp`

**Interfaces:**
- Produce `request_reinforcement(uint64_t item_id)` and `request_coupon(MaterialId, uint64_t item_id)`.
- Produce `ReinforcementReceipt` for UI confirmation and result feedback.

- [ ] **Step 1: Write failure-boundary tests**

```cpp
ARPG_REQUIRE(after_failed_plus_7.reinforcement == 6U);
ARPG_REQUIRE(after_failed_plus_10.reinforcement == 0U);
ARPG_REQUIRE(!contains_item(after_failed_plus_12, equipped_id));
ARPG_REQUIRE(after_coupon_15.reinforcement == 15U);
```

- [ ] **Step 2: Run the failure**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^(items.units|dungeon.units|platform.units)$' --output-on-failure`

Expected: failed assertions until transaction logic exists.

- [ ] **Step 3: Implement deterministic ordinary reinforcement**

Consume exactly one reinforcement stone per valid attempt. Derive success from a dedicated reinforcement domain containing root seed, item ID, current reinforcement and transaction nonce. On failure apply the exact current-level bands; on destroy remove the item, clear any matching equipped ID, and rebuild player combat state in the same pending transaction.

- [ ] **Step 4: Implement coupon application and UI confirmation**

Coupons consume one count and set reinforcement directly to their face only when current is lower. Material bag must require a confirmation modal before an ordinary stone attempt when current is at least12; normal currency operations remain one click after target selection.

- [ ] **Step 5: Run focused tests**

Run: `ctest --test-dir out/build/windows-msvc-debug -R '^(items.units|dungeon.units|platform.units)$' --output-on-failure`

Expected: PASS.

- [ ] **Step 6: Commit**

```powershell
git add src/items src/dungeon src/platform/raylib tests/items tests/dungeon tests/platform
git commit -m "feat: add equipment reinforcement and coupons"
```

## Task 8: 平衡模拟、真实验收和文档

**Files:**
- Create: `tests/dungeon/stage16_loot_reinforcement_simulation.cpp`
- Create: `tests/platform/stage16_loot_reinforcement_game_validation.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `docs/validation/stage16-loot-currency-reinforcement.md`
- Modify: `README.md`

**Interfaces:**
- Produce CTest labels `stage16`, `validation`, `raylib` and deterministic simulation evidence.

- [ ] **Step 1: Write simulation acceptance checks**

```cpp
ARPG_REQUIRE(simulated_material_rate_bp(0U) == 800U);
ARPG_REQUIRE(simulated_material_rate_bp(27U) == 3500U);
ARPG_REQUIRE(replay_a.materials == replay_b.materials);
ARPG_REQUIRE(replay_a.existing_item_stream == replay_b.existing_item_stream);
```

- [ ] **Step 2: Implement million-roll and long-run tests**

Run at least one million deterministic rolls per representative danger band, test all currency boundaries, coupon thresholds, room-clear vacuum, pre-clear death loss, save/reload and saturation. Compare observed rates to expected binomial tolerance and fail on out-of-range values.

- [ ] **Step 3: Implement real raylib scenario**

Drive a deterministic save through material pickup, material bag selection, one craft, one coupon and a `+12 -> +13` destroy confirmation. Capture expected state text and screenshot evidence at 1280×720.

- [ ] **Step 4: Run Stage 16 suite and build release**

Run: `ctest --test-dir out/build/windows-msvc-debug -L stage16 --output-on-failure`

Expected: PASS.

Run: `./scripts/Build.ps1 -Preset windows-msvc-release`

Expected: exit code 0 and `out/build/windows-msvc-release/bin/arpg_game.exe` exists.

- [ ] **Step 5: Update validation record and README**

Record the exact commands, pass counts, expected baseline Stage 9 exception, screenshots, drop distribution tolerance and real-game result in `docs/validation/stage16-loot-currency-reinforcement.md`. Update README current milestone and controls.

- [ ] **Step 6: Commit**

```powershell
git add tests docs README.md
git commit -m "docs: validate stage 16 loot reinforcement"
```

## Self-review checklist

- [ ] Task 1 covers all eighteen bases, multi-effects and stable IDs.
- [ ] Task 2 covers all fourteen material IDs, nine currency semantics and exact reinforcement math.
- [ ] Task 3 covers V7 persistence and V6 migration.
- [ ] Task 4 covers normal/high-danger/deep-abyss drops, 400 slots, pickup, vacuum and death loss.
- [ ] Task 5 covers bag/ground visibility and filtering exceptions.
- [ ] Task 6 covers atomic currency use, equipped targets and three-to-one safety.
- [ ] Task 7 covers successful/failed stone attempts, destruction and all coupon faces.
- [ ] Task 8 covers million-roll balance, real raylib verification, release build and user-facing documentation.

# Task 4 Brief — Deterministic fixed-capacity health-potion model

## Scope

Implement only Task 4 from the approved plan in worktree:
`E:\game\.worktrees\balance-hp-potions`

Allowed files:

- Create `src/dungeon/health_potion_loot.hpp`
- Create `src/dungeon/health_potion_loot.cpp`
- Modify `src/dungeon/CMakeLists.txt`
- Modify `src/dungeon/dungeon_types.hpp`
- Create `tests/dungeon/dungeon_health_potion_tests.cpp`
- Modify `tests/dungeon/CMakeLists.txt`
- Modify `tests/dungeon/dungeon_test_main.cpp`

Report: `.superpowers/sdd/balance-hp-task-4-report.md`

## Required public contract

Create `health_potion_loot.hpp` with this exact contract (formatting may follow project style):

```cpp
#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::dungeon {

inline constexpr std::uint16_t kHealthPotionDropChanceBp = 3000U;
inline constexpr std::uint16_t kHealthPotionRestoreBp = 2500U;
inline constexpr std::uint16_t kHealthPotionAutoUseThresholdBp = 7500U;
inline constexpr std::size_t kGroundHealthPotionCapacity = 192U;
inline constexpr std::size_t kPendingHealthPotionClaimCapacity = 4U;

[[nodiscard]] constexpr std::uint16_t health_potion_claim_ordinal(
    std::uint16_t spawn_ordinal) noexcept {
    return static_cast<std::uint16_t>(spawn_ordinal * 2U + 1U);
}

struct GroundHealthPotion final {
    bool active{};
    std::uint16_t spawn_ordinal{};
    std::uint16_t claim_ordinal{};
    combat::Vec3 position{};
};

struct GroundHealthPotionSnapshot final {
    std::uint16_t spawn_ordinal{};
    std::uint16_t claim_ordinal{};
    combat::Vec3 position{};
};

struct HealthPotionPickupReceipt final {
    bool valid{};
    bool room_clear{};
    std::uint16_t consumed_count{};
    std::uint64_t commit_generation{};
    int restored_hp{};
};

struct PendingHealthPotionClaim final {
    std::array<std::uint16_t, kPendingHealthPotionClaimCapacity> spawn_ordinals{};
    std::uint8_t count{};
    int expected_hp{};
    int expected_max_hp{};
};

[[nodiscard]] bool roll_health_potion_drop(
    std::uint64_t room_seed, std::uint16_t spawn_ordinal) noexcept;
[[nodiscard]] bool health_potion_auto_use_eligible(
    int hp, int max_hp) noexcept;

}  // namespace arpg::dungeon
```

Implement the independent RNG domain exactly:

```cpp
#include "dungeon/health_potion_loot.hpp"

#include "core/deterministic_rng.hpp"

namespace arpg::dungeon {
namespace {
constexpr std::uint64_t kHealthPotionChanceDomain = 0x48505F504F544E31ULL;
}

bool roll_health_potion_drop(
    std::uint64_t room_seed, std::uint16_t spawn_ordinal) noexcept {
    auto ordinal = core::DeterministicRng::derive_stream(
        room_seed, static_cast<std::uint64_t>(spawn_ordinal));
    auto chance = core::DeterministicRng::derive_stream(
        ordinal.next_u64(), kHealthPotionChanceDomain);
    return chance.next_bounded(10000U).value_or(9999U)
        < kHealthPotionDropChanceBp;
}

bool health_potion_auto_use_eligible(int hp, int max_hp) noexcept {
    if (hp <= 0 || max_hp <= 0 || hp > max_hp) return false;
    return static_cast<std::int64_t>(hp) * 10000
        <= static_cast<std::int64_t>(max_hp)
            * kHealthPotionAutoUseThresholdBp;
}

}  // namespace arpg::dungeon
```

This RNG must not consume or alter any existing RNG state.

## Dungeon type additions

In `dungeon_types.hpp` include the new header and add:

```cpp
static_assert(kGroundHealthPotionCapacity == kGroundDropCapacity);
```

Append `health_potion_ground_saturation_count` to `DungeonDiagnostics`.

Append to `DungeonSnapshot`:

```cpp
std::uint16_t ground_health_potion_count{};
std::array<GroundHealthPotionSnapshot, kGroundHealthPotionCapacity>
    ground_health_potions{};
HealthPotionPickupReceipt health_potion_pickup_receipt{};
std::optional<std::uint16_t> pending_health_potion_spawn_ordinal{};
```

Append `health_potion_pickup` at the end of `PendingSaveKind`, and append to `PendingSave`:

```cpp
std::optional<PendingHealthPotionClaim> health_potion_claim{};
```

Do not change `DungeonRunState`, `ItemOwnershipState`, or any persistence codec/version.

## Tests to write first

Create/register `dungeon_health_potion_suite()` with exactly these initial three cases:

```cpp
arpg::test::Failure health_potion_rules_are_frozen() noexcept {
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionDropChanceBp == 3000U);
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionRestoreBp == 2500U);
    ARPG_REQUIRE(arpg::dungeon::kHealthPotionAutoUseThresholdBp == 7500U);
    ARPG_REQUIRE(arpg::dungeon::kGroundHealthPotionCapacity == 192U);
    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(0U) == 1U);
    ARPG_REQUIRE(arpg::dungeon::health_potion_claim_ordinal(191U) == 383U);
    return {};
}

arpg::test::Failure health_potion_roll_is_deterministic_and_has_both_outcomes() noexcept {
    bool found_drop = false;
    bool found_miss = false;
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const bool first = arpg::dungeon::roll_health_potion_drop(seed, 37U);
        const bool second = arpg::dungeon::roll_health_potion_drop(seed, 37U);
        ARPG_REQUIRE(first == second);
        found_drop = found_drop || first;
        found_miss = found_miss || !first;
    }
    ARPG_REQUIRE(found_drop);
    ARPG_REQUIRE(found_miss);
    return {};
}

arpg::test::Failure health_potion_threshold_is_integer_exact() noexcept {
    using arpg::dungeon::health_potion_auto_use_eligible;
    ARPG_REQUIRE(!health_potion_auto_use_eligible(751, 1000));
    ARPG_REQUIRE(health_potion_auto_use_eligible(750, 1000));
    ARPG_REQUIRE(health_potion_auto_use_eligible(749, 1000));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(0, 1000));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(1, 0));
    ARPG_REQUIRE(health_potion_auto_use_eligible(3, 4));
    ARPG_REQUIRE(!health_potion_auto_use_eligible(4, 4));
    return {};
}
```

Add the source to `arpg_dungeon_tests`; declare/register the suite in `dungeon_test_main.cpp`. Add an `ARPG_HEALTH_POTION_ONLY` branch expecting 3 cases; change full total `287 -> 290` (subject to unchanged upstream total at start of this task).

## TDD and verification

Read and follow:
`E:\codex1\.codex\plugins\cache\openai-curated-remote\superpowers\6.1.1\skills\test-driven-development\SKILL.md`

1. Add tests/build registration first.
2. Build to obtain RED due to missing header/symbols.
3. Add the minimum contract/types/implementation.
4. Run:

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_dungeon_tests -- -j1
$env:ARPG_HEALTH_POTION_ONLY='1'
& .\out\build\windows-msvc-core-debug\bin\arpg_dungeon_tests.exe
Remove-Item Env:ARPG_HEALTH_POTION_ONLY
```

Expected: `3 cases, 0 failures`.

Also run the full dungeon suite once to confirm the updated total.

## Hard boundaries

- Model only; do not generate drops, populate pools/snapshots, consume potions, heal, save claims, or draw UI yet.
- Potions are not `MaterialId` and do not enter ownership/inventory.
- Capacity is fixed at 192; pending claim capacity is exactly 4.
- Do not touch persistence codecs or bump formats.
- Do not alter existing loot RNG/material/coupon behavior.
- Do not touch `E:\game` or prior task behavior.

## Completion

Commit message:
`feat: define deterministic fixed-capacity health potion loot`

Write report with status, commit hash, valid RED/GREEN evidence, changed files, concerns. Return `DONE`, `DONE_WITH_CONCERNS`, `NEEDS_CONTEXT`, or `BLOCKED`.

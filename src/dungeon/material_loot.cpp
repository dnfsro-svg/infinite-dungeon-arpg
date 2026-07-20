#include "dungeon/material_loot.hpp"

#include "core/deterministic_rng.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::dungeon {
namespace {

constexpr std::uint64_t kMaterialChanceDomain = 0x4D41545F43484E43ULL;
constexpr std::uint64_t kMaterialTypeDomain = 0x4D41545F54595045ULL;
constexpr std::uint64_t kCouponTierDomain = 0x4355504E5F544945ULL;
constexpr std::uint64_t kAbyssMaterialDomain = 0x414259535F4D4154ULL;

struct MaterialWeight final {
    items::MaterialId id{items::MaterialId::count};
    std::uint64_t minimum_depth{};
    std::uint16_t normal_weight{};
    std::uint16_t abyss_weight{};
};

constexpr std::array<MaterialWeight, 10U> kMaterialWeights{{
    {items::MaterialId::reinforcement_stone, 1U, 40U, 20U},
    {items::MaterialId::transmute, 1U, 18U, 9U},
    {items::MaterialId::augment, 1U, 12U, 6U},
    {items::MaterialId::scour, 1U, 6U, 3U},
    {items::MaterialId::regal, 16U, 6U, 6U},
    {items::MaterialId::annul, 16U, 4U, 4U},
    {items::MaterialId::chaos, 30U, 5U, 19U},
    {items::MaterialId::divine, 30U, 3U, 11U},
    {items::MaterialId::exalt, 45U, 2U, 7U},
    {items::MaterialId::directed, 45U, 4U, 15U},
}};

struct CouponRule final {
    items::MaterialId id{items::MaterialId::count};
    std::uint64_t minimum_depth{};
    std::uint16_t minimum_score{};
    std::uint16_t chance_bp{};
};

constexpr std::array<CouponRule, 4U> kCouponRules{{
    {items::MaterialId::coupon_15, 90U, 18U, 5U},
    {items::MaterialId::coupon_12, 60U, 14U, 20U},
    {items::MaterialId::coupon_9, 30U, 10U, 50U},
    {items::MaterialId::coupon_6, 16U, 6U, 100U},
}};

[[nodiscard]] core::DeterministicRng material_stream(
    std::uint64_t room_seed,
    std::uint16_t ordinal,
    std::uint64_t domain) noexcept {
    auto ordinal_stream = core::DeterministicRng::derive_stream(
        room_seed, ordinal);
    return core::DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), domain);
}

[[nodiscard]] std::optional<items::MaterialId> select_material(
    core::DeterministicRng& rng,
    std::uint64_t depth,
    bool abyss_pool) noexcept {
    std::uint16_t total = 0U;
    for (const MaterialWeight& candidate : kMaterialWeights) {
        if (depth < candidate.minimum_depth) continue;
        total = static_cast<std::uint16_t>(total
            + (abyss_pool ? candidate.abyss_weight
                          : candidate.normal_weight));
    }
    if (total == 0U) return std::nullopt;
    std::uint16_t sample = static_cast<std::uint16_t>(
        rng.next_bounded(total).value());
    for (const MaterialWeight& candidate : kMaterialWeights) {
        if (depth < candidate.minimum_depth) continue;
        const std::uint16_t weight = abyss_pool
            ? candidate.abyss_weight : candidate.normal_weight;
        if (sample < weight) return candidate.id;
        sample = static_cast<std::uint16_t>(sample - weight);
    }
    return std::nullopt;
}

}  // namespace

std::uint16_t material_drop_chance_bp(
    std::uint16_t danger_score) noexcept {
    const std::uint32_t chance = 800U
        + static_cast<std::uint32_t>(danger_score) * 100U;
    return static_cast<std::uint16_t>(chance < 3500U ? chance : 3500U);
}

bool coupon_eligible(
    items::MaterialId coupon,
    std::uint64_t depth,
    std::uint16_t danger_score) noexcept {
    for (const CouponRule& rule : kCouponRules) {
        if (rule.id == coupon) {
            return depth >= rule.minimum_depth
                && danger_score >= rule.minimum_score;
        }
    }
    return false;
}

std::optional<items::MaterialId> roll_material_drop(
    std::uint64_t room_seed,
    std::uint16_t monster_spawn_ordinal,
    std::uint64_t depth,
    std::uint16_t danger_score) noexcept {
    auto chance = material_stream(
        room_seed, monster_spawn_ordinal, kMaterialChanceDomain);
    if (chance.next_bounded(10000U).value()
            >= material_drop_chance_bp(danger_score)) {
        return std::nullopt;
    }
    auto type = material_stream(
        room_seed, monster_spawn_ordinal, kMaterialTypeDomain);
    return select_material(type, depth, false);
}

std::optional<items::MaterialId> roll_coupon_drop(
    std::uint64_t room_seed,
    std::uint16_t monster_spawn_ordinal,
    std::uint64_t depth,
    std::uint16_t danger_score,
    bool abyss_monster) noexcept {
    auto tier = material_stream(
        room_seed, monster_spawn_ordinal, kCouponTierDomain);
    for (const CouponRule& rule : kCouponRules) {
        if (!coupon_eligible(rule.id, depth, danger_score)) continue;
        const std::uint16_t chance = static_cast<std::uint16_t>(
            abyss_monster ? rule.chance_bp * 3U : rule.chance_bp);
        if (tier.next_bounded(10000U).value() < chance) return rule.id;
    }
    return std::nullopt;
}

std::uint8_t abyss_material_reward_count(
    abyss::AbyssDanger danger) noexcept {
    switch (danger) {
    case abyss::AbyssDanger::low: return 1U;
    case abyss::AbyssDanger::medium: return 2U;
    case abyss::AbyssDanger::high: return 3U;
    }
    return 0U;
}

std::optional<items::MaterialId> roll_abyss_material(
    std::uint64_t room_seed,
    std::uint64_t depth,
    std::uint8_t reward_ordinal) noexcept {
    auto reward = material_stream(
        room_seed, reward_ordinal, kAbyssMaterialDomain);
    return select_material(reward, depth, true);
}

}  // namespace arpg::dungeon

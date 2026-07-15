#include "test_framework.hpp"

#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <string_view>

namespace {

using namespace arpg::combat;

struct ExpectedTierValues final {
    std::int32_t primary_bp{};
    std::int32_t secondary_bp{};
    std::uint16_t interval_ticks{};
    std::uint16_t duration_ticks{};
    std::int32_t damage{};
    float radius{};
    std::uint8_t projectile_count{};
};

struct ExpectedAffix final {
    MonsterAffixId id{};
    std::string_view name{};
    std::string_view short_name{};
    MonsterAffixDanger danger{};
    MonsterTag required_tag{};
    std::array<ExpectedTierValues, 3> tiers{};
};

constexpr std::array<ExpectedAffix, 12> kExpectedAffixes{{
    {MonsterAffixId::mighty, "Mighty", "MGT", MonsterAffixDanger::low,
        MonsterTag::none, {{{13000, 8500}, {16000, 7000}, {20000, 5500}}}},
    {MonsterAffixId::frenzy, "Frenzy", "FRZ", MonsterAffixDanger::medium,
        MonsterTag::none, {{{11500, 9000}, {13000, 8000}, {15000, 7000}}}},
    {MonsterAffixId::swift, "Swift", "SWF", MonsterAffixDanger::medium,
        MonsterTag::none, {{{11500, 9200}, {13000, 8400}, {14500, 7600}}}},
    {MonsterAffixId::armored, "Armored", "ARM", MonsterAffixDanger::low,
        MonsterTag::none, {{{75}, {325}, {775}}}},
    {MonsterAffixId::shielding, "Shielding", "SHD", MonsterAffixDanger::medium,
        MonsterTag::none, {{{2000, 0, 0, 180}, {3500, 0, 0, 150}, {5000, 0, 0, 120}}}},
    {MonsterAffixId::multishot, "Multishot", "MULTI", MonsterAffixDanger::high,
        MonsterTag::projectile_capable,
        {{{7500, 0, 0, 0, 0, 0.0F, 2}, {6000, 0, 0, 0, 0, 0.0F, 3}, {5000, 0, 0, 0, 0, 0.0F, 4}}}},
    {MonsterAffixId::burning_ground, "Burning Ground", "BURN", MonsterAffixDanger::high,
        MonsterTag::none,
        {{{0, 0, 180, 120, 25, 0.75F}, {0, 0, 150, 180, 35, 0.90F}, {0, 0, 120, 240, 45, 1.05F}}}},
    {MonsterAffixId::chilling, "Chilling", "CHILL", MonsterAffixDanger::medium,
        MonsterTag::direct_target,
        {{{1500, 1500, 0, 60}, {2500, 2500, 0, 90}, {3500, 3500, 0, 120}}}},
    {MonsterAffixId::chain_lightning, "Chain Lightning", "CHAIN", MonsterAffixDanger::high,
        MonsterTag::direct_target,
        {{{0, 0, 42, 0, 70, 0.65F}, {0, 0, 42, 0, 110, 0.80F}, {0, 0, 42, 0, 160, 0.95F}}}},
    {MonsterAffixId::chaos_corrosion, "Chaos Corrosion", "CORR", MonsterAffixDanger::medium,
        MonsterTag::direct_target,
        {{{0, 0, 0, 120, 20}, {0, 0, 0, 180, 30}, {0, 0, 0, 240, 45}}}},
    {MonsterAffixId::blink_assault, "Blink Assault", "BLINK", MonsterAffixDanger::high,
        MonsterTag::melee,
        {{{12000, 0, 480, 42}, {13500, 0, 360, 36}, {15000, 0, 240, 30}}}},
    {MonsterAffixId::death_blast, "Death Blast", "DEATH", MonsterAffixDanger::high,
        MonsterTag::none,
        {{{0, 0, 66, 0, 120, 0.90F}, {0, 0, 54, 0, 190, 1.15F}, {0, 0, 45, 0, 280, 1.40F}}}},
}};

arpg::test::Failure catalog_matches_frozen_affix_definitions() noexcept {
    std::uint8_t affix_count = static_cast<std::uint8_t>(MonsterAffixId::count);
    ARPG_REQUIRE(affix_count == 12U);
    ARPG_REQUIRE(monster_affix_catalog_valid());
    for (std::size_t index = 0U; index < kExpectedAffixes.size(); ++index) {
        const ExpectedAffix& expected = kExpectedAffixes[index];
        const auto* actual = monster_affix_definition(expected.id);
        ARPG_REQUIRE(actual != nullptr);
        ARPG_REQUIRE(static_cast<std::size_t>(actual->id) == index);
        ARPG_REQUIRE(actual->id == expected.id);
        ARPG_REQUIRE(!actual->name.empty());
        ARPG_REQUIRE(!actual->short_name.empty());
        ARPG_REQUIRE(actual->name == expected.name);
        ARPG_REQUIRE(actual->short_name == expected.short_name);
        ARPG_REQUIRE(actual->danger == expected.danger);
        ARPG_REQUIRE(actual->weight == 100U);
        ARPG_REQUIRE(actual->required_tags == static_cast<std::uint16_t>(expected.required_tag));
        ARPG_REQUIRE(actual->forbidden_tags == static_cast<std::uint16_t>(MonsterTag::none));
        ARPG_REQUIRE(actual->conflict_mask == 0U);
        for (std::size_t tier_index = 0U; tier_index < actual->tiers.size(); ++tier_index) {
            const MonsterAffixTierValues& tier = actual->tiers[tier_index];
            const ExpectedTierValues& values = expected.tiers[tier_index];
            ARPG_REQUIRE(tier.primary_bp == values.primary_bp);
            ARPG_REQUIRE(tier.secondary_bp == values.secondary_bp);
            ARPG_REQUIRE(tier.interval_ticks == values.interval_ticks);
            ARPG_REQUIRE(tier.duration_ticks == values.duration_ticks);
            ARPG_REQUIRE(tier.damage == values.damage);
            ARPG_REQUIRE(tier.radius == values.radius);
            ARPG_REQUIRE(tier.projectile_count == values.projectile_count);
        }
    }
    ARPG_REQUIRE(monster_affix_definition(MonsterAffixId::count) == nullptr);
    return {};
}

arpg::test::Failure projectile_capable_is_exclusive_to_lightning_shooter() noexcept {
    const MonsterDefinition* const lightning =
        monster_definition(MonsterId::lightning_shooter);
    const MonsterDefinition* const chaos = monster_definition(MonsterId::chaos_hazard);
    ARPG_REQUIRE(lightning != nullptr);
    ARPG_REQUIRE(chaos != nullptr);
    ARPG_REQUIRE(has_tag(*lightning, MonsterTag::projectile_capable));
    ARPG_REQUIRE(!has_tag(*chaos, MonsterTag::projectile_capable));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"frozen affix definitions", &catalog_matches_frozen_affix_definitions},
    {"projectile capability exclusivity",
        &projectile_capable_is_exclusive_to_lightning_shooter},
};

}  // namespace

arpg::test::TestSuite monster_affix_catalog_suite() noexcept {
    return arpg::test::make_suite("monster_affix_catalog", kCases);
}

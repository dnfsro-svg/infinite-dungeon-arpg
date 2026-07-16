#include "test_framework.hpp"

#include "monster_affix_test_support.hpp"
#include "combat/monster_affix_generation.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_catalog.hpp"
#include "core/deterministic_rng.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using namespace arpg::combat;

constexpr std::uint64_t kReferenceAffixCountDomain = 0x41464658434E5431ULL;
constexpr std::uint64_t kReferenceAffixSelectionDomain = 0x4146465853454C31ULL;
constexpr std::uint64_t kReferenceAffixTierDomain = 0x4146465854494552ULL;
constexpr std::uint64_t kReferenceAffixContextDomain = 0x4146465843545831ULL;

std::uint64_t reference_context_seed(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index) noexcept {
    auto context = arpg::core::DeterministicRng::derive_stream(room_seed,
        kReferenceAffixContextDomain);
    context = arpg::core::DeterministicRng::derive_stream(context.next_u64(),
        depth);
    context = arpg::core::DeterministicRng::derive_stream(context.next_u64(),
        static_cast<std::uint64_t>(wave_index));
    context = arpg::core::DeterministicRng::derive_stream(context.next_u64(),
        static_cast<std::uint64_t>(spawn_index));
    return context.next_u64();
}

arpg::core::DeterministicRng reference_output_rng(
    std::uint64_t context_seed, std::uint64_t domain,
    std::size_t output_index) noexcept {
    auto domain_rng = arpg::core::DeterministicRng::derive_stream(context_seed,
        domain);
    return arpg::core::DeterministicRng::derive_stream(domain_rng.next_u64(),
        static_cast<std::uint64_t>(output_index));
}

template <std::size_t N>
std::size_t reference_weighted_index(
    arpg::core::DeterministicRng& rng,
    const std::array<std::uint16_t, N>& weights) noexcept {
    std::uint64_t total = 0U;
    for (const std::uint16_t weight : weights) total += weight;
    if (total == 0U) return weights.size();
    std::uint64_t cursor = rng.next_bounded(total).value_or(total);
    for (std::size_t index = 0U; index < weights.size(); ++index) {
        if (cursor < weights[index]) return index;
        cursor -= weights[index];
    }
    return weights.size();
}

const MonsterAffixDefinition* reference_definition(MonsterAffixId id) noexcept {
    const MonsterAffixCatalog& catalog = monster_affix_catalog();
    const std::size_t index = static_cast<std::size_t>(id);
    return index < catalog.size() && catalog[index].id == id
        ? &catalog[index] : nullptr;
}

bool reference_contains(const MonsterAffixSet& set, MonsterAffixId id) noexcept {
    for (std::size_t index = 0U; index < set.count; ++index) {
        if (set.values[index].id == id) return true;
    }
    return false;
}

bool reference_conflicts(const MonsterAffixSet& set,
    const MonsterAffixDefinition& candidate) noexcept {
    const std::uint16_t candidate_bit = static_cast<std::uint16_t>(
        1U << static_cast<std::uint8_t>(candidate.id));
    for (std::size_t index = 0U; index < set.count; ++index) {
        const MonsterAffixDefinition* const selected = reference_definition(
            set.values[index].id);
        if (selected == nullptr || (candidate.conflict_mask
                & static_cast<std::uint16_t>(1U << static_cast<std::uint8_t>(
                    selected->id))) != 0U
            || (selected->conflict_mask & candidate_bit) != 0U) {
            return true;
        }
    }
    return false;
}

std::size_t reference_candidates(const MonsterDefinition& monster,
    const MonsterAffixSet& selected,
    std::array<const MonsterAffixDefinition*,
        static_cast<std::size_t>(MonsterAffixId::count)>& candidates) noexcept {
    std::size_t count = 0U;
    for (std::uint8_t raw = 0U;
         raw < static_cast<std::uint8_t>(MonsterAffixId::count); ++raw) {
        const MonsterAffixDefinition* const definition = reference_definition(
            static_cast<MonsterAffixId>(raw));
        if (definition == nullptr || definition->weight == 0U
                || reference_contains(selected, definition->id)
                || (monster.tags & definition->required_tags)
                    != definition->required_tags
                || (monster.tags & definition->forbidden_tags) != 0U
                || reference_conflicts(selected, *definition)) {
            continue;
        }
        candidates[count++] = definition;
    }
    return count;
}

enum class ReferenceSampling final {
    indexed,
    shared_sequential,
};

std::optional<MonsterAffixSet> reference_generate_affixes(
    std::uint64_t room_seed, std::uint64_t depth,
    std::uint8_t wave_index, std::uint8_t spawn_index,
    const MonsterDefinition& monster, ReferenceSampling sampling,
    bool consume_position_zero_randomness) noexcept {
    const std::uint64_t seed = reference_context_seed(room_seed, depth,
        wave_index, spawn_index);
    if (consume_position_zero_randomness) {
        auto selection_rng = reference_output_rng(seed,
            kReferenceAffixSelectionDomain, 0U);
        auto tier_rng = reference_output_rng(seed, kReferenceAffixTierDomain,
            0U);
        static_cast<void>(selection_rng.next_u64());
        static_cast<void>(tier_rng.next_u64());
    }
    auto count_rng = arpg::core::DeterministicRng::derive_stream(seed,
        kReferenceAffixCountDomain);
    const std::size_t target_count = reference_weighted_index(count_rng,
        affix_count_weights(depth));
    if (target_count > 3U) return std::nullopt;

    auto shared_selection_rng = arpg::core::DeterministicRng::derive_stream(
        seed, kReferenceAffixSelectionDomain);
    auto shared_tier_rng = arpg::core::DeterministicRng::derive_stream(seed,
        kReferenceAffixTierDomain);
    const auto tier_weights = affix_tier_weights(depth);
    MonsterAffixSet result{};
    for (std::size_t output_index = 0U; output_index < target_count;
         ++output_index) {
        std::array<const MonsterAffixDefinition*,
            static_cast<std::size_t>(MonsterAffixId::count)> candidates{};
        const std::size_t candidate_count = reference_candidates(monster, result,
            candidates);
        if (candidate_count == 0U) return std::nullopt;
        std::array<std::uint16_t,
            static_cast<std::size_t>(MonsterAffixId::count)> weights{};
        for (std::size_t index = 0U; index < candidate_count; ++index) {
            weights[index] = candidates[index]->weight;
        }
        arpg::core::DeterministicRng selection_rng = sampling
                == ReferenceSampling::indexed
            ? reference_output_rng(seed, kReferenceAffixSelectionDomain,
                output_index)
            : shared_selection_rng;
        const std::size_t selected_index = reference_weighted_index(selection_rng,
            weights);
        if (selected_index >= candidate_count) return std::nullopt;
        if (sampling == ReferenceSampling::shared_sequential) {
            shared_selection_rng = selection_rng;
        }
        arpg::core::DeterministicRng tier_rng = sampling
                == ReferenceSampling::indexed
            ? reference_output_rng(seed, kReferenceAffixTierDomain, output_index)
            : shared_tier_rng;
        const std::size_t tier_index = reference_weighted_index(tier_rng,
            tier_weights);
        if (tier_index >= tier_weights.size()) return std::nullopt;
        if (sampling == ReferenceSampling::shared_sequential) {
            shared_tier_rng = tier_rng;
        }
        result.values[result.count++] = {candidates[selected_index]->id,
            static_cast<MonsterAffixTier>(tier_index)};
    }
    return result;
}

bool has_affix(const MonsterAffixSet& set, MonsterAffixId id) noexcept {
    for (std::size_t index = 0U; index < set.count; ++index) {
        if (set.values[index].id == id) return true;
    }
    return false;
}

bool affixes_are_unique(const MonsterAffixSet& set) noexcept {
    if (set.count > set.values.size()) return false;
    for (std::size_t left = 0U; left < set.count; ++left) {
        for (std::size_t right = left + 1U; right < set.count; ++right) {
            if (set.values[left].id == set.values[right].id) return false;
        }
    }
    return true;
}

arpg::test::Failure depth_bands_match_frozen_weights() noexcept {
    ARPG_REQUIRE((affix_count_weights(3U)
        == std::array<std::uint16_t, 4>{{80U, 20U, 0U, 0U}}));
    ARPG_REQUIRE((affix_count_weights(4U)
        == std::array<std::uint16_t, 4>{{55U, 38U, 7U, 0U}}));
    ARPG_REQUIRE((affix_count_weights(9U)
        == std::array<std::uint16_t, 4>{{55U, 38U, 7U, 0U}}));
    ARPG_REQUIRE((affix_count_weights(10U)
        == std::array<std::uint16_t, 4>{{30U, 45U, 20U, 5U}}));
    ARPG_REQUIRE((affix_count_weights(19U)
        == std::array<std::uint16_t, 4>{{30U, 45U, 20U, 5U}}));
    ARPG_REQUIRE((affix_count_weights(20U)
        == std::array<std::uint16_t, 4>{{15U, 35U, 35U, 15U}}));
    ARPG_REQUIRE((affix_count_weights(39U)
        == std::array<std::uint16_t, 4>{{15U, 35U, 35U, 15U}}));
    ARPG_REQUIRE((affix_count_weights(40U)
        == std::array<std::uint16_t, 4>{{5U, 20U, 40U, 35U}}));
    ARPG_REQUIRE((affix_tier_weights(3U)
        == std::array<std::uint16_t, 3>{{100U, 0U, 0U}}));
    ARPG_REQUIRE((affix_tier_weights(4U)
        == std::array<std::uint16_t, 3>{{80U, 20U, 0U}}));
    ARPG_REQUIRE((affix_tier_weights(10U)
        == std::array<std::uint16_t, 3>{{50U, 40U, 10U}}));
    ARPG_REQUIRE((affix_tier_weights(20U)
        == std::array<std::uint16_t, 3>{{25U, 50U, 25U}}));
    ARPG_REQUIRE((affix_tier_weights(40U)
        == std::array<std::uint16_t, 3>{{10U, 35U, 55U}}));
    return {};
}

arpg::test::Failure generation_is_deterministic_unique_and_scored() noexcept {
    const MonsterDefinition* const monster =
        monster_definition(MonsterId::lightning_shooter);
    ARPG_REQUIRE(monster != nullptr);
    const auto a = generate_monster_affixes(0xA11CEULL, 40U, 1U, 7U,
        *monster);
    const auto b = generate_monster_affixes(0xA11CEULL, 40U, 1U, 7U,
        *monster);
    ARPG_REQUIRE(a.has_value());
    ARPG_REQUIRE(b.has_value());
    ARPG_REQUIRE(*a == *b);
    ARPG_REQUIRE(affixes_are_unique(*a));
    ARPG_REQUIRE(monster_affix_danger_score(*a) <= 27U);
    return {};
}

arpg::test::Failure applicability_rules_and_three_high_risk_are_preserved() noexcept {
    const MonsterDefinition* const support =
        monster_definition(MonsterId::water_support);
    const MonsterDefinition* const hazard =
        monster_definition(MonsterId::chaos_hazard);
    const MonsterDefinition* const shooter =
        monster_definition(MonsterId::lightning_shooter);
    ARPG_REQUIRE(support != nullptr);
    ARPG_REQUIRE(hazard != nullptr);
    ARPG_REQUIRE(shooter != nullptr);

    bool found_three_high = false;
    for (std::uint64_t seed = 0U; seed < 4096U; ++seed) {
        const auto support_set = generate_monster_affixes(seed, 40U, 0U, 0U,
            *support);
        const auto hazard_set = generate_monster_affixes(seed, 40U, 0U, 1U,
            *hazard);
        const auto shooter_set = generate_monster_affixes(seed, 40U, 0U, 2U,
            *shooter);
        ARPG_REQUIRE(support_set.has_value());
        ARPG_REQUIRE(hazard_set.has_value());
        ARPG_REQUIRE(shooter_set.has_value());
        ARPG_REQUIRE(affixes_are_unique(*support_set));
        ARPG_REQUIRE(affixes_are_unique(*hazard_set));
        ARPG_REQUIRE(affixes_are_unique(*shooter_set));
        ARPG_REQUIRE(!has_affix(*support_set, MonsterAffixId::chilling));
        ARPG_REQUIRE(!has_affix(*support_set, MonsterAffixId::chain_lightning));
        ARPG_REQUIRE(!has_affix(*support_set, MonsterAffixId::chaos_corrosion));
        ARPG_REQUIRE(!has_affix(*hazard_set, MonsterAffixId::multishot));
        if (shooter_set->count == 3U
                && monster_affix_danger_score(*shooter_set) == 27U) {
            found_three_high = true;
        }
    }
    ARPG_REQUIRE(found_three_high);
    return {};
}

arpg::test::Failure malformed_catalogs_are_rejected_explicitly() noexcept {
    MonsterAffixCatalog catalog = monster_affix_catalog();
    catalog[1].id = MonsterAffixId::mighty;
    ARPG_REQUIRE(!test_support::monster_affix_catalog_valid(catalog));

    catalog = monster_affix_catalog();
    catalog[0].weight = 99U;
    ARPG_REQUIRE(!test_support::monster_affix_catalog_valid(catalog));

    catalog = monster_affix_catalog();
    catalog[0].danger = static_cast<MonsterAffixDanger>(3U);
    ARPG_REQUIRE(!test_support::monster_affix_catalog_valid(catalog));

    catalog = monster_affix_catalog();
    catalog[0].required_tags = 0x8000U;
    ARPG_REQUIRE(!test_support::monster_affix_catalog_valid(catalog));

    catalog = monster_affix_catalog();
    catalog[0].conflict_mask = 0x8000U;
    ARPG_REQUIRE(!test_support::monster_affix_catalog_valid(catalog));

    catalog = monster_affix_catalog();
    catalog[0].conflict_mask = 1U;
    ARPG_REQUIRE(!test_support::monster_affix_catalog_valid(catalog));
    return {};
}

arpg::test::Failure malformed_catalog_fails_before_zero_affix_sampling() noexcept {
    const MonsterDefinition* const monster =
        monster_definition(MonsterId::chaos_chaser);
    ARPG_REQUIRE(monster != nullptr);

    MonsterAffixCatalog malformed = monster_affix_catalog();
    malformed[0].weight = 0U;
    bool found_zero_affix_roll = false;
    for (std::uint64_t seed = 0U; seed < 1024U; ++seed) {
        const auto canonical = generate_monster_affixes(seed, 3U, 0U, 0U,
            *monster);
        ARPG_REQUIRE(canonical.has_value());
        if (canonical->count == 0U) {
            found_zero_affix_roll = true;
            ARPG_REQUIRE(!test_support::generate_monster_affixes_with_catalog(seed, 3U,
                0U, 0U, *monster, malformed).has_value());
        }
    }
    ARPG_REQUIRE(found_zero_affix_roll);
    return {};
}

arpg::test::Failure insufficient_candidates_fail_explicitly() noexcept {
    const MonsterDefinition* const monster =
        monster_definition(MonsterId::chaos_chaser);
    ARPG_REQUIRE(monster != nullptr);

    MonsterAffixCatalog constrained = monster_affix_catalog();
    for (std::size_t index = 1U; index < constrained.size(); ++index) {
        constrained[index].required_tags = static_cast<std::uint16_t>(
            MonsterTag::projectile_capable);
    }
    ARPG_REQUIRE(test_support::monster_affix_catalog_valid(constrained));

    bool found_multi_affix_roll = false;
    for (std::uint64_t seed = 0U; seed < 1024U; ++seed) {
        const auto canonical = generate_monster_affixes(seed, 40U, 0U, 0U,
            *monster);
        ARPG_REQUIRE(canonical.has_value());
        if (canonical->count >= 2U) {
            found_multi_affix_roll = true;
            ARPG_REQUIRE(!test_support::generate_monster_affixes_with_catalog(seed, 40U,
                0U, 0U, *monster, constrained).has_value());
        }
    }
    ARPG_REQUIRE(found_multi_affix_roll);
    return {};
}

arpg::test::Failure context_derivation_has_no_depth_wave_or_spawn_alias() noexcept {
    constexpr std::uint64_t kRoomSeed = 0xC0111DEULL;
    const std::uint64_t base = test_support::monster_affix_context_seed(
        kRoomSeed, 0U, 0U, 0U);
    const std::uint64_t depth_wave_alias =
        test_support::monster_affix_context_seed(kRoomSeed,
            std::uint64_t{1} << 48U, 1U, 0U);
    const std::uint64_t depth_spawn_alias =
        test_support::monster_affix_context_seed(kRoomSeed,
            std::uint64_t{1} << 56U, 0U, 1U);
    ARPG_REQUIRE(base != depth_wave_alias);
    ARPG_REQUIRE(base != depth_spawn_alias);
    const std::uint64_t count = test_support::monster_affix_count_seed(
        kRoomSeed, 40U, 1U, 7U);
    const std::uint64_t selection =
        test_support::monster_affix_selection_seed(kRoomSeed, 40U, 1U, 7U);
    const std::uint64_t tier = test_support::monster_affix_tier_seed(
        kRoomSeed, 40U, 1U, 7U);
    ARPG_REQUIRE(count != selection);
    ARPG_REQUIRE(count != tier);
    ARPG_REQUIRE(selection != tier);
    return {};
}

arpg::test::Failure output_position_substreams_isolate_real_generation() noexcept {
    const MonsterDefinition* const monster =
        monster_definition(MonsterId::lightning_shooter);
    ARPG_REQUIRE(monster != nullptr);

    bool found_shared_stream_counterexample = false;
    for (std::uint64_t room_seed = 0U; room_seed < 4096U; ++room_seed) {
        const auto indexed = reference_generate_affixes(room_seed, 40U, 1U, 7U,
            *monster, ReferenceSampling::indexed, false);
        const auto indexed_after_position_zero_consumption =
            reference_generate_affixes(room_seed, 40U, 1U, 7U, *monster,
                ReferenceSampling::indexed, true);
        const auto shared = reference_generate_affixes(room_seed, 40U, 1U, 7U,
            *monster, ReferenceSampling::shared_sequential, false);
        const auto actual = generate_monster_affixes(room_seed, 40U, 1U, 7U,
            *monster);
        ARPG_REQUIRE(indexed.has_value());
        ARPG_REQUIRE(indexed_after_position_zero_consumption.has_value());
        ARPG_REQUIRE(shared.has_value());
        ARPG_REQUIRE(actual.has_value());
        if (indexed->count < 2U || *indexed == *shared) continue;

        // Extra selection/tier consumption at position 0 leaves the independent
        // position 1 prediction unchanged. A shared sequential generator yields
        // the distinct `shared` result for this same real generation input.
        ARPG_REQUIRE(*indexed == *indexed_after_position_zero_consumption);
        ARPG_REQUIRE(*actual == *indexed);
        ARPG_REQUIRE(actual->values[1] == indexed->values[1]);
        ARPG_REQUIRE(!(actual->values[1] == shared->values[1]));
        found_shared_stream_counterexample = true;
        break;
    }
    ARPG_REQUIRE(found_shared_stream_counterexample);
    return {};
}

arpg::test::Failure invalid_monster_definition_fails_explicitly() noexcept {
    MonsterDefinition invalid{};
    invalid.id = MonsterId::count;
    invalid.tags = static_cast<std::uint16_t>(MonsterTag::support);
    ARPG_REQUIRE(!generate_monster_affixes(1U, 40U, 0U, 0U, invalid).has_value());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"frozen depth bands", &depth_bands_match_frozen_weights},
    {"deterministic unique generation and score",
        &generation_is_deterministic_unique_and_scored},
    {"applicability and three high risk", &applicability_rules_and_three_high_risk_are_preserved},
    {"malformed catalogs fail explicitly", &malformed_catalogs_are_rejected_explicitly},
    {"malformed catalog fails before zero affix sampling",
        &malformed_catalog_fails_before_zero_affix_sampling},
    {"insufficient candidates fail explicitly",
        &insufficient_candidates_fail_explicitly},
    {"context has no depth wave or spawn alias",
        &context_derivation_has_no_depth_wave_or_spawn_alias},
    {"output position substreams isolate real generation",
        &output_position_substreams_isolate_real_generation},
    {"invalid monster fails explicitly", &invalid_monster_definition_fails_explicitly},
};

}  // namespace

arpg::test::TestSuite monster_affix_generation_suite() noexcept {
    return arpg::test::make_suite("monster_affix_generation", kCases);
}

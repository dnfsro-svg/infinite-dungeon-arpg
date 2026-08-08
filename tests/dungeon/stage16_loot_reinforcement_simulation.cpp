#include "dungeon/dungeon_progression.hpp"
#include "dungeon/abyss_reward.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/encounter_director.hpp"
#include "dungeon/material_loot.hpp"
#include "dungeon/reinforcement_roll.hpp"
#include "dungeon_test_support.hpp"
#include "items/item_catalog.hpp"
#include "items/item_crafting.hpp"
#include "items/item_generation.hpp"
#include "items/material_catalog.hpp"
#include "persistence/checkpoint_codec.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstdint>
#include <iostream>
#include <limits>
#include <memory>
#include <optional>

namespace {

namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace persistence = arpg::persistence;

constexpr std::uint64_t kTrials = 1000000U;

bool require(bool condition, const char* message) noexcept {
    if (!condition) std::cerr << "FAIL: " << message << '\n';
    return condition;
}

bool commit(dungeon::DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return false;
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending->expected_generation, pending->next_state, pending->kind});
    return session.snapshot().phase != dungeon::RoomPhase::faulted;
}

std::optional<items::ItemInstance> generated(std::uint64_t id,
    std::uint8_t base, items::ItemRarity rarity) noexcept {
    const auto* definition = items::base_definition(base);
    if (definition == nullptr) return std::nullopt;
    for (std::uint64_t seed = 1U; seed < 4096U; ++seed) {
        const auto item = items::generate_item(
            {seed, definition->slot, 90U, id, rarity});
        if (item.has_value() && item->base_id == base) return item;
    }
    return std::nullopt;
}

bool same_item(const items::ItemInstance& left,
    const items::ItemInstance& right) noexcept {
    if (!(left.id == right.id && left.base_id == right.base_id
        && left.rarity == right.rarity && left.item_level == right.item_level
        && left.required_level == right.required_level
        && left.affix_count == right.affix_count
        && left.reinforcement == right.reinforcement)) return false;
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        const auto& a = left.affixes[index];
        const auto& b = right.affixes[index];
        if (a.affix_id != b.affix_id || a.tier != b.tier
            || a.variant != b.variant || a.value_roll_bp != b.value_roll_bp) {
            return false;
        }
    }
    return true;
}

bool distribution() noexcept {
    constexpr std::array<std::uint16_t, 3U> dangers{{0U, 10U, 27U}};
    for (const std::uint16_t danger : dangers) {
        std::uint64_t observed = 0U;
        for (std::uint64_t seed = 1U; seed <= kTrials; ++seed) {
            if (dungeon::roll_material_drop(seed, 17U, 90U, danger)) ++observed;
        }
        const double probability = static_cast<double>(
            dungeon::material_drop_chance_bp(danger)) / 10000.0;
        const double mean = static_cast<double>(kTrials) * probability;
        const double sigma = std::sqrt(static_cast<double>(kTrials)
            * probability * (1.0 - probability));
        // This is a statistical guard around the declared chance, not a claim
        // that every one-basis-point change is distinguishable in one run.
        const double tolerance = 6.0 * sigma + 12.0;
        std::cout << "danger=" << danger << " expected_bp="
                  << dungeon::material_drop_chance_bp(danger)
                  << " observed=" << observed << " tolerance="
                  << static_cast<std::uint64_t>(tolerance) << '\n';
        if (!require(std::fabs(static_cast<double>(observed) - mean)
                <= tolerance, "material binomial tolerance")) return false;
    }
    return true;
}

void mix(std::uint64_t& hash, std::uint64_t value) noexcept {
    hash ^= value + 0x9E3779B97F4A7C15ULL + (hash << 6U) + (hash >> 2U);
}

std::optional<std::uint64_t> frozen_existing_stream_fingerprint(
    bool interleave_material) noexcept {
    std::uint64_t hash = 0x16F10A5EULL;
    for (std::uint64_t seed = 1U; seed <= 512U; ++seed) {
        const auto room = dungeon::make_initial_run_state(seed, dungeon::DungeonRules{});
        const auto equipment = items::generate_item(
            {seed, items::ItemSlot::weapon, 75U, seed, items::ItemRarity::rare});
        const auto abyss = dungeon::derive_abyss_ground_item(
            room.state.current_room.seed, arpg::abyss::AbyssDanger::high,
            75U, 0U, 0U);
        const auto encounter = dungeon::build_encounter_plan(
            room.state.current_room.seed, room.state.current_room.depth,
            room.state.current_room.ecology, dungeon::DungeonRules{}.encounter);
        if (room.fault != dungeon::DungeonFault::none || encounter.fault
                != dungeon::DungeonFault::none || !equipment || !abyss) {
            return std::nullopt;
        }
        mix(hash, room.state.current_room.seed);
        mix(hash, room.state.current_room.depth);
        mix(hash, static_cast<std::uint64_t>(room.state.current_room.ecology));
        const auto hash_item = [&](const items::ItemInstance& item) noexcept {
            mix(hash, item.id);
            mix(hash, item.base_id);
            mix(hash, static_cast<std::uint64_t>(item.rarity));
            mix(hash, item.item_level);
            mix(hash, item.required_level);
            mix(hash, item.affix_count);
            mix(hash, item.reinforcement);
            for (const auto& affix : item.affixes) {
                mix(hash, affix.affix_id);
                mix(hash, affix.tier);
                mix(hash, affix.variant);
                mix(hash, affix.value_roll_bp);
            }
        };
        hash_item(*equipment);
        hash_item(abyss->item);
        mix(hash, encounter.plan.wave_count);
        mix(hash, encounter.plan.total_budget);
        for (std::size_t wave = 0U; wave < encounter.plan.wave_count; ++wave) {
            const auto& current = encounter.plan.waves[wave];
            mix(hash, current.spawn_count);
            mix(hash, current.spent_budget);
            for (std::size_t index = 0U; index < current.spawn_count; ++index) {
                const auto& spawn = current.spawns[index];
                mix(hash, static_cast<std::uint64_t>(spawn.id));
                mix(hash, spawn.spawn_ordinal);
                mix(hash, spawn.affixes.count);
                for (const auto& affix : spawn.affixes.values) {
                    mix(hash, static_cast<std::uint64_t>(affix.id));
                    mix(hash, static_cast<std::uint64_t>(affix.tier));
                }
            }
        }
        if (interleave_material) {
            static_cast<void>(dungeon::roll_material_drop(seed, 17U, 90U, 27U));
            static_cast<void>(dungeon::roll_coupon_drop(seed, 17U, 90U, 27U, false));
            static_cast<void>(dungeon::roll_abyss_material(seed, 90U, 2U));
        }
    }
    return hash;
}

bool replay_keeps_existing_items_stable() noexcept {
    constexpr std::uint64_t kFrozenTask1RoomEquipmentAndAbyss =
        10882612280735857555ULL;
    const auto baseline = frozen_existing_stream_fingerprint(false);
    const auto interleaved = frozen_existing_stream_fingerprint(true);
    std::cout << "frozen_existing_stream=" << baseline.value_or(0U) << '\n';
    return require(baseline.has_value() && interleaved.has_value()
            && *baseline == *interleaved,
            "material RNG does not perturb Task1 room/equipment/abyss streams")
        && require(*baseline == kFrozenTask1RoomEquipmentAndAbyss,
            "Task1 room/equipment/abyss golden fingerprint");
}

bool weighted_material_and_coupon_statistics() noexcept {
    constexpr std::array<std::uint16_t, 10U> kNormalWeights{{18U, 12U, 6U,
        5U, 2U, 4U, 3U, 6U, 4U, 40U}};
    constexpr std::array<std::uint16_t, 10U> kAbyssWeights{{9U, 6U, 6U,
        19U, 7U, 4U, 11U, 3U, 15U, 20U}};
    std::array<std::uint64_t, 10U> normal{};
    std::array<std::uint64_t, 10U> abyss{};
    std::array<std::uint64_t, 4U> coupons{};
    std::array<std::uint64_t, 4U> abyss_coupons{};
    std::uint64_t normal_total = 0U;
    for (std::uint64_t seed = 1U; seed <= kTrials; ++seed) {
        if (const auto drop = dungeon::roll_material_drop(seed, 17U, 90U, 27U)) {
            const std::size_t index = items::material_index(*drop);
            if (index >= normal.size()) return false;
            ++normal[index];
            ++normal_total;
        }
        if (const auto drop = dungeon::roll_abyss_material(seed, 90U, 2U)) {
            const std::size_t index = items::material_index(*drop);
            if (index >= abyss.size()) return false;
            ++abyss[index];
        } else return false;
        if (const auto coupon = dungeon::roll_coupon_drop(seed, 17U, 90U, 27U, false)) {
            const std::size_t index = items::material_index(*coupon);
            if (index < 10U || index >= items::kMaterialCount) return false;
            ++coupons[index - 10U];
        }
        if (const auto coupon = dungeon::roll_coupon_drop(seed, 17U, 90U, 27U, true)) {
            const std::size_t index = items::material_index(*coupon);
            if (index < 10U || index >= items::kMaterialCount) return false;
            ++abyss_coupons[index - 10U];
        }
    }
    constexpr double kNormalTotal = 100.0;
    constexpr double kAbyssTotal = 100.0;
    for (std::size_t index = 0U; index < normal.size(); ++index) {
        const double normal_probability = static_cast<double>(kNormalWeights[index]) / kNormalTotal;
        const double normal_mean = static_cast<double>(normal_total) * normal_probability;
        const double normal_sigma = std::sqrt(static_cast<double>(normal_total)
            * normal_probability * (1.0 - normal_probability));
        const double abyss_probability = static_cast<double>(kAbyssWeights[index]) / kAbyssTotal;
        const double abyss_mean = static_cast<double>(kTrials) * abyss_probability;
        const double abyss_sigma = std::sqrt(static_cast<double>(kTrials)
            * abyss_probability * (1.0 - abyss_probability));
        if (!require(std::fabs(static_cast<double>(normal[index]) - normal_mean)
                    <= 6.0 * normal_sigma + 12.0, "normal material weight")
            || !require(std::fabs(static_cast<double>(abyss[index]) - abyss_mean)
                    <= 6.0 * abyss_sigma + 12.0, "abyss material weight")) return false;
    }
    // Rules are evaluated in descending coupon face value.  The expected
    // probabilities below therefore include the chance that every earlier
    // eligible tier missed, proving both probability and priority.
    constexpr std::array<double, 4U> kCouponProbabilities{{
        (1.0 - 0.0005) * (1.0 - 0.0020) * (1.0 - 0.0050) * 0.0100,
        (1.0 - 0.0005) * (1.0 - 0.0020) * 0.0050,
        (1.0 - 0.0005) * 0.0020,
        0.0005,
    }};
    for (std::size_t index = 0U; index < coupons.size(); ++index) {
        const double mean = static_cast<double>(kTrials) * kCouponProbabilities[index];
        const double sigma = std::sqrt(static_cast<double>(kTrials)
            * kCouponProbabilities[index] * (1.0 - kCouponProbabilities[index]));
        if (!require(std::fabs(static_cast<double>(coupons[index]) - mean)
                <= 6.0 * sigma + 12.0, "coupon probability and priority")) return false;
        const double abyss_tier = (index == 0U ? 0.0300
            : index == 1U ? 0.0150 : index == 2U ? 0.0060 : 0.0015);
        double earlier_miss = 1.0;
        for (std::size_t earlier = 3U; earlier > index; --earlier) {
            const double chance = earlier == 3U ? 0.0015
                : earlier == 2U ? 0.0060 : 0.0150;
            earlier_miss *= 1.0 - chance;
        }
        const double abyss_probability = earlier_miss * abyss_tier;
        const double abyss_mean = static_cast<double>(kTrials) * abyss_probability;
        const double abyss_sigma = std::sqrt(static_cast<double>(kTrials)
            * abyss_probability * (1.0 - abyss_probability));
        if (!require(std::fabs(static_cast<double>(abyss_coupons[index]) - abyss_mean)
                <= 6.0 * abyss_sigma + 12.0, "abyss coupon triple probability")) return false;
    }
    for (std::size_t index = 0U; index < normal.size(); ++index) {
        if (!require(normal[index] != 0U && abyss[index] != 0U,
                "all ten normal materials available")) return false;
    }
    for (const std::uint64_t count : coupons) {
        if (!require(count != 0U, "all four coupons available")) return false;
    }
    return true;
}

bool transaction_boundaries() noexcept {
    const auto rare = generated(1001U, 1U, items::ItemRarity::rare);
    const auto normal_a = generated(1002U, 2U, items::ItemRarity::normal);
    const auto normal_b = generated(1003U, 2U, items::ItemRarity::normal);
    const auto normal_c = generated(1004U, 2U, items::ItemRarity::normal);
    if (!rare || !normal_a || !normal_b || !normal_c) return false;

    auto built = dungeon::make_initial_run_state(0x16A0U, dungeon::DungeonRules{});
    if (built.fault != dungeon::DungeonFault::none) return false;
    auto state = built.state;
    state.item_ownership.items = {*rare, *normal_a, *normal_b, *normal_c};
    state.item_ownership.next_item_sequence = 2000U;
    const std::size_t chaos = items::material_index(items::MaterialId::chaos);
    const std::size_t coupon = items::material_index(items::MaterialId::coupon_12);
    state.item_ownership.materials[chaos] = 1U;
    state.item_ownership.materials[coupon] = 1U;
    dungeon::DungeonSession session{dungeon::DungeonRules{}, state};

    if (!require(session.request_craft(items::MaterialId::chaos, rare->id)
            == dungeon::RequestResult::accepted, "craft accepted")) return false;
    const auto craft_pending = session.pending_save();
    if (!require(craft_pending.has_value()
            && craft_pending->next_state.item_ownership.materials[chaos] == 0U
            && session.item_state().materials[chaos] == 1U,
            "craft atomic before commit")) return false;
    session.resolve_pending_save({dungeon::SaveDisposition::not_committed,
        craft_pending->expected_generation, craft_pending->next_state,
        craft_pending->kind});
    if (!require(session.item_state().materials[chaos] == 1U,
            "craft rollback preserves material")) return false;
    if (!require(session.request_craft(items::MaterialId::chaos, rare->id)
            == dungeon::RequestResult::accepted && commit(session),
            "craft committed")) return false;

    if (!require(session.request_coupon(items::MaterialId::coupon_12, rare->id)
            == dungeon::RequestResult::accepted && commit(session),
            "coupon committed")) return false;
    const auto coupon_item = std::find_if(session.item_state().items.begin(),
        session.item_state().items.end(), [&](const items::ItemInstance& item) {
            return item.id == rare->id;
        });
    if (!require(coupon_item != session.item_state().items.end()
            && coupon_item->reinforcement == 12U, "coupon face threshold")) return false;

    if (!require(session.request_recipe({{normal_a->id, normal_b->id,
            normal_c->id}}) == dungeon::RequestResult::accepted && commit(session),
            "three-to-one exact base")) return false;
    return true;
}

bool all_currency_and_reinforcement_boundaries() noexcept {
    const auto normal = generated(2001U, 1U, items::ItemRarity::normal);
    const auto rare = generated(2002U, 1U, items::ItemRarity::rare);
    if (!normal || !rare) return false;
    items::ItemInstance magic{};
    bool magic_ready = false;
    for (std::uint64_t nonce = 1U; nonce < 512U; ++nonce) {
        const auto result = items::craft_item({0x16C0U, nonce, *normal,
            items::MaterialId::transmute});
        if (result.applied && result.item.affix_count == 1U) {
            magic = result.item;
            magic_ready = true;
            break;
        }
    }
    if (!require(magic_ready, "transmute boundary")) return false;
    const auto augment = items::craft_item({0x16C1U, 1U, magic,
        items::MaterialId::augment});
    const auto regal = items::craft_item({0x16C2U, 1U, magic,
        items::MaterialId::regal});
    const auto chaos = items::craft_item({0x16C3U, 1U, *rare,
        items::MaterialId::chaos});
    const auto exalt = items::craft_item({0x16C4U, 1U, *rare,
        items::MaterialId::exalt});
    const auto annul = items::craft_item({0x16C5U, 1U, *rare,
        items::MaterialId::annul});
    const auto divine = items::craft_item({0x16C6U, 1U, *rare,
        items::MaterialId::divine});
    const auto scour = items::craft_item({0x16C7U, 1U, *rare,
        items::MaterialId::scour});
    bool directed = false;
    for (std::uint64_t nonce = 1U; nonce < 512U && !directed; ++nonce) {
        const auto result = items::craft_item({0x16C8U, nonce, *rare,
            items::MaterialId::directed, items::DirectedCategory::damage});
        directed = result.applied;
    }
    if (!require(augment.applied && regal.applied && chaos.applied
            && exalt.applied && annul.applied && divine.applied && scour.applied
            && directed, "all nine crafting currencies")) return false;

    constexpr std::array<std::uint32_t, 12U> targets{{1U, 3U, 4U, 5U,
        6U, 7U, 12U, 13U, 14U, 15U, 16U,
        (std::numeric_limits<std::uint32_t>::max)()}};
    constexpr std::array<std::uint16_t, 12U> chances{{10000U, 10000U,
        9000U, 8000U, 7000U, 7000U, 7000U, 6000U, 5700U, 5415U, 5144U,
        10U}};
    for (std::size_t index = 0U; index < targets.size(); ++index) {
        if (!require(items::reinforcement_success_chance_bp(targets[index])
                == chances[index], "reinforcement chance boundary")) return false;
    }
    if (!require(items::reinforcement_failure(6U)
            == items::ReinforcementFailure::unchanged
            && items::reinforcement_failure(7U)
                == items::ReinforcementFailure::reset_six
            && items::reinforcement_failure(10U)
                == items::ReinforcementFailure::reset_zero
            && items::reinforcement_failure(12U)
                == items::ReinforcementFailure::destroy,
            "reinforcement failure boundary")) return false;
    for (const auto coupon : {items::MaterialId::coupon_6,
            items::MaterialId::coupon_9, items::MaterialId::coupon_12,
            items::MaterialId::coupon_15}) {
        const auto applied = items::apply_coupon(*normal, coupon);
        if (!require(applied.has_value()
                && applied->reinforcement == items::coupon_reinforcement_level(coupon),
                "coupon face boundary")) return false;
    }
    return true;
}

struct ReinforcementCase final {
    std::uint32_t level;
    std::uint16_t chance_bp;
};

bool reinforcement_pipeline_matches_roll(
    ReinforcementCase sample) noexcept {
    const std::uint64_t pipeline_root = 0x1600U + sample.level;
    const std::uint64_t pipeline_item_id = 0x160000U + pipeline_root;
    auto built = dungeon::make_initial_run_state(
        pipeline_root, dungeon::DungeonRules{});
    const auto item = generated(
        pipeline_item_id, 1U, items::ItemRarity::normal);
    if (built.fault != dungeon::DungeonFault::none || !item) return false;
    auto state = built.state;
    auto reinforced = *item;
    reinforced.reinforcement = sample.level;
    state.item_ownership.items = {reinforced};
    state.item_ownership.next_item_sequence = reinforced.id + 1U;
    state.item_ownership.materials[items::material_index(
        items::MaterialId::reinforcement_stone)] = 1U;
    dungeon::DungeonSession pipeline{dungeon::DungeonRules{}, state};
    if (pipeline.request_reinforcement(reinforced.id)
            != dungeon::RequestResult::accepted) return false;
    const auto pipeline_pending = pipeline.pending_save();
    if (!pipeline_pending
            || !pipeline_pending->reinforcement_receipt.has_value()) {
        return false;
    }
    const auto& pipeline_receipt =
        *pipeline_pending->reinforcement_receipt;
    const bool expected_pipeline_success = dungeon::reinforcement_succeeds(
        state.root_seed, reinforced.id, reinforced.reinforcement,
        state.commit_generation, sample.chance_bp);
    return require(pipeline_receipt.success == expected_pipeline_success
            && pipeline_receipt.before == sample.level
            && pipeline_receipt.success_chance_bp == sample.chance_bp,
        "reinforcement production pipeline matches roll domain")
        && commit(pipeline);
}

bool reinforcement_long_run() noexcept {
    constexpr std::uint64_t kRuns = 75000U;
    constexpr std::array<ReinforcementCase, 4U> kCases{{
        {4U, 8000U}, {7U, 7000U}, {10U, 7000U}, {12U, 6000U}}};
    for (const ReinforcementCase sample : kCases) {
        if (!reinforcement_pipeline_matches_roll(sample)) return false;
        std::uint64_t success_count = 0U;
        std::uint64_t failure_count = 0U;
        for (std::uint64_t root = 1U; root <= kRuns; ++root) {
            const bool success = dungeon::reinforcement_succeeds(root,
                0x160000U + root, sample.level, 0U, sample.chance_bp);
            if (success) ++success_count;
            else ++failure_count;
        }
        const double probability = static_cast<double>(sample.chance_bp) / 10000.0;
        const double mean = static_cast<double>(kRuns) * probability;
        const double sigma = std::sqrt(static_cast<double>(kRuns)
            * probability * (1.0 - probability));
        std::cout << "reinforcement=" << sample.level << " successes="
                  << success_count << " failures=" << failure_count << '\n';
        if (!require(std::fabs(static_cast<double>(success_count) - mean)
                <= 6.0 * sigma + 12.0, "long-run reinforcement probability")) {
            return false;
        }
    }
    return true;
}

bool pickup_vacuum_death_reload_and_saturation() noexcept {
    auto built = dungeon::make_initial_run_state(0x16B0U, dungeon::DungeonRules{});
    if (built.fault != dungeon::DungeonFault::none) return false;
    auto session = std::make_unique<dungeon::DungeonSession>(
        dungeon::DungeonRules{}, built.state);
    const auto player = session->snapshot().combat->player.position;
    arpg::test::install_ground_material(
        *session, 4U, items::MaterialId::chaos, player);
    if (!require(session->request_material_pickup(4U)
            == dungeon::RequestResult::accepted && commit(*session),
            "material pickup committed")) return false;
    auto persisted = arpg::test::DungeonSessionTestAccess::stable_state(*session);
    persisted.item_ownership.items.clear();
    const auto durable_item = generated(7001U, 1U, items::ItemRarity::normal);
    if (!durable_item.has_value()) return false;
    auto reinforced = *durable_item;
    reinforced.reinforcement = 15U;
    persisted.item_ownership.items.push_back(reinforced);
    persisted.item_ownership.material_claimed_drop_bits[0U] |=
        std::uint64_t{1U} << 4U;
    const auto encoded = persistence::encode_checkpoint(persisted);
    if (!require(encoded.has_value(), "V7 checkpoint encodes")) return false;
    const auto decoded = persistence::decode_checkpoint(encoded->data(), encoded->size());
    if (!require(decoded.error == persistence::CodecError::none
            && !decoded.migrated
            && decoded.state.item_ownership.materials[
                items::material_index(items::MaterialId::chaos)] == 1U
            && (decoded.state.item_ownership.material_claimed_drop_bits[0U]
                & (std::uint64_t{1U} << 4U)) != 0U
            && decoded.state.item_ownership.items.size() == 1U
            && decoded.state.item_ownership.items[0].reinforcement == 15U,
            "V7 materials claims reinforcement round trip")) return false;
    auto reload = std::make_unique<dungeon::DungeonSession>(
        dungeon::DungeonRules{}, decoded.state);
    if (!require(reload->item_state().materials[
                items::material_index(items::MaterialId::chaos)] == 1U
            && reload->item_state().items[0].reinforcement == 15U,
            "decoded V7 rebuilds a valid session")) return false;

    auto vacuum_state = arpg::test::DungeonSessionTestAccess::stable_state(*session);
    auto vacuum = std::make_unique<dungeon::DungeonSession>(
        dungeon::DungeonRules{}, vacuum_state);
    arpg::test::install_ground_material(*vacuum, 7U, items::MaterialId::coupon_6,
        player, dungeon::GroundMaterialSource::monster_coupon);
    arpg::test::set_phase(*vacuum, dungeon::RoomPhase::combat);
    arpg::test::prepare_room_clear(*vacuum);
    if (!require(vacuum->pending_save().has_value()
            && vacuum->pending_save()->kind == dungeon::PendingSaveKind::room_clear
            && commit(*vacuum), "room clear vacuum")) return false;

    auto death_state = arpg::test::DungeonSessionTestAccess::stable_state(*vacuum);
    auto death = std::make_unique<dungeon::DungeonSession>(
        dungeon::DungeonRules{}, death_state);
    arpg::test::install_ground_material(
        *death, 8U, items::MaterialId::exalt, player);
    arpg::test::set_phase(*death, dungeon::RoomPhase::combat);
    if (!arpg::test::kill_current_player_through_combat(*death)) return false;
    arpg::test::DungeonSessionTestAccess::handle_player_defeat(*death);
    if (!require(death->pending_save().has_value() && commit(*death)
            && death->item_state().materials[items::material_index(items::MaterialId::exalt)]
                == 0U, "death loses unpicked material")) return false;

    return require(items::reinforced_base_value(1,
        (std::numeric_limits<std::uint32_t>::max)())
        == (std::numeric_limits<std::int64_t>::max)(), "reinforcement saturation");
}

}  // namespace

int main() {
    struct CouponThreshold final {
        items::MaterialId id;
        std::uint64_t depth;
        std::uint16_t score;
    };
    constexpr std::array<CouponThreshold, 4U> kCouponThresholds{{
        {items::MaterialId::coupon_6, 16U, 6U},
        {items::MaterialId::coupon_9, 30U, 10U},
        {items::MaterialId::coupon_12, 60U, 14U},
        {items::MaterialId::coupon_15, 90U, 18U},
    }};
    bool coupon_boundaries = true;
    for (const auto threshold : kCouponThresholds) {
        coupon_boundaries = coupon_boundaries
            && dungeon::coupon_eligible(threshold.id, threshold.depth,
                threshold.score)
            && !dungeon::coupon_eligible(threshold.id, threshold.depth - 1U,
                threshold.score)
            && !dungeon::coupon_eligible(threshold.id, threshold.depth,
                static_cast<std::uint16_t>(threshold.score - 1U));
    }
    const bool passed = distribution()
        && require(coupon_boundaries, "all coupon depth and score boundaries")
        && replay_keeps_existing_items_stable()
        && weighted_material_and_coupon_statistics()
        && transaction_boundaries()
        && all_currency_and_reinforcement_boundaries()
        && reinforcement_long_run()
        && pickup_vacuum_death_reload_and_saturation();
    std::cout << "stage16 simulation=" << (passed ? "PASS" : "FAIL") << '\n';
    return passed ? 0 : 1;
}

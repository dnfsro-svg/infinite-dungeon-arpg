#include "test_framework.hpp"

#include "passives/passive_tree_catalog.hpp"

#include <array>
#include <string>

namespace {

using namespace arpg::passives;
using arpg::modifiers::ModifierOperation;
using arpg::modifiers::StatId;

bool modifier_matches(const PassiveNode& node, std::size_t index, StatId stat,
    ModifierOperation operation, arpg::modifiers::FixedValue value) noexcept {
    return index < node.modifier_count
        && node.modifiers[index].stat == stat
        && node.modifiers[index].operation == operation
        && node.modifiers[index].value == value;
}

arpg::test::Failure catalog_has_exact_stable_shape() noexcept {
    const auto& nodes = passive_nodes();
    ARPG_REQUIRE(nodes.size() == 64U);
    ARPG_REQUIRE(nodes[0].id == 0U && nodes[0].type == PassiveNodeType::start);
    ARPG_REQUIRE(nodes[8].type == PassiveNodeType::connector);
    ARPG_REQUIRE(nodes[21].type == PassiveNodeType::keystone);
    ARPG_REQUIRE(nodes[63].type == PassiveNodeType::keystone);
    ARPG_REQUIRE(catalog_is_valid());
    ARPG_REQUIRE(nodes[0].neighbor_count == 11U);
    for (PassiveNodeId id = 1U; id <= 7U; ++id)
        ARPG_REQUIRE(nodes[0].neighbors[id - 1U] == id);
    ARPG_REQUIRE(nodes[0].neighbors[7] == 8U);
    ARPG_REQUIRE(nodes[0].neighbors[8] == 22U);
    ARPG_REQUIRE(nodes[0].neighbors[9] == 36U);
    ARPG_REQUIRE(nodes[0].neighbors[10] == 50U);
    const auto& expected_route_entries = std::array<PassiveNodeId, 4>{8U, 22U, 36U, 50U};
    for (const auto entry : expected_route_entries) {
        ARPG_REQUIRE(nodes[entry].type == PassiveNodeType::connector);
        ARPG_REQUIRE(nodes[entry].neighbor_count == 2U);
        ARPG_REQUIRE(nodes[entry].neighbors[0] == 0U);
        ARPG_REQUIRE(nodes[entry].neighbors[1] == entry + 1U);
    }
    const std::array<std::array<PassiveNodeId, 4>, 14> expected_neighbors{{
        {{0U, 1U, 0U, 0U}}, {{0U, 2U, 5U, 8U}}, {{1U, 3U, 0U, 0U}},
        {{2U, 4U, 0U, 0U}}, {{3U, 10U, 0U, 0U}}, {{1U, 6U, 0U, 0U}},
        {{5U, 7U, 0U, 0U}}, {{6U, 11U, 0U, 0U}}, {{1U, 9U, 0U, 0U}},
        {{8U, 12U, 0U, 0U}}, {{4U, 13U, 0U, 0U}}, {{7U, 0U, 0U, 0U}},
        {{9U, 0U, 0U, 0U}}, {{10U, 0U, 0U, 0U}}}};
    const std::array<std::uint8_t, 14> expected_neighbor_counts{{2U, 4U, 2U,
        2U, 2U, 2U, 2U, 2U, 2U, 2U, 2U, 1U, 1U, 1U}};
    const char* central_names[] = {"origin", "core_vitality", "core_guard",
        "core_strength", "core_cadence", "core_force", "core_stride", "core_aerial"};
    for (std::size_t id = 0; id < 8U; ++id) {
        ARPG_REQUIRE(std::string(nodes[id].name) == central_names[id]);
        const std::uint8_t expected_modifiers = id == 0U ? 0U
            : id == 2U || id == 7U ? 2U : 1U;
        ARPG_REQUIRE(nodes[id].modifier_count == expected_modifiers);
        if (id != 0U) {
            ARPG_REQUIRE(nodes[id].neighbor_count == 1U);
            ARPG_REQUIRE(nodes[id].neighbors[0] == 0U);
        }
    }
    const char* keystone_names[] = {"cinder_vow", "tide_bastion", "stormstep", "blood_pact"};
    const char* route_names[] = {"entry", "branch", "flat_a", "flat_b",
        "damage_a", "res_a", "res_b", "damage_b", "melee", "guard",
        "notable_damage", "notable_resistance", "notable_vitality", "keystone"};
    for (std::size_t route = 0; route < 4U; ++route) {
        const PassiveNodeId base = expected_route_entries[route];
        ARPG_REQUIRE(std::string(nodes[base + 13U].name) == keystone_names[route]);
        for (std::size_t offset = 0; offset < 14U; ++offset) {
            const auto& node = nodes[base + offset];
            const char* expected_name = offset == 13U
                ? keystone_names[route] : route_names[offset];
            ARPG_REQUIRE(std::string(node.name) == expected_name);
            const auto expected_type = offset < 2U ? PassiveNodeType::connector
                : offset >= 10U && offset <= 12U ? PassiveNodeType::notable
                : offset == 13U ? PassiveNodeType::keystone : PassiveNodeType::small;
            ARPG_REQUIRE(node.type == expected_type);
            const std::uint8_t expected_modifier_count = offset < 2U ? 0U
                : offset <= 9U ? 1U
                : offset <= 12U ? 2U
                : route == 2U ? 4U : route == 1U ? 3U : 2U;
            ARPG_REQUIRE(node.modifier_count == expected_modifier_count);
            ARPG_REQUIRE(node.neighbor_count == expected_neighbor_counts[offset]);
            for (std::size_t edge = 0; edge < node.neighbor_count; ++edge) {
                const PassiveNodeId expected = static_cast<PassiveNodeId>(base
                    + expected_neighbors[offset][edge]);
                const PassiveNodeId adjusted = expected_neighbors[offset][edge] == 0U
                    ? (offset == 0U ? 0U : base) : expected;
                ARPG_REQUIRE(node.neighbors[edge] == adjusted);
            }
        }
    }
    return {};
}

arpg::test::Failure catalog_has_symmetric_route_edges() noexcept {
    const auto& nodes = passive_nodes();
    for (const auto& node : nodes) {
        for (std::size_t index = 0; index < node.neighbor_count; ++index) {
            const auto neighbor = nodes[node.neighbors[index]];
            bool found = false;
            for (std::size_t reverse = 0; reverse < neighbor.neighbor_count; ++reverse) {
                found = found || neighbor.neighbors[reverse] == node.id;
            }
            ARPG_REQUIRE(found);
        }
    }
    return {};
}

arpg::test::Failure central_and_keystone_modifiers_match_spec() noexcept {
    const auto& nodes = passive_nodes();
    ARPG_REQUIRE(nodes[1].modifier_count == 1U);
    ARPG_REQUIRE(modifier_matches(nodes[1], 0U, StatId::max_health,
        ModifierOperation::flat, 20 * arpg::modifiers::kFixedOne));
    ARPG_REQUIRE(nodes[2].modifier_count == 2U);
    ARPG_REQUIRE(modifier_matches(nodes[2], 0U, StatId::max_barrier,
        ModifierOperation::flat, 10 * arpg::modifiers::kFixedOne));
    ARPG_REQUIRE(modifier_matches(nodes[2], 1U, StatId::damage_taken,
        ModifierOperation::increased, -300));
    ARPG_REQUIRE(modifier_matches(nodes[3], 0U, StatId::melee_damage,
        ModifierOperation::increased, 600));
    ARPG_REQUIRE(modifier_matches(nodes[4], 0U, StatId::attack_speed,
        ModifierOperation::increased, 500));
    ARPG_REQUIRE(modifier_matches(nodes[5], 0U, StatId::impulse_scale,
        ModifierOperation::increased, 1000));
    ARPG_REQUIRE(modifier_matches(nodes[6], 0U, StatId::move_speed,
        ModifierOperation::increased, 600));
    ARPG_REQUIRE(modifier_matches(nodes[7], 0U, StatId::jump_speed,
        ModifierOperation::increased, 1000));
    ARPG_REQUIRE(modifier_matches(nodes[7], 1U, StatId::air_control,
        ModifierOperation::increased, 1000));
    ARPG_REQUIRE(nodes[21].name != nullptr);

    const std::array<StatId, 4> flat_stats{{StatId::fire_flat_damage,
        StatId::water_flat_damage, StatId::lightning_flat_damage,
        StatId::chaos_flat_damage}};
    const std::array<StatId, 4> damage_stats{{StatId::fire_damage,
        StatId::water_damage, StatId::lightning_damage, StatId::chaos_damage}};
    const std::array<StatId, 4> damage_reduction_stats{{
        StatId::fire_damage_reduction, StatId::water_damage_reduction,
        StatId::lightning_damage_reduction, StatId::chaos_damage_reduction}};
    const std::array<PassiveNodeId, 4> route_bases{{8U, 22U, 36U, 50U}};
    for (std::size_t route = 0; route < route_bases.size(); ++route) {
        const auto base = route_bases[route];
        ARPG_REQUIRE(nodes[base + 2U].modifier_count == 1U);
        ARPG_REQUIRE(modifier_matches(nodes[base + 2U], 0U, flat_stats[route],
            ModifierOperation::flat, 3 * arpg::modifiers::kFixedOne));
        ARPG_REQUIRE(modifier_matches(nodes[base + 3U], 0U, flat_stats[route],
            ModifierOperation::flat, 3 * arpg::modifiers::kFixedOne));
        ARPG_REQUIRE(modifier_matches(nodes[base + 4U], 0U, damage_stats[route],
            ModifierOperation::increased, 800));
        ARPG_REQUIRE(modifier_matches(nodes[base + 5U], 0U,
            damage_reduction_stats[route],
            ModifierOperation::flat, 700));
        ARPG_REQUIRE(modifier_matches(nodes[base + 6U], 0U,
            damage_reduction_stats[route],
            ModifierOperation::flat, 700));
        ARPG_REQUIRE(modifier_matches(nodes[base + 7U], 0U, damage_stats[route],
            ModifierOperation::increased, 800));
        ARPG_REQUIRE(modifier_matches(nodes[base + 8U], 0U, StatId::melee_damage,
            ModifierOperation::increased, 400));
        ARPG_REQUIRE(modifier_matches(nodes[base + 9U], 0U, StatId::damage_taken,
            ModifierOperation::increased, -200));
        ARPG_REQUIRE(nodes[base + 10U].modifier_count == 2U);
        ARPG_REQUIRE(modifier_matches(nodes[base + 10U], 0U, flat_stats[route],
            ModifierOperation::flat, 8 * arpg::modifiers::kFixedOne));
        ARPG_REQUIRE(modifier_matches(nodes[base + 10U], 1U, damage_stats[route],
            ModifierOperation::increased, 1500));
        ARPG_REQUIRE(nodes[base + 11U].modifier_count == 2U);
        ARPG_REQUIRE(modifier_matches(nodes[base + 11U], 0U,
            damage_reduction_stats[route],
            ModifierOperation::flat, 1500));
        ARPG_REQUIRE(modifier_matches(nodes[base + 11U], 1U, StatId::max_barrier,
            ModifierOperation::flat, 8 * arpg::modifiers::kFixedOne));
        ARPG_REQUIRE(modifier_matches(nodes[base + 12U], 0U, StatId::max_health,
            ModifierOperation::flat, 12 * arpg::modifiers::kFixedOne));
        ARPG_REQUIRE(modifier_matches(nodes[base + 12U], 1U, StatId::move_speed,
            ModifierOperation::increased, 300));
    }
    ARPG_REQUIRE(nodes[21].modifier_count == 2U);
    ARPG_REQUIRE(modifier_matches(nodes[21], 0U, StatId::fire_damage,
        ModifierOperation::more, 13000));
    ARPG_REQUIRE(modifier_matches(nodes[21], 1U,
        StatId::water_damage_reduction,
        ModifierOperation::flat, -2000));
    ARPG_REQUIRE(nodes[35].modifier_count == 3U);
    ARPG_REQUIRE(modifier_matches(nodes[35], 0U, StatId::water_damage,
        ModifierOperation::more, 12500));
    ARPG_REQUIRE(modifier_matches(nodes[35], 1U, StatId::max_barrier,
        ModifierOperation::flat, 24 * arpg::modifiers::kFixedOne));
    ARPG_REQUIRE(modifier_matches(nodes[35], 2U, StatId::melee_damage,
        ModifierOperation::more, 8500));
    ARPG_REQUIRE(nodes[49].modifier_count == 4U);
    ARPG_REQUIRE(modifier_matches(nodes[49], 0U, StatId::lightning_damage,
        ModifierOperation::more, 12500));
    ARPG_REQUIRE(modifier_matches(nodes[49], 1U, StatId::move_speed,
        ModifierOperation::increased, 1200));
    ARPG_REQUIRE(modifier_matches(nodes[49], 2U, StatId::attack_speed,
        ModifierOperation::increased, 1200));
    ARPG_REQUIRE(modifier_matches(nodes[49], 3U, StatId::max_health_more,
        ModifierOperation::more, 8000));
    ARPG_REQUIRE(nodes[63].modifier_count == 2U);
    ARPG_REQUIRE(modifier_matches(nodes[63], 0U, StatId::chaos_damage,
        ModifierOperation::more, 13000));
    ARPG_REQUIRE(modifier_matches(nodes[63], 1U, StatId::damage_taken,
        ModifierOperation::more, 11500));
    return {};
}

arpg::test::Failure all_modifier_ids_are_stable_and_unique() noexcept {
    const auto& nodes = passive_nodes();
    std::array<std::uint32_t, 128> ids{};
    std::size_t count = 0U;
    for (const auto& node : nodes) {
        for (std::size_t index = 0; index < node.modifier_count; ++index) {
            const auto id = node.modifiers[index].id;
            ARPG_REQUIRE(id == 1000U + node.id * 3U + index);
            for (std::size_t prior = 0; prior < count; ++prior) {
                ARPG_REQUIRE(ids[prior] != id);
            }
            ids[count++] = id;
        }
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"catalog has exact stable shape", &catalog_has_exact_stable_shape},
    {"catalog has symmetric route edges", &catalog_has_symmetric_route_edges},
    {"central and keystone modifiers match spec", &central_and_keystone_modifiers_match_spec},
    {"modifier ids are stable and unique", &all_modifier_ids_are_stable_and_unique},
};

}  // namespace

arpg::test::TestSuite passive_tree_catalog_suite() noexcept {
    return arpg::test::make_suite("passive_tree_catalog", kCases);
}

#include "test_framework.hpp"

#include "passives/passive_tree_catalog.hpp"

#include <array>
#include <string>

namespace {

using namespace arpg::passives;
using arpg::modifiers::ModifierOperation;
using arpg::modifiers::StatId;

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
        if (id != 0U) {
            ARPG_REQUIRE(nodes[id].neighbor_count == 1U);
            ARPG_REQUIRE(nodes[id].neighbors[0] == 0U);
        }
    }
    const char* keystone_names[] = {"cinder_vow", "tide_bastion", "stormstep", "blood_pact"};
    for (std::size_t route = 0; route < 4U; ++route) {
        const PassiveNodeId base = expected_route_entries[route];
        ARPG_REQUIRE(std::string(nodes[base + 13U].name) == keystone_names[route]);
        for (std::size_t offset = 0; offset < 14U; ++offset) {
            const auto& node = nodes[base + offset];
            const auto expected_type = offset < 2U ? PassiveNodeType::connector
                : offset >= 10U && offset <= 12U ? PassiveNodeType::notable
                : offset == 13U ? PassiveNodeType::keystone : PassiveNodeType::small;
            ARPG_REQUIRE(node.type == expected_type);
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
    ARPG_REQUIRE(nodes[1].modifiers[0].stat == StatId::max_health);
    ARPG_REQUIRE(nodes[1].modifiers[0].value == 20 * arpg::modifiers::kFixedOne);
    ARPG_REQUIRE(nodes[2].modifiers[0].stat == StatId::max_barrier);
    ARPG_REQUIRE(nodes[2].modifiers[1].stat == StatId::damage_taken);
    ARPG_REQUIRE(nodes[3].modifier_count == 1U);
    ARPG_REQUIRE(nodes[3].modifiers[0].stat == StatId::melee_damage);
    ARPG_REQUIRE(nodes[4].modifiers[0].stat == StatId::attack_speed);
    ARPG_REQUIRE(nodes[5].modifiers[0].stat == StatId::impulse_scale);
    ARPG_REQUIRE(nodes[6].modifiers[0].stat == StatId::move_speed);
    ARPG_REQUIRE(nodes[7].modifier_count == 2U);
    ARPG_REQUIRE(nodes[7].modifiers[0].stat == StatId::jump_speed);
    ARPG_REQUIRE(nodes[7].modifiers[1].stat == StatId::air_control);
    ARPG_REQUIRE(nodes[21].name != nullptr);
    ARPG_REQUIRE(nodes[21].modifiers[0].stat == StatId::fire_damage);
    ARPG_REQUIRE(nodes[21].modifiers[0].operation == ModifierOperation::more);
    ARPG_REQUIRE(nodes[21].modifiers[0].value == 13000);
    ARPG_REQUIRE(nodes[21].modifiers[1].stat == StatId::water_resistance);
    ARPG_REQUIRE(nodes[21].modifiers[1].value == -2000);

    const std::array<StatId, 4> flat_stats{{StatId::fire_flat_damage,
        StatId::water_flat_damage, StatId::lightning_flat_damage,
        StatId::chaos_flat_damage}};
    const std::array<StatId, 4> damage_stats{{StatId::fire_damage,
        StatId::water_damage, StatId::lightning_damage, StatId::chaos_damage}};
    const std::array<StatId, 4> resistance_stats{{StatId::fire_resistance,
        StatId::water_resistance, StatId::lightning_resistance,
        StatId::chaos_resistance}};
    const std::array<PassiveNodeId, 4> route_bases{{8U, 22U, 36U, 50U}};
    for (std::size_t route = 0; route < route_bases.size(); ++route) {
        const auto base = route_bases[route];
        ARPG_REQUIRE(nodes[base + 2U].modifier_count == 1U);
        ARPG_REQUIRE(nodes[base + 2U].modifiers[0].stat == flat_stats[route]);
        ARPG_REQUIRE(nodes[base + 2U].modifiers[0].value == 3 * arpg::modifiers::kFixedOne);
        ARPG_REQUIRE(nodes[base + 3U].modifiers[0].stat == flat_stats[route]);
        ARPG_REQUIRE(nodes[base + 3U].modifiers[0].operation == ModifierOperation::flat);
        ARPG_REQUIRE(nodes[base + 3U].modifiers[0].value == 3 * arpg::modifiers::kFixedOne);
        ARPG_REQUIRE(nodes[base + 4U].modifiers[0].stat == damage_stats[route]);
        ARPG_REQUIRE(nodes[base + 4U].modifiers[0].operation == ModifierOperation::increased);
        ARPG_REQUIRE(nodes[base + 4U].modifiers[0].value == 800);
        ARPG_REQUIRE(nodes[base + 5U].modifiers[0].stat == resistance_stats[route]);
        ARPG_REQUIRE(nodes[base + 5U].modifiers[0].value == 700);
        ARPG_REQUIRE(nodes[base + 6U].modifiers[0].stat == resistance_stats[route]);
        ARPG_REQUIRE(nodes[base + 6U].modifiers[0].value == 700);
        ARPG_REQUIRE(nodes[base + 7U].modifiers[0].stat == damage_stats[route]);
        ARPG_REQUIRE(nodes[base + 7U].modifiers[0].value == 800);
        ARPG_REQUIRE(nodes[base + 8U].modifiers[0].stat == StatId::melee_damage);
        ARPG_REQUIRE(nodes[base + 8U].modifiers[0].value == 400);
        ARPG_REQUIRE(nodes[base + 9U].modifiers[0].stat == StatId::damage_taken);
        ARPG_REQUIRE(nodes[base + 9U].modifiers[0].value == -200);
        ARPG_REQUIRE(nodes[base + 10U].modifier_count == 2U);
        ARPG_REQUIRE(nodes[base + 10U].modifiers[0].stat == flat_stats[route]);
        ARPG_REQUIRE(nodes[base + 10U].modifiers[0].value == 8 * arpg::modifiers::kFixedOne);
        ARPG_REQUIRE(nodes[base + 10U].modifiers[1].stat == damage_stats[route]);
        ARPG_REQUIRE(nodes[base + 10U].modifiers[1].value == 1500);
        ARPG_REQUIRE(nodes[base + 11U].modifier_count == 2U);
        ARPG_REQUIRE(nodes[base + 11U].modifiers[0].stat == resistance_stats[route]);
        ARPG_REQUIRE(nodes[base + 11U].modifiers[0].value == 1500);
        ARPG_REQUIRE(nodes[base + 11U].modifiers[1].stat == StatId::max_barrier);
        ARPG_REQUIRE(nodes[base + 11U].modifiers[1].value == 8 * arpg::modifiers::kFixedOne);
        ARPG_REQUIRE(nodes[base + 12U].modifiers[0].stat == StatId::max_health);
        ARPG_REQUIRE(nodes[base + 12U].modifiers[0].value == 12 * arpg::modifiers::kFixedOne);
        ARPG_REQUIRE(nodes[base + 12U].modifiers[1].stat == StatId::move_speed);
        ARPG_REQUIRE(nodes[base + 12U].modifiers[1].value == 300);
    }
    ARPG_REQUIRE(nodes[35].modifiers[0].stat == StatId::water_damage);
    ARPG_REQUIRE(nodes[35].modifiers[1].stat == StatId::max_barrier);
    ARPG_REQUIRE(nodes[35].modifiers[2].stat == StatId::melee_damage);
    ARPG_REQUIRE(nodes[49].modifier_count == 4U);
    ARPG_REQUIRE(nodes[49].modifiers[0].stat == StatId::lightning_damage);
    ARPG_REQUIRE(nodes[49].modifiers[1].stat == StatId::move_speed);
    ARPG_REQUIRE(nodes[49].modifiers[2].stat == StatId::attack_speed);
    ARPG_REQUIRE(nodes[49].modifiers[3].stat == StatId::max_health_more);
    ARPG_REQUIRE(nodes[63].modifiers[0].stat == StatId::chaos_damage);
    ARPG_REQUIRE(nodes[63].modifiers[1].stat == StatId::damage_taken);
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

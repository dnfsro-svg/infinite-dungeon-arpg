#include "test_framework.hpp"

#include "passives/passive_tree_catalog.hpp"

#include <array>

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
    ARPG_REQUIRE(nodes[21].name != nullptr);
    ARPG_REQUIRE(nodes[21].modifiers[0].stat == StatId::fire_damage);
    ARPG_REQUIRE(nodes[21].modifiers[0].operation == ModifierOperation::more);
    ARPG_REQUIRE(nodes[21].modifiers[0].value == 13000);
    ARPG_REQUIRE(nodes[21].modifiers[1].stat == StatId::water_resistance);
    ARPG_REQUIRE(nodes[21].modifiers[1].value == -2000);
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

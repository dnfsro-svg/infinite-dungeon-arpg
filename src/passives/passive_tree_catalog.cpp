#include "passives/passive_tree_catalog.hpp"

#include "modifiers/damage_types.hpp"

#include <cstddef>

namespace arpg::passives {
namespace {

using modifiers::DamageType;
using modifiers::FixedValue;
using modifiers::Modifier;
using modifiers::ModifierOperation;
using modifiers::ModifierId;
using modifiers::StatId;

constexpr FixedValue kOne = modifiers::kFixedOne;

struct RouteSpec final {
    DamageType damage;
    const char* prefix;
    std::int16_t x;
};

constexpr RouteSpec kRoutes[] = {
    {DamageType::fire, "fire", -420},
    {DamageType::water, "water", 420},
    {DamageType::lightning, "lightning", -420},
    {DamageType::chaos, "chaos", 420},
};

StatId flat_stat(DamageType type) noexcept {
    switch (type) {
    case DamageType::fire: return StatId::fire_flat_damage;
    case DamageType::water: return StatId::water_flat_damage;
    case DamageType::lightning: return StatId::lightning_flat_damage;
    case DamageType::chaos: return StatId::chaos_flat_damage;
    default: return StatId::fire_flat_damage;
    }
}

StatId increased_stat(DamageType type) noexcept {
    switch (type) {
    case DamageType::fire: return StatId::fire_damage;
    case DamageType::water: return StatId::water_damage;
    case DamageType::lightning: return StatId::lightning_damage;
    case DamageType::chaos: return StatId::chaos_damage;
    default: return StatId::fire_damage;
    }
}

StatId resistance_stat(DamageType type) noexcept {
    switch (type) {
    case DamageType::fire: return StatId::fire_resistance;
    case DamageType::water: return StatId::water_resistance;
    case DamageType::lightning: return StatId::lightning_resistance;
    case DamageType::chaos: return StatId::chaos_resistance;
    default: return StatId::fire_resistance;
    }
}

void add_modifier(PassiveNode& node, StatId stat, ModifierOperation operation,
    FixedValue value) noexcept {
    const std::size_t index = node.modifier_count++;
    node.modifiers[index] = Modifier{
        static_cast<ModifierId>(1000U + node.id * 3U + index),
        stat, operation, value};
}

void add_neighbor(PassiveNode& node, PassiveNodeId neighbor) noexcept {
    node.neighbors[node.neighbor_count++] = neighbor;
}

void connect(std::array<PassiveNode, kPassiveNodeCount>& nodes,
    PassiveNodeId left, PassiveNodeId right) noexcept {
    add_neighbor(nodes[left], right);
    add_neighbor(nodes[right], left);
}

void initialize_node(PassiveNode& node, PassiveNodeId id,
    PassiveNodeType type, const char* name, std::int16_t x,
    std::int16_t y) noexcept {
    node = PassiveNode{};
    node.id = id;
    node.type = type;
    node.name = name;
    node.x = x;
    node.y = y;
}

std::array<PassiveNode, kPassiveNodeCount> make_catalog() noexcept {
    std::array<PassiveNode, kPassiveNodeCount> nodes{};
    initialize_node(nodes[0], 0U, PassiveNodeType::start, "origin", 0, 0);
    initialize_node(nodes[1], 1U, PassiveNodeType::small, "core_vitality", -90, -40);
    initialize_node(nodes[2], 2U, PassiveNodeType::small, "core_guard", 0, -90);
    initialize_node(nodes[3], 3U, PassiveNodeType::small, "core_strength", 90, -40);
    initialize_node(nodes[4], 4U, PassiveNodeType::small, "core_cadence", 90, 40);
    initialize_node(nodes[5], 5U, PassiveNodeType::small, "core_force", 0, 90);
    initialize_node(nodes[6], 6U, PassiveNodeType::small, "core_stride", -90, 40);
    initialize_node(nodes[7], 7U, PassiveNodeType::small, "core_aerial", -150, 0);
    add_modifier(nodes[1], StatId::max_health, ModifierOperation::flat, 20 * kOne);
    add_modifier(nodes[2], StatId::max_barrier, ModifierOperation::flat, 10 * kOne);
    add_modifier(nodes[2], StatId::damage_taken, ModifierOperation::increased, -300);
    add_modifier(nodes[3], StatId::melee_damage, ModifierOperation::increased, 600);
    add_modifier(nodes[4], StatId::attack_speed, ModifierOperation::increased, 500);
    add_modifier(nodes[5], StatId::impulse_scale, ModifierOperation::increased, 1000);
    add_modifier(nodes[6], StatId::move_speed, ModifierOperation::increased, 600);
    add_modifier(nodes[7], StatId::jump_speed, ModifierOperation::increased, 1000);
    add_modifier(nodes[7], StatId::air_control, ModifierOperation::increased, 1000);

    for (const auto& route : kRoutes) {
        const PassiveNodeId base = static_cast<PassiveNodeId>(8U
            + (&route - kRoutes) * 14U);
        const auto index = static_cast<std::size_t>(&route - kRoutes);
        const char* names[] = {"entry", "branch", "flat_a", "flat_b",
            "damage_a", "res_a", "res_b", "damage_b", "melee",
            "guard", "notable_damage", "notable_resistance",
            "notable_vitality", "keystone"};
        for (std::size_t offset = 0; offset < 14U; ++offset) {
            const PassiveNodeType type = offset == 0U || offset == 1U
                ? PassiveNodeType::connector
                : offset == 10U || offset == 11U || offset == 12U
                    ? PassiveNodeType::notable
                    : offset == 13U ? PassiveNodeType::keystone
                                    : PassiveNodeType::small;
            const std::int16_t y = static_cast<std::int16_t>(
                (static_cast<int>(offset % 3U) - 1) * 90);
            initialize_node(nodes[base + offset],
                static_cast<PassiveNodeId>(base + offset), type,
                names[offset], route.x + static_cast<std::int16_t>(
                    offset * (route.x < 0 ? 45 : -45)), y);
        }
        const char* keystone_name = index == 0U ? "cinder_vow"
            : index == 1U ? "tide_bastion"
            : index == 2U ? "stormstep" : "blood_pact";
        nodes[base + 13U].name = keystone_name;
        add_modifier(nodes[base + 2], flat_stat(route.damage),
            ModifierOperation::flat, 3 * kOne);
        add_modifier(nodes[base + 3], flat_stat(route.damage),
            ModifierOperation::flat, 3 * kOne);
        add_modifier(nodes[base + 4], increased_stat(route.damage),
            ModifierOperation::increased, 800);
        add_modifier(nodes[base + 5], resistance_stat(route.damage),
            ModifierOperation::flat, 700);
        add_modifier(nodes[base + 6], resistance_stat(route.damage),
            ModifierOperation::flat, 700);
        add_modifier(nodes[base + 7], increased_stat(route.damage),
            ModifierOperation::increased, 800);
        add_modifier(nodes[base + 8], StatId::melee_damage,
            ModifierOperation::increased, 400);
        add_modifier(nodes[base + 9], StatId::damage_taken,
            ModifierOperation::increased, -200);
        add_modifier(nodes[base + 10], flat_stat(route.damage),
            ModifierOperation::flat, 8 * kOne);
        add_modifier(nodes[base + 10], increased_stat(route.damage),
            ModifierOperation::increased, 1500);
        add_modifier(nodes[base + 11], resistance_stat(route.damage),
            ModifierOperation::flat, 1500);
        add_modifier(nodes[base + 11], StatId::max_barrier,
            ModifierOperation::flat, 8 * kOne);
        add_modifier(nodes[base + 12], StatId::max_health,
            ModifierOperation::flat, 12 * kOne);
        add_modifier(nodes[base + 12], StatId::move_speed,
            ModifierOperation::increased, 300);

        if (index == 0U) {
            add_modifier(nodes[base + 13], StatId::fire_damage,
                ModifierOperation::more, 13000);
            add_modifier(nodes[base + 13], StatId::water_resistance,
                ModifierOperation::flat, -2000);
        } else if (index == 1U) {
            add_modifier(nodes[base + 13], StatId::water_damage,
                ModifierOperation::more, 12500);
            add_modifier(nodes[base + 13], StatId::max_barrier,
                ModifierOperation::flat, 24 * kOne);
            add_modifier(nodes[base + 13], StatId::melee_damage,
                ModifierOperation::more, 8500);
        } else if (index == 2U) {
            add_modifier(nodes[base + 13], StatId::lightning_damage,
                ModifierOperation::more, 12500);
            add_modifier(nodes[base + 13], StatId::move_speed,
                ModifierOperation::increased, 1200);
            add_modifier(nodes[base + 13], StatId::attack_speed,
                ModifierOperation::increased, 1200);
            add_modifier(nodes[base + 13], StatId::max_health_more,
                ModifierOperation::more, 8000);
        } else {
            add_modifier(nodes[base + 13], StatId::chaos_damage,
                ModifierOperation::more, 13000);
            add_modifier(nodes[base + 13], StatId::damage_taken,
                ModifierOperation::more, 11500);
        }
    }

    for (PassiveNodeId id = 1U; id <= 7U; ++id) connect(nodes, 0U, id);
    for (const PassiveNodeId entry : {PassiveNodeId{8U}, PassiveNodeId{22U},
        PassiveNodeId{36U}, PassiveNodeId{50U}}) connect(nodes, 0U, entry);
    for (PassiveNodeId base : {PassiveNodeId{8U}, PassiveNodeId{22U},
        PassiveNodeId{36U}, PassiveNodeId{50U}}) {
        connect(nodes, base, static_cast<PassiveNodeId>(base + 1U));
        connect(nodes, static_cast<PassiveNodeId>(base + 1U),
            static_cast<PassiveNodeId>(base + 2U));
        connect(nodes, static_cast<PassiveNodeId>(base + 2U),
            static_cast<PassiveNodeId>(base + 3U));
        connect(nodes, static_cast<PassiveNodeId>(base + 3U),
            static_cast<PassiveNodeId>(base + 4U));
        connect(nodes, static_cast<PassiveNodeId>(base + 4U),
            static_cast<PassiveNodeId>(base + 10U));
        connect(nodes, static_cast<PassiveNodeId>(base + 10U),
            static_cast<PassiveNodeId>(base + 13U));
        connect(nodes, static_cast<PassiveNodeId>(base + 1U),
            static_cast<PassiveNodeId>(base + 5U));
        connect(nodes, static_cast<PassiveNodeId>(base + 5U),
            static_cast<PassiveNodeId>(base + 6U));
        connect(nodes, static_cast<PassiveNodeId>(base + 6U),
            static_cast<PassiveNodeId>(base + 7U));
        connect(nodes, static_cast<PassiveNodeId>(base + 7U),
            static_cast<PassiveNodeId>(base + 11U));
        connect(nodes, static_cast<PassiveNodeId>(base + 1U),
            static_cast<PassiveNodeId>(base + 8U));
        connect(nodes, static_cast<PassiveNodeId>(base + 8U),
            static_cast<PassiveNodeId>(base + 9U));
        connect(nodes, static_cast<PassiveNodeId>(base + 9U),
            static_cast<PassiveNodeId>(base + 12U));
    }
    return nodes;
}

}  // namespace

const std::array<PassiveNode, kPassiveNodeCount>& passive_nodes() noexcept {
    static const std::array<PassiveNode, kPassiveNodeCount> nodes = make_catalog();
    return nodes;
}

bool catalog_is_valid() noexcept {
    const auto& nodes = passive_nodes();
    std::array<bool, kPassiveNodeCount> visited{};
    std::array<PassiveNodeId, kPassiveNodeCount> queue{};
    std::array<bool, 2048U> modifier_seen{};
    std::size_t modifier_count = 0U;
    for (std::size_t index = 0; index < nodes.size(); ++index) {
        const auto& node = nodes[index];
        if (node.id != index || node.neighbor_count > kPassiveNeighborCapacity
            || node.modifier_count > kPassiveModifierCapacity) return false;
        for (std::size_t modifier = 0; modifier < node.modifier_count; ++modifier) {
            const auto id = node.modifiers[modifier].id;
            if (id != 1000U + node.id * 3U + modifier || id >= modifier_seen.size()
                || modifier_seen[id]) return false;
            modifier_seen[id] = true;
            ++modifier_count;
        }
        for (std::size_t neighbor = 0; neighbor < node.neighbor_count; ++neighbor) {
            const auto other = node.neighbors[neighbor];
            if (other >= nodes.size() || other == node.id) return false;
            for (std::size_t prior = 0; prior < neighbor; ++prior)
                if (node.neighbors[prior] == other) return false;
            bool reverse = false;
            for (std::size_t candidate = 0; candidate < nodes[other].neighbor_count; ++candidate)
                reverse = reverse || nodes[other].neighbors[candidate] == node.id;
            if (!reverse) return false;
        }
    }
    (void)modifier_count;
    std::size_t queue_head = 0U;
    std::size_t queue_tail = 0U;
    queue[queue_tail++] = 0U;
    visited.fill(false);
    visited[0] = true;
    while (queue_head < queue_tail) {
        const auto node = queue[queue_head++];
        for (std::size_t index = 0; index < nodes[node].neighbor_count; ++index) {
            const auto other = nodes[node].neighbors[index];
            if (!visited[other]) { visited[other] = true; queue[queue_tail++] = other; }
        }
    }
    for (const bool reachable : visited) if (!reachable) return false;
    return true;
}

}  // namespace arpg::passives

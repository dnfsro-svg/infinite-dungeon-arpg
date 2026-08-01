#include "dungeon/room_monster_plan_builder.hpp"

#include "combat/combat_collision.hpp"
#include "combat/monster_affix_catalog.hpp"
#include "combat/monster_affix_generation.hpp"
#include "combat/monster_catalog.hpp"
#include "combat/room_bounds.hpp"
#include "combat/room_spatial_grid.hpp"
#include "core/deterministic_rng.hpp"
#include "core/gameplay_limits.hpp"
#include "dungeon/encounter_director.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstddef>
#include <cstdint>
#include <cstring>
#include <limits>

namespace arpg::dungeon {
namespace {

namespace bounds = combat::room_bounds;
namespace spatial = combat::room_spatial;

constexpr std::uint64_t kCellOrderDomain = 0x524F4F4D5F43454CULL;
constexpr std::uint64_t kIdentityDomain = 0x524F4F4D5F49444EULL;
constexpr std::uint64_t kPositionDomain = 0x524F4F4D5F504F53ULL;
constexpr std::size_t kJitterAttempts = 32U;
constexpr std::size_t kFallbackSide = 17U;
constexpr combat::Vec3 kHoleCenter{0.0F, 3.5F, 0.0F};
constexpr std::array<combat::Vec3, 4U> kDoorCenters{{
    {bounds::min_x, 0.0F, 0.0F},
    {bounds::max_x, 0.0F, 0.0F},
    {0.0F, bounds::min_y, 0.0F},
    {0.0F, bounds::max_y, 0.0F},
}};

struct RoleCounts final {
    std::uint8_t high_priority{};
    std::uint8_t ranged{};
    std::uint8_t support{};
    std::uint8_t ground_hazard{};
    bool has_direct_target{};

    [[nodiscard]] bool fits(
        const combat::MonsterDefinition& monster) const noexcept {
        return (!combat::has_tag(monster, combat::MonsterTag::high_priority)
                   || high_priority == 0U)
            && (!combat::has_tag(monster, combat::MonsterTag::ranged)
                   || ranged == 0U)
            && (!combat::has_tag(monster, combat::MonsterTag::support)
                   || support == 0U)
            && (!combat::has_tag(monster, combat::MonsterTag::ground_hazard)
                   || ground_hazard == 0U);
    }

    void add(const combat::MonsterDefinition& monster) noexcept {
        high_priority += combat::has_tag(
            monster, combat::MonsterTag::high_priority);
        ranged += combat::has_tag(monster, combat::MonsterTag::ranged);
        support += combat::has_tag(monster, combat::MonsterTag::support);
        ground_hazard += combat::has_tag(
            monster, combat::MonsterTag::ground_hazard);
        has_direct_target = has_direct_target
            || combat::has_tag(monster, combat::MonsterTag::direct_target);
    }
};

struct Candidate final {
    const combat::MonsterDefinition* monster{};
    std::uint64_t weight{};
};

void clear_room_monster_plan(combat::RoomMonsterPlan& plan) noexcept {
    for (combat::RoomMonsterBlueprint& monster : plan.monsters) {
        monster = combat::RoomMonsterBlueprint{};
    }
    plan.cell_offsets.fill(0U);
    plan.cell_counts.fill(0U);
    plan.monster_count = 0U;
    plan.threat_total = 0U;
    plan.generator_version = 0U;
    plan.blueprint_hash = 0U;
}

[[nodiscard]] bool valid_ecology(
    checkpoint::DungeonElement ecology) noexcept {
    return static_cast<std::uint8_t>(ecology)
        < static_cast<std::uint8_t>(checkpoint::DungeonElement::chaos) + 1U;
}

[[nodiscard]] bool valid_entry(checkpoint::EntrySide entry) noexcept {
    switch (entry) {
    case checkpoint::EntrySide::initial:
    case checkpoint::EntrySide::top:
    case checkpoint::EntrySide::bottom:
    case checkpoint::EntrySide::left:
    case checkpoint::EntrySide::right:
        return true;
    }
    return false;
}

[[nodiscard]] combat::Vec3 entry_spawn(
    checkpoint::EntrySide entry) noexcept {
    switch (entry) {
    case checkpoint::EntrySide::initial:
        return {};
    case checkpoint::EntrySide::top:
        return {0.0F, bounds::min_y + 0.75F, 0.0F};
    case checkpoint::EntrySide::bottom:
        return {0.0F, bounds::max_y - 0.75F, 0.0F};
    case checkpoint::EntrySide::left:
        return {bounds::min_x + 1.50F, 0.0F, 0.0F};
    case checkpoint::EntrySide::right:
        return {bounds::max_x - 1.50F, 0.0F, 0.0F};
    }
    return {};
}

[[nodiscard]] float distance_squared(
    combat::Vec3 left,
    combat::Vec3 right) noexcept {
    const float x = left.x - right.x;
    const float y = left.y - right.y;
    return x * x + y * y;
}

[[nodiscard]] bool position_avoids_reserved_space(
    combat::Vec3 position,
    const checkpoint::RoomDescriptor& room) noexcept {
    if (distance_squared(position, entry_spawn(room.entry)) < 16.0F) {
        return false;
    }
    for (const combat::Vec3 door : kDoorCenters) {
        if (distance_squared(position, door) < 9.0F) return false;
    }
    return !room.has_hole
        || distance_squared(position, kHoleCenter) >= 9.0F;
}

[[nodiscard]] core::DeterministicRng ordinal_stream(
    std::uint64_t room_seed,
    std::uint32_t generator_version,
    std::uint64_t domain,
    std::uint16_t ordinal) noexcept {
    auto domain_stream = core::DeterministicRng::derive_stream(
        room_seed, domain);
    auto version_stream = core::DeterministicRng::derive_stream(
        domain_stream.next_u64(), generator_version);
    return core::DeterministicRng::derive_stream(
        version_stream.next_u64(), ordinal);
}

[[nodiscard]] const combat::MonsterDefinition* direct_target_fallback(
    checkpoint::DungeonElement ecology,
    const RoleCounts& counts) noexcept {
    const combat::MonsterDefinition* matching = nullptr;
    const combat::MonsterDefinition* any = nullptr;
    for (std::uint8_t raw = 0U;
         raw < static_cast<std::uint8_t>(combat::MonsterId::count); ++raw) {
        const auto* monster = combat::monster_definition(
            static_cast<combat::MonsterId>(raw));
        if (monster == nullptr
                || !combat::has_tag(
                    *monster, combat::MonsterTag::direct_target)
                || !counts.fits(*monster)) {
            continue;
        }
        if (any == nullptr || monster->threat_cost < any->threat_cost
                || (monster->threat_cost == any->threat_cost
                    && static_cast<std::uint8_t>(monster->id)
                        < static_cast<std::uint8_t>(any->id))) {
            any = monster;
        }
        if (monster->preferred_ecology == static_cast<std::uint8_t>(ecology)
                && (matching == nullptr
                    || monster->threat_cost < matching->threat_cost
                    || (monster->threat_cost == matching->threat_cost
                        && static_cast<std::uint8_t>(monster->id)
                            < static_cast<std::uint8_t>(matching->id)))) {
            matching = monster;
        }
    }
    return matching != nullptr ? matching : any;
}

[[nodiscard]] const combat::MonsterDefinition* select_identity(
    checkpoint::DungeonElement ecology,
    const EncounterDirectorConfig& config,
    const RoleCounts& counts,
    core::DeterministicRng& rng) noexcept {
    std::array<Candidate,
        static_cast<std::size_t>(combat::MonsterId::count)> candidates{};
    std::size_t candidate_count = 0U;
    std::uint64_t total_weight = 0U;
    for (std::uint8_t raw = 0U;
         raw < static_cast<std::uint8_t>(combat::MonsterId::count); ++raw) {
        const auto* monster = combat::monster_definition(
            static_cast<combat::MonsterId>(raw));
        if (monster == nullptr || !counts.fits(*monster)) continue;
        const std::uint64_t weight = detail::ecology_monster_weight(
            *monster, ecology, config);
        if (weight == 0U
                || total_weight
                    > (std::numeric_limits<std::uint64_t>::max)() - weight) {
            continue;
        }
        candidates[candidate_count++] = {monster, weight};
        total_weight += weight;
    }
    if (candidate_count == 0U || total_weight == 0U) return nullptr;
    std::uint64_t cursor = rng.next_bounded(total_weight).value_or(0U);
    std::size_t selected = 0U;
    while (selected + 1U < candidate_count
            && cursor >= candidates[selected].weight) {
        cursor -= candidates[selected++].weight;
    }
    return candidates[selected].monster;
}

[[nodiscard]] combat::Vec3 position_in_cell(
    std::uint16_t home_cell,
    std::uint16_t ordinal,
    const checkpoint::RoomDescriptor& room,
    std::uint32_t generator_version,
    const combat::RoomMonsterPlan& plan,
    std::uint16_t cell_begin,
    bool& found) noexcept {
    const std::size_t column = home_cell % spatial::columns;
    const std::size_t row = home_cell / spatial::columns;
    constexpr float kInsetX = 0.70F;
    constexpr float kInsetY = 0.50F;
    const float minimum_x = bounds::min_x
        + static_cast<float>(column) * spatial::cell_width + kInsetX;
    const float maximum_x = bounds::min_x
        + static_cast<float>(column + 1U) * spatial::cell_width - kInsetX;
    const float minimum_y = bounds::min_y
        + static_cast<float>(row) * spatial::cell_depth + kInsetY;
    const float maximum_y = bounds::min_y
        + static_cast<float>(row + 1U) * spatial::cell_depth - kInsetY;
    auto rng = ordinal_stream(
        room.seed, generator_version, kPositionDomain, ordinal);
    constexpr std::uint64_t kUnitBound = 1000000U;
    for (std::size_t attempt = 0U; attempt < kJitterAttempts; ++attempt) {
        const float x_fraction = static_cast<float>(
            rng.next_bounded(kUnitBound).value_or(0U))
            / static_cast<float>(kUnitBound);
        const float y_fraction = static_cast<float>(
            rng.next_bounded(kUnitBound).value_or(0U))
            / static_cast<float>(kUnitBound);
        const combat::Vec3 candidate{
            minimum_x + (maximum_x - minimum_x) * x_fraction,
            minimum_y + (maximum_y - minimum_y) * y_fraction,
            0.0F,
        };
        combat::RoomMonsterBlueprint candidate_monster{};
        candidate_monster.initial_position = candidate;
        const combat::Aabb candidate_bounds =
            combat::room_monster_initial_bounds(candidate_monster);
        bool overlaps_previous = false;
        for (std::uint16_t previous = cell_begin; previous < ordinal;
             ++previous) {
            if (combat::overlaps_inclusive(candidate_bounds,
                    combat::room_monster_initial_bounds(
                        plan.monsters[previous]))) {
                overlaps_previous = true;
                break;
            }
        }
        if (position_avoids_reserved_space(candidate, room)
                && !overlaps_previous) {
            found = true;
            return candidate;
        }
    }

    constexpr std::size_t kFallbackCount = kFallbackSide * kFallbackSide;
    const std::size_t start = ordinal % kFallbackCount;
    for (std::size_t offset = 0U; offset < kFallbackCount; ++offset) {
        const std::size_t sample = (start + offset) % kFallbackCount;
        const std::size_t sample_column = sample % kFallbackSide;
        const std::size_t sample_row = sample / kFallbackSide;
        const float x_fraction = (static_cast<float>(sample_column) + 0.5F)
            / static_cast<float>(kFallbackSide);
        const float y_fraction = (static_cast<float>(sample_row) + 0.5F)
            / static_cast<float>(kFallbackSide);
        const combat::Vec3 candidate{
            minimum_x + (maximum_x - minimum_x) * x_fraction,
            minimum_y + (maximum_y - minimum_y) * y_fraction,
            0.0F,
        };
        combat::RoomMonsterBlueprint candidate_monster{};
        candidate_monster.initial_position = candidate;
        const combat::Aabb candidate_bounds =
            combat::room_monster_initial_bounds(candidate_monster);
        bool overlaps_previous = false;
        for (std::uint16_t previous = cell_begin; previous < ordinal;
             ++previous) {
            if (combat::overlaps_inclusive(candidate_bounds,
                    combat::room_monster_initial_bounds(
                        plan.monsters[previous]))) {
                overlaps_previous = true;
                break;
            }
        }
        if (position_avoids_reserved_space(candidate, room)
                && !overlaps_previous) {
            found = true;
            return candidate;
        }
    }
    found = false;
    return {};
}

void hash_byte(std::uint64_t& hash, std::uint8_t value) noexcept {
    hash ^= value;
    hash *= 1099511628211ULL;
}

void hash_u16(std::uint64_t& hash, std::uint16_t value) noexcept {
    hash_byte(hash, static_cast<std::uint8_t>(value));
    hash_byte(hash, static_cast<std::uint8_t>(value >> 8U));
}

void hash_u32(std::uint64_t& hash, std::uint32_t value) noexcept {
    for (unsigned int shift = 0U; shift < 32U; shift += 8U) {
        hash_byte(hash, static_cast<std::uint8_t>(value >> shift));
    }
}

void hash_float(std::uint64_t& hash, float value) noexcept {
    if (value == 0.0F) value = 0.0F;
    std::uint32_t bits = 0U;
    static_assert(sizeof(bits) == sizeof(value));
    std::memcpy(&bits, &value, sizeof(bits));
    hash_u32(hash, bits);
}

[[nodiscard]] std::uint64_t blueprint_hash(
    const combat::RoomMonsterPlan& plan) noexcept {
    std::uint64_t hash = 14695981039346656037ULL;
    hash_u32(hash, plan.generator_version);
    hash_u16(hash, plan.monster_count);
    hash_u32(hash, plan.threat_total);
    for (const std::uint16_t offset : plan.cell_offsets) {
        hash_u16(hash, offset);
    }
    for (const std::uint8_t count : plan.cell_counts) hash_byte(hash, count);
    for (std::uint16_t ordinal = 0U; ordinal < plan.monster_count; ++ordinal) {
        const combat::RoomMonsterBlueprint& monster = plan.monsters[ordinal];
        hash_byte(hash, static_cast<std::uint8_t>(monster.id));
        hash_byte(hash, monster.affixes.count);
        for (std::size_t index = 0U; index < monster.affixes.count; ++index) {
            const combat::MonsterAffixInstance affix =
                monster.affixes.values[index];
            hash_byte(hash, static_cast<std::uint8_t>(affix.id));
            hash_byte(hash, static_cast<std::uint8_t>(affix.tier));
        }
        hash_float(hash, monster.initial_position.x);
        hash_float(hash, monster.initial_position.y);
        hash_float(hash, monster.initial_position.z);
        hash_u16(hash, monster.spawn_ordinal);
        hash_u16(hash, monster.home_cell);
        hash_byte(hash, monster.roaming_leash_cells);
    }
    return hash;
}

[[nodiscard]] bool same_blueprint_fields(
    const combat::RoomMonsterBlueprint& left,
    const combat::RoomMonsterBlueprint& right) noexcept {
    return left.id == right.id
        && left.affixes == right.affixes
        && left.initial_position.x == right.initial_position.x
        && left.initial_position.y == right.initial_position.y
        && left.initial_position.z == right.initial_position.z
        && left.spawn_ordinal == right.spawn_ordinal
        && left.home_cell == right.home_cell
        && left.roaming_leash_cells == right.roaming_leash_cells;
}

[[nodiscard]] bool affixes_legal(
    const combat::RoomMonsterBlueprint& monster,
    const combat::MonsterDefinition& definition) noexcept {
    if (monster.affixes.count > monster.affixes.values.size()) return false;
    for (std::size_t index = 0U; index < monster.affixes.count; ++index) {
        const combat::MonsterAffixInstance instance =
            monster.affixes.values[index];
        const auto* affix = combat::monster_affix_definition(instance.id);
        if (affix == nullptr
                || static_cast<std::uint8_t>(instance.tier)
                    >= static_cast<std::uint8_t>(
                        combat::MonsterAffixTier::count)
                || (definition.tags & affix->required_tags)
                    != affix->required_tags
                || (definition.tags & affix->forbidden_tags) != 0U) {
            return false;
        }
        for (std::size_t previous = 0U; previous < index; ++previous) {
            if (monster.affixes.values[previous].id == instance.id) {
                return false;
            }
        }
    }
    return true;
}

[[nodiscard]] RoomMonsterPlanBuildResult build_with_count(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    RoomDensityRoll density,
    std::uint16_t requested_count,
    combat::RoomMonsterPlan& out_plan) noexcept {
    clear_room_monster_plan(out_plan);
    density.total_count = requested_count;
    if (requested_count > limits::kRoomMonsterCapacity) {
        return {DungeonFault::population_capacity, density};
    }
    if (requested_count == 0U
            || generator_version == 0U
            || validate_rules(rules) != DungeonFault::none
            || !valid_ecology(room.ecology)
            || !valid_entry(room.entry)) {
        return {DungeonFault::invalid_rules, density};
    }

    std::array<std::uint16_t, combat::kRoomMonsterCellCount> cell_order{};
    for (std::size_t index = 0U; index < cell_order.size(); ++index) {
        cell_order[index] = static_cast<std::uint16_t>(index);
    }
    auto cell_rng = ordinal_stream(
        room.seed, generator_version, kCellOrderDomain, 0U);
    for (std::size_t remaining = cell_order.size(); remaining > 1U;
         --remaining) {
        const std::size_t selected = static_cast<std::size_t>(
            cell_rng.next_bounded(remaining).value_or(0U));
        std::swap(cell_order[remaining - 1U], cell_order[selected]);
    }
    const std::uint16_t base_count = static_cast<std::uint16_t>(
        requested_count / combat::kRoomMonsterCellCount);
    const std::uint16_t extra_count = static_cast<std::uint16_t>(
        requested_count % combat::kRoomMonsterCellCount);
    out_plan.cell_counts.fill(static_cast<std::uint8_t>(base_count));
    for (std::uint16_t index = 0U; index < extra_count; ++index) {
        ++out_plan.cell_counts[cell_order[index]];
    }
    std::uint16_t offset = 0U;
    for (std::size_t cell = 0U; cell < combat::kRoomMonsterCellCount;
         ++cell) {
        out_plan.cell_offsets[cell] = offset;
        offset = static_cast<std::uint16_t>(offset
            + out_plan.cell_counts[cell]);
    }
    out_plan.cell_offsets[combat::kRoomMonsterCellCount] = offset;

    std::uint32_t threat_total = 0U;
    for (std::size_t cell = 0U; cell < combat::kRoomMonsterCellCount;
         ++cell) {
        RoleCounts role_counts{};
        const std::uint16_t begin = out_plan.cell_offsets[cell];
        const std::uint16_t end = out_plan.cell_offsets[cell + 1U];
        for (std::uint16_t ordinal = begin; ordinal < end; ++ordinal) {
            auto identity_rng = ordinal_stream(
                room.seed, generator_version, kIdentityDomain, ordinal);
            const combat::MonsterDefinition* definition = select_identity(
                room.ecology, rules.encounter, role_counts, identity_rng);
            if (definition == nullptr) {
                clear_room_monster_plan(out_plan);
                return {DungeonFault::monster_selection_failed, density};
            }
            bool found_position = false;
            const combat::Vec3 position = position_in_cell(
                static_cast<std::uint16_t>(cell), ordinal, room,
                generator_version, out_plan, begin, found_position);
            if (!found_position) {
                clear_room_monster_plan(out_plan);
                return {DungeonFault::monster_placement, density};
            }
            out_plan.monsters[ordinal] = {
                definition->id,
                {},
                position,
                ordinal,
                static_cast<std::uint16_t>(cell),
                1U,
            };
            role_counts.add(*definition);
            threat_total += definition->threat_cost;
        }
        if (begin != end && !role_counts.has_direct_target) {
            const std::uint16_t last = static_cast<std::uint16_t>(end - 1U);
            RoleCounts replacement_counts{};
            for (std::uint16_t ordinal = begin; ordinal < last; ++ordinal) {
                const auto* definition = combat::monster_definition(
                    out_plan.monsters[ordinal].id);
                if (definition == nullptr) {
                    clear_room_monster_plan(out_plan);
                    return {
                        DungeonFault::monster_selection_failed, density};
                }
                replacement_counts.add(*definition);
            }
            const auto* previous = combat::monster_definition(
                out_plan.monsters[last].id);
            const auto* fallback = direct_target_fallback(
                room.ecology, replacement_counts);
            if (previous == nullptr || fallback == nullptr) {
                clear_room_monster_plan(out_plan);
                return {DungeonFault::monster_selection_failed, density};
            }
            threat_total -= previous->threat_cost;
            threat_total += fallback->threat_cost;
            out_plan.monsters[last].id = fallback->id;
        }
    }

    for (std::uint16_t ordinal = 0U; ordinal < requested_count; ++ordinal) {
        combat::RoomMonsterBlueprint& monster = out_plan.monsters[ordinal];
        const auto* definition = combat::monster_definition(monster.id);
        if (definition == nullptr) {
            clear_room_monster_plan(out_plan);
            return {DungeonFault::monster_selection_failed, density};
        }
        auto affixes = combat::generate_monster_affixes_for_ordinal(
            room.seed, room.depth, generator_version, ordinal, *definition);
        if (!affixes.has_value()) {
            clear_room_monster_plan(out_plan);
            return {DungeonFault::monster_selection_failed, density};
        }
        if (room.is_abyss) {
            affixes = combat::supplement_abyss_affixes_for_ordinal(
                room.seed, room.depth, generator_version, ordinal,
                *definition, *affixes);
        }
        if (!affixes.has_value()) {
            clear_room_monster_plan(out_plan);
            return {DungeonFault::monster_selection_failed, density};
        }
        monster.affixes = *affixes;
    }

    out_plan.monster_count = requested_count;
    out_plan.threat_total = threat_total;
    out_plan.generator_version = generator_version;
    out_plan.blueprint_hash = blueprint_hash(out_plan);
    if (!room_monster_plan_legal(room, out_plan)) {
        clear_room_monster_plan(out_plan);
        return {DungeonFault::invalid_monster_plan, density};
    }
    return {DungeonFault::none, density};
}

}  // namespace

RoomMonsterPlanBuildResult build_room_monster_plan(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    combat::RoomMonsterPlan& out_plan) noexcept {
    const RoomDensityRoll density = roll_room_density(
        room.seed, room.is_abyss);
    return build_with_count(room, rules, generator_version, density,
        density.total_count, out_plan);
}

bool room_monster_plan_legal(
    const checkpoint::RoomDescriptor& room,
    const combat::RoomMonsterPlan& plan) noexcept {
    if (!valid_ecology(room.ecology) || !valid_entry(room.entry)
            || plan.generator_version == 0U
            || plan.monster_count == 0U
            || plan.monster_count > limits::kRoomMonsterCapacity
            || plan.cell_offsets[0] != 0U
            || plan.cell_offsets.back() != plan.monster_count) {
        return false;
    }
    const std::uint16_t base_count = static_cast<std::uint16_t>(
        plan.monster_count / combat::kRoomMonsterCellCount);
    const std::uint16_t expected_extra = static_cast<std::uint16_t>(
        plan.monster_count % combat::kRoomMonsterCellCount);
    std::uint16_t actual_extra = 0U;
    std::uint32_t threat_total = 0U;
    for (std::size_t cell = 0U; cell < combat::kRoomMonsterCellCount;
         ++cell) {
        const std::uint16_t begin = plan.cell_offsets[cell];
        const std::uint16_t end = plan.cell_offsets[cell + 1U];
        if (begin > end || end > plan.monster_count
                || end - begin != plan.cell_counts[cell]
                || plan.cell_counts[cell] > spatial::maximum_monsters_per_cell
                || (plan.cell_counts[cell] != base_count
                    && plan.cell_counts[cell] != base_count + 1U)) {
            return false;
        }
        actual_extra += plan.cell_counts[cell] == base_count + 1U;
        RoleCounts roles{};
        const std::size_t column = cell % spatial::columns;
        const std::size_t row = cell / spatial::columns;
        const float minimum_x = bounds::min_x
            + static_cast<float>(column) * spatial::cell_width;
        const float maximum_x = minimum_x + spatial::cell_width;
        const float minimum_y = bounds::min_y
            + static_cast<float>(row) * spatial::cell_depth;
        const float maximum_y = minimum_y + spatial::cell_depth;
        for (std::uint16_t ordinal = begin; ordinal < end; ++ordinal) {
            const combat::RoomMonsterBlueprint& monster =
                plan.monsters[ordinal];
            const auto* definition = combat::monster_definition(monster.id);
            if (definition == nullptr || !roles.fits(*definition)
                    || !affixes_legal(monster, *definition)
                    || monster.spawn_ordinal != ordinal
                    || monster.home_cell != cell
                    || monster.roaming_leash_cells != 1U
                    || !std::isfinite(monster.initial_position.x)
                    || !std::isfinite(monster.initial_position.y)
                    || monster.initial_position.z != 0.0F
                    || monster.initial_position.x < minimum_x
                    || monster.initial_position.x > maximum_x
                    || monster.initial_position.y < minimum_y
                    || monster.initial_position.y > maximum_y
                    || !position_avoids_reserved_space(
                        monster.initial_position, room)) {
                return false;
            }
            const combat::Aabb initial_bounds =
                combat::room_monster_initial_bounds(monster);
            if (initial_bounds.minimum.x < minimum_x
                    || initial_bounds.maximum.x > maximum_x
                    || initial_bounds.minimum.y < minimum_y
                    || initial_bounds.maximum.y > maximum_y) {
                return false;
            }
            for (std::uint16_t previous = begin; previous < ordinal;
                 ++previous) {
                if (combat::overlaps_inclusive(initial_bounds,
                        combat::room_monster_initial_bounds(
                            plan.monsters[previous]))) {
                    return false;
                }
            }
            roles.add(*definition);
            threat_total += definition->threat_cost;
        }
        if (begin != end && !roles.has_direct_target) return false;
    }
    return actual_extra == expected_extra
        && threat_total == plan.threat_total
        && plan.blueprint_hash == blueprint_hash(plan);
}

bool room_monster_plan_equal_fields(
    const combat::RoomMonsterPlan& left,
    const combat::RoomMonsterPlan& right) noexcept {
    if (left.cell_offsets != right.cell_offsets
            || left.cell_counts != right.cell_counts
            || left.monster_count != right.monster_count
            || left.threat_total != right.threat_total
            || left.generator_version != right.generator_version
            || left.blueprint_hash != right.blueprint_hash) {
        return false;
    }
    for (std::uint16_t ordinal = 0U; ordinal < left.monster_count; ++ordinal) {
        if (!same_blueprint_fields(
                left.monsters[ordinal], right.monsters[ordinal])) {
            return false;
        }
    }
    return true;
}

namespace test_support {

RoomMonsterPlanBuildResult build_room_monster_plan_with_count(
    const checkpoint::RoomDescriptor& room,
    const DungeonRules& rules,
    std::uint32_t generator_version,
    std::uint16_t forced_count,
    combat::RoomMonsterPlan& out_plan) noexcept {
    RoomDensityRoll density = roll_room_density(room.seed, room.is_abyss);
    return build_with_count(room, rules, generator_version, density,
        forced_count, out_plan);
}

}  // namespace test_support

}  // namespace arpg::dungeon

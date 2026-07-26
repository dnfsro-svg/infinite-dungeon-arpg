#include "material_residency.hpp"

#include <algorithm>
#include <limits>

namespace arpg::platform {
namespace {

[[nodiscard]] constexpr bool is_known_atlas(MaterialAtlasId id) noexcept {
    return id < MaterialAtlasId::count;
}

[[nodiscard]] constexpr std::size_t atlas_index(MaterialAtlasId id) noexcept {
    return static_cast<std::size_t>(id);
}

[[nodiscard]] constexpr std::size_t saturating_add(
    std::size_t left, std::size_t right) noexcept {
    constexpr std::size_t kMaximum = (std::numeric_limits<std::size_t>::max)();
    return right > kMaximum - left ? kMaximum : left + right;
}

void require_room_atlases(MaterialResidencyRequest& request,
    dungeon::DungeonElement ecology) noexcept {
    switch (ecology) {
    case dungeon::DungeonElement::fire:
        request.require(MaterialAtlasId::fire_environment);
        request.require(MaterialAtlasId::fire_room_background);
        return;
    case dungeon::DungeonElement::water:
        request.require(MaterialAtlasId::water_environment);
        request.require(MaterialAtlasId::water_room_background);
        return;
    case dungeon::DungeonElement::lightning:
        request.require(MaterialAtlasId::lightning_environment);
        request.require(MaterialAtlasId::lightning_room_background);
        return;
    case dungeon::DungeonElement::chaos:
        request.require(MaterialAtlasId::chaos_environment);
        request.require(MaterialAtlasId::chaos_room_background);
        return;
    }
}

void require_monster_atlas(MaterialResidencyRequest& request,
    combat::MonsterId id) noexcept {
    switch (id) {
    case combat::MonsterId::fire_bomber:
        request.require(MaterialAtlasId::fire_bomber);
        return;
    case combat::MonsterId::fire_charger:
        request.require(MaterialAtlasId::fire_charger);
        return;
    case combat::MonsterId::water_bulwark:
        request.require(MaterialAtlasId::water_bulwark);
        return;
    case combat::MonsterId::water_support:
        request.require(MaterialAtlasId::water_support);
        return;
    case combat::MonsterId::lightning_shooter:
        request.require(MaterialAtlasId::lightning_shooter);
        return;
    case combat::MonsterId::lightning_dasher:
        request.require(MaterialAtlasId::lightning_dasher);
        return;
    case combat::MonsterId::chaos_chaser:
        request.require(MaterialAtlasId::chaos_chaser);
        return;
    case combat::MonsterId::chaos_hazard:
        request.require(MaterialAtlasId::chaos_hazard);
        return;
    case combat::MonsterId::count:
        return;
    }
}

}  // namespace

void MaterialResidencyRequest::require(MaterialAtlasId id) noexcept {
    if (!is_known_atlas(id)) return;
    atlases |= MaterialAtlasMask{1U} << atlas_index(id);
}

bool MaterialResidencyRequest::contains(MaterialAtlasId id) const noexcept {
    return is_known_atlas(id)
        && (atlases & (MaterialAtlasMask{1U} << atlas_index(id))) != 0U;
}

MaterialResidencyRequest base_material_residency_request() noexcept {
    MaterialResidencyRequest request{};
    request.require(MaterialAtlasId::environment);
    request.require(MaterialAtlasId::actors);
    request.require(MaterialAtlasId::effects_ui);
    request.require(MaterialAtlasId::player_locomotion);
    request.require(MaterialAtlasId::player_combo_a);
    request.require(MaterialAtlasId::player_combo_b);
    request.require(MaterialAtlasId::player_reaction);
    request.require(MaterialAtlasId::player_air);
    request.require(MaterialAtlasId::items_ui);
    request.require(MaterialAtlasId::ui_material);
    return request;
}

MaterialResidencyRequest make_material_residency_request(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    MaterialResidencyRequest request = base_material_residency_request();
    if (snapshot.has_active_room) {
        require_room_atlases(request, snapshot.ecology);
    }
    if (!snapshot.combat.has_value()) return request;
    const combat::CombatSnapshot& combat = *snapshot.combat;
    const std::size_t count = std::min(combat.monster_count, combat.monsters.size());
    for (std::size_t index{}; index < count; ++index) {
        const combat::MonsterSnapshot& monster = combat.monsters[index];
        if (monster.active) require_monster_atlas(request, monster.id);
    }
    return request;
}

std::size_t material_residency_bytes(
    const MaterialManifestDefinition& manifest,
    MaterialResidencyRequest request) noexcept {
    constexpr std::size_t kMaximum = (std::numeric_limits<std::size_t>::max)();
    if (manifest.atlas_count != 0U && manifest.atlases == nullptr) {
        return kMaximum;
    }
    std::size_t total{};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const MaterialAtlasDefinition& atlas = manifest.atlases[index];
        if (!is_known_atlas(atlas.id)) return kMaximum;
        if (!request.contains(atlas.id)) continue;
        total = saturating_add(total, saturating_add(
            atlas.rgba_bytes, atlas.rgba_bytes));
    }
    return total;
}

}  // namespace arpg::platform

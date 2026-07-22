#include "material_pack.hpp"

#include "material_asset_validation.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstdio>

namespace arpg::platform {
namespace {

[[nodiscard]] constexpr bool is_known_atlas(MaterialAtlasId id) noexcept {
    return id < MaterialAtlasId::count;
}

[[nodiscard]] constexpr bool is_known_sprite(MaterialSpriteId id) noexcept {
    return id > MaterialSpriteId::missing && id < MaterialSpriteId::count;
}

[[nodiscard]] constexpr std::size_t atlas_index(MaterialAtlasId id) noexcept {
    return static_cast<std::size_t>(id);
}

[[nodiscard]] constexpr MaterialAtlasId atlas_for_sprite(
    MaterialSpriteId id) noexcept {
    switch (id) {
    case MaterialSpriteId::environment_floor_fire:
    case MaterialSpriteId::environment_floor_lightning:
    case MaterialSpriteId::environment_floor_chaos:
    case MaterialSpriteId::environment_door_fire:
    case MaterialSpriteId::environment_door_lightning:
    case MaterialSpriteId::environment_door_chaos:
    case MaterialSpriteId::environment_hole:
        return MaterialAtlasId::environment;
    case MaterialSpriteId::environment_floor_water:
    case MaterialSpriteId::environment_door_water:
    case MaterialSpriteId::water_wall:
    case MaterialSpriteId::water_hole:
    case MaterialSpriteId::water_lantern:
    case MaterialSpriteId::water_coral:
    case MaterialSpriteId::water_grate:
        return MaterialAtlasId::water_environment;
    case MaterialSpriteId::fire_wall:
    case MaterialSpriteId::fire_torch:
    case MaterialSpriteId::fire_chain:
    case MaterialSpriteId::fire_banner:
    case MaterialSpriteId::fire_weapon_rack:
    case MaterialSpriteId::fire_bone_pile:
    case MaterialSpriteId::fire_breakable_crate:
    case MaterialSpriteId::fire_solid_brazier:
        return MaterialAtlasId::fire_environment;
    case MaterialSpriteId::fire_bomber_idle:
    case MaterialSpriteId::fire_bomber_move:
    case MaterialSpriteId::fire_bomber_telegraph:
    case MaterialSpriteId::fire_bomber_active:
    case MaterialSpriteId::fire_bomber_recovery:
    case MaterialSpriteId::fire_bomber_cooldown:
    case MaterialSpriteId::fire_bomber_defeated:
        return MaterialAtlasId::fire_bomber;
    case MaterialSpriteId::fire_charger_idle:
    case MaterialSpriteId::fire_charger_move:
    case MaterialSpriteId::fire_charger_telegraph:
    case MaterialSpriteId::fire_charger_active:
    case MaterialSpriteId::fire_charger_recovery:
    case MaterialSpriteId::fire_charger_cooldown:
    case MaterialSpriteId::fire_charger_defeated:
        return MaterialAtlasId::fire_charger;
    case MaterialSpriteId::water_bulwark_idle:
    case MaterialSpriteId::water_bulwark_move:
    case MaterialSpriteId::water_bulwark_telegraph:
    case MaterialSpriteId::water_bulwark_active:
    case MaterialSpriteId::water_bulwark_recovery:
    case MaterialSpriteId::water_bulwark_cooldown:
    case MaterialSpriteId::water_bulwark_defeated:
        return MaterialAtlasId::water_bulwark;
    case MaterialSpriteId::water_support_idle:
    case MaterialSpriteId::water_support_move:
    case MaterialSpriteId::water_support_telegraph:
    case MaterialSpriteId::water_support_active:
    case MaterialSpriteId::water_support_recovery:
    case MaterialSpriteId::water_support_cooldown:
    case MaterialSpriteId::water_support_defeated:
        return MaterialAtlasId::water_support;
    case MaterialSpriteId::effect_fire:
    case MaterialSpriteId::effect_water:
    case MaterialSpriteId::effect_lightning:
    case MaterialSpriteId::effect_chaos:
    case MaterialSpriteId::effect_hit_spark:
    case MaterialSpriteId::effect_launcher_trail:
    case MaterialSpriteId::effect_landing_dust:
    case MaterialSpriteId::effect_affix_aura:
    case MaterialSpriteId::loot_icon_normal:
    case MaterialSpriteId::loot_icon_magic:
    case MaterialSpriteId::loot_icon_rare:
    case MaterialSpriteId::loot_icon_abyss:
        return MaterialAtlasId::effects_ui;
    default:
        break;
    }
    return MaterialAtlasId::actors;
}

[[nodiscard]] const MaterialFrameDefinition* find_frame(
    const MaterialManifestDefinition& manifest, MaterialSpriteId id) noexcept {
    for (std::size_t index = 0U; index < manifest.frame_count; ++index) {
        if (manifest.frames[index].id == id) return &manifest.frames[index];
    }
    return nullptr;
}

[[nodiscard]] constexpr bool valid_texture_api(
    MaterialTextureApi texture_api) noexcept {
    return texture_api.load != nullptr && texture_api.valid != nullptr
        && texture_api.unload != nullptr && texture_api.draw != nullptr;
}

void draw_texture(Texture2D texture, Rectangle source, Rectangle destination,
    Vector2 origin, float rotation, Color tint) noexcept {
    DrawTexturePro(texture, source, destination, origin, rotation, tint);
}

[[nodiscard]] MaterialTextureApi default_material_texture_api() noexcept {
    return {&LoadTexture, &IsTextureValid, &UnloadTexture, &draw_texture};
}

}  // namespace

MaterialPack::MaterialPack() noexcept
    : MaterialPack(default_material_texture_api()) {}

MaterialPack::MaterialPack(MaterialTextureApi texture_api) noexcept
    : texture_api_(texture_api) {}

void MaterialPackState::set_available(MaterialAtlasId id, bool available) noexcept {
    if (!is_known_atlas(id)) return;
    available_[atlas_index(id)] = available;
}

bool MaterialPackState::available(MaterialAtlasId id) const noexcept {
    return is_known_atlas(id) && available_[atlas_index(id)];
}

bool MaterialPackState::can_draw(MaterialSpriteId id) const noexcept {
    return is_known_sprite(id) && available(atlas_for_sprite(id));
}

bool MaterialPackState::any_available() const noexcept {
    for (const bool available : available_) {
        if (available) return true;
    }
    return false;
}

void MaterialPackState::reset() noexcept {
    available_.fill(false);
}

bool MaterialPack::load() noexcept {
    unload();

    if (!valid_texture_api(texture_api_)) {
        TraceLog(LOG_WARNING,
            "Stage 12 material texture API is incomplete; using program fallback");
        return false;
    }

    const MaterialManifestDefinition manifest = default_material_manifest();
    if (!validate_material_manifest(manifest).valid) {
        TraceLog(LOG_WARNING, "Stage 12 material manifest is invalid; using program fallback");
        return false;
    }

    for (std::size_t index = 0U; index < manifest.atlas_count; ++index) {
        const MaterialAtlasDefinition& definition = manifest.atlases[index];
        const std::size_t texture_index = atlas_index(definition.id);
        std::array<char, 512> color_deployed_path{};
        std::array<char, 512> material_deployed_path{};
        const int color_written = std::snprintf(color_deployed_path.data(),
            color_deployed_path.size(), "%s%s", GetApplicationDirectory(),
            definition.color_path);
        const int material_written = std::snprintf(material_deployed_path.data(),
            material_deployed_path.size(), "%s%s", GetApplicationDirectory(),
            definition.material_path);
        const char* const color_path = color_written > 0
                && static_cast<std::size_t>(color_written)
                    < color_deployed_path.size()
            ? color_deployed_path.data() : definition.color_path;
        const char* const material_path = material_written > 0
                && static_cast<std::size_t>(material_written)
                    < material_deployed_path.size()
            ? material_deployed_path.data() : definition.material_path;
        Texture2D color_texture = texture_api_.load(color_path);
        Texture2D material_texture = texture_api_.load(material_path);
        const bool color_dimensions_match = texture_api_.valid(color_texture)
            && color_texture.width == definition.width
            && color_texture.height == definition.height;
        const bool material_dimensions_match = texture_api_.valid(material_texture)
            && material_texture.width == definition.width
            && material_texture.height == definition.height;
        if (!color_dimensions_match || !material_dimensions_match) {
            if (texture_api_.valid(color_texture)) {
                texture_api_.unload(color_texture);
            }
            if (texture_api_.valid(material_texture)) {
                texture_api_.unload(material_texture);
            }
            if (!warnings_emitted_[texture_index]) {
                TraceLog(LOG_WARNING,
                    "Stage 12 color/material atlas pair unavailable or has unexpected dimensions: %s | %s; using program fallback",
                    definition.color_path, definition.material_path);
                warnings_emitted_[texture_index] = true;
            }
            continue;
        }
        color_textures_[texture_index] = color_texture;
        material_textures_[texture_index] = material_texture;
        state_.set_available(definition.id, true);
    }
    return state_.any_available();
}

void MaterialPack::unload() noexcept {
    for (std::size_t index{}; index < color_textures_.size(); ++index) {
        if (valid_texture_api(texture_api_)
            && texture_api_.valid(color_textures_[index])) {
            texture_api_.unload(color_textures_[index]);
        }
        if (valid_texture_api(texture_api_)
            && texture_api_.valid(material_textures_[index])) {
            texture_api_.unload(material_textures_[index]);
        }
        color_textures_[index] = Texture2D{};
        material_textures_[index] = Texture2D{};
    }
    state_.reset();
}

bool MaterialPack::available(MaterialAtlasId id) const noexcept {
    return state_.available(id);
}

bool MaterialPack::can_draw(MaterialSpriteId id) const noexcept {
    if (!state_.can_draw(id) || !valid_texture_api(texture_api_)) return false;

    const MaterialManifestDefinition manifest = default_material_manifest();
    const MaterialFrameDefinition* const frame = find_frame(manifest, id);
    if (frame == nullptr || !state_.available(frame->atlas)) return false;

    const std::size_t index = atlas_index(frame->atlas);
    return texture_api_.valid(color_textures_[index])
        && texture_api_.valid(material_textures_[index]);
}

bool MaterialPack::draw(MaterialSpriteId id, Vector2 foot_position,
    bool flip_x, float scale, Color tint) const noexcept {
    if (!can_draw(id) || scale <= 0.0F) return false;

    const MaterialManifestDefinition manifest = default_material_manifest();
    const MaterialFrameDefinition* const frame = find_frame(manifest, id);
    if (frame == nullptr || !state_.available(frame->atlas)) return false;

    const std::size_t index = atlas_index(frame->atlas);
    const Texture2D& color_texture = color_textures_[index];
    const Texture2D& material_texture = material_textures_[index];

    Rectangle source = frame->source;
    if (flip_x) {
        source.x += source.width;
        source.width = -source.width;
    }
    const Rectangle destination{foot_position.x - frame->foot_anchor.x * scale,
        foot_position.y - frame->foot_anchor.y * scale,
        frame->source.width * scale, frame->source.height * scale};
    texture_api_.draw(color_texture, source, destination, {0.0F, 0.0F}, 0.0F, tint);
    texture_api_.draw(material_texture, source, destination, {0.0F, 0.0F},
        0.0F, Color{255U, 255U, 255U,
            static_cast<unsigned char>((std::min)(48U,
                static_cast<unsigned int>(tint.a)))});
    return true;
}

bool MaterialPack::draw_frame(MaterialAtlasId atlas, Rectangle source,
    Vector2 foot_anchor, Vector2 foot_position, bool flip_x, float scale,
    Color tint) const noexcept {
    if (!is_known_atlas(atlas) || !state_.available(atlas)
        || !valid_texture_api(texture_api_) || scale <= 0.0F
        || source.width <= 0.0F || source.height <= 0.0F
        || source.x < 0.0F || source.y < 0.0F) {
        return false;
    }
    const std::size_t index = atlas_index(atlas);
    const Texture2D& color_texture = color_textures_[index];
    const Texture2D& material_texture = material_textures_[index];
    if (!texture_api_.valid(color_texture)
        || !texture_api_.valid(material_texture)
        || source.x + source.width > static_cast<float>(color_texture.width)
        || source.y + source.height > static_cast<float>(color_texture.height)) {
        return false;
    }
    if (flip_x) {
        source.x += source.width;
        source.width = -source.width;
    }
    const Rectangle destination{foot_position.x - foot_anchor.x * scale,
        foot_position.y - foot_anchor.y * scale,
        source.width < 0.0F ? -source.width * scale : source.width * scale,
        source.height * scale};
    texture_api_.draw(color_texture, source, destination, {0.0F, 0.0F}, 0.0F, tint);
    texture_api_.draw(material_texture, source, destination, {0.0F, 0.0F},
        0.0F, Color{255U, 255U, 255U,
            static_cast<unsigned char>((std::min)(48U,
                static_cast<unsigned int>(tint.a)))});
    return true;
}

}  // namespace arpg::platform

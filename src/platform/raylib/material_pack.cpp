#include "material_pack.hpp"

#include "material_asset_validation.hpp"

#include <raylib.h>

namespace arpg::platform {
namespace {

constexpr const char* kAtlasPaths[] = {
    "assets/stage12/environment.png",
    "assets/stage12/actors.png",
    "assets/stage12/effects_ui.png",
};

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
    case MaterialSpriteId::environment_floor_water:
    case MaterialSpriteId::environment_floor_lightning:
    case MaterialSpriteId::environment_floor_chaos:
    case MaterialSpriteId::environment_door_fire:
    case MaterialSpriteId::environment_door_water:
    case MaterialSpriteId::environment_door_lightning:
    case MaterialSpriteId::environment_door_chaos:
    case MaterialSpriteId::environment_hole:
        return MaterialAtlasId::environment;
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
        && texture_api.unload != nullptr;
}

[[nodiscard]] MaterialTextureApi default_material_texture_api() noexcept {
    return {&LoadTexture, &IsTextureValid, &UnloadTexture};
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
        Texture2D texture = texture_api_.load(kAtlasPaths[texture_index]);
        const bool dimensions_match = texture_api_.valid(texture)
            && texture.width == definition.width && texture.height == definition.height;
        if (!dimensions_match) {
            if (texture_api_.valid(texture)) texture_api_.unload(texture);
            if (!warnings_emitted_[texture_index]) {
                TraceLog(LOG_WARNING,
                    "Stage 12 material atlas unavailable or has unexpected dimensions: %s; using program fallback",
                    kAtlasPaths[texture_index]);
                warnings_emitted_[texture_index] = true;
            }
            continue;
        }
        textures_[texture_index] = texture;
        state_.set_available(definition.id, true);
    }
    return state_.any_available();
}

void MaterialPack::unload() noexcept {
    for (Texture2D& texture : textures_) {
        if (valid_texture_api(texture_api_) && texture_api_.valid(texture)) {
            texture_api_.unload(texture);
        }
        texture = Texture2D{};
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

    return texture_api_.valid(textures_[atlas_index(frame->atlas)]);
}

bool MaterialPack::draw(MaterialSpriteId id, Vector2 foot_position,
    bool flip_x, float scale, Color tint) const noexcept {
    if (!can_draw(id) || scale <= 0.0F) return false;

    const MaterialManifestDefinition manifest = default_material_manifest();
    const MaterialFrameDefinition* const frame = find_frame(manifest, id);
    if (frame == nullptr || !state_.available(frame->atlas)) return false;

    const Texture2D& texture = textures_[atlas_index(frame->atlas)];

    Rectangle source = frame->source;
    if (flip_x) {
        source.x += source.width;
        source.width = -source.width;
    }
    const Rectangle destination{foot_position.x - frame->foot_anchor.x * scale,
        foot_position.y - frame->foot_anchor.y * scale,
        frame->source.width * scale, frame->source.height * scale};
    DrawTexturePro(texture, source, destination, {0.0F, 0.0F}, 0.0F, tint);
    return true;
}

}  // namespace arpg::platform

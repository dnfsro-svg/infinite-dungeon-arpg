#include "active_skill_assets.hpp"

#include <raylib.h>

#include <array>
#include <cstdio>

namespace arpg::platform {
namespace {

constexpr std::array<ActiveSkillAtlasDefinition,
    static_cast<std::size_t>(ActiveSkillAtlasId::count)> kAtlases{{
    {ActiveSkillAtlasId::draw_slash, "assets/skills/draw_slash_atlas.png",
        1254, 1254, 6U, 6U, 36U},
    {ActiveSkillAtlasId::storm_swords, "assets/skills/storm_swords_atlas.png",
        1024, 1536, 4U, 6U, 24U},
}};

[[nodiscard]] constexpr std::size_t atlas_index(ActiveSkillAtlasId id) noexcept {
    return static_cast<std::size_t>(id);
}

[[nodiscard]] constexpr bool known_atlas(ActiveSkillAtlasId id) noexcept {
    return atlas_index(id) < kAtlases.size();
}

}  // namespace

const ActiveSkillAtlasDefinition* active_skill_atlas_definition(
    ActiveSkillAtlasId id) noexcept {
    return known_atlas(id) ? &kAtlases[atlas_index(id)] : nullptr;
}

std::optional<ActiveSkillAtlasFrame> active_skill_atlas_frame(
    ActiveSkillAtlasId id, std::size_t frame_index) noexcept {
    const ActiveSkillAtlasDefinition* const definition =
        active_skill_atlas_definition(id);
    if (definition == nullptr || frame_index >= definition->frame_count) {
        return std::nullopt;
    }
    const float cell_width = static_cast<float>(definition->width)
        / static_cast<float>(definition->columns);
    const float cell_height = static_cast<float>(definition->height)
        / static_cast<float>(definition->rows);
    const std::size_t column = frame_index % definition->columns;
    const std::size_t row = frame_index / definition->columns;
    return ActiveSkillAtlasFrame{
        {static_cast<float>(column) * cell_width,
         static_cast<float>(row) * cell_height, cell_width, cell_height},
        {cell_width * 0.50F, cell_height * 0.94F},
        {cell_width * 0.64F, cell_height * 0.48F},
    };
}

bool ActiveSkillAssets::load() noexcept {
    unload();
    bool any_loaded = false;
    for (const ActiveSkillAtlasDefinition& definition : kAtlases) {
        std::array<char, 512> deployed_path{};
        const int written = std::snprintf(deployed_path.data(),
            deployed_path.size(), "%s%s", GetApplicationDirectory(),
            definition.path);
        const char* const path = written > 0
                && static_cast<std::size_t>(written) < deployed_path.size()
            ? deployed_path.data()
            : definition.path;
        Texture2D texture = LoadTexture(path);
        if (!IsTextureValid(texture) || texture.width != definition.width
            || texture.height != definition.height) {
            if (IsTextureValid(texture)) UnloadTexture(texture);
            TraceLog(LOG_WARNING,
                "Active skill atlas unavailable or has unexpected dimensions: %s",
                definition.path);
            continue;
        }
        textures_[atlas_index(definition.id)] = texture;
        any_loaded = true;
    }
    return any_loaded;
}

void ActiveSkillAssets::unload() noexcept {
    for (Texture2D& texture : textures_) {
        if (IsTextureValid(texture)) UnloadTexture(texture);
        texture = Texture2D{};
    }
}

bool ActiveSkillAssets::ready(ActiveSkillAtlasId id) const noexcept {
    return known_atlas(id) && IsTextureValid(textures_[atlas_index(id)]);
}

bool ActiveSkillAssets::draw(ActiveSkillAtlasId id, std::size_t frame_index,
    Vector2 foot_position, bool flip_x, float scale, Color tint) const noexcept {
    const auto frame = active_skill_atlas_frame(id, frame_index);
    if (!ready(id) || !frame.has_value() || scale <= 0.0F) return false;

    Rectangle source = frame->source;
    if (flip_x) {
        source.x += source.width;
        source.width = -source.width;
    }
    const Rectangle destination{
        foot_position.x - frame->foot_anchor.x * scale,
        foot_position.y - frame->foot_anchor.y * scale,
        frame->source.width * scale, frame->source.height * scale,
    };
    DrawTexturePro(textures_[atlas_index(id)], source, destination,
        {0.0F, 0.0F}, 0.0F, tint);
    return true;
}

}  // namespace arpg::platform

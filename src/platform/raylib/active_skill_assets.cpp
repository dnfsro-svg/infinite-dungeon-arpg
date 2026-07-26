#include "active_skill_assets.hpp"

#include "material_manifest.hpp"

namespace arpg::platform {
namespace {

struct ActiveSkillAtlasGrid final {
    MaterialAtlasId atlas{MaterialAtlasId::count};
    std::size_t columns{};
    std::size_t rows{};
    std::size_t frame_count{};
    float cell_width{};
    float cell_height{};
};

[[nodiscard]] constexpr ActiveSkillAtlasGrid active_skill_grid(
    skills::ActiveSkillId id) noexcept {
    switch (id) {
    case skills::ActiveSkillId::draw_slash:
        return {MaterialAtlasId::skill_draw_slash, 6U, 6U, 36U,
            209.0F, 209.0F};
    case skills::ActiveSkillId::storm_swords:
        return {MaterialAtlasId::skill_storm_swords, 4U, 6U, 24U,
            256.0F, 256.0F};
    case skills::ActiveSkillId::none:
    case skills::ActiveSkillId::count:
        return {};
    }
    return {};
}

[[nodiscard]] constexpr bool atlas_matches_manifest(
    MaterialAtlasId id, int width, int height) noexcept {
    const MaterialManifestDefinition manifest = default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const MaterialAtlasDefinition& atlas = manifest.atlases[index];
        if (atlas.id == id) {
            return atlas.width == width && atlas.height == height
                && atlas.rgba_bytes == static_cast<std::size_t>(width)
                    * static_cast<std::size_t>(height) * 4U;
        }
    }
    return false;
}

}  // namespace

MaterialAtlasId active_skill_material_atlas(
    skills::ActiveSkillId id) noexcept {
    return active_skill_grid(id).atlas;
}

std::optional<ActiveSkillAtlasFrame> active_skill_atlas_frame(
    skills::ActiveSkillId id, std::size_t frame_index) noexcept {
    const ActiveSkillAtlasGrid grid = active_skill_grid(id);
    if (grid.atlas == MaterialAtlasId::count
        || frame_index >= grid.frame_count) {
        return std::nullopt;
    }
    const std::size_t column = frame_index % grid.columns;
    const std::size_t row = frame_index / grid.columns;
    return ActiveSkillAtlasFrame{
        grid.atlas,
        {static_cast<float>(column) * grid.cell_width,
         static_cast<float>(row) * grid.cell_height,
         grid.cell_width, grid.cell_height},
        {grid.cell_width * 0.50F, grid.cell_height * 0.94F},
        {grid.cell_width * 0.64F, grid.cell_height * 0.48F},
    };
}

bool active_skill_assets_ready() noexcept {
    if (!atlas_matches_manifest(
            MaterialAtlasId::skill_draw_slash, 1254, 1254)
        || !atlas_matches_manifest(
            MaterialAtlasId::skill_storm_swords, 1024, 1536)) {
        return false;
    }
    for (std::size_t frame{}; frame < 36U; ++frame) {
        if (!active_skill_atlas_frame(
                skills::ActiveSkillId::draw_slash, frame).has_value()) {
            return false;
        }
    }
    for (std::size_t frame{}; frame < 24U; ++frame) {
        if (!active_skill_atlas_frame(
                skills::ActiveSkillId::storm_swords, frame).has_value()) {
            return false;
        }
    }
    return true;
}

}  // namespace arpg::platform

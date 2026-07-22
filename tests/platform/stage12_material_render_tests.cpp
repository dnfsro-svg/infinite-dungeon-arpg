#include "test_framework.hpp"

#include "items/item_types.hpp"
#include "material_asset_validation.hpp"
#include "material_manifest.hpp"
#include "material_pack.hpp"

#include <filesystem>

namespace {

arpg::test::Failure effect_manifest_stays_within_texture_budget() noexcept {
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(
        arpg::platform::default_material_manifest()).valid);
    return {};
}

arpg::test::Failure effect_sprites_require_effect_atlas() noexcept {
    arpg::platform::MaterialPackState state{};
    state.set_available(arpg::platform::MaterialAtlasId::actors, true);
    ARPG_REQUIRE(!state.can_draw(arpg::platform::MaterialSpriteId::effect_fire));
    state.set_available(arpg::platform::MaterialAtlasId::effects_ui, true);
    ARPG_REQUIRE(state.can_draw(arpg::platform::MaterialSpriteId::effect_fire));
    return {};
}

arpg::test::Failure effects_atlas_has_no_chroma_green_spill() noexcept {
    const std::filesystem::path path = std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}
        / "assets" / "stage12" / "effects_ui.png";
    const Image image = LoadImage(path.string().c_str());
    ARPG_REQUIRE(image.data != nullptr);
    for (int y = 0; y < image.height; ++y) {
        for (int x = 0; x < image.width; ++x) {
            const Color pixel = GetImageColor(image, x, y);
            ARPG_REQUIRE(pixel.a == 0U || !(pixel.g > 20U
                && pixel.g >= pixel.r + 8U && pixel.g >= pixel.b + 8U));
        }
    }
    UnloadImage(image);
    return {};
}

arpg::test::Failure declared_effect_frames_are_nonempty() noexcept {
    const std::filesystem::path path = std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}
        / "assets" / "stage12" / "effects_ui.png";
    const Image image = LoadImage(path.string().c_str());
    ARPG_REQUIRE(image.data != nullptr);
    const auto manifest = arpg::platform::default_material_manifest();
    std::size_t seen{};
    for (std::size_t index{}; index < manifest.frame_count; ++index) {
        const auto& frame = manifest.frames[index];
        if (frame.atlas != arpg::platform::MaterialAtlasId::effects_ui) continue;
        bool nonempty{};
        for (int y = static_cast<int>(frame.source.y);
             y < static_cast<int>(frame.source.y + frame.source.height) && !nonempty; ++y) {
            for (int x = static_cast<int>(frame.source.x);
                 x < static_cast<int>(frame.source.x + frame.source.width); ++x) {
                nonempty = GetImageColor(image, x, y).a != 0U;
                if (nonempty) break;
            }
        }
        ARPG_REQUIRE(nonempty);
        ++seen;
    }
    UnloadImage(image);
    ARPG_REQUIRE(seen == 8U);
    return {};
}

arpg::test::Failure loot_rarity_uses_distinct_material_icons() noexcept {
    ARPG_REQUIRE(arpg::platform::select_loot_sprite(
        arpg::items::ItemRarity::normal)
        != arpg::platform::select_loot_sprite(arpg::items::ItemRarity::rare));
    const auto manifest = arpg::platform::default_material_manifest();
    const auto* frame = arpg::platform::find_material_frame(manifest,
        arpg::platform::select_loot_sprite(arpg::items::ItemRarity::rare));
    ARPG_REQUIRE(frame != nullptr);
    ARPG_REQUIRE(frame->atlas == arpg::platform::MaterialAtlasId::items_ui);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"effect manifest texture budget", &effect_manifest_stays_within_texture_budget},
    {"loot rarity material icons", &loot_rarity_uses_distinct_material_icons},
    {"effect sprites use effects atlas", &effect_sprites_require_effect_atlas},
    {"effects atlas removes chroma spill", &effects_atlas_has_no_chroma_green_spill},
    {"declared effect frames are nonempty", &declared_effect_frames_are_nonempty},
};

}  // namespace

arpg::test::TestSuite stage12_material_render_suite() noexcept {
    return arpg::test::make_suite("stage12_material_render", kCases);
}

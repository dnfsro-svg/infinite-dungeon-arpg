#include "test_framework.hpp"

#include "material_asset_validation.hpp"
#include "material_animation.hpp"
#include "material_pack.hpp"
#include "material_residency.hpp"
#include "allocation_probe.hpp"

#include <array>
#include <cstdio>
#include <cstddef>
#include <cstring>
#include <filesystem>
#include <limits>
#include <string>

namespace {

using arpg::platform::MaterialAtlasDefinition;
using arpg::platform::MaterialLayer;
using arpg::platform::MaterialEcology;
using arpg::platform::MaterialClass;
using arpg::platform::AnimationClipId;
using arpg::platform::AnimationClipDefinition;
using arpg::platform::MaterialAtlasId;
using arpg::platform::MaterialFrameDefinition;
using arpg::platform::MaterialManifestDefinition;
using arpg::platform::MaterialCompositeParameters;
using arpg::platform::MaterialPackState;
using arpg::platform::MaterialSpriteId;

struct FakeMaterialTextures final {
    static constexpr std::size_t kTextureCapacity =
        static_cast<std::size_t>(MaterialAtlasId::count) * 2U;
    static constexpr std::size_t kCallHistoryCapacity =
        static_cast<std::size_t>(MaterialAtlasId::count) * 8U;
    static constexpr std::size_t kDrawCapacity = 4096U;
    enum class CallKind : std::uint8_t { load, unload };
    struct Call final {
        CallKind kind{};
    };
    std::array<Texture2D, kTextureCapacity> loaded{};
    std::array<std::array<char, 512>, kCallHistoryCapacity> loaded_paths{};
    std::array<unsigned int, kCallHistoryCapacity> unloaded_ids{};
    std::array<Call, kCallHistoryCapacity> calls{};
    std::array<unsigned int, kDrawCapacity> drawn_color_ids{};
    std::array<unsigned int, kDrawCapacity> drawn_material_ids{};
    std::array<Rectangle, kDrawCapacity> drawn_sources{};
    std::array<Rectangle, kDrawCapacity> drawn_destinations{};
    std::array<arpg::platform::MaterialScreenQuad,
        kDrawCapacity> drawn_quads{};
    std::array<float, kDrawCapacity> drawn_rotations{};
    std::array<Color, kDrawCapacity> drawn_tints{};
    std::array<MaterialCompositeParameters, kDrawCapacity> composites{};
    std::size_t load_count{};
    std::size_t unload_count{};
    std::size_t call_count{};
    std::size_t draw_count{};
    std::size_t pipeline_initialize_count{};
    std::size_t pipeline_shutdown_count{};
    bool pipeline_initialize_succeeds{true};
};

FakeMaterialTextures* g_fake_material_textures{};

Texture2D fake_load_texture(const char* path) noexcept {
    if (g_fake_material_textures == nullptr
        || g_fake_material_textures->load_count
            >= g_fake_material_textures->loaded.size()) {
        return {};
    }
    const std::size_t record_index = g_fake_material_textures->load_count++;
    std::snprintf(g_fake_material_textures->loaded_paths[record_index].data(),
        g_fake_material_textures->loaded_paths[record_index].size(), "%s", path);
    if (g_fake_material_textures->call_count
        < g_fake_material_textures->calls.size()) {
        g_fake_material_textures->calls[
            g_fake_material_textures->call_count++] = {
            FakeMaterialTextures::CallKind::load};
    }
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        if (std::strstr(path, manifest.atlases[index].color_path) != nullptr) {
            return g_fake_material_textures->loaded[index * 2U];
        }
        if (std::strstr(path, manifest.atlases[index].material_path) != nullptr) {
            return g_fake_material_textures->loaded[index * 2U + 1U];
        }
    }
    return {};
}

bool fake_texture_valid(Texture2D texture) noexcept {
    return texture.id != 0U;
}

void fake_unload_texture(Texture2D texture) noexcept {
    if (g_fake_material_textures == nullptr
        || g_fake_material_textures->unload_count
            >= g_fake_material_textures->unloaded_ids.size()) {
        return;
    }
    g_fake_material_textures->unloaded_ids[
        g_fake_material_textures->unload_count++] = texture.id;
    if (g_fake_material_textures->call_count
        < g_fake_material_textures->calls.size()) {
        g_fake_material_textures->calls[
            g_fake_material_textures->call_count++] = {
            FakeMaterialTextures::CallKind::unload};
    }
}

bool fake_initialize_material_pipeline() noexcept {
    if (g_fake_material_textures == nullptr) return false;
    ++g_fake_material_textures->pipeline_initialize_count;
    return g_fake_material_textures->pipeline_initialize_succeeds;
}

void fake_shutdown_material_pipeline() noexcept {
    if (g_fake_material_textures != nullptr) {
        ++g_fake_material_textures->pipeline_shutdown_count;
    }
}

void fake_draw_material(Texture2D color, Texture2D material,
    Rectangle source, Rectangle destination, Vector2, float rotation,
    Color tint,
    MaterialCompositeParameters parameters) noexcept {
    if (g_fake_material_textures == nullptr
        || g_fake_material_textures->draw_count
            >= g_fake_material_textures->drawn_color_ids.size()) return;
    const std::size_t index = g_fake_material_textures->draw_count++;
    g_fake_material_textures->drawn_color_ids[index] = color.id;
    g_fake_material_textures->drawn_material_ids[index] = material.id;
    g_fake_material_textures->drawn_sources[index] = source;
    g_fake_material_textures->drawn_destinations[index] = destination;
    g_fake_material_textures->drawn_rotations[index] = rotation;
    g_fake_material_textures->drawn_tints[index] = tint;
    g_fake_material_textures->composites[index] = parameters;
}

void fake_draw_material_quad(Texture2D color, Texture2D material,
    Rectangle source, arpg::platform::MaterialScreenQuad destination,
    Color tint, MaterialCompositeParameters parameters) noexcept {
    if (g_fake_material_textures == nullptr
        || g_fake_material_textures->draw_count
            >= g_fake_material_textures->drawn_color_ids.size()) return;
    const std::size_t index = g_fake_material_textures->draw_count++;
    g_fake_material_textures->drawn_color_ids[index] = color.id;
    g_fake_material_textures->drawn_material_ids[index] = material.id;
    g_fake_material_textures->drawn_sources[index] = source;
    g_fake_material_textures->drawn_quads[index] = destination;
    g_fake_material_textures->drawn_tints[index] = tint;
    g_fake_material_textures->composites[index] = parameters;
}

arpg::platform::MaterialTextureApi fake_material_texture_api() noexcept {
    return {&fake_load_texture, &fake_texture_valid, &fake_unload_texture,
        &fake_initialize_material_pipeline, &fake_shutdown_material_pipeline,
        &fake_draw_material, &fake_draw_material_quad};
}

std::size_t atlas_count_for_ecology(MaterialEcology ecology) noexcept {
    const auto manifest = arpg::platform::default_material_manifest();
    std::size_t count{};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        if (manifest.atlases[index].ecology == MaterialEcology::common
            || manifest.atlases[index].ecology == ecology) ++count;
    }
    return count;
}

std::size_t ecology_only_atlas_count(MaterialEcology ecology) noexcept {
    const auto manifest = arpg::platform::default_material_manifest();
    std::size_t count{};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        if (manifest.atlases[index].ecology == ecology) ++count;
    }
    return count;
}

arpg::test::Failure material_pack_falls_back_when_atlas_is_unavailable() noexcept {
    MaterialPackState state{};
    state.set_available(MaterialAtlasId::actors, false);
    ARPG_REQUIRE(!state.can_draw(MaterialSpriteId::player_idle));
    return {};
}

arpg::test::Failure material_pack_allows_repeated_shutdown() noexcept {
    MaterialPackState state{};
    state.reset();
    state.reset();
    ARPG_REQUIRE(!state.any_available());
    return {};
}

arpg::test::Failure material_pack_loads_missing_atlases_without_unloading() noexcept {
    FakeMaterialTextures fake{};
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(!pack.load());
    ARPG_REQUIRE(fake.load_count
        == atlas_count_for_ecology(MaterialEcology::common) * 2U);
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::environment));
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::actors));
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::effects_ui));
    ARPG_REQUIRE(fake.unload_count == 0U);
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_pack_loads_all_original_player_action_atlases() noexcept {
    FakeMaterialTextures fake{};
    for (std::size_t index = 3U; index <= 7U; ++index) {
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(301U + index), 1024, 1024, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(401U + index), 1024, 1024, 1, 7};
    }
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.load());
    ARPG_REQUIRE(pack.available(MaterialAtlasId::player_locomotion));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::player_combo_a));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::player_combo_b));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::player_reaction));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::player_air));
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_pack_rejects_unavailable_shader_pipeline() noexcept {
    FakeMaterialTextures fake{};
    fake.pipeline_initialize_succeeds = false;
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(!pack.load(MaterialEcology::water));
    ARPG_REQUIRE(!pack.material_pipeline_ready());
    ARPG_REQUIRE(!pack.ecology_ready(MaterialEcology::water));
    ARPG_REQUIRE(fake.pipeline_initialize_count == 1U);
    ARPG_REQUIRE(fake.load_count == 0U);
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_pack_ecology_ready_requires_every_texture_pair() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(5000U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(5001U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        if (atlas.id == MaterialAtlasId::water_support) {
            fake.loaded[index * 2U + 1U] = {};
        }
    }
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.load(MaterialEcology::water));
    ARPG_REQUIRE(pack.material_pipeline_ready());
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::water_support));
    ARPG_REQUIRE(!pack.ecology_ready(MaterialEcology::water));
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_pack_loads_and_draws_color_material_pairs() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(1000U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(1001U + index * 2U),
            atlas.width, atlas.height, 1, 7};
    }
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.load(MaterialEcology::water));
    ARPG_REQUIRE(pack.material_pipeline_ready());
    ARPG_REQUIRE(pack.ecology_ready(MaterialEcology::water));
    ARPG_REQUIRE(fake.load_count
        == atlas_count_for_ecology(MaterialEcology::water) * 2U);
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::fire_environment));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::water_environment));
    ARPG_REQUIRE(pack.draw_frame(MaterialAtlasId::water_bulwark,
        {0.0F, 0.0F, 96.0F, 96.0F}, {48.0F, 93.0F},
        {100.0F, 100.0F}, false));
    ARPG_REQUIRE(fake.draw_count == 1U);
    const std::size_t water_index = static_cast<std::size_t>(
        MaterialAtlasId::water_bulwark);
    ARPG_REQUIRE(fake.drawn_color_ids[0] == 1000U + water_index * 2U);
    ARPG_REQUIRE(fake.drawn_material_ids[0] == 1001U + water_index * 2U);
    ARPG_REQUIRE(fake.composites[0].roughness_channel == 0U);
    ARPG_REQUIRE(fake.composites[0].emissive_channel == 1U);
    ARPG_REQUIRE(fake.composites[0].metalness_channel == 2U);
    ARPG_REQUIRE(fake.composites[0].roughness_strength > 0.0F);
    ARPG_REQUIRE(fake.composites[0].metalness_strength > 0.0F);
    ARPG_REQUIRE(fake.composites[0].emissive_strength > 0.0F);
    pack.unload();
    ARPG_REQUIRE(fake.unload_count
        == atlas_count_for_ecology(MaterialEcology::water) * 2U);
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_pack_records_successful_item_sprite_draws() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(3000U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(3001U + index * 2U),
            atlas.width, atlas.height, 1, 7};
    }
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.load(MaterialEcology::fire));
    ARPG_REQUIRE(pack.sprite_draw_count(MaterialSpriteId::item_weapon) == 0U);
    ARPG_REQUIRE(pack.draw(MaterialSpriteId::item_weapon,
        {100.0F, 100.0F}, false, 0.25F));
    ARPG_REQUIRE(pack.draw(MaterialSpriteId::item_weapon,
        {120.0F, 100.0F}, false, 0.25F));
    ARPG_REQUIRE(pack.draw(MaterialSpriteId::material_chaos,
        {140.0F, 100.0F}, false, 0.25F));
    ARPG_REQUIRE(pack.sprite_draw_count(MaterialSpriteId::item_weapon) == 2U);
    ARPG_REQUIRE(pack.sprite_draw_count(MaterialSpriteId::material_chaos) == 1U);
    ARPG_REQUIRE(pack.sprite_draw_count(MaterialSpriteId::missing) == 0U);
    pack.unload();
    ARPG_REQUIRE(pack.sprite_draw_count(MaterialSpriteId::item_weapon) == 0U);
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure transformed_sprite_draw_forwards_rotation_and_tint() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(3500U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(3501U + index * 2U),
            atlas.width, atlas.height, 1, 7};
    }
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.load(MaterialEcology::fire));
    const Color tint{113U, 97U, 83U, 211U};
    ARPG_REQUIRE(pack.draw_transformed(MaterialSpriteId::item_weapon,
        {100.0F, 100.0F}, false, 0.25F, 270.0F, tint));
    ARPG_REQUIRE(fake.draw_count == 1U);
    ARPG_REQUIRE(fake.drawn_rotations[0U] == 270.0F);
    ARPG_REQUIRE(fake.drawn_tints[0U].r == tint.r);
    ARPG_REQUIRE(fake.drawn_tints[0U].g == tint.g);
    ARPG_REQUIRE(fake.drawn_tints[0U].b == tint.b);
    ARPG_REQUIRE(fake.drawn_tints[0U].a == tint.a);
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure atlas_region_draw_forwards_exact_destination() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(3700U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(3701U + index * 2U),
            atlas.width, atlas.height, 1, 7};
    }
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.load(MaterialEcology::water));
    const Rectangle source{512.0F, 288.0F, 256.0F, 144.0F};
    const Rectangle destination{-25.0F, 31.0F, 640.0F, 360.0F};
    ARPG_REQUIRE(pack.draw_frame_to(
        MaterialAtlasId::water_room_background, source, destination));
    ARPG_REQUIRE(fake.draw_count == 1U);
    ARPG_REQUIRE(std::memcmp(&fake.drawn_sources[0U],
        &source, sizeof(Rectangle)) == 0);
    ARPG_REQUIRE(std::memcmp(&fake.drawn_destinations[0U],
        &destination, sizeof(Rectangle)) == 0);
    const arpg::platform::MaterialScreenQuad quad{
        {-25.0F, 31.0F}, {-40.0F, 391.0F},
        {640.0F, 391.0F}, {615.0F, 31.0F}};
    ARPG_REQUIRE(pack.draw_frame_quad(
        MaterialAtlasId::water_room_background, source, quad));
    ARPG_REQUIRE(fake.draw_count == 2U);
    ARPG_REQUIRE(std::memcmp(&fake.drawn_sources[1U],
        &source, sizeof(Rectangle)) == 0);
    ARPG_REQUIRE(std::memcmp(&fake.drawn_quads[1U],
        &quad, sizeof(quad)) == 0);
    const arpg::platform::MaterialScreenQuad subpixel_quad{
        {0.0F, 0.0F}, {0.0F, 0.001F},
        {0.001F, 0.001F}, {0.001F, 0.0F}};
    ARPG_REQUIRE(pack.draw_frame_quad(
        MaterialAtlasId::water_room_background, source, subpixel_quad));
    const arpg::platform::MaterialScreenQuad zero_area_quad{
        {0.0F, 0.0F}, {1.0F, 1.0F}, {2.0F, 2.0F}, {1.0F, 1.0F}};
    ARPG_REQUIRE(!pack.draw_frame_quad(
        MaterialAtlasId::water_room_background, source, zero_area_quad));
    const arpg::platform::MaterialScreenQuad concave_quad{
        {0.0F, 0.0F}, {1.0F, 5.0F}, {2.0F, 2.0F}, {4.0F, 1.0F}};
    ARPG_REQUIRE(!pack.draw_frame_quad(
        MaterialAtlasId::water_room_background, source, concave_quad));
    const arpg::platform::MaterialScreenQuad bow_tie_quad{
        {0.0F, 0.0F}, {0.0F, 1.0F}, {4.0F, 5.0F}, {1.0F, 4.0F}};
    ARPG_REQUIRE(!pack.draw_frame_quad(
        MaterialAtlasId::water_room_background, source, bow_tie_quad));
    ARPG_REQUIRE(fake.draw_count == 3U);
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_pack_nine_slice_preserves_panel_corners() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(6000U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(6001U + index * 2U),
            atlas.width, atlas.height, 1, 7};
    }
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.load(MaterialEcology::fire));
    ARPG_REQUIRE(pack.draw_nine_slice(
        MaterialSpriteId::ui_inventory_panel_grid,
        {10.0F, 20.0F, 800.0F, 600.0F}, 32.0F));
    ARPG_REQUIRE(fake.draw_count > 9U);
    ARPG_REQUIRE(pack.sprite_draw_count(
        MaterialSpriteId::ui_inventory_panel_grid) == 1U);
    std::size_t preserved_centerpieces{};
    for (std::size_t index{}; index < fake.draw_count; ++index) {
        const Rectangle source = fake.drawn_sources[index];
        const Rectangle destination = fake.drawn_destinations[index];
        const bool pure_background_sample = source.width == 1.0F
            && source.height == 1.0F;
        if (!pure_background_sample) {
            ARPG_REQUIRE(arpg::test::near(
                source.width / source.height,
                destination.width / destination.height));
        }
        if (arpg::test::near(source.width, 64.0F)
                && arpg::test::near(source.height, 64.0F)
                && arpg::test::near(destination.width, 64.0F)
                && arpg::test::near(destination.height, 64.0F)) {
            ++preserved_centerpieces;
        }
    }
    ARPG_REQUIRE(preserved_centerpieces == 1U);

    const MaterialFrameDefinition* const warning_frame =
        arpg::platform::find_material_frame(
            manifest, MaterialSpriteId::ui_warning_modal);
    ARPG_REQUIRE(warning_frame != nullptr);
    const std::size_t before_warning = fake.draw_count;
    ARPG_REQUIRE(pack.draw_nine_slice(
        MaterialSpriteId::ui_warning_modal,
        {20.0F, 30.0F, 720.0F, 390.0F}, 32.0F));
    ARPG_REQUIRE(fake.draw_count > before_warning);
    const Rectangle warning_background = fake.drawn_sources[before_warning];
    ARPG_REQUIRE(arpg::test::near(warning_background.width, 1.0F));
    ARPG_REQUIRE(arpg::test::near(warning_background.height, 1.0F));
    ARPG_REQUIRE(arpg::test::near(warning_background.x,
        warning_frame->source.x + 64.0F));
    ARPG_REQUIRE(arpg::test::near(warning_background.y,
        warning_frame->source.y + 64.0F));
    std::size_t warning_centerpieces{};
    for (std::size_t index = before_warning; index < fake.draw_count; ++index) {
        const Rectangle source = fake.drawn_sources[index];
        const Rectangle destination = fake.drawn_destinations[index];
        if (arpg::test::near(source.width, 64.0F)
                && arpg::test::near(source.height, 64.0F)
                && arpg::test::near(destination.width, 64.0F)
                && arpg::test::near(destination.height, 64.0F)) {
            ++warning_centerpieces;
        }
    }
    ARPG_REQUIRE(warning_centerpieces == 0U);
    const std::size_t before_label = fake.draw_count;
    ARPG_REQUIRE(pack.draw_region_fit(MaterialSpriteId::ui_label_plate,
        {4.0F, 30.0F, 120.0F, 67.0F},
        {100.0F, 120.0F, 190.0F, 30.0F}));
    ARPG_REQUIRE(fake.draw_count == before_label + 1U);
    const Rectangle label_source = fake.drawn_sources[before_label];
    const Rectangle label_destination = fake.drawn_destinations[before_label];
    ARPG_REQUIRE(arpg::test::near(label_source.width / label_source.height,
        label_destination.width / label_destination.height));
    ARPG_REQUIRE(arpg::test::near(label_destination.height, 30.0F));
    ARPG_REQUIRE(label_destination.width < 60.0F);
    ARPG_REQUIRE(pack.sprite_draw_count(MaterialSpriteId::ui_label_plate) == 1U);
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_pack_horizontal_slice_preserves_decorated_caps() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(7000U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(7001U + index * 2U),
            atlas.width, atlas.height, 1, 7};
    }
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.load(MaterialEcology::fire));
    ARPG_REQUIRE(pack.draw_horizontal_slice(
        MaterialSpriteId::ui_pause_row_selected,
        {4.0F, 41.0F, 120.0F, 46.0F}, 24.0F,
        {100.0F, 120.0F, 712.0F, 20.0F}));
    ARPG_REQUIRE(fake.draw_count == 3U);
    for (const std::size_t index : {0U, 2U}) {
        const Rectangle source = fake.drawn_sources[index];
        const Rectangle destination = fake.drawn_destinations[index];
        ARPG_REQUIRE(arpg::test::near(source.width / source.height,
            destination.width / destination.height));
    }
    ARPG_REQUIRE(fake.drawn_sources[1].width <= 8.0F);
    ARPG_REQUIRE(pack.sprite_draw_count(
        MaterialSpriteId::ui_pause_row_selected) == 1U);
    ARPG_REQUIRE(pack.direct_stretch_draw_count(
        MaterialSpriteId::ui_pause_row_selected) == 0U);
    ARPG_REQUIRE(pack.draw_to(MaterialSpriteId::ui_pause_row_selected,
        {100.0F, 150.0F, 712.0F, 20.0F}));
    ARPG_REQUIRE(pack.direct_stretch_draw_count(
        MaterialSpriteId::ui_pause_row_selected) == 1U);
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_pack_switches_ecology_without_reloading_common() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(2000U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(2001U + index * 2U),
            atlas.width, atlas.height, 1, 7};
    }
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.load(MaterialEcology::fire));
    ARPG_REQUIRE(pack.material_pipeline_ready());
    ARPG_REQUIRE(pack.ecology_ready(MaterialEcology::fire));
    ARPG_REQUIRE(pack.current_ecology() == MaterialEcology::fire);
    ARPG_REQUIRE(pack.available(MaterialAtlasId::environment));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::fire_bomber));
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::water_bulwark));
    const std::size_t fire_loads = fake.load_count;
    ARPG_REQUIRE(fire_loads
        == atlas_count_for_ecology(MaterialEcology::fire) * 2U);
    ARPG_REQUIRE(pack.load(MaterialEcology::fire));
    ARPG_REQUIRE(fake.load_count == fire_loads);
    ARPG_REQUIRE(fake.unload_count == 0U);

    ARPG_REQUIRE(pack.load(MaterialEcology::water));
    ARPG_REQUIRE(pack.ecology_ready(MaterialEcology::water));
    ARPG_REQUIRE(!pack.ecology_ready(MaterialEcology::fire));
    ARPG_REQUIRE(pack.current_ecology() == MaterialEcology::water);
    ARPG_REQUIRE(pack.available(MaterialAtlasId::environment));
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::fire_bomber));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::water_bulwark));
    const std::size_t fire_pair_count =
        ecology_only_atlas_count(MaterialEcology::fire) * 2U;
    const std::size_t water_pair_count =
        ecology_only_atlas_count(MaterialEcology::water) * 2U;
    const std::size_t lightning_pair_count =
        ecology_only_atlas_count(MaterialEcology::lightning) * 2U;
    ARPG_REQUIRE(fake.unload_count == fire_pair_count);
    ARPG_REQUIRE(fake.load_count == fire_loads + water_pair_count);

    ARPG_REQUIRE(pack.load(MaterialEcology::lightning));
    ARPG_REQUIRE(pack.ecology_ready(MaterialEcology::lightning));
    ARPG_REQUIRE(!pack.ecology_ready(MaterialEcology::water));
    ARPG_REQUIRE(pack.current_ecology() == MaterialEcology::lightning);
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::water_bulwark));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::lightning_environment));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::lightning_shooter));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::lightning_dasher));
    ARPG_REQUIRE(fake.unload_count == fire_pair_count + water_pair_count);
    ARPG_REQUIRE(fake.load_count
        == fire_loads + water_pair_count + lightning_pair_count);

    ARPG_REQUIRE(pack.load(MaterialEcology::chaos));
    ARPG_REQUIRE(pack.ecology_ready(MaterialEcology::chaos));
    ARPG_REQUIRE(!pack.ecology_ready(MaterialEcology::lightning));
    ARPG_REQUIRE(pack.current_ecology() == MaterialEcology::chaos);
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::lightning_shooter));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::chaos_environment));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::chaos_chaser));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::chaos_hazard));
    ARPG_REQUIRE(fake.unload_count
        == fire_pair_count + water_pair_count + lightning_pair_count);
    ARPG_REQUIRE(fake.load_count == fire_loads + water_pair_count
        + lightning_pair_count
        + ecology_only_atlas_count(MaterialEcology::chaos) * 2U);
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_residency_request_selects_drawable_room_and_monster_atlases() noexcept {
    static_assert(static_cast<std::size_t>(MaterialAtlasId::count) <= 64U);
    arpg::dungeon::DungeonSnapshot snapshot{};
    snapshot.has_active_room = true;
    snapshot.ecology = arpg::dungeon::DungeonElement::fire;
    snapshot.combat.emplace();
    snapshot.combat->monster_count = 3U;
    snapshot.combat->monsters[0].active = true;
    snapshot.combat->monsters[0].id = arpg::combat::MonsterId::chaos_chaser;
    snapshot.combat->monsters[0].hp = 1;
    snapshot.combat->monsters[1].active = true;
    snapshot.combat->monsters[1].id = arpg::combat::MonsterId::fire_bomber;
    snapshot.combat->monsters[1].hp = 0;
    snapshot.combat->monsters[2].active = false;
    snapshot.combat->monsters[2].id = arpg::combat::MonsterId::water_support;

    const auto request = arpg::platform::make_material_residency_request(snapshot);
    ARPG_REQUIRE(request.contains(MaterialAtlasId::fire_environment));
    ARPG_REQUIRE(request.contains(MaterialAtlasId::fire_room_background));
    ARPG_REQUIRE(request.contains(MaterialAtlasId::chaos_chaser));
    ARPG_REQUIRE(request.contains(MaterialAtlasId::fire_bomber));
    ARPG_REQUIRE(!request.contains(MaterialAtlasId::water_support));
    ARPG_REQUIRE(!request.contains(MaterialAtlasId::chaos_environment));
    ARPG_REQUIRE(arpg::platform::material_residency_bytes(
        arpg::platform::default_material_manifest(), request)
        <= 256U * 1024U * 1024U);
    return {};
}

arpg::test::Failure material_residency_requests_only_equipped_draw_slash()
    noexcept {
    arpg::dungeon::DungeonSnapshot snapshot{};
    snapshot.skill_loadout.slots[0].active =
        arpg::skills::ActiveSkillId::draw_slash;
    const auto request = arpg::platform::make_material_residency_request(snapshot);
    ARPG_REQUIRE(request.contains(MaterialAtlasId::skill_draw_slash));
    ARPG_REQUIRE(!request.contains(MaterialAtlasId::skill_storm_swords));
    return {};
}

arpg::test::Failure material_residency_requests_both_equipped_skill_atlases_under_budget()
    noexcept {
    arpg::dungeon::DungeonSnapshot snapshot{};
    snapshot.has_active_room = true;
    snapshot.ecology = arpg::dungeon::DungeonElement::fire;
    snapshot.skill_loadout.slots[0].active =
        arpg::skills::ActiveSkillId::draw_slash;
    snapshot.skill_loadout.slots[1].active =
        arpg::skills::ActiveSkillId::storm_swords;
    snapshot.combat.emplace();
    constexpr std::array<arpg::combat::MonsterId, 8> kWorstMonsters{{
        arpg::combat::MonsterId::fire_bomber,
        arpg::combat::MonsterId::fire_charger,
        arpg::combat::MonsterId::water_bulwark,
        arpg::combat::MonsterId::water_support,
        arpg::combat::MonsterId::lightning_shooter,
        arpg::combat::MonsterId::lightning_dasher,
        arpg::combat::MonsterId::chaos_chaser,
        arpg::combat::MonsterId::chaos_hazard,
    }};
    snapshot.combat->monster_count = kWorstMonsters.size();
    for (std::size_t index{}; index < kWorstMonsters.size(); ++index) {
        snapshot.combat->monsters[index].active = true;
        snapshot.combat->monsters[index].id = kWorstMonsters[index];
    }
    const auto request = arpg::platform::make_material_residency_request(snapshot);
    ARPG_REQUIRE(request.contains(MaterialAtlasId::skill_draw_slash));
    ARPG_REQUIRE(request.contains(MaterialAtlasId::skill_storm_swords));
    ARPG_REQUIRE(arpg::platform::material_residency_bytes(
        arpg::platform::default_material_manifest(), request)
        <= 256U * 1024U * 1024U);
    return {};
}

arpg::test::Failure material_residency_noop_sync_is_allocation_free() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(8000U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(8001U + index * 2U),
            atlas.width, atlas.height, 1, 7};
    }
    arpg::dungeon::DungeonSnapshot snapshot{};
    snapshot.has_active_room = true;
    snapshot.ecology = arpg::dungeon::DungeonElement::fire;
    const auto request = arpg::platform::make_material_residency_request(snapshot);
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.synchronize_residency(request));
    const auto loads = fake.load_count;
    const auto unloads = fake.unload_count;
    ARPG_REQUIRE(pack.synchronize_residency(request));
    ARPG_REQUIRE(fake.load_count == loads);
    ARPG_REQUIRE(fake.unload_count == unloads);
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t iteration{}; iteration < 10'000U; ++iteration) {
        ARPG_REQUIRE(pack.synchronize_residency(request));
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    ARPG_REQUIRE(fake.load_count == loads);
    ARPG_REQUIRE(fake.unload_count == unloads);
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_residency_failure_isolated_and_not_retried() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(9000U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(9001U + index * 2U),
            atlas.width, atlas.height, 1, 7};
    }
    const std::size_t chaos = static_cast<std::size_t>(MaterialAtlasId::chaos_chaser);
    fake.loaded[chaos * 2U + 1U] = {};
    arpg::dungeon::DungeonSnapshot snapshot{};
    snapshot.has_active_room = true;
    snapshot.ecology = arpg::dungeon::DungeonElement::fire;
    snapshot.combat.emplace();
    snapshot.combat->monster_count = 1U;
    snapshot.combat->monsters[0].active = true;
    snapshot.combat->monsters[0].id = arpg::combat::MonsterId::chaos_chaser;
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    const auto request = arpg::platform::make_material_residency_request(snapshot);
    ARPG_REQUIRE(!pack.synchronize_residency(request));
    ARPG_REQUIRE(pack.available(MaterialAtlasId::fire_room_background));
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::chaos_chaser));
    const auto loads = fake.load_count;
    const auto unloads = fake.unload_count;
    ARPG_REQUIRE(!pack.synchronize_residency(request));
    ARPG_REQUIRE(fake.load_count == loads);
    ARPG_REQUIRE(fake.unload_count == unloads);
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_residency_loads_before_unloading_and_rejects_budget() noexcept {
    FakeMaterialTextures fake{};
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        fake.loaded[index * 2U] = {
            static_cast<unsigned int>(10'000U + index * 2U),
            atlas.width, atlas.height, 1, 7};
        fake.loaded[index * 2U + 1U] = {
            static_cast<unsigned int>(10'001U + index * 2U),
            atlas.width, atlas.height, 1, 7};
    }
    arpg::dungeon::DungeonSnapshot fire{};
    fire.has_active_room = true;
    fire.ecology = arpg::dungeon::DungeonElement::fire;
    arpg::dungeon::DungeonSnapshot water{};
    water.has_active_room = true;
    water.ecology = arpg::dungeon::DungeonElement::water;
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.synchronize_residency(
        arpg::platform::make_material_residency_request(fire)));
    const std::size_t first_transition_call = fake.call_count;
    ARPG_REQUIRE(pack.synchronize_residency(
        arpg::platform::make_material_residency_request(water)));
    bool saw_unload{};
    for (std::size_t index = first_transition_call; index < fake.call_count; ++index) {
        const auto kind = fake.calls[index].kind;
        if (kind == FakeMaterialTextures::CallKind::unload) {
            saw_unload = true;
        } else {
            ARPG_REQUIRE(!saw_unload);
        }
    }
    ARPG_REQUIRE(saw_unload);
    arpg::platform::MaterialResidencyRequest every_atlas{};
    for (std::size_t index{};
            index < static_cast<std::size_t>(MaterialAtlasId::count); ++index) {
        every_atlas.require(static_cast<MaterialAtlasId>(index));
    }
    const auto loads = fake.load_count;
    const auto unloads = fake.unload_count;
    ARPG_REQUIRE(!pack.synchronize_residency(every_atlas));
    ARPG_REQUIRE(fake.load_count == loads);
    ARPG_REQUIRE(fake.unload_count == unloads);
    pack.unload();
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_manifest_all_texture_pairs_exist_and_match() noexcept {
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    const std::filesystem::path project{ARPG_PROJECT_SOURCE_DIR};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto& atlas = manifest.atlases[index];
        ARPG_REQUIRE(atlas.color_path != nullptr);
        ARPG_REQUIRE(atlas.material_path != nullptr);
        const Image color = LoadImage(
            (project / atlas.color_path).string().c_str());
        const Image material = LoadImage(
            (project / atlas.material_path).string().c_str());
        ARPG_REQUIRE(color.data != nullptr);
        ARPG_REQUIRE(material.data != nullptr);
        ARPG_REQUIRE(color.width == atlas.width);
        ARPG_REQUIRE(color.height == atlas.height);
        ARPG_REQUIRE(material.width == atlas.width);
        ARPG_REQUIRE(material.height == atlas.height);
        if (color.data != nullptr) UnloadImage(color);
        if (material.data != nullptr) UnloadImage(material);
    }
    return {};
}

arpg::test::Failure player_action_atlas_files_are_opaque_only_on_drawn_pixels() noexcept {
    constexpr std::array<const char*, 5> kAtlasNames{
        "player_locomotion", "player_combo_a", "player_combo_b",
        "player_reaction", "player_air"};
    const std::filesystem::path root = std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}
        / "assets" / "player";
    for (const char* name : kAtlasNames) {
        const std::filesystem::path color = root / (std::string{name} + ".png");
        const std::filesystem::path material = root
            / (std::string{name} + "_material.png");
        ARPG_REQUIRE(std::filesystem::is_regular_file(color));
        ARPG_REQUIRE(std::filesystem::is_regular_file(material));
        const Image image = LoadImage(color.string().c_str());
        ARPG_REQUIRE(image.data != nullptr);
        ARPG_REQUIRE(image.width == 1024);
        ARPG_REQUIRE(image.height == 1024);
        ARPG_REQUIRE(GetImageColor(image, 0, 0).a == 0U);
        UnloadImage(image);
    }
    return {};
}

arpg::test::Failure player_action_clips_map_only_to_drawn_non_chroma_cells() noexcept {
    constexpr std::array<arpg::platform::PlayerAnimationClipId, 13> kClips{{
        arpg::platform::PlayerAnimationClipId::idle,
        arpg::platform::PlayerAnimationClipId::move,
        arpg::platform::PlayerAnimationClipId::jump,
        arpg::platform::PlayerAnimationClipId::j1,
        arpg::platform::PlayerAnimationClipId::j2,
        arpg::platform::PlayerAnimationClipId::j3,
        arpg::platform::PlayerAnimationClipId::launcher,
        arpg::platform::PlayerAnimationClipId::air_j,
        arpg::platform::PlayerAnimationClipId::landing,
        arpg::platform::PlayerAnimationClipId::hurt,
        arpg::platform::PlayerAnimationClipId::down,
        arpg::platform::PlayerAnimationClipId::get_up,
        arpg::platform::PlayerAnimationClipId::death,
    }};
    const MaterialManifestDefinition manifest = arpg::platform::default_material_manifest();
    const std::filesystem::path project{ARPG_PROJECT_SOURCE_DIR};
    for (const auto id : kClips) {
        const auto* const clip = arpg::platform::player_animation_clip(id);
        ARPG_REQUIRE(clip != nullptr);
        const MaterialAtlasDefinition* atlas{};
        for (std::size_t index{}; index < manifest.atlas_count; ++index) {
            if (manifest.atlases[index].id == clip->atlas) {
                atlas = &manifest.atlases[index];
                break;
            }
        }
        ARPG_REQUIRE(atlas != nullptr);
        const Image image = LoadImage((project / atlas->color_path).string().c_str());
        ARPG_REQUIRE(image.data != nullptr);
        for (std::uint16_t index{}; index < clip->frame_count; ++index) {
            const auto frame = arpg::platform::player_animation_frame(*clip, index);
            ARPG_REQUIRE(frame.has_value());
            bool has_drawn_pixel{};
            const int right = static_cast<int>(frame->source.x + frame->source.width);
            const int bottom = static_cast<int>(frame->source.y + frame->source.height);
            for (int y = static_cast<int>(frame->source.y); y < bottom; ++y) {
                for (int x = static_cast<int>(frame->source.x); x < right; ++x) {
                    const Color pixel = GetImageColor(image, x, y);
                    if (pixel.a == 0U) continue;
                    has_drawn_pixel = true;
                    ARPG_REQUIRE(!(pixel.g >= pixel.r + 42U
                        && pixel.g >= pixel.b + 42U));
                }
            }
            ARPG_REQUIRE(has_drawn_pixel);
        }
        UnloadImage(image);
    }
    return {};
}

arpg::test::Failure material_pack_unloads_wrong_sized_valid_atlas_once() noexcept {
    FakeMaterialTextures fake{};
    fake.loaded[2] = {101U, 256, 256, 1, 7};
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(!pack.load());
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::actors));
    ARPG_REQUIRE(fake.unload_count == 1U);
    ARPG_REQUIRE(fake.unloaded_ids[0] == 101U);
    pack.unload();
    ARPG_REQUIRE(fake.unload_count == 1U);
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_pack_repeated_unload_releases_valid_atlas_once() noexcept {
    FakeMaterialTextures fake{};
    fake.loaded[0] = {201U, 1024, 1024, 1, 7};
    fake.loaded[1] = {202U, 1024, 1024, 1, 7};
    g_fake_material_textures = &fake;
    arpg::platform::MaterialPack pack{fake_material_texture_api()};
    ARPG_REQUIRE(pack.load());
    ARPG_REQUIRE(pack.available(MaterialAtlasId::environment));
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::actors));
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::effects_ui));
    pack.unload();
    pack.unload();
    ARPG_REQUIRE(fake.unload_count == 2U);
    ARPG_REQUIRE(fake.unloaded_ids[0] == 201U);
    ARPG_REQUIRE(fake.unloaded_ids[1] == 202U);
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::environment));
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::actors));
    ARPG_REQUIRE(!pack.available(MaterialAtlasId::effects_ui));
    g_fake_material_textures = nullptr;
    return {};
}

arpg::test::Failure material_manifest_rejects_frame_outside_atlas() noexcept {
    const MaterialAtlasDefinition atlas{
        MaterialAtlasId::actors, 256, 256, 4U * 256U * 256U};
    const MaterialFrameDefinition frame{MaterialSpriteId::player_idle,
        MaterialAtlasId::actors, {240, 0, 32, 32}, {16, 30}, 100};
    ARPG_REQUIRE(!arpg::platform::validate_material_frame(atlas, frame).valid);
    return {};
}

arpg::test::Failure material_manifest_rejects_invalid_frame_geometry_and_anchor() noexcept {
    const MaterialAtlasDefinition atlas{
        MaterialAtlasId::actors, 256, 256, 4U * 256U * 256U};
    const MaterialFrameDefinition zero_width{MaterialSpriteId::player_idle,
        MaterialAtlasId::actors, {0, 0, 0, 32}, {16, 30}, 100};
    const MaterialFrameDefinition negative_x{MaterialSpriteId::player_idle,
        MaterialAtlasId::actors, {-1, 0, 32, 32}, {16, 30}, 100};
    const MaterialFrameDefinition negative_y{MaterialSpriteId::player_idle,
        MaterialAtlasId::actors, {0, -1, 32, 32}, {16, 30}, 100};
    const MaterialFrameDefinition zero_height{MaterialSpriteId::player_idle,
        MaterialAtlasId::actors, {0, 0, 32, 0}, {16, 30}, 100};
    const MaterialFrameDefinition negative_anchor{MaterialSpriteId::player_idle,
        MaterialAtlasId::actors, {0, 0, 32, 32}, {-1, 30}, 100};
    const MaterialFrameDefinition bad_anchor{MaterialSpriteId::player_idle,
        MaterialAtlasId::actors, {0, 0, 32, 32}, {16, 33}, 100};
    ARPG_REQUIRE(!arpg::platform::validate_material_frame(atlas, zero_width).valid);
    ARPG_REQUIRE(!arpg::platform::validate_material_frame(atlas, negative_x).valid);
    ARPG_REQUIRE(!arpg::platform::validate_material_frame(atlas, negative_y).valid);
    ARPG_REQUIRE(!arpg::platform::validate_material_frame(atlas, zero_height).valid);
    ARPG_REQUIRE(!arpg::platform::validate_material_frame(atlas, negative_anchor).valid);
    ARPG_REQUIRE(!arpg::platform::validate_material_frame(atlas, bad_anchor).valid);
    return {};
}

arpg::test::Failure material_manifest_rejects_non_finite_frame_geometry_and_anchor() noexcept {
    const MaterialAtlasDefinition atlas{
        MaterialAtlasId::actors, 256, 256, 4U * 256U * 256U};
    const float nan = std::numeric_limits<float>::quiet_NaN();
    const float infinity = std::numeric_limits<float>::infinity();
    const std::array<MaterialFrameDefinition, 12> frames{{
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {nan, 0, 32, 32}, {16, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, nan, 32, 32}, {16, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, 0, nan, 32}, {16, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, 0, 32, nan}, {16, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, 0, 32, 32}, {nan, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, 0, 32, 32}, {16, nan}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {infinity, 0, 32, 32}, {16, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, -infinity, 32, 32}, {16, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, 0, infinity, 32}, {16, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, 0, 32, -infinity}, {16, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, 0, 32, 32}, {infinity, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, 0, 32, 32}, {16, -infinity}, 100},
    }};
    for (const MaterialFrameDefinition& frame : frames) {
        ARPG_REQUIRE(!arpg::platform::validate_material_frame(atlas, frame).valid);
    }
    return {};
}

arpg::test::Failure material_manifest_rejects_oversized_atlas_and_memory_budget() noexcept {
    constexpr std::size_t kMiB = 1024U * 1024U;
    const std::array<MaterialAtlasDefinition, 1> oversized{{
        {MaterialAtlasId::actors, 2049, 128, 4U * 2049U * 128U},
    }};
    const MaterialManifestDefinition oversized_manifest{
        oversized.data(), oversized.size(), nullptr, 0U};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(oversized_manifest).valid);

    const std::array<MaterialAtlasDefinition, 3> over_budget{{
        {MaterialAtlasId::environment, 2048, 2048, 128U * kMiB},
        {MaterialAtlasId::actors, 2048, 2048, 128U * kMiB},
        {MaterialAtlasId::effects_ui, 1, 1, 1U},
    }};
    const MaterialManifestDefinition over_budget_manifest{
        over_budget.data(), over_budget.size(), nullptr, 0U};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(over_budget_manifest).valid);
    return {};
}

arpg::test::Failure material_manifest_reports_true_resident_peak() noexcept {
    constexpr std::size_t kExpectedFullPackBytes = 329'430'304U;
    constexpr std::size_t kExpectedResidentPeakBytes = 226'800'928U;
    constexpr std::size_t kExpectedFireEcologyPeakBytes = 165'806'080U;
    constexpr std::size_t kExpectedNonFirePeakBytes = 157'302'784U;
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    ARPG_REQUIRE(arpg::platform::full_pack_bytes(manifest)
        == kExpectedFullPackBytes);
    ARPG_REQUIRE(arpg::platform::resident_peak_bytes(manifest)
        == kExpectedResidentPeakBytes);
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(manifest).valid);

    for (const MaterialEcology ecology : {MaterialEcology::fire,
             MaterialEcology::water, MaterialEcology::lightning,
             MaterialEcology::chaos}) {
        std::array<MaterialAtlasDefinition,
            static_cast<std::size_t>(MaterialAtlasId::count)> resident{};
        std::size_t resident_count{};
        for (std::size_t index{}; index < manifest.atlas_count; ++index) {
            if (manifest.atlases[index].ecology == MaterialEcology::common
                || manifest.atlases[index].ecology == ecology) {
                resident[resident_count++] = manifest.atlases[index];
            }
        }
        const MaterialManifestDefinition ecology_manifest{
            resident.data(), resident_count, nullptr, 0U};
        ARPG_REQUIRE(arpg::platform::resident_peak_bytes(ecology_manifest)
            == (ecology == MaterialEcology::fire
                ? kExpectedFireEcologyPeakBytes : kExpectedNonFirePeakBytes));
    }
    return {};
}

arpg::test::Failure resident_peak_saturates_instead_of_overflowing() noexcept {
    constexpr std::size_t kMaximum = (std::numeric_limits<std::size_t>::max)();
    const std::array<MaterialAtlasDefinition, 2> atlases{{
        {MaterialAtlasId::environment, 1, 1, kMaximum / 2U - 8U, "a", "b",
            MaterialEcology::common},
        {MaterialAtlasId::actors, 1, 1, 16U, "c", "d",
            MaterialEcology::fire},
    }};
    const MaterialManifestDefinition manifest{
        atlases.data(), atlases.size(), nullptr, 0U};
    ARPG_REQUIRE(arpg::platform::resident_peak_bytes(manifest) == kMaximum);
    const std::array<MaterialAtlasDefinition, 1> full_overflow{{
        {MaterialAtlasId::environment, 1, 1, kMaximum, "a", "b"},
    }};
    const MaterialManifestDefinition full_overflow_manifest{
        full_overflow.data(), full_overflow.size(), nullptr, 0U};
    ARPG_REQUIRE(arpg::platform::full_pack_bytes(full_overflow_manifest)
        == kMaximum);
    return {};
}

arpg::test::Failure material_manifest_never_exceeds_hard_resident_cap() noexcept {
    constexpr std::size_t kMiB = 1024U * 1024U;
    constexpr std::size_t kAtlasBytes = 16U * kMiB;
    const std::array<MaterialAtlasDefinition, 9> atlases{{
        {MaterialAtlasId::environment, 2048, 2048, kAtlasBytes, "a", "b"},
        {MaterialAtlasId::actors, 2048, 2048, kAtlasBytes, "c", "d"},
        {MaterialAtlasId::effects_ui, 2048, 2048, kAtlasBytes, "e", "f"},
        {MaterialAtlasId::player_locomotion, 2048, 2048, kAtlasBytes, "g", "h"},
        {MaterialAtlasId::player_combo_a, 2048, 2048, kAtlasBytes, "i", "j"},
        {MaterialAtlasId::player_combo_b, 2048, 2048, kAtlasBytes, "k", "l"},
        {MaterialAtlasId::player_reaction, 2048, 2048, kAtlasBytes, "m", "n"},
        {MaterialAtlasId::player_air, 2048, 2048, kAtlasBytes, "o", "p"},
        {MaterialAtlasId::items_ui, 2048, 2048, kAtlasBytes, "q", "r"},
    }};
    const MaterialManifestDefinition exactly_at_hard_cap{
        atlases.data(), 8U, nullptr, 0U, nullptr, 0U,
        nullptr, 0U, 512U * kMiB};
    ARPG_REQUIRE(arpg::platform::resident_peak_bytes(exactly_at_hard_cap)
        == 256U * kMiB);
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(
        exactly_at_hard_cap).valid);

    const MaterialManifestDefinition one_pair_above_hard_cap{
        atlases.data(), atlases.size(), nullptr, 0U, nullptr, 0U,
        nullptr, 0U, 512U * kMiB};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(
        one_pair_above_hard_cap).valid);
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(
        one_pair_above_hard_cap).error
        == arpg::platform::MaterialValidationError::memory_budget_exceeded);

    const std::array<MaterialAtlasDefinition, 1> small_atlas{{
        {MaterialAtlasId::environment, 64, 64, 4U * 64U * 64U, "a", "b"},
    }};
    const MaterialManifestDefinition requested_too_small{
        small_atlas.data(), small_atlas.size(), nullptr, 0U, nullptr, 0U,
        nullptr, 0U, 1U};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(
        requested_too_small).valid);
    return {};
}

arpg::test::Failure material_manifest_memory_stats_fail_closed() noexcept {
    constexpr std::size_t kMaximum = (std::numeric_limits<std::size_t>::max)();
    const MaterialManifestDefinition missing_atlases{
        nullptr, 1U, nullptr, 0U};
    ARPG_REQUIRE(arpg::platform::resident_peak_bytes(missing_atlases)
        == kMaximum);
    ARPG_REQUIRE(arpg::platform::full_pack_bytes(missing_atlases) == kMaximum);
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(
        missing_atlases).valid);

    const MaterialAtlasDefinition illegal_ecology{
        MaterialAtlasId::actors, 1, 1, 4U, "a", "b",
        MaterialEcology::count};
    const MaterialManifestDefinition illegal_ecology_manifest{
        &illegal_ecology, 1U, nullptr, 0U};
    ARPG_REQUIRE(arpg::platform::resident_peak_bytes(
        illegal_ecology_manifest) == kMaximum);
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(
        illegal_ecology_manifest).valid);
    return {};
}

arpg::test::Failure material_manifest_rejects_false_rgba_byte_claim() noexcept {
    const std::array<MaterialAtlasDefinition, 1> atlases{{
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U - 1U,
            "actors.png", "actors_material.png"},
    }};
    const MaterialManifestDefinition manifest{
        atlases.data(), atlases.size(), nullptr, 0U};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(manifest).valid);
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(manifest).error
        == arpg::platform::MaterialValidationError::invalid_atlas);
    return {};
}

arpg::test::Failure material_manifest_rejects_invalid_background_contracts() noexcept {
    constexpr auto valid_bytes = [](int width, int height) noexcept {
        return static_cast<std::size_t>(width)
            * static_cast<std::size_t>(height) * 4U;
    };
    const std::array<MaterialAtlasDefinition, 4> invalid{{
        {MaterialAtlasId::fire_room_background, 2559, 1440,
            valid_bytes(2559, 1440), "a", "b", MaterialEcology::fire},
        {MaterialAtlasId::water_room_background, 2561, 1440,
            valid_bytes(2561, 1440), "c", "d", MaterialEcology::water},
        {MaterialAtlasId::lightning_room_background, 2560, 1439,
            valid_bytes(2560, 1439), "e", "f", MaterialEcology::lightning},
        {MaterialAtlasId::chaos_room_background, 2560, 1440,
            valid_bytes(2560, 1440), "g", "h", MaterialEcology::fire},
    }};
    for (const MaterialAtlasDefinition& atlas : invalid) {
        const MaterialManifestDefinition manifest{&atlas, 1U, nullptr, 0U};
        ARPG_REQUIRE(!arpg::platform::validate_material_manifest(manifest).valid);
        ARPG_REQUIRE(arpg::platform::validate_material_manifest(manifest).error
            == arpg::platform::MaterialValidationError::invalid_atlas);
    }
    return {};
}

arpg::test::Failure material_manifest_rejects_duplicate_sprite_ids() noexcept {
    const std::array<MaterialAtlasDefinition, 1> atlases{{
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U, "actors.png",
            "actors_material.png"},
    }};
    const std::array<MaterialFrameDefinition, 2> frames{{
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, 0, 32, 32}, {16, 30}, 100},
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {32, 0, 32, 32}, {16, 30}, 100},
    }};
    const MaterialManifestDefinition manifest{
        atlases.data(), atlases.size(), frames.data(), frames.size()};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(manifest).valid);

    const std::array<MaterialAtlasDefinition, 2> duplicate_atlases{{
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U, "actors.png",
            "actors_material.png"},
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U, "actors.png",
            "actors_material.png"},
    }};
    const MaterialManifestDefinition duplicate_atlas_manifest{
        duplicate_atlases.data(), duplicate_atlases.size(), nullptr, 0U};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(
        duplicate_atlas_manifest).valid);

    const std::array<MaterialFrameDefinition, 1> missing_atlas_frames{{
        {MaterialSpriteId::player_idle, MaterialAtlasId::effects_ui,
            {0, 0, 32, 32}, {16, 30}, 100},
    }};
    const MaterialManifestDefinition missing_atlas_manifest{
        atlases.data(), atlases.size(), missing_atlas_frames.data(),
        missing_atlas_frames.size()};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(
        missing_atlas_manifest).valid);
    return {};
}

arpg::test::Failure material_manifest_accepts_valid_unique_frames() noexcept {
    const std::array<MaterialAtlasDefinition, 1> atlases{{
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U, "actors.png",
            "actors_material.png"},
    }};
    const std::array<MaterialFrameDefinition, 2> frames{{
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors,
            {0, 0, 32, 32}, {16, 30}, 100},
        {MaterialSpriteId::player_move, MaterialAtlasId::actors,
            {32, 0, 32, 32}, {16, 30}, 100},
    }};
    const MaterialManifestDefinition manifest{
        atlases.data(), atlases.size(), frames.data(), frames.size()};
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(manifest).valid);
    return {};
}


arpg::test::Failure material_manifest_rejects_missing_color_or_material_maps() noexcept {
    const std::array<MaterialAtlasDefinition, 1> missing_color{{
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U, nullptr,
            "actors_material.png"},
    }};
    const MaterialManifestDefinition missing_color_manifest{
        missing_color.data(), missing_color.size(), nullptr, 0U};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(
        missing_color_manifest).valid);

    const std::array<MaterialAtlasDefinition, 1> missing_material{{
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U, "actors.png", nullptr},
    }};
    const MaterialManifestDefinition missing_material_manifest{
        missing_material.data(), missing_material.size(), nullptr, 0U};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(
        missing_material_manifest).valid);
    return {};
}

arpg::test::Failure material_manifest_rejects_invalid_weapon_anchor_and_clips() noexcept {
    const std::array<MaterialAtlasDefinition, 1> atlases{{
        {MaterialAtlasId::actors, 128, 128, 4U * 128U * 128U, "actors.png",
            "actors_material.png"},
    }};
    const std::array<MaterialFrameDefinition, 2> frames{{
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors, {0, 0, 32, 32},
            {16, 30}, 42U, {33, 16}, MaterialLayer::body, MaterialClass::actor, 1U},
        {MaterialSpriteId::player_move, MaterialAtlasId::actors, {32, 0, 32, 32},
            {16, 30}, 42U, {16, 16}, MaterialLayer::body, MaterialClass::actor, 2U},
    }};
    ARPG_REQUIRE(!arpg::platform::validate_material_frame(atlases[0], frames[0]).valid);
    const std::array<AnimationClipDefinition, 1> too_short{{
        {AnimationClipId::player_idle, MaterialSpriteId::player_idle, 1U, 1U, 16U},
    }};
    const MaterialManifestDefinition manifest{atlases.data(), atlases.size(),
        frames.data() + 1, 1U, too_short.data(), too_short.size()};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(manifest).valid);
    return {};
}

arpg::test::Failure material_manifest_rejects_repeated_hash_and_ecology_boundary() noexcept {
    const std::array<MaterialAtlasDefinition, 1> atlases{{
        {MaterialAtlasId::actors, 128, 128, 4U * 128U * 128U, "actors.png",
            "actors_material.png"},
    }};
    const std::array<MaterialFrameDefinition, 4> frames{{
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors, {0, 0, 16, 16}, {8, 15}, 42U, {8, 8}, MaterialLayer::body, MaterialClass::actor, 7U},
        {MaterialSpriteId::player_move, MaterialAtlasId::actors, {16, 0, 16, 16}, {8, 15}, 42U, {8, 8}, MaterialLayer::body, MaterialClass::actor, 7U},
        {MaterialSpriteId::player_j1, MaterialAtlasId::actors, {32, 0, 16, 16}, {8, 15}, 42U, {8, 8}, MaterialLayer::body, MaterialClass::actor, 7U},
        {MaterialSpriteId::player_j2, MaterialAtlasId::actors, {48, 0, 16, 16}, {8, 15}, 42U, {8, 8}, MaterialLayer::body, MaterialClass::actor, 9U},
    }};
    const std::array<AnimationClipDefinition, 1> repeated{{
        {AnimationClipId::player_idle, MaterialSpriteId::player_idle, 0U, 4U, 4U},
    }};
    const MaterialManifestDefinition repeated_manifest{atlases.data(), atlases.size(),
        frames.data(), frames.size(), repeated.data(), repeated.size()};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(repeated_manifest).valid);

    const std::array<MaterialFrameDefinition, 4> unique_frames{{
        {MaterialSpriteId::player_idle, MaterialAtlasId::actors, {0, 0, 16, 16}, {8, 15}, 42U, {8, 8}, MaterialLayer::body, MaterialClass::actor, 11U},
        {MaterialSpriteId::player_move, MaterialAtlasId::actors, {16, 0, 16, 16}, {8, 15}, 42U, {8, 8}, MaterialLayer::body, MaterialClass::actor, 12U},
        {MaterialSpriteId::player_j1, MaterialAtlasId::actors, {32, 0, 16, 16}, {8, 15}, 42U, {8, 8}, MaterialLayer::body, MaterialClass::actor, 13U},
        {MaterialSpriteId::player_j2, MaterialAtlasId::actors, {48, 0, 16, 16}, {8, 15}, 42U, {8, 8}, MaterialLayer::body, MaterialClass::actor, 14U},
    }};
    const std::array<AnimationClipDefinition, 1> water_clip{{
        {AnimationClipId::monster_idle, MaterialSpriteId::player_idle, 0U, 4U, 4U,
            24U, 0U, 0U, MaterialEcology::water},
    }};
    const MaterialManifestDefinition water_manifest{atlases.data(), atlases.size(),
        unique_frames.data(), unique_frames.size(), water_clip.data(), water_clip.size()};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest_for_ecology(
        water_manifest, MaterialEcology::fire).valid);
    ARPG_REQUIRE(arpg::platform::validate_material_manifest_for_ecology(
        water_manifest, MaterialEcology::water).valid);
    return {};
}

arpg::test::Failure material_manifest_accepts_default_layered_animation_manifest() noexcept {
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(
        arpg::platform::default_material_manifest()).valid);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"falls back when atlas is unavailable",
        &material_pack_falls_back_when_atlas_is_unavailable},
    {"allows repeated shutdown", &material_pack_allows_repeated_shutdown},
    {"loads missing atlases without unloading",
        &material_pack_loads_missing_atlases_without_unloading},
    {"rejects unavailable shader pipeline",
        &material_pack_rejects_unavailable_shader_pipeline},
    {"ecology readiness requires every texture pair",
        &material_pack_ecology_ready_requires_every_texture_pair},
    {"loads all original player action atlases",
        &material_pack_loads_all_original_player_action_atlases},
    {"loads and draws paired color and material atlases",
        &material_pack_loads_and_draws_color_material_pairs},
    {"records successful item sprite draws",
        &material_pack_records_successful_item_sprite_draws},
    {"transformed sprite draw forwards rotation and tint",
        &transformed_sprite_draw_forwards_rotation_and_tint},
    {"atlas region draw forwards exact destination",
        &atlas_region_draw_forwards_exact_destination},
    {"nine slice preserves authored panel corners",
        &material_pack_nine_slice_preserves_panel_corners},
    {"horizontal slice preserves decorated caps",
        &material_pack_horizontal_slice_preserves_decorated_caps},
    {"switches ecology without reloading common atlases",
        &material_pack_switches_ecology_without_reloading_common},
    {"residency request selects drawable room and monster atlases",
        &material_residency_request_selects_drawable_room_and_monster_atlases},
    {"residency requests only equipped draw slash",
        &material_residency_requests_only_equipped_draw_slash},
    {"residency requests both equipped skill atlases under budget",
        &material_residency_requests_both_equipped_skill_atlases_under_budget},
    {"residency noop sync is allocation free",
        &material_residency_noop_sync_is_allocation_free},
    {"residency failure is isolated and not retried",
        &material_residency_failure_isolated_and_not_retried},
    {"residency loads before unloading and rejects budget",
        &material_residency_loads_before_unloading_and_rejects_budget},
    {"all manifest texture pairs exist and match declared dimensions",
        &material_manifest_all_texture_pairs_exist_and_match},
    {"player action atlas files have transparent borders",
        &player_action_atlas_files_are_opaque_only_on_drawn_pixels},
    {"player action clips use drawn non-chroma atlas cells",
        &player_action_clips_map_only_to_drawn_non_chroma_cells},
    {"unloads wrong-sized valid atlas once",
        &material_pack_unloads_wrong_sized_valid_atlas_once},
    {"repeated unload releases valid atlas once",
        &material_pack_repeated_unload_releases_valid_atlas_once},
    {"rejects frame outside atlas", &material_manifest_rejects_frame_outside_atlas},
    {"rejects invalid frame geometry and anchor",
        &material_manifest_rejects_invalid_frame_geometry_and_anchor},
    {"rejects non-finite frame geometry and anchor",
        &material_manifest_rejects_non_finite_frame_geometry_and_anchor},
    {"rejects oversized atlas and memory budget",
        &material_manifest_rejects_oversized_atlas_and_memory_budget},
    {"reports true resident peak",
        &material_manifest_reports_true_resident_peak},
    {"resident peak saturates instead of overflowing",
        &resident_peak_saturates_instead_of_overflowing},
    {"never exceeds hard resident cap",
        &material_manifest_never_exceeds_hard_resident_cap},
    {"memory stats fail closed",
        &material_manifest_memory_stats_fail_closed},
    {"rejects false rgba byte claim",
        &material_manifest_rejects_false_rgba_byte_claim},
    {"rejects invalid background contracts",
        &material_manifest_rejects_invalid_background_contracts},
    {"rejects duplicate sprite ids", &material_manifest_rejects_duplicate_sprite_ids},
    {"accepts valid unique frames", &material_manifest_accepts_valid_unique_frames},
    {"rejects missing color or material maps",
        &material_manifest_rejects_missing_color_or_material_maps},
    {"rejects invalid weapon anchor and clips",
        &material_manifest_rejects_invalid_weapon_anchor_and_clips},
    {"rejects repeated hash and ecology boundary",
        &material_manifest_rejects_repeated_hash_and_ecology_boundary},
    {"accepts default layered animation manifest",
        &material_manifest_accepts_default_layered_animation_manifest},
};

}  // namespace

arpg::test::TestSuite material_asset_validation_suite() noexcept {
    return arpg::test::make_suite("material_asset_validation", kCases);
}

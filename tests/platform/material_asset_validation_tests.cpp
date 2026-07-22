#include "test_framework.hpp"

#include "material_asset_validation.hpp"
#include "material_animation.hpp"
#include "material_pack.hpp"

#include <array>
#include <cstdio>
#include <cstddef>
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
    static constexpr std::size_t kCapacity =
        static_cast<std::size_t>(MaterialAtlasId::count) * 2U;
    std::array<Texture2D, kCapacity> loaded{};
    std::array<std::array<char, 512>, kCapacity> loaded_paths{};
    std::array<unsigned int, kCapacity> unloaded_ids{};
    std::array<unsigned int, kCapacity> drawn_color_ids{};
    std::array<unsigned int, kCapacity> drawn_material_ids{};
    std::array<MaterialCompositeParameters, kCapacity> composites{};
    std::size_t load_count{};
    std::size_t unload_count{};
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
    const MaterialManifestDefinition manifest =
        arpg::platform::default_material_manifest();
    const std::string loaded_path{path};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        if (loaded_path.find(manifest.atlases[index].color_path)
            != std::string::npos) {
            return g_fake_material_textures->loaded[index * 2U];
        }
        if (loaded_path.find(manifest.atlases[index].material_path)
            != std::string::npos) {
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
    Rectangle, Rectangle, Vector2, float, Color,
    MaterialCompositeParameters parameters) noexcept {
    if (g_fake_material_textures == nullptr
        || g_fake_material_textures->draw_count
            >= g_fake_material_textures->drawn_color_ids.size()) return;
    const std::size_t index = g_fake_material_textures->draw_count++;
    g_fake_material_textures->drawn_color_ids[index] = color.id;
    g_fake_material_textures->drawn_material_ids[index] = material.id;
    g_fake_material_textures->composites[index] = parameters;
}

arpg::platform::MaterialTextureApi fake_material_texture_api() noexcept {
    return {&fake_load_texture, &fake_texture_valid, &fake_unload_texture,
        &fake_initialize_material_pipeline, &fake_shutdown_material_pipeline,
        &fake_draw_material};
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
    ARPG_REQUIRE(fake.unload_count == 6U);
    ARPG_REQUIRE(fake.load_count == fire_loads + 6U);
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
    {"switches ecology without reloading common atlases",
        &material_pack_switches_ecology_without_reloading_common},
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

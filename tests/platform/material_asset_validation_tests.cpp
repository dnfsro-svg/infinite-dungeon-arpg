#include "test_framework.hpp"

#include "material_asset_validation.hpp"

#include <array>
#include <cstddef>
#include <limits>

namespace {

using arpg::platform::MaterialAtlasDefinition;
using arpg::platform::MaterialAtlasId;
using arpg::platform::MaterialFrameDefinition;
using arpg::platform::MaterialManifestDefinition;
using arpg::platform::MaterialSpriteId;

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
        {MaterialAtlasId::environment, 2048, 2048, 32U * kMiB},
        {MaterialAtlasId::actors, 2048, 2048, 32U * kMiB},
        {MaterialAtlasId::effects_ui, 1, 1, 1U},
    }};
    const MaterialManifestDefinition over_budget_manifest{
        over_budget.data(), over_budget.size(), nullptr, 0U};
    ARPG_REQUIRE(!arpg::platform::validate_material_manifest(over_budget_manifest).valid);
    return {};
}

arpg::test::Failure material_manifest_rejects_duplicate_sprite_ids() noexcept {
    const std::array<MaterialAtlasDefinition, 1> atlases{{
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U},
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
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U},
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U},
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
        {MaterialAtlasId::actors, 64, 64, 4U * 64U * 64U},
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

constexpr arpg::test::TestCase kCases[] = {
    {"rejects frame outside atlas", &material_manifest_rejects_frame_outside_atlas},
    {"rejects invalid frame geometry and anchor",
        &material_manifest_rejects_invalid_frame_geometry_and_anchor},
    {"rejects non-finite frame geometry and anchor",
        &material_manifest_rejects_non_finite_frame_geometry_and_anchor},
    {"rejects oversized atlas and memory budget",
        &material_manifest_rejects_oversized_atlas_and_memory_budget},
    {"rejects duplicate sprite ids", &material_manifest_rejects_duplicate_sprite_ids},
    {"accepts valid unique frames", &material_manifest_accepts_valid_unique_frames},
};

}  // namespace

arpg::test::TestSuite material_asset_validation_suite() noexcept {
    return arpg::test::make_suite("material_asset_validation", kCases);
}

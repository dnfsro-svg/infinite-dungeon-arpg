#include "test_framework.hpp"

#include "material_animation.hpp"
#include "material_asset_validation.hpp"
#include "material_manifest.hpp"
#include "monster_material_presenter.hpp"
#include "water_room_material_slice.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <string>

namespace {

using arpg::combat::MonsterAiPhase;
using arpg::combat::MonsterId;
using arpg::platform::MaterialAtlasDefinition;
using arpg::platform::MaterialAtlasId;
using arpg::platform::MaterialEcology;
using arpg::platform::MonsterAnimationState;

const MaterialAtlasDefinition* find_atlas(MaterialAtlasId id) noexcept {
    const auto manifest = arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        if (manifest.atlases[index].id == id) return &manifest.atlases[index];
    }
    return nullptr;
}

bool image_has_visible_color(const char* relative_path, int width, int height) noexcept {
    const std::filesystem::path path = std::filesystem::path{
        ARPG_PROJECT_SOURCE_DIR} / relative_path;
    const Image image = LoadImage(path.string().c_str());
    if (image.data == nullptr || image.width != width || image.height != height) {
        if (image.data != nullptr) UnloadImage(image);
        return false;
    }
    Color* const pixels = LoadImageColors(image);
    bool visible{};
    bool chromatic{};
    if (pixels != nullptr) {
        const std::size_t pixel_count = static_cast<std::size_t>(image.width)
            * static_cast<std::size_t>(image.height);
        for (std::size_t index{}; index < pixel_count; ++index) {
            const Color pixel = pixels[index];
            visible = visible || pixel.a > 24U;
            chromatic = chromatic || (pixel.a > 24U
                && (pixel.r != pixel.g || pixel.g != pixel.b));
        }
        UnloadImageColors(pixels);
    }
    UnloadImage(image);
    return visible && chromatic;
}

std::uint64_t frame_pixel_hash(const Color* pixels, int image_width,
    const Rectangle& source) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const int left = static_cast<int>(source.x);
    const int top = static_cast<int>(source.y);
    const int right = left + static_cast<int>(source.width);
    const int bottom = top + static_cast<int>(source.height);
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const Color pixel = pixels[y * image_width + x];
            hash ^= pixel.r; hash *= 1099511628211ULL;
            hash ^= pixel.g; hash *= 1099511628211ULL;
            hash ^= pixel.b; hash *= 1099511628211ULL;
            hash ^= pixel.a; hash *= 1099511628211ULL;
        }
    }
    return hash;
}

struct OpaqueBounds final {
    int width{};
    int height{};
};

struct SilhouetteMetrics final {
    OpaqueBounds bounds{};
    std::uint64_t visible_pixels{};
    std::uint64_t soft_pixels{};
    int centroid_x{};
    int centroid_y{};
};

OpaqueBounds frame_opaque_bounds(const Color* pixels, int image_width,
    const Rectangle& source) noexcept {
    const int left = static_cast<int>(source.x);
    const int top = static_cast<int>(source.y);
    const int right = left + static_cast<int>(source.width);
    const int bottom = top + static_cast<int>(source.height);
    int min_x = right;
    int min_y = bottom;
    int max_x = left - 1;
    int max_y = top - 1;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            if (pixels[y * image_width + x].a <= 24U) continue;
            min_x = (std::min)(min_x, x);
            min_y = (std::min)(min_y, y);
            max_x = (std::max)(max_x, x);
            max_y = (std::max)(max_y, y);
        }
    }
    return max_x < min_x || max_y < min_y
        ? OpaqueBounds{} : OpaqueBounds{max_x - min_x + 1, max_y - min_y + 1};
}

SilhouetteMetrics frame_silhouette_metrics(const Color* pixels,
    int image_width, const Rectangle& source) noexcept {
    SilhouetteMetrics metrics{};
    metrics.bounds = frame_opaque_bounds(pixels, image_width, source);
    const int left = static_cast<int>(source.x);
    const int top = static_cast<int>(source.y);
    const int right = left + static_cast<int>(source.width);
    const int bottom = top + static_cast<int>(source.height);
    std::uint64_t x_sum{};
    std::uint64_t y_sum{};
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const unsigned char alpha = pixels[y * image_width + x].a;
            if (alpha <= 24U) continue;
            ++metrics.visible_pixels;
            if (alpha >= 48U && alpha <= 207U) ++metrics.soft_pixels;
            x_sum += static_cast<std::uint64_t>(x - left);
            y_sum += static_cast<std::uint64_t>(y - top);
        }
    }
    if (metrics.visible_pixels > 0U) {
        metrics.centroid_x = static_cast<int>(x_sum / metrics.visible_pixels);
        metrics.centroid_y = static_cast<int>(y_sum / metrics.visible_pixels);
    }
    return metrics;
}

arpg::test::Failure water_ecology_has_independent_loadable_color_and_material_atlases() noexcept {
    constexpr std::array<MaterialAtlasId, 3> kAtlases{{
        MaterialAtlasId::water_environment,
        MaterialAtlasId::water_bulwark,
        MaterialAtlasId::water_support,
    }};
    for (const MaterialAtlasId id : kAtlases) {
        const MaterialAtlasDefinition* const atlas = find_atlas(id);
        ARPG_REQUIRE(atlas != nullptr);
        ARPG_REQUIRE(atlas->ecology == MaterialEcology::water);
        ARPG_REQUIRE(atlas->color_path != nullptr);
        ARPG_REQUIRE(atlas->material_path != nullptr);
        ARPG_REQUIRE(std::filesystem::path{atlas->color_path}.filename().string()
            .find("fire") == std::string::npos);
        ARPG_REQUIRE(image_has_visible_color(
            atlas->color_path, atlas->width, atlas->height));
        ARPG_REQUIRE(image_has_visible_color(
            atlas->material_path, atlas->width, atlas->height));
    }
    return {};
}

arpg::test::Failure water_room_consumes_only_water_environment_materials() noexcept {
    const auto plan = arpg::platform::water_room_render_plan(
        arpg::dungeon::DungeonElement::water);
    ARPG_REQUIRE(plan.active);
    ARPG_REQUIRE(plan.background_atlas == MaterialAtlasId::water_environment);
    ARPG_REQUIRE(plan.background_source.width > 0.0F);
    ARPG_REQUIRE(plan.background_source.height > 0.0F);
    ARPG_REQUIRE(!arpg::platform::water_room_render_plan(
        arpg::dungeon::DungeonElement::fire).active);

    const auto& slice = arpg::platform::water_room_material_slice();
    constexpr std::array<arpg::platform::WaterRoomPropId, 7> kRequired{{
        arpg::platform::WaterRoomPropId::floor,
        arpg::platform::WaterRoomPropId::wall,
        arpg::platform::WaterRoomPropId::door,
        arpg::platform::WaterRoomPropId::hole,
        arpg::platform::WaterRoomPropId::lantern,
        arpg::platform::WaterRoomPropId::coral,
        arpg::platform::WaterRoomPropId::grate,
    }};
    const auto manifest = arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < kRequired.size(); ++index) {
        const auto& prop = slice.props[index];
        ARPG_REQUIRE(prop.id == kRequired[index]);
        ARPG_REQUIRE(prop.sprite != arpg::platform::MaterialSpriteId::missing);
        const arpg::platform::MaterialFrameDefinition* frame{};
        for (std::size_t frame_index{}; frame_index < manifest.frame_count;
             ++frame_index) {
            if (manifest.frames[frame_index].id == prop.sprite) {
                frame = &manifest.frames[frame_index];
                break;
            }
        }
        ARPG_REQUIRE(frame != nullptr);
        ARPG_REQUIRE(frame->atlas == MaterialAtlasId::water_environment);
    }
    return {};
}

arpg::test::Failure water_monsters_expose_complete_multiframe_state_groups() noexcept {
    constexpr std::array<MonsterId, 2> kMonsters{{
        MonsterId::water_bulwark, MonsterId::water_support}};
    constexpr std::array<MonsterAnimationState, 5> kStates{{
        MonsterAnimationState::idle,
        MonsterAnimationState::move,
        MonsterAnimationState::special,
        MonsterAnimationState::hurt,
        MonsterAnimationState::death,
    }};
    constexpr std::array<std::uint16_t, 5> kMinimumFrames{{12U, 16U, 20U, 8U, 16U}};
    constexpr std::array<MaterialAtlasId, 2> kAtlases{{
        MaterialAtlasId::water_bulwark, MaterialAtlasId::water_support}};
    for (std::size_t monster_index{}; monster_index < kMonsters.size();
         ++monster_index) {
        const MaterialAtlasDefinition* const atlas = find_atlas(
            kAtlases[monster_index]);
        ARPG_REQUIRE(atlas != nullptr);
        const std::filesystem::path path = std::filesystem::path{
            ARPG_PROJECT_SOURCE_DIR} / atlas->color_path;
        const Image image = LoadImage(path.string().c_str());
        ARPG_REQUIRE(image.data != nullptr);
        Color* const pixels = LoadImageColors(image);
        ARPG_REQUIRE(pixels != nullptr);
        for (std::size_t state_index{}; state_index < kStates.size(); ++state_index) {
            const auto* const clip = arpg::platform::monster_animation_clip(
                kMonsters[monster_index], kStates[state_index]);
            ARPG_REQUIRE(clip != nullptr);
            ARPG_REQUIRE(clip->atlas == kAtlases[monster_index]);
            ARPG_REQUIRE(clip->frame_count >= kMinimumFrames[state_index]);
            ARPG_REQUIRE(clip->key_pose_count >= 4U);
            std::array<bool, 4> key_poses_seen{};
            std::array<std::uint64_t, 20> hashes{};
            std::size_t unique_hashes{};
            SilhouetteMetrics previous_metrics{};
            for (std::uint16_t frame{}; frame < clip->frame_count; ++frame) {
                const auto current = arpg::platform::monster_animation_frame(*clip, frame);
                ARPG_REQUIRE(current.has_value());
                ARPG_REQUIRE(current->source.width > 1.0F);
                ARPG_REQUIRE(current->source.height > 1.0F);
                ARPG_REQUIRE(current->key_pose_index < key_poses_seen.size());
                key_poses_seen[current->key_pose_index] = true;
                if (frame > 0U) {
                    const auto previous = arpg::platform::monster_animation_frame(
                        *clip, static_cast<std::uint16_t>(frame - 1U));
                    ARPG_REQUIRE(previous.has_value());
                    ARPG_REQUIRE(current->source.x != previous->source.x
                        || current->source.y != previous->source.y);
                }
                const std::uint64_t hash = frame_pixel_hash(
                    pixels, image.width, current->source);
                const SilhouetteMetrics metrics = frame_silhouette_metrics(
                    pixels, image.width, current->source);
                ARPG_REQUIRE(metrics.visible_pixels > 0U);
                ARPG_REQUIRE(metrics.soft_pixels * 100U
                    <= metrics.visible_pixels * 55U);
                if (frame > 0U) {
                    ARPG_REQUIRE(std::abs(metrics.bounds.height
                        - previous_metrics.bounds.height) <= 42);
                    const int centroid_dx = metrics.centroid_x
                        - previous_metrics.centroid_x;
                    const int centroid_dy = metrics.centroid_y
                        - previous_metrics.centroid_y;
                    ARPG_REQUIRE(centroid_dx * centroid_dx
                        + centroid_dy * centroid_dy <= 34 * 34);
                }
                previous_metrics = metrics;
                bool seen{};
                for (std::size_t prior{}; prior < unique_hashes; ++prior) {
                    if (hashes[prior] == hash) seen = true;
                }
                if (!seen) hashes[unique_hashes++] = hash;
            }
            ARPG_REQUIRE(unique_hashes >= clip->key_pose_count);
            for (const bool seen : key_poses_seen) ARPG_REQUIRE(seen);
        }
        const auto* const idle = arpg::platform::monster_animation_clip(
            kMonsters[monster_index], MonsterAnimationState::idle);
        const auto* const death = arpg::platform::monster_animation_clip(
            kMonsters[monster_index], MonsterAnimationState::death);
        ARPG_REQUIRE(idle != nullptr);
        ARPG_REQUIRE(death != nullptr);
        const auto standing_frame = arpg::platform::monster_animation_frame(
            *idle, 0U);
        const auto collapsed_frame = arpg::platform::monster_animation_frame(
            *death, static_cast<std::uint16_t>(death->frame_count - 1U));
        ARPG_REQUIRE(standing_frame.has_value());
        ARPG_REQUIRE(collapsed_frame.has_value());
        const OpaqueBounds standing = frame_opaque_bounds(
            pixels, image.width, standing_frame->source);
        const OpaqueBounds collapsed = frame_opaque_bounds(
            pixels, image.width, collapsed_frame->source);
        ARPG_REQUIRE(standing.height >= 80);
        ARPG_REQUIRE(collapsed.height * 2 < standing.height);
        ARPG_REQUIRE(collapsed.width > collapsed.height * 2);
        UnloadImageColors(pixels);
        UnloadImage(image);
    }
    return {};
}

arpg::test::Failure water_monster_presentation_collects_and_completes_death() noexcept {
    arpg::platform::MonsterMaterialPresenter presenter{};
    arpg::combat::MonsterSnapshot monster{};
    monster.active = true;
    monster.id = MonsterId::water_bulwark;
    monster.generation = 7U;
    monster.ai_phase = MonsterAiPhase::move;

    const auto moving = presenter.collect_draw_plan(0U, monster, 100U, false);
    ARPG_REQUIRE(moving.visible);
    ARPG_REQUIRE(moving.animation_state == MonsterAnimationState::move);
    ARPG_REQUIRE(moving.frame_index == 0U);

    monster.ai_phase = MonsterAiPhase::defeated;
    const auto death_start = presenter.collect_draw_plan(0U, monster, 101U, false);
    ARPG_REQUIRE(death_start.visible);
    ARPG_REQUIRE(death_start.use_material_frame);
    ARPG_REQUIRE(death_start.animation_state == MonsterAnimationState::death);
    ARPG_REQUIRE(death_start.frame_index == 0U);
    ARPG_REQUIRE(death_start.frame.has_value());

    const auto death_complete = presenter.collect_draw_plan(0U, monster, 161U, false);
    ARPG_REQUIRE(death_complete.visible);
    ARPG_REQUIRE(death_complete.frame_index == 15U);
    ARPG_REQUIRE(death_complete.frame.has_value());
    const auto death_hold = presenter.collect_draw_plan(0U, monster, 220U, false);
    ARPG_REQUIRE(death_hold.visible);
    ARPG_REQUIRE(death_hold.frame_index == 15U);

    monster.active = false;
    ARPG_REQUIRE(!presenter.collect_draw_plan(0U, monster, 221U, false).visible);
    return {};
}

arpg::test::Failure water_monster_presentation_restarts_non_looping_states() noexcept {
    arpg::platform::MonsterMaterialPresenter presenter{};
    arpg::combat::MonsterSnapshot monster{};
    monster.active = true;
    monster.id = MonsterId::water_support;
    monster.generation = 3U;
    monster.ai_phase = MonsterAiPhase::move;
    ARPG_REQUIRE(presenter.collect_draw_plan(1U, monster, 40U, false).frame_index == 0U);
    ARPG_REQUIRE(presenter.collect_draw_plan(1U, monster, 48U, false).frame_index > 0U);

    monster.ai_phase = MonsterAiPhase::telegraph;
    const auto special = presenter.collect_draw_plan(1U, monster, 49U, false);
    ARPG_REQUIRE(special.animation_state == MonsterAnimationState::special);
    ARPG_REQUIRE(special.frame_index == 0U);

    const auto hurt = presenter.collect_draw_plan(1U, monster, 50U, true);
    ARPG_REQUIRE(hurt.animation_state == MonsterAnimationState::hurt);
    ARPG_REQUIRE(hurt.frame_index == 0U);

    ++monster.generation;
    monster.ai_phase = MonsterAiPhase::move;
    const auto reused = presenter.collect_draw_plan(1U, monster, 90U, false);
    ARPG_REQUIRE(reused.animation_state == MonsterAnimationState::move);
    ARPG_REQUIRE(reused.frame_index == 0U);
    return {};
}

arpg::test::Failure water_monster_runtime_states_select_frame_groups() noexcept {
    ARPG_REQUIRE(arpg::platform::select_monster_animation_state(
        MonsterAiPhase::idle, false) == MonsterAnimationState::idle);
    ARPG_REQUIRE(arpg::platform::select_monster_animation_state(
        MonsterAiPhase::move, false) == MonsterAnimationState::move);
    for (const MonsterAiPhase phase : {MonsterAiPhase::telegraph,
             MonsterAiPhase::active, MonsterAiPhase::recovery,
             MonsterAiPhase::cooldown}) {
        ARPG_REQUIRE(arpg::platform::select_monster_animation_state(
            phase, false) == MonsterAnimationState::special);
    }
    ARPG_REQUIRE(arpg::platform::select_monster_animation_state(
        MonsterAiPhase::move, true) == MonsterAnimationState::hurt);
    ARPG_REQUIRE(arpg::platform::select_monster_animation_state(
        MonsterAiPhase::defeated, true) == MonsterAnimationState::death);
    return {};
}

arpg::test::Failure water_ecology_stays_inside_manifest_loading_budget() noexcept {
    const auto manifest = arpg::platform::default_material_manifest();
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(manifest).valid);
    std::size_t loaded_bytes{};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto ecology = manifest.atlases[index].ecology;
        if (ecology == MaterialEcology::common || ecology == MaterialEcology::water) {
            loaded_bytes += manifest.atlases[index].rgba_bytes * 2U;
        }
    }
    ARPG_REQUIRE(loaded_bytes <= manifest.memory_budget_bytes);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"loads independent water color and material atlases",
        &water_ecology_has_independent_loadable_color_and_material_atlases},
    {"water room consumes water-only environment materials",
        &water_room_consumes_only_water_environment_materials},
    {"water monsters expose required multiframe state groups",
        &water_monsters_expose_complete_multiframe_state_groups},
    {"water runtime states select animation frame groups",
        &water_monster_runtime_states_select_frame_groups},
    {"defeated water monster reaches and completes death frames",
        &water_monster_presentation_collects_and_completes_death},
    {"water presentation restarts non-looping states",
        &water_monster_presentation_restarts_non_looping_states},
    {"water ecology remains inside loading budget",
        &water_ecology_stays_inside_manifest_loading_budget},
};

}  // namespace

arpg::test::TestSuite ecology_material_coverage_suite() noexcept {
    return arpg::test::make_suite("ecology_material_coverage", kCases);
}

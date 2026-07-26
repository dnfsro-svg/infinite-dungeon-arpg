#include "test_framework.hpp"

#include "chaos_room_material_slice.hpp"
#include "material_animation.hpp"
#include "material_asset_validation.hpp"
#include "material_manifest.hpp"
#include "monster_material_presenter.hpp"
#include "lightning_room_material_slice.hpp"
#include "water_room_material_slice.hpp"

#include <raylib.h>

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdlib>
#include <filesystem>
#include <fstream>
#include <sstream>
#include <string>
#include <vector>

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

std::uint64_t frame_perceptual_hash(const Color* pixels, int image_width,
    const Rectangle& source) noexcept {
    std::uint64_t hash = 1469598103934665603ULL;
    const int left = static_cast<int>(source.x);
    const int top = static_cast<int>(source.y);
    constexpr int kBlocks = 8;
    const int block_width = static_cast<int>(source.width) / kBlocks;
    const int block_height = static_cast<int>(source.height) / kBlocks;
    for (int block_y{}; block_y < kBlocks; ++block_y) {
        for (int block_x{}; block_x < kBlocks; ++block_x) {
            std::array<std::uint32_t, 4> sums{};
            for (int y{}; y < block_height; ++y) {
                for (int x{}; x < block_width; ++x) {
                    const Color pixel = pixels[
                        (top + block_y * block_height + y) * image_width
                        + left + block_x * block_width + x];
                    sums[0] += pixel.r;
                    sums[1] += pixel.g;
                    sums[2] += pixel.b;
                    sums[3] += pixel.a;
                }
            }
            constexpr std::uint32_t kPixelsPerBlock = 12U * 12U;
            for (const std::uint32_t sum : sums) {
                const std::uint64_t quantized = (sum / kPixelsPerBlock) / 16U;
                hash ^= quantized;
                hash *= 1099511628211ULL;
            }
        }
    }
    return hash;
}

std::uint64_t adjacent_changed_pixels(const Color* pixels, int image_width,
    const Rectangle& lhs, const Rectangle& rhs,
    std::uint64_t& visible_union) noexcept {
    std::uint64_t changed{};
    visible_union = 0U;
    const int width = static_cast<int>(lhs.width);
    const int height = static_cast<int>(lhs.height);
    for (int y{}; y < height; ++y) {
        for (int x{}; x < width; ++x) {
            const Color a = pixels[(static_cast<int>(lhs.y) + y) * image_width
                + static_cast<int>(lhs.x) + x];
            const Color b = pixels[(static_cast<int>(rhs.y) + y) * image_width
                + static_cast<int>(rhs.x) + x];
            if (a.a > 24U || b.a > 24U) ++visible_union;
            const int difference = std::abs(static_cast<int>(a.r) - b.r)
                + std::abs(static_cast<int>(a.g) - b.g)
                + std::abs(static_cast<int>(a.b) - b.b)
                + std::abs(static_cast<int>(a.a) - b.a);
            if (difference >= 48) ++changed;
        }
    }
    return changed;
}

struct OpaqueBounds final {
    int width{};
    int height{};
};

struct ConnectedSilhouette final {
    std::uint64_t visible_pixels{};
    std::uint64_t largest_component{};
    std::uint64_t second_component{};
};

ConnectedSilhouette connected_silhouette(const Color* pixels, int image_width,
    const Rectangle& source) noexcept {
    constexpr int kCell = 96;
    std::array<bool, kCell * kCell> visited{};
    std::array<int, kCell * kCell> queue{};
    ConnectedSilhouette result{};
    const int left = static_cast<int>(source.x);
    const int top = static_cast<int>(source.y);
    const auto visible = [&](int x, int y) noexcept {
        return pixels[(top + y) * image_width + left + x].a > 96U;
    };
    for (int y{}; y < kCell; ++y) {
        for (int x{}; x < kCell; ++x) {
            if (visible(x, y)) ++result.visible_pixels;
            const int start = y * kCell + x;
            if (visited[start] || !visible(x, y)) continue;
            std::size_t read{};
            std::size_t write{1U};
            queue[0] = start;
            visited[start] = true;
            while (read < write) {
                const int cell = queue[read++];
                const int cx = cell % kCell;
                const int cy = cell / kCell;
                for (int dy = -1; dy <= 1; ++dy) {
                    for (int dx = -1; dx <= 1; ++dx) {
                        if ((dx == 0 && dy == 0) || cx + dx < 0
                            || cx + dx >= kCell || cy + dy < 0
                            || cy + dy >= kCell) continue;
                        const int neighbor = (cy + dy) * kCell + cx + dx;
                        if (visited[neighbor] || !visible(cx + dx, cy + dy)) {
                            continue;
                        }
                        visited[neighbor] = true;
                        queue[write++] = neighbor;
                    }
                }
            }
            const std::uint64_t component = write;
            if (component > result.largest_component) {
                result.second_component = result.largest_component;
                result.largest_component = component;
            } else if (component > result.second_component) {
                result.second_component = component;
            }
        }
    }
    return result;
}

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

int frame_opaque_bottom(const Color* pixels, int image_width,
    const Rectangle& source) noexcept {
    const int left = static_cast<int>(source.x);
    const int top = static_cast<int>(source.y);
    int opaque_bottom{-1};
    for (int y{}; y < static_cast<int>(source.height); ++y) {
        for (int x{}; x < static_cast<int>(source.width); ++x) {
            if (pixels[(top + y) * image_width + left + x].a > 96U) {
                opaque_bottom = (std::max)(opaque_bottom, y);
            }
        }
    }
    return opaque_bottom;
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
        ARPG_REQUIRE(frame->atlas == (index == 2U
            ? MaterialAtlasId::element_doors : MaterialAtlasId::water_environment));
    }
    return {};
}

arpg::test::Failure monsters_expose_complete_multiframe_state_groups(
    const std::array<MonsterId, 2>& monsters,
    const std::array<MaterialAtlasId, 2>& atlases) noexcept {
    constexpr std::array<MonsterAnimationState, 5> kStates{{
        MonsterAnimationState::idle,
        MonsterAnimationState::move,
        MonsterAnimationState::special,
        MonsterAnimationState::hurt,
        MonsterAnimationState::death,
    }};
    constexpr std::array<std::uint16_t, 5> kMinimumFrames{{12U, 16U, 20U, 8U, 16U}};
    for (std::size_t monster_index{}; monster_index < monsters.size();
         ++monster_index) {
        const MaterialAtlasDefinition* const atlas = find_atlas(
            atlases[monster_index]);
        ARPG_REQUIRE(atlas != nullptr);
        const std::filesystem::path path = std::filesystem::path{
            ARPG_PROJECT_SOURCE_DIR} / atlas->color_path;
        const Image image = LoadImage(path.string().c_str());
        ARPG_REQUIRE(image.data != nullptr);
        Color* const pixels = LoadImageColors(image);
        ARPG_REQUIRE(pixels != nullptr);
        for (std::size_t state_index{}; state_index < kStates.size(); ++state_index) {
            const auto* const clip = arpg::platform::monster_animation_clip(
                monsters[monster_index], kStates[state_index]);
            ARPG_REQUIRE(clip != nullptr);
            ARPG_REQUIRE(clip->atlas == atlases[monster_index]);
            ARPG_REQUIRE(clip->frame_count >= kMinimumFrames[state_index]);
            ARPG_REQUIRE(clip->key_pose_count >= 4U);
            std::array<bool, 4> key_poses_seen{};
            std::array<std::uint64_t, 20> hashes{};
            std::size_t unique_hashes{};
            SilhouetteMetrics previous_metrics{};
            int minimum_bottom{96};
            int maximum_bottom{-1};
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
                    std::uint64_t visible_union{};
                    const std::uint64_t changed = adjacent_changed_pixels(
                        pixels, image.width, previous->source, current->source,
                        visible_union);
                    ARPG_REQUIRE(visible_union > 0U);
                    ARPG_REQUIRE(changed * 100U >= visible_union * 3U);
                }
                const std::uint64_t hash = frame_perceptual_hash(
                    pixels, image.width, current->source);
                const SilhouetteMetrics metrics = frame_silhouette_metrics(
                    pixels, image.width, current->source);
                const ConnectedSilhouette connected = connected_silhouette(
                    pixels, image.width, current->source);
                const int opaque_bottom = frame_opaque_bottom(
                    pixels, image.width, current->source);
                ARPG_REQUIRE(metrics.visible_pixels > 0U);
                ARPG_REQUIRE(connected.visible_pixels > 0U);
                ARPG_REQUIRE(connected.largest_component * 100U
                    >= connected.visible_pixels * 94U);
                ARPG_REQUIRE(connected.second_component * 100U
                    <= connected.visible_pixels * 2U);
                minimum_bottom = (std::min)(minimum_bottom, opaque_bottom);
                maximum_bottom = (std::max)(maximum_bottom, opaque_bottom);
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
            ARPG_REQUIRE(unique_hashes == clip->frame_count);
            for (const bool seen : key_poses_seen) ARPG_REQUIRE(seen);
            if (atlases[monster_index] == MaterialAtlasId::lightning_shooter
                || atlases[monster_index] == MaterialAtlasId::lightning_dasher) {
                ARPG_REQUIRE(maximum_bottom - minimum_bottom <= 1);
            }
        }
        const auto* const idle = arpg::platform::monster_animation_clip(
            monsters[monster_index], MonsterAnimationState::idle);
        const auto* const death = arpg::platform::monster_animation_clip(
            monsters[monster_index], MonsterAnimationState::death);
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

arpg::test::Failure water_monsters_expose_complete_multiframe_state_groups() noexcept {
    return monsters_expose_complete_multiframe_state_groups(
        {MonsterId::water_bulwark, MonsterId::water_support},
        {MaterialAtlasId::water_bulwark, MaterialAtlasId::water_support});
}

arpg::test::Failure lightning_ecology_has_independent_loadable_color_and_material_atlases() noexcept {
    constexpr std::array<MaterialAtlasId, 3> kAtlases{{
        MaterialAtlasId::lightning_environment,
        MaterialAtlasId::lightning_shooter,
        MaterialAtlasId::lightning_dasher,
    }};
    for (const MaterialAtlasId id : kAtlases) {
        const MaterialAtlasDefinition* const atlas = find_atlas(id);
        ARPG_REQUIRE(atlas != nullptr);
        ARPG_REQUIRE(atlas->ecology == MaterialEcology::lightning);
        ARPG_REQUIRE(atlas->color_path != nullptr);
        ARPG_REQUIRE(atlas->material_path != nullptr);
        const std::string color = std::filesystem::path{atlas->color_path}
            .filename().string();
        const std::string material = std::filesystem::path{atlas->material_path}
            .filename().string();
        ARPG_REQUIRE(color.find("lightning") != std::string::npos);
        ARPG_REQUIRE(material.find("lightning") != std::string::npos);
        ARPG_REQUIRE(color.find("fire") == std::string::npos);
        ARPG_REQUIRE(color.find("water") == std::string::npos);
        ARPG_REQUIRE(image_has_visible_color(
            atlas->color_path, atlas->width, atlas->height));
        ARPG_REQUIRE(image_has_visible_color(
            atlas->material_path, atlas->width, atlas->height));
    }
    return {};
}

arpg::test::Failure lightning_room_consumes_only_lightning_environment_materials() noexcept {
    const auto& slice = arpg::platform::lightning_room_material_slice();
    constexpr std::array<arpg::platform::LightningRoomPropId, 7> kRequired{{
        arpg::platform::LightningRoomPropId::floor,
        arpg::platform::LightningRoomPropId::wall,
        arpg::platform::LightningRoomPropId::door,
        arpg::platform::LightningRoomPropId::hole,
        arpg::platform::LightningRoomPropId::arc_lamp,
        arpg::platform::LightningRoomPropId::capacitor_bank,
        arpg::platform::LightningRoomPropId::grounding_rod,
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
        ARPG_REQUIRE(frame->atlas == (index == 2U
            ? MaterialAtlasId::element_doors : MaterialAtlasId::lightning_environment));
    }
    return {};
}

arpg::test::Failure lightning_environment_keeps_high_contrast_warning_palette() noexcept {
    const MaterialAtlasDefinition* const atlas = find_atlas(
        MaterialAtlasId::lightning_environment);
    ARPG_REQUIRE(atlas != nullptr);
    const std::filesystem::path path = std::filesystem::path{
        ARPG_PROJECT_SOURCE_DIR} / atlas->color_path;
    const Image image = LoadImage(path.string().c_str());
    ARPG_REQUIRE(image.data != nullptr);
    Color* const pixels = LoadImageColors(image);
    ARPG_REQUIRE(pixels != nullptr);
    std::uint64_t dark{};
    std::uint64_t warning{};
    std::uint64_t cyan{};
    const std::size_t count = static_cast<std::size_t>(image.width)
        * static_cast<std::size_t>(image.height);
    for (std::size_t index{}; index < count; ++index) {
        const Color pixel = pixels[index];
        if (pixel.a <= 24U) continue;
        const unsigned int luminance = static_cast<unsigned int>(pixel.r)
            + pixel.g + pixel.b;
        if (luminance < 180U) ++dark;
        if (pixel.r > 180U && pixel.g > 140U && pixel.b < 100U) ++warning;
        if (pixel.b > 170U && pixel.g > 120U && pixel.r < 140U) ++cyan;
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    ARPG_REQUIRE(dark > count / 20U);
    ARPG_REQUIRE(warning > count / 500U);
    ARPG_REQUIRE(cyan > count / 1000U);
    return {};
}

arpg::test::Failure lightning_monsters_expose_complete_multiframe_state_groups() noexcept {
    return monsters_expose_complete_multiframe_state_groups(
        {MonsterId::lightning_shooter, MonsterId::lightning_dasher},
        {MaterialAtlasId::lightning_shooter, MaterialAtlasId::lightning_dasher});
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

arpg::test::Failure lightning_monster_presentation_runs_special_hurt_and_death() noexcept {
    arpg::platform::MonsterMaterialPresenter presenter{};
    arpg::combat::MonsterSnapshot monster{};
    monster.active = true;
    monster.id = MonsterId::lightning_shooter;
    monster.generation = 11U;
    monster.ai_phase = MonsterAiPhase::move;
    ARPG_REQUIRE(presenter.collect_draw_plan(0U, monster, 40U, false).frame_index == 0U);
    ARPG_REQUIRE(presenter.collect_draw_plan(0U, monster, 48U, false).frame_index > 0U);

    monster.ai_phase = MonsterAiPhase::telegraph;
    const auto special = presenter.collect_draw_plan(0U, monster, 49U, false);
    ARPG_REQUIRE(special.use_material_frame);
    ARPG_REQUIRE(special.animation_state == MonsterAnimationState::special);
    ARPG_REQUIRE(special.frame_index == 0U);
    const auto hurt = presenter.collect_draw_plan(0U, monster, 50U, true);
    ARPG_REQUIRE(hurt.animation_state == MonsterAnimationState::hurt);
    ARPG_REQUIRE(hurt.frame_index == 0U);

    monster.ai_phase = MonsterAiPhase::defeated;
    const auto death = presenter.collect_draw_plan(0U, monster, 51U, true);
    ARPG_REQUIRE(death.animation_state == MonsterAnimationState::death);
    ARPG_REQUIRE(death.frame_index == 0U);
    ARPG_REQUIRE(death.frame.has_value());
    const auto complete = presenter.collect_draw_plan(0U, monster, 120U, false);
    ARPG_REQUIRE(complete.frame_index == 15U);
    ARPG_REQUIRE(complete.frame.has_value());
    return {};
}

arpg::test::Failure lightning_ecology_stays_inside_manifest_loading_budget() noexcept {
    const auto manifest = arpg::platform::default_material_manifest();
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(manifest).valid);
    std::size_t loaded_bytes{};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto ecology = manifest.atlases[index].ecology;
        if (ecology == MaterialEcology::common
            || ecology == MaterialEcology::lightning) {
            loaded_bytes += manifest.atlases[index].rgba_bytes * 2U;
        }
    }
    ARPG_REQUIRE(loaded_bytes <= manifest.memory_budget_bytes);
    return {};
}

arpg::test::Failure chaos_ecology_has_independent_loadable_color_and_material_atlases() noexcept {
    constexpr std::array<MaterialAtlasId, 3> kAtlases{{
        MaterialAtlasId::chaos_environment,
        MaterialAtlasId::chaos_chaser,
        MaterialAtlasId::chaos_hazard,
    }};
    for (const MaterialAtlasId id : kAtlases) {
        const MaterialAtlasDefinition* const atlas = find_atlas(id);
        ARPG_REQUIRE(atlas != nullptr);
        ARPG_REQUIRE(atlas->ecology == MaterialEcology::chaos);
        ARPG_REQUIRE(atlas->color_path != nullptr);
        ARPG_REQUIRE(atlas->material_path != nullptr);
        const std::string color = std::filesystem::path{atlas->color_path}
            .filename().string();
        const std::string material = std::filesystem::path{atlas->material_path}
            .filename().string();
        ARPG_REQUIRE(color.find("chaos") != std::string::npos);
        ARPG_REQUIRE(material.find("chaos") != std::string::npos);
        ARPG_REQUIRE(color.find("fire") == std::string::npos);
        ARPG_REQUIRE(color.find("water") == std::string::npos);
        ARPG_REQUIRE(color.find("lightning") == std::string::npos);
        ARPG_REQUIRE(image_has_visible_color(
            atlas->color_path, atlas->width, atlas->height));
        ARPG_REQUIRE(image_has_visible_color(
            atlas->material_path, atlas->width, atlas->height));
    }
    return {};
}

arpg::test::Failure chaos_room_consumes_only_chaos_environment_materials() noexcept {
    const auto& slice = arpg::platform::chaos_room_material_slice();
    constexpr std::array<arpg::platform::ChaosRoomPropId, 7> kRequired{{
        arpg::platform::ChaosRoomPropId::floor,
        arpg::platform::ChaosRoomPropId::wall,
        arpg::platform::ChaosRoomPropId::door,
        arpg::platform::ChaosRoomPropId::hole,
        arpg::platform::ChaosRoomPropId::rift_lantern,
        arpg::platform::ChaosRoomPropId::anomaly_condenser,
        arpg::platform::ChaosRoomPropId::warning_obelisk,
    }};
    const auto manifest = arpg::platform::default_material_manifest();
    for (std::size_t index{}; index < kRequired.size(); ++index) {
        const auto& prop = slice.props[index];
        ARPG_REQUIRE(prop.id == kRequired[index]);
        const arpg::platform::MaterialFrameDefinition* frame{};
        for (std::size_t frame_index{}; frame_index < manifest.frame_count;
             ++frame_index) {
            if (manifest.frames[frame_index].id == prop.sprite) {
                frame = &manifest.frames[frame_index];
                break;
            }
        }
        ARPG_REQUIRE(frame != nullptr);
        ARPG_REQUIRE(frame->atlas == (index == 2U
            ? MaterialAtlasId::element_doors : MaterialAtlasId::chaos_environment));
    }
    return {};
}

arpg::test::Failure chaos_environment_keeps_readable_rift_warning_palette() noexcept {
    const auto* atlas = find_atlas(MaterialAtlasId::chaos_environment);
    ARPG_REQUIRE(atlas != nullptr);
    const std::filesystem::path path = std::filesystem::path{
        ARPG_PROJECT_SOURCE_DIR} / atlas->color_path;
    const Image image = LoadImage(path.string().c_str());
    ARPG_REQUIRE(image.data != nullptr);
    Color* const pixels = LoadImageColors(image);
    ARPG_REQUIRE(pixels != nullptr);
    std::uint64_t dark{};
    std::uint64_t magenta{};
    std::uint64_t acid{};
    const std::size_t count = static_cast<std::size_t>(image.width)
        * static_cast<std::size_t>(image.height);
    for (std::size_t index{}; index < count; ++index) {
        const Color pixel = pixels[index];
        if (pixel.a <= 24U) continue;
        if (static_cast<unsigned int>(pixel.r) + pixel.g + pixel.b < 190U) ++dark;
        if (pixel.r > 115U && pixel.b > 105U && pixel.g < 100U) ++magenta;
        if (pixel.g > 100U && pixel.g > pixel.r + 15U
            && pixel.r > pixel.b) ++acid;
    }
    UnloadImageColors(pixels);
    UnloadImage(image);
    ARPG_REQUIRE(dark > count / 20U);
    ARPG_REQUIRE(magenta > count / 1000U);
    ARPG_REQUIRE(acid > count / 2500U);
    return {};
}

arpg::test::Failure chaos_monsters_expose_complete_multiframe_state_groups() noexcept {
    return monsters_expose_complete_multiframe_state_groups(
        {MonsterId::chaos_chaser, MonsterId::chaos_hazard},
        {MaterialAtlasId::chaos_chaser, MaterialAtlasId::chaos_hazard});
}

arpg::test::Failure chaos_monster_presentation_runs_special_hurt_and_death() noexcept {
    arpg::platform::MonsterMaterialPresenter presenter{};
    arpg::combat::MonsterSnapshot monster{};
    monster.active = true;
    monster.id = MonsterId::chaos_chaser;
    monster.generation = 13U;
    monster.ai_phase = MonsterAiPhase::move;
    ARPG_REQUIRE(presenter.collect_draw_plan(0U, monster, 40U, false).frame_index == 0U);
    ARPG_REQUIRE(presenter.collect_draw_plan(0U, monster, 48U, false).frame_index > 0U);
    monster.ai_phase = MonsterAiPhase::telegraph;
    const auto special = presenter.collect_draw_plan(0U, monster, 49U, false);
    ARPG_REQUIRE(special.use_material_frame);
    ARPG_REQUIRE(special.animation_state == MonsterAnimationState::special);
    ARPG_REQUIRE(special.frame_index == 0U);
    const auto hurt = presenter.collect_draw_plan(0U, monster, 50U, true);
    ARPG_REQUIRE(hurt.animation_state == MonsterAnimationState::hurt);
    monster.ai_phase = MonsterAiPhase::defeated;
    const auto death = presenter.collect_draw_plan(0U, monster, 51U, true);
    ARPG_REQUIRE(death.animation_state == MonsterAnimationState::death);
    ARPG_REQUIRE(death.frame_index == 0U);
    const auto complete = presenter.collect_draw_plan(0U, monster, 120U, false);
    ARPG_REQUIRE(complete.frame_index == 15U);
    return {};
}

arpg::test::Failure chaos_ecology_stays_inside_manifest_loading_budget() noexcept {
    const auto manifest = arpg::platform::default_material_manifest();
    ARPG_REQUIRE(arpg::platform::validate_material_manifest(manifest).valid);
    std::size_t loaded_bytes{};
    for (std::size_t index{}; index < manifest.atlas_count; ++index) {
        const auto ecology = manifest.atlases[index].ecology;
        if (ecology == MaterialEcology::common || ecology == MaterialEcology::chaos) {
            loaded_bytes += manifest.atlases[index].rgba_bytes * 2U;
        }
    }
    ARPG_REQUIRE(loaded_bytes <= manifest.memory_budget_bytes);
    return {};
}

struct EmittedPropRecord final {
    arpg::platform::MaterialSpriteId sprite{arpg::platform::MaterialSpriteId::missing};
    std::array<float, 4> source_rect{};
    std::array<float, 2> foot_anchor{};
    std::array<float, 4> alpha_bbox{};
};

class JsonCursor final {
public:
    explicit JsonCursor(const std::string& text) noexcept : text_{text} {}

    bool consume(char expected) noexcept {
        skip_space();
        if (position_ == text_.size() || text_[position_] != expected) return false;
        ++position_;
        return true;
    }

    bool string(std::string& value) noexcept {
        if (!consume('\"')) return false;
        value.clear();
        while (position_ < text_.size() && text_[position_] != '\"') {
            if (text_[position_] == '\\' || text_[position_] < ' ') return false;
            value += text_[position_++];
        }
        if (position_ == text_.size()) return false;
        ++position_;
        return true;
    }

    bool numbers(float* values, std::size_t count) noexcept {
        if (!consume('[')) return false;
        for (std::size_t index{}; index < count; ++index) {
            skip_space();
            bool negative{};
            if (position_ < text_.size() && text_[position_] == '-') {
                negative = true;
                ++position_;
            }
            if (position_ == text_.size() || text_[position_] < '0'
                || text_[position_] > '9') return false;
            unsigned int number{};
            while (position_ < text_.size() && text_[position_] >= '0'
                && text_[position_] <= '9') {
                number = number * 10U + static_cast<unsigned int>(text_[position_] - '0');
                ++position_;
            }
            values[index] = static_cast<float>(negative ? -static_cast<int>(number) : number);
            if (index + 1U != count && !consume(',')) return false;
        }
        return consume(']');
    }

    bool skip_value() noexcept {
        skip_space();
        if (position_ == text_.size()) return false;
        if (text_[position_] == '\"') {
            std::string ignored;
            return string(ignored);
        }
        if (text_[position_] == '{') return skip_container('{', '}');
        if (text_[position_] == '[') return skip_container('[', ']');
        const std::size_t start = position_;
        while (position_ < text_.size() && text_[position_] != ','
            && text_[position_] != '}' && text_[position_] != ']'
            && text_[position_] != ' ' && text_[position_] != '\n') ++position_;
        return position_ != start;
    }

private:
    bool skip_container(char open, char close) noexcept {
        if (!consume(open)) return false;
        skip_space();
        if (position_ < text_.size() && text_[position_] == close) {
            ++position_;
            return true;
        }
        while (true) {
            if (open == '{') {
                std::string key;
                if (!string(key) || !consume(':')) return false;
            }
            if (!skip_value()) return false;
            skip_space();
            if (position_ < text_.size() && text_[position_] == close) {
                ++position_;
                return true;
            }
            if (!consume(',')) return false;
        }
    }

    void skip_space() noexcept {
        while (position_ < text_.size() && (text_[position_] == ' '
            || text_[position_] == '\n' || text_[position_] == '\r'
            || text_[position_] == '\t')) ++position_;
    }

    const std::string& text_;
    std::size_t position_{};
};

arpg::platform::MaterialSpriteId sprite_for_prop(const std::string& ecology,
    const std::string& name) noexcept {
    using arpg::platform::MaterialSpriteId;
    if (ecology == "water") {
        if (name == "wall") return MaterialSpriteId::water_wall;
        if (name == "surface_prop") return MaterialSpriteId::water_grate;
        if (name == "hole") return MaterialSpriteId::water_hole;
        if (name == "light") return MaterialSpriteId::water_lantern;
        if (name == "solid_prop") return MaterialSpriteId::water_coral;
    }
    if (ecology == "lightning") {
        if (name == "wall") return MaterialSpriteId::lightning_wall;
        if (name == "surface_prop") return MaterialSpriteId::lightning_capacitor_bank;
        if (name == "hole") return MaterialSpriteId::lightning_hole;
        if (name == "light") return MaterialSpriteId::lightning_arc_lamp;
        if (name == "solid_prop") return MaterialSpriteId::lightning_grounding_rod;
    }
    if (ecology == "chaos") {
        if (name == "wall") return MaterialSpriteId::chaos_wall;
        if (name == "surface_prop") return MaterialSpriteId::chaos_anomaly_condenser;
        if (name == "hole") return MaterialSpriteId::chaos_hole;
        if (name == "light") return MaterialSpriteId::chaos_rift_lantern;
        if (name == "solid_prop") return MaterialSpriteId::chaos_warning_obelisk;
    }
    return MaterialSpriteId::missing;
}

bool parse_prop_record(JsonCursor& cursor, const std::string& ecology,
    const std::string& name, EmittedPropRecord& record) noexcept {
    if (!cursor.consume('{')) return false;
    bool source_seen{};
    bool anchor_seen{};
    bool alpha_seen{};
    while (true) {
        std::string field;
        if (!cursor.string(field) || !cursor.consume(':')) return false;
        if (field == "source_rect") source_seen = cursor.numbers(record.source_rect.data(), 4U);
        else if (field == "foot_anchor") anchor_seen = cursor.numbers(record.foot_anchor.data(), 2U);
        else if (field == "alpha_bbox") alpha_seen = cursor.numbers(record.alpha_bbox.data(), 4U);
        else if (!cursor.skip_value()) return false;
        if (!source_seen && field == "source_rect") return false;
        if (!anchor_seen && field == "foot_anchor") return false;
        if (!alpha_seen && field == "alpha_bbox") return false;
        if (cursor.consume('}')) break;
        if (!cursor.consume(',')) return false;
    }
    record.sprite = sprite_for_prop(ecology, name);
    return source_seen && anchor_seen && alpha_seen
        && record.sprite != arpg::platform::MaterialSpriteId::missing;
}

bool parse_props(JsonCursor& cursor, const std::string& ecology,
    std::vector<EmittedPropRecord>& records) noexcept {
    if (!cursor.consume('{')) return false;
    while (true) {
        std::string name;
        EmittedPropRecord record;
        if (!cursor.string(name) || !cursor.consume(':')
            || !parse_prop_record(cursor, ecology, name, record)) return false;
        records.push_back(record);
        if (cursor.consume('}')) return true;
        if (!cursor.consume(',')) return false;
    }
}

std::vector<EmittedPropRecord> parse_environment_prop_records() noexcept {
    const std::filesystem::path path = std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}
        / "assets/stage12/environment-props-build.json";
    std::ifstream file{path};
    std::ostringstream stream;
    stream << file.rdbuf();
    const std::string text = stream.str();
    JsonCursor cursor{text};
    std::vector<EmittedPropRecord> records;
    if (!file.good() && !file.eof()) return records;
    if (!cursor.consume('{')) return records;
    while (true) {
        std::string root_key;
        if (!cursor.string(root_key) || !cursor.consume(':')) return {};
        if (root_key != "ecologies") {
            if (!cursor.skip_value()) return {};
        } else if (!cursor.consume('{')) return {};
        else {
            while (true) {
                std::string ecology;
                if (!cursor.string(ecology) || !cursor.consume(':') || !cursor.consume('{')) return {};
                while (true) {
                    std::string field;
                    if (!cursor.string(field) || !cursor.consume(':')) return {};
                    if (field == "objects") {
                        if (!parse_props(cursor, ecology, records)) return {};
                    } else if (!cursor.skip_value()) return {};
                    if (cursor.consume('}')) break;
                    if (!cursor.consume(',')) return {};
                }
                if (cursor.consume('}')) break;
                if (!cursor.consume(',')) return {};
            }
        }
        if (cursor.consume('}')) return records;
        if (!cursor.consume(',')) return {};
    }
}

arpg::test::Failure common_element_doors_have_unique_frames() noexcept {
    const auto manifest = arpg::platform::default_material_manifest();
    const auto* const atlas = find_atlas(MaterialAtlasId::element_doors);
    ARPG_REQUIRE(atlas != nullptr);
    ARPG_REQUIRE(atlas->ecology == MaterialEcology::common);
    ARPG_REQUIRE(atlas->width == 1024 && atlas->height == 256);
    ARPG_REQUIRE(atlas->rgba_bytes == 1'048'576U);
    constexpr std::array<arpg::platform::MaterialSpriteId, 4> kDoors{{
        arpg::platform::MaterialSpriteId::environment_door_fire,
        arpg::platform::MaterialSpriteId::environment_door_water,
        arpg::platform::MaterialSpriteId::environment_door_lightning,
        arpg::platform::MaterialSpriteId::environment_door_chaos,
    }};
    for (std::size_t index{}; index < kDoors.size(); ++index) {
        const auto* const frame = arpg::platform::find_material_frame(manifest, kDoors[index]);
        ARPG_REQUIRE(frame != nullptr);
        ARPG_REQUIRE(frame->atlas == MaterialAtlasId::element_doors);
        ARPG_REQUIRE(frame->source.x == static_cast<float>(index * 256U));
        ARPG_REQUIRE(frame->source.y == 0.0F);
        ARPG_REQUIRE(frame->source.width == 256.0F && frame->source.height == 256.0F);
        ARPG_REQUIRE(frame->foot_anchor.x == 128.0F && frame->foot_anchor.y == 244.0F);
    }
    return {};
}

arpg::test::Failure ecology_prop_records_match_emitted_layout() noexcept {
    const auto records = parse_environment_prop_records();
    ARPG_REQUIRE(records.size() == 15U);
    const auto manifest = arpg::platform::default_material_manifest();
    for (const auto& record : records) {
        const auto* const frame = arpg::platform::find_material_frame(
            manifest, record.sprite);
        ARPG_REQUIRE(frame != nullptr);
        const auto* const atlas = find_atlas(frame->atlas);
        ARPG_REQUIRE(atlas != nullptr);
        ARPG_REQUIRE(frame->source.x == record.source_rect[0]
            && frame->source.y == record.source_rect[1]);
        ARPG_REQUIRE(frame->source.width == record.source_rect[2]
            && frame->source.height == record.source_rect[3]);
        ARPG_REQUIRE(frame->foot_anchor.x == record.foot_anchor[0]
            && frame->foot_anchor.y == record.foot_anchor[1]);
        ARPG_REQUIRE(frame->source.x >= 0.0F && frame->source.y >= 0.0F);
        ARPG_REQUIRE(frame->source.x + frame->source.width <= static_cast<float>(atlas->width));
        ARPG_REQUIRE(frame->source.y + frame->source.height <= static_cast<float>(atlas->height));
    }
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
    {"loads independent lightning color and material atlases",
        &lightning_ecology_has_independent_loadable_color_and_material_atlases},
    {"lightning room consumes lightning-only environment materials",
        &lightning_room_consumes_only_lightning_environment_materials},
    {"lightning room keeps a high contrast warning palette",
        &lightning_environment_keeps_high_contrast_warning_palette},
    {"lightning monsters expose required multiframe state groups",
        &lightning_monsters_expose_complete_multiframe_state_groups},
    {"lightning presentation runs special hurt and death frames",
        &lightning_monster_presentation_runs_special_hurt_and_death},
    {"lightning ecology remains inside loading budget",
        &lightning_ecology_stays_inside_manifest_loading_budget},
    {"loads independent chaos color and material atlases",
        &chaos_ecology_has_independent_loadable_color_and_material_atlases},
    {"chaos room consumes chaos-only environment materials",
        &chaos_room_consumes_only_chaos_environment_materials},
    {"chaos room keeps readable rift warning palette",
        &chaos_environment_keeps_readable_rift_warning_palette},
    {"chaos monsters expose required multiframe state groups",
        &chaos_monsters_expose_complete_multiframe_state_groups},
    {"chaos presentation runs special hurt and death frames",
        &chaos_monster_presentation_runs_special_hurt_and_death},
    {"chaos ecology remains inside loading budget",
        &chaos_ecology_stays_inside_manifest_loading_budget},
    {"common element doors use four unique frames", &common_element_doors_have_unique_frames},
    {"ecology props match emitted layout records", &ecology_prop_records_match_emitted_layout},
};

}  // namespace

arpg::test::TestSuite ecology_material_coverage_suite() noexcept {
    return arpg::test::make_suite("ecology_material_coverage", kCases);
}

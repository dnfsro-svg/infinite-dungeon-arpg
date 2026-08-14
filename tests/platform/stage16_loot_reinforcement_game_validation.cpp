#include "combat_renderer.hpp"
#include "combat_view_math.hpp"
#include "control_hints.hpp"
#include "dungeon_test_support.hpp"
#include "combat/active_skill_runtime.hpp"
#include "combat/fire_room_obstacle.hpp"
#include "combat/monster_affix_generation.hpp"
#include "combat/monster_catalog.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/material_loot.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_combat_template.hpp"
#include "dungeon/room_environment.hpp"
#include "dungeon/room_monster_plan_builder.hpp"
#include "dungeon_runtime.hpp"
#include "host_input.hpp"
#include "inventory_renderer.hpp"
#include "inventory_view_math.hpp"
#include "shared_ui_font.hpp"
#include "items/item_catalog.hpp"
#include "items/item_crafting.hpp"
#include "items/item_generation.hpp"
#include "items/material_catalog.hpp"
#include "persistence/save_store.hpp"
#include "stage10_validation_build.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <array>
#include <chrono>
#include <cmath>
#include <cstring>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <thread>
#include <utility>

namespace {

namespace combat = arpg::combat;
namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace persistence = arpg::persistence;
namespace platform = arpg::platform;

struct Resolution final {
    int width{};
    int height{};
};

constexpr Resolution kLegacyResolution{1280, 720};
constexpr std::array<Resolution, 3U> kPoe2Resolutions{{
    {800, 450},
    {1280, 720},
    {1920, 1080},
}};
constexpr std::array<const char*, 16U> kPoe2InventoryScenarios{{
    "empty",
    "filter-empty",
    "all-equipped",
    "selected",
    "recipe",
    "max-tooltip",
    "tooltip-left-source",
    "tooltip-closed",
    "saving",
    "saved",
    "error",
    "material-target",
    "material-accepted",
    "material-rejected",
    "material-exhausted",
    "receipt",
}};

enum class Poe2Scenario : std::size_t {
    empty,
    filter_empty,
    all_equipped,
    selected,
    recipe,
    max_tooltip,
    tooltip_left_source,
    tooltip_closed,
    saving,
    saved,
    error,
    material_target,
    material_accepted,
    material_rejected,
    material_exhausted,
    receipt,
    count,
};

static_assert(static_cast<std::size_t>(Poe2Scenario::count)
    == kPoe2InventoryScenarios.size());

[[nodiscard]] bool supported_resolution(Resolution resolution) noexcept {
    return std::any_of(kPoe2Resolutions.begin(), kPoe2Resolutions.end(),
        [resolution](Resolution supported) noexcept {
            return supported.width == resolution.width
                && supported.height == resolution.height;
        });
}

[[nodiscard]] bool set_exact_window_resolution(Resolution resolution) noexcept {
    if (!supported_resolution(resolution)) return false;
    if (GetScreenWidth() != resolution.width
            || GetScreenHeight() != resolution.height) {
        SetWindowSize(resolution.width, resolution.height);
    }
    return GetScreenWidth() == resolution.width
        && GetScreenHeight() == resolution.height;
}

[[nodiscard]] std::string poe2_capture_name(
    const char* scenario, Resolution resolution) {
    return std::string{"poe2-inventory-"} + scenario + '-'
        + std::to_string(resolution.width) + 'x'
        + std::to_string(resolution.height) + ".png";
}

struct FrameObservation final {
    int width{};
    int height{};
    std::uint64_t status_digest{};
    std::uint64_t probe_digest{};
    bool gold_outline{};
    bool cyan_outline{};
    bool hover_outline{};
    bool valid{};
};

struct Poe2CaptureResult final {
    FrameObservation frame{};
    bool exported{};
};

struct Poe2ResolutionResult final {
    Resolution resolution{};
    std::array<Poe2CaptureResult,
        static_cast<std::size_t>(Poe2Scenario::count)> captures{};
    bool tooltip_transition_trace{};
    bool tooltip_comparison_trace{};
    bool status_transition_trace{};
    bool passed{};
};

struct Poe2ValidationResult final {
    std::array<Poe2ResolutionResult, kPoe2Resolutions.size()> resolutions{};
    bool passed{};
};

constexpr std::size_t poe2_scenario_index(Poe2Scenario scenario) noexcept {
    return static_cast<std::size_t>(scenario);
}

struct ScenarioResult final {
    bool ground_material_visible{};
    bool real_material_pickup{};
    bool bag_selection{};
    bool craft{};
    bool coupon{};
    bool confirmation_visible{};
    bool destroyed{};
    bool restarted{};
    std::uint64_t pickup_generation{};
    std::uint64_t root_seed{};
    std::uint16_t first_drop_ordinal{};
    float first_spawn_distance{};
    combat::Vec3 first_spawn_position{};
    float ground_spawn_distance{};
    items::MaterialId picked{items::MaterialId::count};
};

bool within(const std::filesystem::path& child,
    const std::filesystem::path& parent) noexcept {
    const auto relative = child.lexically_relative(parent);
    return !relative.empty() && !relative.is_absolute()
        && *relative.begin() != "..";
}

bool prepare_evidence(const std::filesystem::path& candidate,
    std::filesystem::path& run) noexcept {
    std::error_code error{};
    const auto parent = std::filesystem::absolute(candidate, error)
        .lexically_normal();
    if (error || parent.filename() != "stage16 loot reinforcement evidence"
        || parent.generic_string().find("/out/build/") == std::string::npos) {
        return false;
    }
    run = parent / "stage16-run";
    if (!within(run, parent)) return false;
    const auto status = std::filesystem::symlink_status(run, error);
    if (error && error != std::errc::no_such_file_or_directory) return false;
    if (!error && std::filesystem::is_symlink(status)) return false;
    error.clear();
    std::filesystem::remove_all(run, error);
    if (error) return false;
    std::filesystem::create_directories(run, error);
    return !error;
}

std::optional<items::ItemInstance> make_item(std::uint64_t id,
    std::uint8_t base_id, items::ItemRarity rarity) noexcept {
    const auto* const base = items::base_definition(base_id);
    if (base == nullptr) return std::nullopt;
    for (std::uint64_t seed = 1U; seed < 8192U; ++seed) {
        const auto item = items::generate_item(
            {seed, base->slot, 90U, id, rarity});
        if (item.has_value() && item->base_id == base_id) return item;
    }
    return std::nullopt;
}

std::optional<items::ItemInstance> make_item_with_affix_count(
    std::uint64_t id, std::uint8_t base_id, items::ItemRarity rarity,
    std::uint8_t affix_count) noexcept {
    const auto* const base = items::base_definition(base_id);
    if (base == nullptr) return std::nullopt;
    for (std::uint64_t seed = 1U; seed < 65536U; ++seed) {
        const auto item = items::generate_item(
            {seed, base->slot, 90U, id, rarity});
        if (item.has_value() && item->base_id == base_id
                && item->affix_count == affix_count) {
            return item;
        }
    }
    return std::nullopt;
}

bool exists(const items::ItemOwnershipState& state,
    std::uint64_t id) noexcept {
    return std::any_of(state.items.begin(), state.items.end(),
        [&](const items::ItemInstance& item) { return item.id == id; });
}

std::optional<dungeon::DungeonRunState> prepared_state(
    std::uint64_t& selected_root, std::uint64_t& target_id,
    std::uint16_t& first_drop_ordinal, float& first_spawn_distance,
    combat::Vec3& first_spawn_position) noexcept {
    const dungeon::DungeonRules rules{};
    std::unique_ptr<combat::RoomMonsterPlan> plan{
        new (std::nothrow) combat::RoomMonsterPlan{}};
    if (!plan) return std::nullopt;
    for (std::uint64_t root = 1U; root <= 8192U; ++root) {
        const auto built = dungeon::make_initial_run_state(root, rules);
        if (built.fault != dungeon::DungeonFault::none) continue;
        const auto population = dungeon::build_room_monster_plan(
            built.state.current_room, rules,
            dungeon::kRoomMonsterGeneratorVersion, *plan);
        if (population.fault != dungeon::DungeonFault::none
                || !dungeon::room_monster_plan_legal(
                    built.state.current_room, *plan)) {
            continue;
        }
        const combat::RoomMonsterBlueprint* first_spawn = nullptr;
        float closest_squared = 0.0F;
        const auto combat_config = dungeon::make_combat_lab_config(
            built.state.current_room.entry, rules.rules_version);
        if (!combat_config.has_value()) continue;
        const combat::Vec3 player_spawn = combat_config->player_spawn;
        for (std::uint16_t index = 0U; index < plan->monster_count; ++index) {
            const auto& spawn = plan->monsters[index];
            const auto* const definition = combat::monster_definition(spawn.id);
            if (spawn.spawn_ordinal >= dungeon::kGroundDropCapacity
                    || definition == nullptr
                    || !combat::has_tag(*definition, combat::MonsterTag::melee)) {
                continue;
            }
            const std::uint16_t score =
                combat::monster_affix_danger_score(spawn.affixes);
            if (!dungeon::roll_material_drop(built.state.current_room.seed,
                    spawn.spawn_ordinal, built.state.current_room.depth,
                    score).has_value()) {
                continue;
            }
            const float dx = spawn.initial_position.x - player_spawn.x;
            const float dy = spawn.initial_position.y - player_spawn.y;
            const float squared = dx * dx + dy * dy;
            if (first_spawn == nullptr || squared < closest_squared) {
                first_spawn = &spawn;
                closest_squared = squared;
            }
        }
        if (first_spawn == nullptr) continue;
        // Keep this production-path validation bounded to a target already near
        // the entry streaming region.  The root is still selected solely from
        // real deterministic population/drop data; no drop is injected.
        if (closest_squared > 36.0F) continue;
        for (std::uint64_t offset = 0U; offset < 256U; ++offset) {
            const std::uint64_t candidate_id = 0x1601U + offset;
            const auto target = make_item(candidate_id, 1U, items::ItemRarity::rare);
            if (!target.has_value()) continue;
            auto state = built.state;
            if (!arpg::test::install_stage10_validation_build(state)) continue;
            state.item_ownership.items.push_back(*target);
            state.item_ownership.next_item_sequence = candidate_id + 1U;
            state.item_ownership.materials[items::material_index(items::MaterialId::chaos)] = 1U;
            state.item_ownership.materials[items::material_index(items::MaterialId::coupon_12)] = 1U;
            state.item_ownership.materials[items::material_index(items::MaterialId::reinforcement_stone)] = 1U;
            auto reinforcement_state = state;
            reinforcement_state.item_ownership.items.back().reinforcement = 12U;
            reinforcement_state.commit_generation += 3U;
            dungeon::DungeonSession probe{dungeon::DungeonRules{}, reinforcement_state};
            if (probe.request_reinforcement(candidate_id) != dungeon::RequestResult::accepted) continue;
            const auto pending = probe.pending_save();
            if (!pending || !pending->reinforcement_receipt.has_value()
                    || pending->reinforcement_receipt->success
                    || !pending->reinforcement_receipt->destroyed) continue;
            auto recipe_second = *target;
            recipe_second.id = candidate_id + 1U;
            auto recipe_third = *target;
            recipe_third.id = candidate_id + 2U;
            if (!items::validate_item(recipe_second)
                    || !items::validate_item(recipe_third)) {
                continue;
            }
            state.item_ownership.items.push_back(recipe_second);
            state.item_ownership.items.push_back(recipe_third);
            state.item_ownership.next_item_sequence = candidate_id + 3U;
            target_id = candidate_id;
            selected_root = root;
            first_drop_ordinal = first_spawn->spawn_ordinal;
            first_spawn_distance = std::sqrt(closest_squared);
            first_spawn_position = first_spawn->initial_position;
            return state;
        }
    }
    return std::nullopt;
}

struct Poe2ShowcaseState final {
    dungeon::DungeonRunState state{};
    std::uint64_t root_seed{};
    std::uint64_t comparison_a{};
    std::uint64_t comparison_b{};
    std::uint64_t equipped_source{};
    std::array<std::uint64_t, 3U> recipe{};
};

std::optional<Poe2ShowcaseState> make_poe2_showcase_state() noexcept {
    std::uint64_t ignored_target{};
    std::uint64_t root{};
    std::uint16_t ignored_ordinal{};
    float ignored_distance{};
    combat::Vec3 ignored_position{};
    const auto prepared = prepared_state(root, ignored_target,
        ignored_ordinal, ignored_distance, ignored_position);
    if (!prepared.has_value()) return std::nullopt;

    constexpr std::uint64_t kComparisonA = 0x2601U;
    constexpr std::uint64_t kComparisonB = 0x2602U;
    constexpr std::uint64_t kEquippedSource = 0x2603U;
    constexpr std::array<std::uint64_t, 3U> kRecipe{{
        0x2604U, 0x2605U, 0x2606U,
    }};
    const auto comparison_a = make_item_with_affix_count(kComparisonA, 7U,
        items::ItemRarity::rare, 6U);
    const auto comparison_b = make_item_with_affix_count(kComparisonB, 8U,
        items::ItemRarity::rare, 6U);
    const auto equipped_source = make_item_with_affix_count(kEquippedSource,
        1U, items::ItemRarity::rare, 6U);
    const auto recipe_item = make_item_with_affix_count(kRecipe[0U], 2U,
        items::ItemRarity::rare, 6U);
    if (!comparison_a.has_value() || !comparison_b.has_value()
            || !equipped_source.has_value() || !recipe_item.has_value()) {
        return std::nullopt;
    }

    Poe2ShowcaseState result{};
    result.state = *prepared;
    result.root_seed = root;
    result.comparison_a = kComparisonA;
    result.comparison_b = kComparisonB;
    result.equipped_source = kEquippedSource;
    result.recipe = kRecipe;
    auto& ownership = result.state.item_ownership;
    ownership = {};
    ownership.items.reserve(6U);
    ownership.items.push_back(*equipped_source);
    ownership.items.push_back(*comparison_a);
    ownership.items.push_back(*comparison_b);
    for (std::size_t index{}; index < kRecipe.size(); ++index) {
        items::ItemInstance item = *recipe_item;
        item.id = kRecipe[index];
        if (!items::validate_item(item)) return std::nullopt;
        ownership.items.push_back(item);
    }
    ownership.equipment.equipped_ids[
        static_cast<std::size_t>(items::ItemSlot::weapon)] = kEquippedSource;
    ownership.materials[items::material_index(items::MaterialId::transmute)] = 1U;
    ownership.materials[items::material_index(items::MaterialId::chaos)] = 2U;
    ownership.materials[items::material_index(items::MaterialId::coupon_12)] = 1U;
    ownership.materials[items::material_index(
        items::MaterialId::reinforcement_stone)] = 1U;
    ownership.material_discovery_bits = items::kMaterialDiscoveryMask;
    ownership.next_item_sequence = kRecipe.back() + 1U;
    return items::validate_ownership(ownership)
        ? std::optional<Poe2ShowcaseState>{std::move(result)}
        : std::nullopt;
}

std::optional<dungeon::DungeonRunState> make_poe2_empty_state(
    const Poe2ShowcaseState& source) noexcept {
    dungeon::DungeonRunState result = source.state;
    result.item_ownership = {};
    return items::validate_ownership(result.item_ownership)
        ? std::optional<dungeon::DungeonRunState>{std::move(result)}
        : std::nullopt;
}

std::optional<dungeon::DungeonRunState> make_poe2_all_equipped_state(
    const Poe2ShowcaseState& source) noexcept {
    dungeon::DungeonRunState result = source.state;
    auto& ownership = result.item_ownership;
    ownership = {};
    ownership.items.reserve(static_cast<std::size_t>(items::ItemSlot::count));
    for (std::uint8_t base_id = 1U;
            base_id <= static_cast<std::uint8_t>(items::ItemSlot::count);
            ++base_id) {
        const std::uint64_t id = 0x2700U + base_id;
        const auto item = make_item_with_affix_count(id, base_id,
            items::ItemRarity::normal, 0U);
        const auto* const base = item.has_value()
            ? items::base_definition(item->base_id) : nullptr;
        if (!item.has_value() || base == nullptr) return std::nullopt;
        ownership.items.push_back(*item);
        ownership.equipment.equipped_ids[
            static_cast<std::size_t>(base->slot)] = id;
    }
    ownership.material_discovery_bits = items::kMaterialDiscoveryMask;
    ownership.next_item_sequence = 0x2800U;
    return items::validate_ownership(ownership)
        ? std::optional<dungeon::DungeonRunState>{std::move(result)}
        : std::nullopt;
}

combat::MovementInput toward(combat::Vec3 from, combat::Vec3 to) noexcept {
    combat::MovementInput result{};
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    result.x = dx > 0.35F ? 1 : (dx < -0.35F ? -1 : 0);
    result.y = dy > 0.25F ? 1 : (dy < -0.25F ? -1 : 0);
    return result;
}

const combat::MonsterSnapshot* find_spawn_ordinal(
    const combat::CombatSnapshot& snapshot,
    std::uint16_t spawn_ordinal) noexcept {
    for (const auto& monster : snapshot.monsters) {
        if (monster.active && monster.hp > 0
                && monster.spawn_ordinal == spawn_ordinal) {
            return &monster;
        }
    }
    return nullptr;
}

bool image_has_rendered_content(const Image& image) noexcept {
    if (image.data == nullptr || image.width <= 0 || image.height <= 0) return false;
    std::uint32_t distinct = 0U;
    Color previous{};
    bool first = true;
    for (int y = 0; y < image.height; y += 48) {
        for (int x = 0; x < image.width; x += 48) {
            const Color pixel = GetImageColor(image, x, y);
            if (first || pixel.r != previous.r || pixel.g != previous.g
                    || pixel.b != previous.b) {
                ++distinct;
                previous = pixel;
                first = false;
            }
        }
    }
    return distinct >= 8U;
}

bool export_flushed_frame(const std::filesystem::path& path) noexcept {
    // EndDrawing swaps the GLFW buffers.  Reading only after that swap is
    // undefined on the Intel OpenGL driver used by the Windows gate and can
    // intermittently yield a valid all-black PNG.  Flush the exact frame and
    // read it before the swap instead.
    rlDrawRenderBatchActive();
    Image frame = LoadImageFromScreen();
    if (frame.data == nullptr) return false;
    const bool exported = ExportImage(frame, path.string().c_str());
    UnloadImage(frame);
    return exported;
}

bool finite_positive_rectangle(Rectangle rectangle) noexcept {
    return std::isfinite(rectangle.x) && std::isfinite(rectangle.y)
        && std::isfinite(rectangle.width) && std::isfinite(rectangle.height)
        && rectangle.width > 0.0F && rectangle.height > 0.0F;
}

Rectangle rectangle_intersection(Rectangle left, Rectangle right) noexcept {
    const float x = std::max(left.x, right.x);
    const float y = std::max(left.y, right.y);
    const float far_x = std::min(left.x + left.width, right.x + right.width);
    const float far_y = std::min(left.y + left.height, right.y + right.height);
    return {x, y, std::max(0.0F, far_x - x),
        std::max(0.0F, far_y - y)};
}

bool rectangle_contains(Rectangle outer, Rectangle inner) noexcept {
    return finite_positive_rectangle(outer) && finite_positive_rectangle(inner)
        && inner.x >= outer.x && inner.y >= outer.y
        && inner.x + inner.width <= outer.x + outer.width
        && inner.y + inner.height <= outer.y + outer.height;
}

std::uint64_t image_region_digest(const Image& image,
    Rectangle rectangle) noexcept {
    if (image.data == nullptr || image.width <= 0 || image.height <= 0
            || !finite_positive_rectangle(rectangle)) {
        return 0U;
    }
    const int left = std::clamp(static_cast<int>(std::floor(rectangle.x)),
        0, image.width - 1);
    const int top = std::clamp(static_cast<int>(std::floor(rectangle.y)),
        0, image.height - 1);
    const int right = std::clamp(static_cast<int>(std::ceil(
        rectangle.x + rectangle.width)), left + 1, image.width);
    const int bottom = std::clamp(static_cast<int>(std::ceil(
        rectangle.y + rectangle.height)), top + 1, image.height);
    std::uint64_t digest = 1469598103934665603ULL;
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const Color pixel = GetImageColor(image, x, y);
            const std::array<std::uint8_t, 4U> channels{{
                pixel.r, pixel.g, pixel.b, pixel.a,
            }};
            for (const std::uint8_t channel : channels) {
                digest ^= channel;
                digest *= 1099511628211ULL;
            }
        }
    }
    return digest;
}

bool image_region_has_color(const Image& image, Rectangle rectangle,
    Color expected, std::uint8_t tolerance = 2U) noexcept {
    if (image.data == nullptr || image.width <= 0 || image.height <= 0
            || !finite_positive_rectangle(rectangle)) {
        return false;
    }
    const int left = std::clamp(static_cast<int>(std::floor(rectangle.x)),
        0, image.width - 1);
    const int top = std::clamp(static_cast<int>(std::floor(rectangle.y)),
        0, image.height - 1);
    const int right = std::clamp(static_cast<int>(std::ceil(
        rectangle.x + rectangle.width)), left + 1, image.width);
    const int bottom = std::clamp(static_cast<int>(std::ceil(
        rectangle.y + rectangle.height)), top + 1, image.height);
    const auto close = [tolerance](std::uint8_t actual,
                           std::uint8_t wanted) noexcept {
        return std::abs(static_cast<int>(actual)
            - static_cast<int>(wanted)) <= static_cast<int>(tolerance);
    };
    for (int y = top; y < bottom; ++y) {
        for (int x = left; x < right; ++x) {
            const Color pixel = GetImageColor(image, x, y);
            if (close(pixel.r, expected.r) && close(pixel.g, expected.g)
                    && close(pixel.b, expected.b)
                    && close(pixel.a, expected.a)) {
                return true;
            }
        }
    }
    return false;
}

bool image_region_border_has_color(const Image& image, Rectangle rectangle,
    Color expected, std::uint8_t tolerance = 2U) noexcept {
    if (!finite_positive_rectangle(rectangle)) return false;
    constexpr float kBorderProbeWidth = 4.0F;
    const float horizontal = std::min(kBorderProbeWidth,
        rectangle.height * 0.5F);
    const float vertical = std::min(kBorderProbeWidth,
        rectangle.width * 0.5F);
    return image_region_has_color(image,
               {rectangle.x, rectangle.y, rectangle.width, horizontal},
               expected, tolerance)
        || image_region_has_color(image,
               {rectangle.x, rectangle.y + rectangle.height - horizontal,
                   rectangle.width, horizontal}, expected, tolerance)
        || image_region_has_color(image,
               {rectangle.x, rectangle.y, vertical, rectangle.height},
               expected, tolerance)
        || image_region_has_color(image,
               {rectangle.x + rectangle.width - vertical, rectangle.y,
                   vertical, rectangle.height}, expected, tolerance);
}

std::unique_ptr<platform::DungeonRuntime> make_poe2_runtime(
    const std::filesystem::path& save_directory,
    const dungeon::DungeonRunState& state,
    std::uint64_t root_seed) noexcept {
    persistence::SaveStore store{{save_directory}};
    const auto initial = store.commit(state);
    if (initial.state != persistence::SaveCommitState::committed) return {};
    platform::DungeonRuntimeConfig config{};
    config.save.directory = save_directory;
    config.new_run_seed = root_seed;
    auto runtime = std::make_unique<platform::DungeonRuntime>(config);
    if (!runtime->initialize()
            || runtime->state() != platform::DungeonRuntimeState::running) {
        return {};
    }
    runtime->acknowledge_gameplay_rearmed();
    if (!runtime->authority_requests_enabled()) return {};
    return runtime;
}

bool render_poe2_inventory_frame(const std::filesystem::path& run,
    const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    const platform::DungeonRenderStatus& status,
    platform::InventoryRenderer& inventory,
    const platform::CombatRenderer& assets,
    Resolution resolution, const char* scenario,
    Rectangle probe, Rectangle semantic_probe,
    FrameObservation* observation) noexcept {
    if (!set_exact_window_resolution(resolution)) return false;
    const platform::InventoryShellLayout shell =
        platform::inventory_shell_layout(resolution.width, resolution.height);
    if (!shell.valid) return false;
    if (observation != nullptr) *observation = {};

    const bool capture = scenario != nullptr;
    const auto start = std::filesystem::file_time_type::clock::now()
        - std::chrono::seconds(1);
    const std::filesystem::path image_path = capture
        ? run / poe2_capture_name(scenario, resolution)
        : std::filesystem::path{};

    BeginDrawing();
    ClearBackground(BLACK);
    inventory.draw(session, snapshot, status, assets.material_pack(),
        u8"右 Shift", assets.hud_font(), assets.hud_font_ready());

    bool exported = true;
    Image observed{};
    if (capture) {
        exported = export_flushed_frame(image_path);
    } else if (observation != nullptr) {
        rlDrawRenderBatchActive();
        observed = LoadImageFromScreen();
    }
    EndDrawing();

    if (!capture && observation == nullptr) return true;

    if (capture) observed = LoadImage(image_path.string().c_str());
    const bool dimensions_match = observed.data != nullptr
        && observed.width == resolution.width
        && observed.height == resolution.height;
    const bool rendered = dimensions_match
        && (!capture || image_has_rendered_content(observed));
    if (observation != nullptr && dimensions_match) {
        observation->width = observed.width;
        observation->height = observed.height;
        observation->status_digest = image_region_digest(
            observed, shell.status_bounds);
        observation->probe_digest = finite_positive_rectangle(probe)
            ? image_region_digest(observed, probe) : 0U;
        observation->gold_outline = finite_positive_rectangle(semantic_probe)
            && image_region_has_color(observed, semantic_probe,
                Color{218, 172, 72, 255});
        observation->cyan_outline = finite_positive_rectangle(semantic_probe)
            && image_region_border_has_color(observed, semantic_probe,
                Color{87, 220, 244, 255});
        observation->hover_outline = finite_positive_rectangle(semantic_probe)
            && image_region_has_color(observed, semantic_probe,
                Color{144, 170, 190, 255});
    }
    if (observed.data != nullptr) UnloadImage(observed);

    bool file_valid = true;
    if (capture) {
        std::error_code error{};
        const bool regular = std::filesystem::is_regular_file(
            image_path, error) && !error;
        error.clear();
        const bool large = std::filesystem::file_size(image_path, error)
            > 4096U && !error;
        error.clear();
        const bool fresh = std::filesystem::last_write_time(image_path, error)
            >= start && !error;
        file_valid = exported && regular && large && fresh;
    }
    const bool valid = rendered && file_valid;
    if (observation != nullptr) observation->valid = valid;
    return valid;
}

bool capture_poe2_scenario(const std::filesystem::path& run,
    Poe2ResolutionResult& result, Poe2Scenario scenario,
    const dungeon::DungeonSession& session,
    const dungeon::DungeonSnapshot& snapshot,
    const platform::DungeonRenderStatus& status,
    platform::InventoryRenderer& inventory,
    const platform::CombatRenderer& assets,
    Rectangle probe = {}, Rectangle semantic_probe = {}) noexcept {
    const std::size_t index = poe2_scenario_index(scenario);
    if (index >= result.captures.size()) return false;
    Poe2CaptureResult& capture = result.captures[index];
    capture.exported = render_poe2_inventory_frame(run, session, snapshot,
        status, inventory, assets, result.resolution,
        kPoe2InventoryScenarios[index], probe, semantic_probe,
        &capture.frame);
    return capture.exported && capture.frame.valid
        && capture.frame.width == result.resolution.width
        && capture.frame.height == result.resolution.height;
}

bool render_world_capture(const std::filesystem::path& run,
    platform::DungeonRuntime& runtime,
    const platform::SharedUiFont& shared_ui_font,
    Resolution resolution, const char* name) noexcept {
    const auto start = std::filesystem::file_time_type::clock::now()
        - std::chrono::seconds(1);
    const auto room = std::make_unique<platform::CombatRenderer>();
    struct RendererShutdown final {
        platform::CombatRenderer* renderer{};
        ~RendererShutdown() noexcept {
            if (renderer != nullptr) renderer->shutdown_resources();
        }
    } renderer_shutdown{room.get()};
    if (!room->initialize_resources(shared_ui_font)
            || runtime.session() == nullptr) {
        return false;
    }
    const auto current = std::make_unique<dungeon::DungeonSnapshot>(
        runtime.session()->snapshot());
    dungeon::DungeonRenderSnapshot* const world =
        runtime.render_snapshot_storage();
    if (world == nullptr || !current->combat.has_value()) return false;
    const platform::CombatCameraView camera =
        platform::make_combat_camera_view(
            current->combat->player.position,
            static_cast<float>(resolution.width),
            static_cast<float>(resolution.height));
    const dungeon::WorldViewQuery query = platform::make_world_view_query(
        camera, static_cast<float>(resolution.width),
        static_cast<float>(resolution.height), 1U);
    if (!runtime.session()->write_render_snapshot(query, *world)) return false;
    const platform::CombatFeedback feedback{};
    platform::ControlHints hints{};
    platform::refresh_control_hints(
        hints, arpg::settings::default_settings());
    room->observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        *current, *current, runtime.render_status(), hints, 1.0F / 60.0F, false);
    BeginDrawing();
    ClearBackground(BLACK);
    static_cast<void>(room->draw(*current, *current, *world, camera,
        runtime.render_status(), 1.0F, false, feedback, false, "E"));
    const auto image = run / name;
    const bool exported = export_flushed_frame(image);
    EndDrawing();
    std::error_code error{};
    const Image decoded = LoadImage(image.string().c_str());
    const bool valid = decoded.data != nullptr
        && decoded.width == resolution.width
        && decoded.height == resolution.height
        && image_has_rendered_content(decoded);
    if (decoded.data != nullptr) UnloadImage(decoded);
    const bool regular = std::filesystem::is_regular_file(image, error) && !error;
    error.clear();
    const bool large = std::filesystem::file_size(image, error) > 4096U && !error;
    error.clear();
    const bool fresh = std::filesystem::last_write_time(image, error) >= start && !error;
    return exported && valid && regular && large && fresh;
}

bool drive_real_material_pickup(const std::filesystem::path& run,
    platform::DungeonRuntime& runtime,
    const platform::SharedUiFont& shared_ui_font,
    Resolution resolution, ScenarioResult& result) noexcept {
    std::int8_t retreat_direction{};
    int last_hp{-1};
    int last_max_hp{-1};
    std::size_t last_living_monsters{};
    std::uint32_t last_defeated_monsters{};
    combat::Vec3 last_player_position{};
    combat::Vec3 last_target_position{};
    int last_target_hp{-1};
    for (int tick = 0; tick < 30000; ++tick) {
        const auto* const session = runtime.session();
        if (session == nullptr) return false;
        const auto snapshot = std::make_unique<dungeon::DungeonSnapshot>(session->snapshot());
        if (snapshot->phase == dungeon::RoomPhase::death_pending) {
            const int hp = snapshot->combat.has_value()
                ? snapshot->combat->player.hp : -1;
            const int max_hp = snapshot->combat.has_value()
                ? snapshot->combat->player.max_hp : -1;
            std::size_t living_monsters{};
            if (snapshot->combat.has_value()) {
                for (const auto& monster : snapshot->combat->monsters) {
                    if (monster.active && monster.hp > 0) ++living_monsters;
                }
            }
            std::cerr << "stage16 failure=player_death tick=" << tick
                      << " hp=" << hp << '/' << max_hp
                      << " previous_hp=" << last_hp << '/' << last_max_hp
                      << " living_monsters=" << living_monsters
                      << " previous_living=" << last_living_monsters
                      << " defeated=" << last_defeated_monsters
                      << " player=" << last_player_position.x << ','
                      << last_player_position.y
                      << " target=" << last_target_position.x << ','
                      << last_target_position.y
                      << " target_hp=" << last_target_hp
                      << " ground_materials=" << snapshot->ground_material_count
                      << '\n';
            return false;
        }
        if (snapshot->combat.has_value()) {
            last_hp = snapshot->combat->player.hp;
            last_max_hp = snapshot->combat->player.max_hp;
            last_player_position = snapshot->combat->player.position;
            last_living_monsters = 0U;
            for (const auto& monster : snapshot->combat->monsters) {
                if (monster.active && monster.hp > 0) ++last_living_monsters;
            }
            last_defeated_monsters = snapshot->defeated_monster_count;
            if (const auto* const target = find_spawn_ordinal(
                    *snapshot->combat, result.first_drop_ordinal)) {
                last_target_position = target->position;
                last_target_hp = target->hp;
            }
        }
        if (snapshot->material_pickup_receipt.valid) {
            result.real_material_pickup = true;
            result.pickup_generation =
                snapshot->material_pickup_receipt.commit_generation;
            for (const auto& material : snapshot->material_pickup_receipt.counts) {
                if (material != 0U) break;
            }
            for (std::size_t index = 0U; index < items::kMaterialCount; ++index) {
                if (snapshot->material_pickup_receipt.counts[index] != 0U) {
                    result.picked = static_cast<items::MaterialId>(index);
                    break;
                }
            }
            return result.picked != items::MaterialId::count;
        }
        combat::MovementInput movement{};
        if (snapshot->combat.has_value()) {
            if (snapshot->ground_material_count != 0U) {
                const float dx = snapshot->ground_materials[0].position.x
                    - snapshot->combat->player.position.x;
                const float dy = snapshot->ground_materials[0].position.y
                    - snapshot->combat->player.position.y;
                if (!result.ground_material_visible) {
                    result.ground_spawn_distance = std::sqrt(dx * dx + dy * dy);
                    result.ground_material_visible = render_world_capture(
                        run, runtime, shared_ui_font, resolution,
                        "stage16-ground-material-1280x720.png");
                    if (!result.ground_material_visible) {
                        std::cerr << "stage16 failure=ground_material_capture tick="
                                  << tick << '\n';
                        return false;
                    }
                }
                const combat::Vec3 player =
                    snapshot->combat->player.position;
                const combat::Vec3 material =
                    snapshot->ground_materials[0].position;
                movement = snapshot->ecology
                            == dungeon::checkpoint::DungeonElement::fire
                        && arpg::test::segment_crosses_fire_brazier(
                            player, material)
                    ? arpg::test::fire_room_robot_movement(player, material)
                    : toward(player, material);
            } else {
                const auto* const target = find_spawn_ordinal(
                    *snapshot->combat, result.first_drop_ordinal);
                if (target == nullptr) {
                    const combat::Vec3 player = snapshot->combat->player.position;
                    movement = snapshot->ecology
                                == dungeon::checkpoint::DungeonElement::fire
                            && arpg::test::segment_crosses_fire_brazier(
                                player, result.first_spawn_position)
                        ? arpg::test::fire_room_robot_movement(
                            player, result.first_spawn_position)
                        : toward(player, result.first_spawn_position);
                } else {
                    const float dx = target->position.x
                        - snapshot->combat->player.position.x;
                    const float dy = target->position.y
                        - snapshot->combat->player.position.y;
                    const float horizontal = std::abs(dx);
                    const bool target_on_right = dx >= 0.0F;
                    const combat::Facing desired_facing = target_on_right
                        ? combat::Facing::right : combat::Facing::left;
                    const bool fire_path_blocked = snapshot->ecology
                            == dungeon::checkpoint::DungeonElement::fire
                        && arpg::test::segment_crosses_fire_brazier(
                            snapshot->combat->player.position, target->position);
                    if (fire_path_blocked) {
                        retreat_direction = 0;
                        movement = arpg::test::fire_room_robot_movement(
                            snapshot->combat->player.position, target->position);
                    } else if (snapshot->combat->player.active_attack
                            != combat::AttackId::none) {
                        // Movement is suppressed during attacks by CombatWorld.  Keeping the
                        // input neutral prevents a recovery frame from entering pickup range.
                        movement = {};
                    } else if (std::abs(dy) <= 0.45F
                        && snapshot->combat->player.facing == desired_facing
                        && horizontal >= 2.25F && horizontal <= 2.45F) {
                        // This test drives the actual J1 at its validated 2.25-2.45
                        // launch band.  The ground-spawn distance is logged before
                        // default auto-pickup, so the required >1.50 separation is
                        // independently evidenced rather than assumed.
                        retreat_direction = 0;
                        movement = {};
                        const auto storm = runtime.session()
                            ->request_active_skill_slot(1U);
                        if (storm != combat::SkillCastResult::accepted
                                && runtime.session()->request_active_skill_slot(0U)
                                    != combat::SkillCastResult::accepted) {
                            static_cast<void>(runtime.session()->queue_action(
                                combat::Action::light));
                        }
                    } else if (horizontal < 2.40F) {
                        // Do not pin the player and its pursuer against a room edge:
                        // leave the edge first, then reopen the normal attack band.
                        const float player_x = snapshot->combat->player.position.x;
                        if (retreat_direction == 0) {
                            retreat_direction = player_x <= -8.0F ? 1
                                : (player_x >= 8.0F ? -1
                                    : (target_on_right ? -1 : 1));
                        }
                        if (player_x <= -8.0F && retreat_direction < 0) {
                            retreat_direction = 1;
                        } else if (player_x >= 8.0F && retreat_direction > 0) {
                            retreat_direction = -1;
                        }
                        if (snapshot->ecology
                                == dungeon::checkpoint::DungeonElement::fire) {
                            auto candidate = snapshot->combat->player.position;
                            candidate.x += 0.10F
                                * static_cast<float>(retreat_direction);
                            if (combat::fire_room_obstacle::blocks_player(
                                    snapshot->combat->player.position, candidate)) {
                                retreat_direction = -retreat_direction;
                            }
                        }
                        movement = {retreat_direction, 0};
                    } else if (horizontal > 2.45F) {
                        retreat_direction = 0;
                        movement = {target_on_right ? 1 : -1, 0};
                    } else if (std::abs(dy) > 0.45F) {
                        retreat_direction = 0;
                        movement = {0, dy >= 0.0F ? 1 : -1};
                    } else if (snapshot->combat->player.facing != desired_facing) {
                        retreat_direction = 0;
                        movement = {target_on_right ? 1 : -1, 0};
                    } else {
                        retreat_direction = 0;
                        movement = {};
                    }
                }
            }
        }
        runtime.fixed_tick(movement);
        // The production host drains this relay once per fixed tick for
        // feedback/audio.  The validation has no host frame, but must consume
        // the same production queue so combat cannot saturate it artificially.
        while (runtime.session() != nullptr) {
            const auto event = runtime.session()->try_pop_combat_event();
            if (!event.has_value()) break;
        }
        while (runtime.session() != nullptr
                && runtime.session()->try_pop_event().has_value()) {}
        if (runtime.state() != platform::DungeonRuntimeState::running) {
            std::cerr << "stage16 failure=runtime_not_running tick=" << tick
                      << " state=" << static_cast<unsigned>(runtime.state())
                      << '\n';
            return false;
        }
    }
    const auto* const session = runtime.session();
    if (session != nullptr) {
        const dungeon::DungeonSnapshot snapshot = session->snapshot();
        std::cerr << "stage16 failure=material_pickup_tick_limit"
                  << " phase=" << static_cast<unsigned>(snapshot.phase)
                  << " ground_materials=" << snapshot.ground_material_count
                  << " receipt=" << (snapshot.material_pickup_receipt.valid ? 1 : 0)
                  << '\n';
    } else {
        std::cerr << "stage16 failure=material_pickup_tick_limit session=null\n";
    }
    return false;
}

platform::HostFrameInput click(Vector2 point) noexcept {
    platform::HostFrameInput input{};
    input.mouse_left_pressed = true;
    input.mouse_position = point;
    return input;
}

platform::HostFrameInput right_click(Vector2 point) noexcept {
    platform::HostFrameInput input{};
    input.mouse_right_pressed = true;
    input.mouse_position = point;
    return input;
}

Vector2 center(Rectangle rectangle) noexcept {
    return {rectangle.x + rectangle.width * 0.5F,
        rectangle.y + rectangle.height * 0.5F};
}

Vector2 first_inventory_cell(Resolution resolution) noexcept {
    const auto shell = platform::inventory_shell_layout(
        resolution.width, resolution.height);
    return center(platform::inventory_grid_item_rectangle(shell, 0U, 0.0F));
}

Vector2 inventory_cell(std::size_t filtered_position,
    Resolution resolution) noexcept {
    const auto shell = platform::inventory_shell_layout(
        resolution.width, resolution.height);
    return center(platform::inventory_grid_item_rectangle(
        shell, filtered_position, 0.0F));
}

platform::HostFrameInput recipe_click(std::size_t filtered_position,
    Resolution resolution) noexcept {
    auto input = click(inventory_cell(filtered_position, resolution));
    input.control_down = true;
    return input;
}

platform::HostFrameInput hover(Vector2 point) noexcept {
    platform::HostFrameInput input{};
    input.mouse_position = point;
    return input;
}

Vector2 combine_button_center(Resolution resolution) noexcept {
    const auto shell = platform::inventory_shell_layout(
        resolution.width, resolution.height);
    return center(shell.action_bounds);
}

Vector2 inventory_page_tab_center(
    platform::InventoryPage page, Resolution resolution) noexcept {
    const auto shell = platform::inventory_shell_layout(
        resolution.width, resolution.height);
    return center(shell.tabs[static_cast<std::size_t>(page)]);
}

void select_inventory_page(platform::InventoryRenderer& inventory,
    platform::DungeonRuntime& runtime, platform::InventoryPage page,
    Resolution resolution) noexcept {
    if (runtime.session() == nullptr) return;
    static_cast<void>(inventory.process_input(runtime,
        runtime.session()->snapshot(),
        click(inventory_page_tab_center(page, resolution))));
}

bool select_material_for_equipment(platform::InventoryRenderer& inventory,
    platform::DungeonRuntime& runtime, items::MaterialId material,
    Resolution resolution) noexcept {
    if (runtime.session() == nullptr) return false;
    const std::size_t index = items::material_index(material);
    if (index >= items::kMaterialCount) return false;
    const auto shell = platform::inventory_shell_layout(
        resolution.width, resolution.height);
    const auto bag = platform::material_bag_layout(
        shell.right_panel, shell.geometry_scale);
    if (!shell.valid || !bag.contains_all_slots()) return false;
    select_inventory_page(inventory, runtime,
        platform::InventoryPage::materials, resolution);
    const bool committed = inventory.process_input(runtime,
        runtime.session()->snapshot(), click(center(bag.slots[index])));
    select_inventory_page(inventory, runtime,
        platform::InventoryPage::equipment, resolution);
    return !committed;
}

const items::ItemInstance* owned_item(
    const items::ItemOwnershipState& state,
    std::uint64_t item_id) noexcept {
    const auto found = std::find_if(state.items.begin(), state.items.end(),
        [item_id](const items::ItemInstance& item) noexcept {
            return item.id == item_id;
        });
    return found == state.items.end() ? nullptr : &*found;
}

platform::UiTextExtent measure_poe2_inventory_text(const char* text,
    float font_size, float spacing, void* context) noexcept {
    if (context == nullptr) return {};
    const Font font = *static_cast<const Font*>(context);
    if (!IsFontValid(font)) return {};
    const Vector2 extent = MeasureTextEx(font, text, font_size, spacing);
    return {extent.x, extent.y};
}

std::optional<platform::BuildDifference> comparison_for_item(
    const dungeon::DungeonSession& session,
    const items::ItemInstance& item) noexcept {
    const auto* const base = items::base_definition(item.base_id);
    const auto current = session.preview_equipment_build(
        session.item_state().equipment);
    if (base == nullptr || !current.has_value()) return std::nullopt;
    items::EquipmentState candidate = session.item_state().equipment;
    candidate.equipped_ids[static_cast<std::size_t>(base->slot)] = item.id;
    const auto equipped = session.preview_equipment_build(candidate);
    return equipped.has_value()
        ? std::optional<platform::BuildDifference>{
            platform::compare_player_builds(*current, *equipped)}
        : std::nullopt;
}

std::uint64_t tooltip_content_signature(
    const platform::InventoryTooltipView& view) noexcept {
    std::uint64_t digest = 1469598103934665603ULL;
    for (std::size_t line_index{}; line_index < view.line_count;
         ++line_index) {
        const auto& line = view.lines[line_index];
        for (std::size_t segment_index{};
             segment_index < line.segment_count; ++segment_index) {
            const auto& segment = line.segments[segment_index];
            digest ^= static_cast<std::uint8_t>(segment.tone);
            digest *= 1099511628211ULL;
            for (const char* cursor = segment.text.data(); *cursor != '\0';
                 ++cursor) {
                digest ^= static_cast<std::uint8_t>(*cursor);
                digest *= 1099511628211ULL;
            }
        }
    }
    return digest;
}

struct ExpectedTooltip final {
    platform::InventoryTooltipView view{};
    platform::InventoryTooltipLayout layout{};
    bool valid{};
};

ExpectedTooltip expected_tooltip(const dungeon::DungeonSession& session,
    std::uint64_t item_id, platform::InventoryShellLayout shell,
    Rectangle source, bool source_is_in_right_panel,
    Font font) noexcept {
    ExpectedTooltip result{};
    const items::ItemInstance* const item = owned_item(
        session.item_state(), item_id);
    if (item == nullptr || !IsFontValid(font)) return result;
    result.view = platform::inventory_tooltip_view(
        *item, comparison_for_item(session, *item));
    result.layout = platform::inventory_tooltip_layout(shell, source,
        source_is_in_right_panel, result.view,
        &measure_poe2_inventory_text, &font);
    result.valid = result.view.valid && result.layout.valid;
    return result;
}

bool settle_pending_save(platform::DungeonRuntime& runtime) noexcept {
    const auto deadline = std::chrono::steady_clock::now()
        + std::chrono::seconds{10};
    while (runtime.state() == platform::DungeonRuntimeState::running
            && std::chrono::steady_clock::now() < deadline) {
        const auto* const session = runtime.session();
        if (session == nullptr) return false;
        if (!session->snapshot().pending_save_kind.has_value()) {
            runtime.acknowledge_gameplay_rearmed();
            return runtime.authority_requests_enabled();
        }
        runtime.service_pending_save();
        std::this_thread::sleep_for(std::chrono::milliseconds{1});
    }
    return false;
}

bool inventory_close_clears_material_selection(
    platform::InventoryRenderer& inventory,
    platform::DungeonRuntime& runtime, Resolution resolution) noexcept {
    const auto* const session = runtime.session();
    if (session == nullptr || !settle_pending_save(runtime)
            || runtime.item_state() == nullptr) {
        return false;
    }
    const std::size_t material_index = items::material_index(
        items::MaterialId::chaos);
    const auto before = session->snapshot();
    const std::uint64_t material_before =
        runtime.item_state()->materials[material_index];
    const auto shell = platform::inventory_shell_layout(
        resolution.width, resolution.height);
    const auto bag = platform::material_bag_layout(
        shell.right_panel, shell.geometry_scale);
    select_inventory_page(inventory, runtime,
        platform::InventoryPage::materials, resolution);
    const bool selection_committed = inventory.process_input(runtime, before,
        click(center(bag.slots[material_index])));
    inventory.close();
    inventory.open(*session, session->snapshot());
    const bool item_click_committed = inventory.process_input(
        runtime, session->snapshot(), click(first_inventory_cell(resolution)));
    const auto after = session->snapshot();
    return !selection_committed && !item_click_committed
        && !after.pending_save_kind.has_value()
        && after.commit_generation == before.commit_generation
        && runtime.item_state() != nullptr
        && runtime.item_state()->materials[material_index] == material_before;
}

bool inventory_close_clears_recipe_selection(
    platform::InventoryRenderer& inventory,
    platform::DungeonRuntime& runtime, Resolution resolution) noexcept {
    const auto* const session = runtime.session();
    if (session == nullptr || !settle_pending_save(runtime)
            || runtime.item_state() == nullptr) {
        return false;
    }
    const auto before = session->snapshot();
    const std::size_t item_count_before = runtime.item_state()->items.size();
    for (std::size_t filtered_position = 0U;
            filtered_position < 3U; ++filtered_position) {
        if (inventory.process_input(runtime, session->snapshot(),
                recipe_click(filtered_position, resolution))) {
            return false;
        }
    }
    inventory.close();
    inventory.open(*session, session->snapshot());
    const bool combine_committed = inventory.process_input(runtime,
        session->snapshot(), click(combine_button_center(resolution)));
    const auto after = session->snapshot();
    return !combine_committed && !after.pending_save_kind.has_value()
        && after.commit_generation == before.commit_generation
        && runtime.item_state() != nullptr
        && runtime.item_state()->items.size() == item_count_before;
}

bool process_item_action(platform::InventoryRenderer& inventory,
    platform::DungeonRuntime& runtime, items::MaterialId material,
    std::uint64_t expected_item, Resolution resolution,
    bool& action_result) noexcept {
    const auto* const session = runtime.session();
    if (session == nullptr || !settle_pending_save(runtime)) return false;
    const std::size_t index = items::material_index(material);
    const std::uint64_t before = runtime.item_state() != nullptr
            && index < items::kMaterialCount
        ? runtime.item_state()->materials[index] : 0U;
    auto snapshot = session->snapshot();
    const auto shell = platform::inventory_shell_layout(
        resolution.width, resolution.height);
    const auto bag = platform::material_bag_layout(
        shell.right_panel, shell.geometry_scale);
    select_inventory_page(inventory, runtime,
        platform::InventoryPage::materials, resolution);
    if (!inventory.process_input(runtime, snapshot,
            click(center(bag.slots[items::material_index(material)])))) {
        // Selecting a material is intentionally a UI-only click.
    }
    select_inventory_page(inventory, runtime,
        platform::InventoryPage::equipment, resolution);
    snapshot = runtime.session()->snapshot();
    const bool committed = inventory.process_input(runtime, snapshot,
        click(first_inventory_cell(resolution)));
    action_result = committed;
    if (committed && !settle_pending_save(runtime)) return false;
    if (runtime.item_state() == nullptr || !exists(*runtime.item_state(), expected_item)) {
        std::cerr << "stage16 failure=item_action_item_missing material="
                  << static_cast<unsigned>(material)
                  << " committed=" << (committed ? 1 : 0) << '\n';
        return false;
    }
    const std::uint64_t after = index < items::kMaterialCount
        ? runtime.item_state()->materials[index] : 0U;
    const bool valid = committed && index < items::kMaterialCount
        && before > 0U && after + 1U == before;
    if (!valid) {
        const auto filtered = platform::filtered_inventory_indices(
            *runtime.item_state(), {});
        const auto* const selected = filtered.empty() ? nullptr
            : &runtime.item_state()->items[filtered.front()];
        const auto debug_snapshot = runtime.session()->snapshot();
        const auto direct = selected != nullptr
            ? items::craft_item({1U, debug_snapshot.commit_generation,
                *selected, material, std::nullopt})
            : items::CraftResult{};
        std::cerr << "stage16 failure=item_action material="
                  << static_cast<unsigned>(material)
                  << " committed=" << (committed ? 1 : 0)
                  << " before=" << before << " after=" << after
                  << " expected_item=" << expected_item
                  << " selected_item=" << (selected != nullptr ? selected->id : 0U)
                  << " selected_rarity=" << (selected != nullptr
                        ? static_cast<unsigned>(selected->rarity) : 0U)
                  << " selected_base=" << (selected != nullptr
                        ? static_cast<unsigned>(selected->base_id) : 0U)
                  << " selected_affixes=" << (selected != nullptr
                        ? static_cast<unsigned>(selected->affix_count) : 0U)
                  << " phase=" << static_cast<unsigned>(debug_snapshot.phase)
                  << " pending=" << (debug_snapshot.pending_save_kind.has_value()
                        ? 1 : 0)
                  << " authority=" << (runtime.authority_requests_enabled()
                        ? 1 : 0)
                  << " direct_applied=" << (direct.applied ? 1 : 0)
                  << " direct_consumed=" << (direct.consumed ? 1 : 0)
                  << '\n';
    }
    return valid;
}

bool render_inventory_capture(const std::filesystem::path& run,
    platform::DungeonRuntime& runtime, platform::InventoryRenderer& inventory,
    const platform::SharedUiFont& shared_ui_font, Resolution resolution,
    const char* name) noexcept {
    const auto start = std::filesystem::file_time_type::clock::now()
        - std::chrono::seconds(1);
    const auto room = std::make_unique<platform::CombatRenderer>();
    struct RendererShutdown final {
        platform::CombatRenderer* renderer{};
        ~RendererShutdown() noexcept {
            if (renderer != nullptr) renderer->shutdown_resources();
        }
    } renderer_shutdown{room.get()};
    if (!room->initialize_resources(shared_ui_font)
            || runtime.session() == nullptr) {
        return false;
    }
    const auto current = std::make_unique<dungeon::DungeonSnapshot>(
        runtime.session()->snapshot());
    dungeon::DungeonRenderSnapshot* const world =
        runtime.render_snapshot_storage();
    if (world == nullptr || !current->combat.has_value()) return false;
    const platform::CombatCameraView camera =
        platform::make_combat_camera_view(
            current->combat->player.position,
            static_cast<float>(resolution.width),
            static_cast<float>(resolution.height));
    const dungeon::WorldViewQuery query = platform::make_world_view_query(
        camera, static_cast<float>(resolution.width),
        static_cast<float>(resolution.height), 2U);
    if (!runtime.session()->write_render_snapshot(query, *world)) return false;
    const platform::CombatFeedback feedback{};
    platform::ControlHints hints{};
    platform::refresh_control_hints(
        hints, arpg::settings::default_settings());
    room->observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        *current, *current, runtime.render_status(), hints, 1.0F / 60.0F, false);
    BeginDrawing();
    ClearBackground(BLACK);
    static_cast<void>(room->draw(*current, *current, *world, camera,
        runtime.render_status(), 1.0F, false, feedback, false, "E"));
    inventory.draw(*runtime.session(), *current, runtime.render_status(),
        room->material_pack(), "I", room->hud_font(), room->hud_font_ready());
    const auto image = run / name;
    const bool exported = export_flushed_frame(image);
    EndDrawing();
    std::error_code error{};
    const Image decoded = LoadImage(image.string().c_str());
    const bool valid = decoded.data != nullptr
        && decoded.width == resolution.width
        && decoded.height == resolution.height
        && image_has_rendered_content(decoded);
    if (decoded.data != nullptr) UnloadImage(decoded);
    const bool regular = std::filesystem::is_regular_file(image, error) && !error;
    error.clear();
    const bool large = std::filesystem::file_size(image, error) > 4096U && !error;
    error.clear();
    const bool fresh = std::filesystem::last_write_time(image, error) >= start && !error;
    return exported && valid && regular && large && fresh;
}

std::optional<ScenarioResult> run_production_scenario(
    const std::filesystem::path& run,
    const platform::SharedUiFont& shared_ui_font,
    Resolution resolution) noexcept {
    std::uint64_t target_id = 0x1601U;
    std::uint64_t root{};
    std::uint16_t first_drop_ordinal{};
    float first_spawn_distance{};
    combat::Vec3 first_spawn_position{};
    const auto state = prepared_state(root, target_id, first_drop_ordinal,
        first_spawn_distance, first_spawn_position);
    if (!state.has_value()) {
        std::cerr << "stage16 failure=prepared_state\n";
        return std::nullopt;
    }
    const auto save_directory = run / "save";
    persistence::SaveStore store{{save_directory}};
    const auto initial = store.commit(*state);
    if (initial.state != persistence::SaveCommitState::committed) {
        std::cerr << "stage16 failure=initial_commit state="
                  << static_cast<unsigned>(initial.state) << '\n';
        return std::nullopt;
    }

    platform::DungeonRuntimeConfig config{};
    config.save.directory = save_directory;
    config.new_run_seed = root;
    auto runtime = std::make_unique<platform::DungeonRuntime>(config);
    if (!runtime->initialize() || runtime->state()
            != platform::DungeonRuntimeState::running) {
        std::cerr << "stage16 failure=runtime_initialize state="
                  << static_cast<unsigned>(runtime->state()) << '\n';
        return std::nullopt;
    }
    // The production host acknowledges a neutral input frame immediately
    // after restore before enabling authoritative UI requests.  This direct
    // runtime fixture must reproduce the same rearm boundary.
    runtime->acknowledge_gameplay_rearmed();
    if (!runtime->authority_requests_enabled()) {
        std::cerr << "stage16 failure=authority_rearm\n";
        return std::nullopt;
    }
    ScenarioResult result{};
    result.root_seed = root;
    result.first_drop_ordinal = first_drop_ordinal;
    result.first_spawn_distance = first_spawn_distance;
    result.first_spawn_position = first_spawn_position;
    if (!drive_real_material_pickup(
            run, *runtime, shared_ui_font, resolution, result)
            || runtime->item_state() == nullptr
            || !settle_pending_save(*runtime)) {
        std::cerr << "stage16 failure=drive_real_material_pickup"
                  << " item_state=" << (runtime->item_state() != nullptr ? 1 : 0)
                  << '\n';
        return std::nullopt;
    }

    const auto inventory = std::make_unique<platform::InventoryRenderer>();
    inventory->open(*runtime->session(), runtime->session()->snapshot());
    if (!inventory_close_clears_material_selection(
            *inventory, *runtime, resolution)) {
        std::cerr << "stage16 failure=inventory_close_material_selection\n";
        return std::nullopt;
    }
    if (!inventory_close_clears_recipe_selection(
            *inventory, *runtime, resolution)) {
        std::cerr << "stage16 failure=inventory_close_recipe_selection\n";
        return std::nullopt;
    }
    if (!process_item_action(*inventory, *runtime, items::MaterialId::chaos,
            target_id, resolution, result.craft) || !result.craft) {
        std::cerr << "stage16 failure=chaos_craft\n";
        return std::nullopt;
    }
    if (!process_item_action(*inventory, *runtime, items::MaterialId::coupon_12,
            target_id, resolution, result.coupon) || !result.coupon) {
        std::cerr << "stage16 failure=coupon_12\n";
        return std::nullopt;
    }

    const auto shell = platform::inventory_shell_layout(
        resolution.width, resolution.height);
    const auto bag = platform::material_bag_layout(
        shell.right_panel, shell.geometry_scale);
    select_inventory_page(*inventory, *runtime,
        platform::InventoryPage::materials, resolution);
    static_cast<void>(inventory->process_input(*runtime, runtime->session()->snapshot(),
        click(center(bag.slots[items::material_index(
            items::MaterialId::reinforcement_stone)]))));
    select_inventory_page(*inventory, *runtime,
        platform::InventoryPage::equipment, resolution);
    static_cast<void>(inventory->process_input(*runtime, runtime->session()->snapshot(),
        click(first_inventory_cell(resolution))));
    const auto confirmation = platform::reinforcement_confirmation_layout(
        resolution.width, resolution.height);
    result.confirmation_visible = render_inventory_capture(
        run, *runtime, *inventory, shared_ui_font, resolution,
        "stage16-confirmation-1280x720.png");
    if (!result.confirmation_visible) {
        std::cerr << "stage16 failure=confirmation_capture\n";
        return std::nullopt;
    }
    const bool confirmation_committed = inventory->process_input(*runtime,
        runtime->session()->snapshot(), click(center(confirmation.confirm)));
    const bool confirmation_settled = confirmation_committed
        && settle_pending_save(*runtime);
    result.destroyed = runtime->item_state() != nullptr
        && !exists(*runtime->item_state(), target_id);
    if (!confirmation_settled || !result.destroyed) {
        std::cerr << "stage16 failure=confirmation_commit committed="
                  << (confirmation_committed ? 1 : 0)
                  << " destroyed=" << (result.destroyed ? 1 : 0) << '\n';
        return std::nullopt;
    }
    if (!render_inventory_capture(
            run, *runtime, *inventory, shared_ui_font, resolution,
            "stage16-complete-1280x720.png")) {
        std::cerr << "stage16 failure=complete_capture\n";
        return std::nullopt;
    }

    runtime.reset();
    const auto restarted = std::make_unique<platform::DungeonRuntime>(config);
    const bool restart_initialized = restarted->initialize();
    const bool restart_running = restarted->state()
        == platform::DungeonRuntimeState::running;
    const auto* const restarted_items = restarted->item_state();
    const bool material_persisted = restarted_items != nullptr
        && result.picked != items::MaterialId::count
        && restarted_items->materials[items::material_index(result.picked)] == 1U;
    const auto expected_remaining = [&](items::MaterialId material) noexcept {
        return result.picked == material ? std::uint64_t{1U} : std::uint64_t{0U};
    };
    const bool consumables_spent = restarted_items != nullptr
        && restarted_items->materials[items::material_index(items::MaterialId::chaos)]
            == expected_remaining(items::MaterialId::chaos)
        && restarted_items->materials[items::material_index(items::MaterialId::coupon_12)]
            == expected_remaining(items::MaterialId::coupon_12)
        && restarted_items->materials[
            items::material_index(items::MaterialId::reinforcement_stone)]
            == expected_remaining(items::MaterialId::reinforcement_stone);
    const bool target_destroyed = restarted_items != nullptr
        && !exists(*restarted_items, target_id);
    result.restarted = restart_initialized && restart_running && material_persisted
        && consumables_spent && target_destroyed;
    result.bag_selection = result.craft && result.coupon;
    return result;
}

std::optional<ScenarioResult> run_production_scenario(
    const std::filesystem::path& run,
    const platform::SharedUiFont& shared_ui_font) noexcept {
    return run_production_scenario(
        run, shared_ui_font, kLegacyResolution);
}

bool run_poe2_inventory_scenarios(const std::filesystem::path& run,
    const platform::SharedUiFont& shared_ui_font,
    Resolution resolution, Poe2ResolutionResult& result) noexcept {
    result = {};
    result.resolution = resolution;
    if (!set_exact_window_resolution(resolution)) return false;

    platform::CombatRenderer assets{};
    struct AssetShutdown final {
        platform::CombatRenderer* renderer{};
        ~AssetShutdown() noexcept {
            if (renderer != nullptr) renderer->shutdown_resources();
        }
    } asset_shutdown{&assets};
    if (!assets.initialize_resources(shared_ui_font)
            || !assets.hud_font_ready()) {
        return false;
    }

    const auto showcase = make_poe2_showcase_state();
    if (!showcase.has_value()) return false;
    const auto empty_state = make_poe2_empty_state(*showcase);
    const auto all_equipped_state = make_poe2_all_equipped_state(*showcase);
    if (!empty_state.has_value() || !all_equipped_state.has_value()) {
        return false;
    }
    const auto shell = platform::inventory_shell_layout(
        resolution.width, resolution.height);
    if (!shell.valid) return false;
    const std::string resolution_tag = std::to_string(resolution.width)
        + 'x' + std::to_string(resolution.height);
    const auto save_path = [&](const char* scenario) {
        return run / (std::string{"poe2-"} + resolution_tag + '-'
            + scenario + "-save");
    };

    const auto empty_presentation = platform::inventory_empty_presentation(
        empty_state->item_ownership, 0U);
    auto empty_runtime = make_poe2_runtime(save_path("empty"),
        *empty_state, showcase->root_seed);
    if (!empty_runtime || empty_runtime->session() == nullptr
            || empty_presentation.state
                != platform::InventoryEmptyState::inventory_empty) {
        return false;
    }
    platform::InventoryRenderer empty_inventory{};
    empty_inventory.open(*empty_runtime->session(),
        empty_runtime->session()->snapshot());
    if (!capture_poe2_scenario(run, result, Poe2Scenario::empty,
            *empty_runtime->session(), empty_runtime->session()->snapshot(),
            empty_runtime->render_status(), empty_inventory, assets)) {
        return false;
    }
    empty_runtime.reset();

    platform::InventoryFilter normal_filter{};
    normal_filter.rarity = items::ItemRarity::normal;
    const auto normal_filtered = platform::filtered_inventory_indices(
        showcase->state.item_ownership, normal_filter);
    auto filter_runtime = make_poe2_runtime(save_path("filter-empty"),
        showcase->state, showcase->root_seed);
    if (!filter_runtime || filter_runtime->session() == nullptr
            || !normal_filtered.empty()) {
        return false;
    }
    platform::InventoryRenderer filter_inventory{};
    filter_inventory.open(*filter_runtime->session(),
        filter_runtime->session()->snapshot());
    if (filter_inventory.process_input(*filter_runtime,
            filter_runtime->session()->snapshot(),
            click(center(shell.rarity_filter_bounds)))) {
        return false;
    }
    if (!capture_poe2_scenario(run, result, Poe2Scenario::filter_empty,
            *filter_runtime->session(), filter_runtime->session()->snapshot(),
            filter_runtime->render_status(), filter_inventory, assets)) {
        return false;
    }
    filter_runtime.reset();

    const auto all_equipped_presentation =
        platform::inventory_empty_presentation(
            all_equipped_state->item_ownership, 0U);
    auto all_runtime = make_poe2_runtime(save_path("all-equipped"),
        *all_equipped_state, showcase->root_seed);
    if (!all_runtime || all_runtime->session() == nullptr
            || all_equipped_presentation.state
                != platform::InventoryEmptyState::all_equipped) {
        return false;
    }
    platform::InventoryRenderer all_inventory{};
    all_inventory.open(*all_runtime->session(),
        all_runtime->session()->snapshot());
    if (!capture_poe2_scenario(run, result, Poe2Scenario::all_equipped,
            *all_runtime->session(), all_runtime->session()->snapshot(),
            all_runtime->render_status(), all_inventory, assets)) {
        return false;
    }
    all_runtime.reset();

    auto showcase_runtime = make_poe2_runtime(save_path("showcase"),
        showcase->state, showcase->root_seed);
    if (!showcase_runtime || showcase_runtime->session() == nullptr) {
        return false;
    }
    platform::InventoryRenderer showcase_inventory{};
    showcase_inventory.open(*showcase_runtime->session(),
        showcase_runtime->session()->snapshot());
    const auto prime_showcase = [&]() noexcept {
        return render_poe2_inventory_frame(run,
            *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, {}, {}, nullptr);
    };
    if (!prime_showcase()) return false;

    const Rectangle comparison_a_cell =
        platform::inventory_grid_item_rectangle(shell, 0U, 0.0F);
    const Rectangle comparison_b_cell =
        platform::inventory_grid_item_rectangle(shell, 1U, 0.0F);
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            click(center(comparison_a_cell)))) {
        return false;
    }
    if (!capture_poe2_scenario(run, result, Poe2Scenario::selected,
            *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            {}, comparison_a_cell)) {
        return false;
    }

    showcase_inventory.close();
    showcase_inventory.open(*showcase_runtime->session(),
        showcase_runtime->session()->snapshot());
    if (!prime_showcase()) return false;
    for (std::size_t position = 2U; position < 5U; ++position) {
        if (showcase_inventory.process_input(*showcase_runtime,
                showcase_runtime->session()->snapshot(),
                recipe_click(position, resolution))) {
            return false;
        }
    }
    if (!capture_poe2_scenario(run, result, Poe2Scenario::recipe,
            *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets)) {
        return false;
    }

    showcase_inventory.close();
    showcase_inventory.open(*showcase_runtime->session(),
        showcase_runtime->session()->snapshot());
    if (!prime_showcase()) return false;
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            click(center(comparison_a_cell)))) {
        return false;
    }
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            hover(center(comparison_b_cell)))) {
        return false;
    }
    const ExpectedTooltip comparison_a_tooltip = expected_tooltip(
        *showcase_runtime->session(), showcase->comparison_a, shell,
        comparison_a_cell, true, assets.hud_font());
    const ExpectedTooltip comparison_b_tooltip = expected_tooltip(
        *showcase_runtime->session(), showcase->comparison_b, shell,
        comparison_b_cell, true, assets.hud_font());
    if (!comparison_a_tooltip.valid || !comparison_b_tooltip.valid
            || std::strcmp(comparison_a_tooltip.view.title.data(),
                comparison_b_tooltip.view.title.data()) == 0
            || tooltip_content_signature(comparison_a_tooltip.view)
                == tooltip_content_signature(comparison_b_tooltip.view)
            || !rectangle_contains(shell.left_panel,
                comparison_b_tooltip.layout.backing)) {
        return false;
    }
    if (!capture_poe2_scenario(run, result, Poe2Scenario::max_tooltip,
            *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            comparison_b_tooltip.layout.backing, comparison_b_cell)) {
        return false;
    }
    FrameObservation actual_comparison_frame{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, comparison_b_tooltip.layout.backing,
            comparison_b_cell, &actual_comparison_frame)) {
        return false;
    }
    const auto render_tooltip_reference = [&](Rectangle source,
            Rectangle probe, FrameObservation& observation) noexcept {
        platform::InventoryRenderer reference{};
        reference.open(*showcase_runtime->session(),
            showcase_runtime->session()->snapshot());
        if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
                showcase_runtime->session()->snapshot(),
                showcase_runtime->render_status(), reference, assets,
                resolution, nullptr, {}, {}, nullptr)) {
            return false;
        }
        if (reference.process_input(*showcase_runtime,
                showcase_runtime->session()->snapshot(),
                hover(center(source)))) {
            return false;
        }
        return render_poe2_inventory_frame(run,
            *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), reference, assets,
            resolution, nullptr, probe, source, &observation);
    };
    FrameObservation comparison_a_reference{};
    FrameObservation comparison_b_reference{};
    if (!render_tooltip_reference(comparison_a_cell,
            comparison_b_tooltip.layout.backing, comparison_a_reference)
            || !render_tooltip_reference(comparison_b_cell,
                comparison_b_tooltip.layout.backing,
                comparison_b_reference)) {
        return false;
    }
    result.tooltip_comparison_trace = actual_comparison_frame.hover_outline
        && actual_comparison_frame.probe_digest != 0U
        && comparison_a_reference.probe_digest != 0U
        && comparison_b_reference.probe_digest != 0U
        && actual_comparison_frame.probe_digest
            == comparison_b_reference.probe_digest
        && actual_comparison_frame.probe_digest
            != comparison_a_reference.probe_digest;
    if (!result.tooltip_comparison_trace) return false;

    showcase_inventory.close();
    showcase_inventory.open(*showcase_runtime->session(),
        showcase_runtime->session()->snapshot());
    if (!prime_showcase()) return false;
    FrameObservation unselected_a_cell{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, comparison_a_cell, {},
            &unselected_a_cell)) {
        return false;
    }
    FrameObservation no_a_tooltip{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, comparison_a_tooltip.layout.backing, {},
            &no_a_tooltip)) {
        return false;
    }
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            click(center(comparison_a_cell)))) {
        return false;
    }
    FrameObservation selected_a_cell{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, comparison_a_cell, {}, &selected_a_cell)
            || selected_a_cell.probe_digest == unselected_a_cell.probe_digest) {
        return false;
    }
    FrameObservation selected_tooltip_first{};
    FrameObservation selected_tooltip_second{};
    FrameObservation selected_tooltip_third{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, comparison_a_tooltip.layout.backing, {},
            &selected_tooltip_first)
            || showcase_inventory.process_input(*showcase_runtime,
                showcase_runtime->session()->snapshot(),
                hover(center(shell.status_bar)))
            || !render_poe2_inventory_frame(run,
                *showcase_runtime->session(),
                showcase_runtime->session()->snapshot(),
                showcase_runtime->render_status(), showcase_inventory,
                assets, resolution, nullptr,
                comparison_a_tooltip.layout.backing, {},
                &selected_tooltip_second)
            || showcase_inventory.process_input(*showcase_runtime,
                showcase_runtime->session()->snapshot(),
                hover(center(shell.status_bar)))
            || !render_poe2_inventory_frame(run,
                *showcase_runtime->session(),
                showcase_runtime->session()->snapshot(),
                showcase_runtime->render_status(), showcase_inventory,
                assets, resolution, nullptr,
                comparison_a_tooltip.layout.backing, {},
                &selected_tooltip_third)) {
        return false;
    }
    const bool selected_tooltip_stable =
        selected_tooltip_first.probe_digest != no_a_tooltip.probe_digest
        && selected_tooltip_second.probe_digest
            == selected_tooltip_first.probe_digest
        && selected_tooltip_third.probe_digest
            == selected_tooltip_first.probe_digest;
    if (!selected_tooltip_stable) return false;
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            right_click(center(comparison_a_tooltip.layout.backing)))) {
        return false;
    }
    FrameObservation selected_a_after_tooltip_close{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, comparison_a_cell, {},
            &selected_a_after_tooltip_close)
            || selected_a_after_tooltip_close.probe_digest
                != selected_a_cell.probe_digest) {
        return false;
    }
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            hover(center(shell.status_bounds)))
            || showcase_inventory.process_input(*showcase_runtime,
                showcase_runtime->session()->snapshot(),
                click(center(comparison_a_tooltip.layout.backing)))) {
        return false;
    }
    FrameObservation cleared_a_cell{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, comparison_a_cell, {}, &cleared_a_cell)
            || cleared_a_cell.probe_digest != unselected_a_cell.probe_digest
            || showcase_inventory.process_input(*showcase_runtime,
                showcase_runtime->session()->snapshot(),
                hover(center(shell.status_bounds)))) {
        return false;
    }
    const Rectangle equipped_source_bounds = shell.equipment_slots[
        static_cast<std::size_t>(items::ItemSlot::weapon)];
    const ExpectedTooltip retained_tooltip = expected_tooltip(
        *showcase_runtime->session(), showcase->equipped_source, shell,
        equipped_source_bounds, false, assets.hud_font());
    const Rectangle covered_b = rectangle_intersection(
        retained_tooltip.layout.backing, comparison_b_cell);
    if (!retained_tooltip.valid
            || !rectangle_contains(shell.right_panel,
                retained_tooltip.layout.backing)
            || covered_b.width < 4.0F || covered_b.height < 4.0F) {
        return false;
    }
    FrameObservation equipped_tooltip_reference{};
    if (!render_tooltip_reference(equipped_source_bounds,
            retained_tooltip.layout.backing,
            equipped_tooltip_reference)) {
        return false;
    }
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            hover(center(equipped_source_bounds)))) {
        return false;
    }
    const Vector2 retained_pointer = center(covered_b);
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            hover(retained_pointer))) {
        return false;
    }
    const auto before_retained_click = showcase_runtime->session()->snapshot();
    if (!capture_poe2_scenario(run, result,
            Poe2Scenario::tooltip_left_source,
            *showcase_runtime->session(), before_retained_click,
            showcase_runtime->render_status(), showcase_inventory, assets,
            retained_tooltip.layout.backing, comparison_b_cell)) {
        return false;
    }
    FrameObservation retained_observation{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            before_retained_click, showcase_runtime->render_status(),
            showcase_inventory, assets, resolution, nullptr,
            retained_tooltip.layout.backing, comparison_b_cell,
            &retained_observation)) {
        return false;
    }
    const bool close_committed = showcase_inventory.process_input(
        *showcase_runtime, showcase_runtime->session()->snapshot(),
        click(retained_pointer));
    const auto after_retained_click = showcase_runtime->session()->snapshot();
    if (close_committed
            || after_retained_click.commit_generation
                != before_retained_click.commit_generation
            || after_retained_click.equipped_ids
                != before_retained_click.equipped_ids) {
        return false;
    }
    if (!capture_poe2_scenario(run, result, Poe2Scenario::tooltip_closed,
            *showcase_runtime->session(), after_retained_click,
            showcase_runtime->render_status(), showcase_inventory, assets,
            retained_tooltip.layout.backing, comparison_b_cell)) {
        return false;
    }
    FrameObservation closed_observation{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            after_retained_click, showcase_runtime->render_status(),
            showcase_inventory, assets, resolution, nullptr,
            retained_tooltip.layout.backing, comparison_b_cell,
            &closed_observation)) {
        return false;
    }
    FrameObservation closed_a_cell{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            after_retained_click, showcase_runtime->render_status(),
            showcase_inventory, assets, resolution, nullptr,
            comparison_a_cell, {}, &closed_a_cell)
            || closed_a_cell.probe_digest != unselected_a_cell.probe_digest) {
        return false;
    }
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            hover(retained_pointer))) {
        return false;
    }
    FrameObservation alternate_suppressed_observation{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, retained_tooltip.layout.backing,
            comparison_b_cell, &alternate_suppressed_observation)) {
        return false;
    }
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            hover(center(equipped_source_bounds)))) {
        return false;
    }
    FrameObservation suppressed_observation{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, retained_tooltip.layout.backing,
            comparison_b_cell, &suppressed_observation)) {
        return false;
    }
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            hover(center(shell.status_bar)))) {
        return false;
    }
    FrameObservation leave_observation{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, retained_tooltip.layout.backing,
            comparison_b_cell, &leave_observation)) {
        return false;
    }
    FrameObservation leave_a_tooltip{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, comparison_a_tooltip.layout.backing, {},
            &leave_a_tooltip)) {
        return false;
    }
    FrameObservation leave_a_cell{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, comparison_a_cell, {}, &leave_a_cell)) {
        return false;
    }
    const bool persistent_a_cleared_after_leave =
        leave_a_tooltip.probe_digest == no_a_tooltip.probe_digest
        && leave_a_cell.probe_digest == unselected_a_cell.probe_digest;
    if (!persistent_a_cleared_after_leave) return false;
    if (showcase_inventory.process_input(*showcase_runtime,
            showcase_runtime->session()->snapshot(),
            hover(center(equipped_source_bounds)))) {
        return false;
    }
    FrameObservation restored_observation{};
    if (!render_poe2_inventory_frame(run, *showcase_runtime->session(),
            showcase_runtime->session()->snapshot(),
            showcase_runtime->render_status(), showcase_inventory, assets,
            resolution, nullptr, retained_tooltip.layout.backing,
            comparison_b_cell, &restored_observation)) {
        return false;
    }
    result.tooltip_transition_trace = retained_observation.probe_digest != 0U
        && retained_observation.probe_digest
            != closed_observation.probe_digest
        && suppressed_observation.probe_digest
            == closed_observation.probe_digest
        && alternate_suppressed_observation.probe_digest
            == closed_observation.probe_digest
        && leave_observation.probe_digest == closed_observation.probe_digest
        && restored_observation.probe_digest
            == equipped_tooltip_reference.probe_digest
        && restored_observation.probe_digest
            != leave_observation.probe_digest
        && persistent_a_cleared_after_leave
        && selected_tooltip_stable
        && !retained_observation.hover_outline;
    if (!result.tooltip_transition_trace) return false;
    showcase_runtime.reset();

    auto material_runtime = make_poe2_runtime(save_path("material-status"),
        showcase->state, showcase->root_seed);
    if (!material_runtime || material_runtime->session() == nullptr) {
        return false;
    }
    platform::InventoryRenderer material_inventory{};
    material_inventory.open(*material_runtime->session(),
        material_runtime->session()->snapshot());
    const Rectangle target_cell =
        platform::inventory_grid_item_rectangle(shell, 0U, 0.0F);
    const Rectangle equipment_tab = shell.tabs[static_cast<std::size_t>(
        platform::InventoryPage::equipment)];
    FrameObservation material_unselected_target{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            material_runtime->session()->snapshot(),
            material_runtime->render_status(), material_inventory, assets,
            resolution, nullptr, target_cell, target_cell,
            &material_unselected_target)
            || material_unselected_target.probe_digest == 0U
            || material_unselected_target.gold_outline
            || material_unselected_target.cyan_outline) {
        return false;
    }
    FrameObservation material_unselected_tab{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            material_runtime->session()->snapshot(),
            material_runtime->render_status(), material_inventory, assets,
            resolution, nullptr, equipment_tab, equipment_tab,
            &material_unselected_tab)
            || material_unselected_tab.probe_digest == 0U
            || material_unselected_tab.gold_outline) {
        return false;
    }

    platform::DungeonRenderStatus error_status =
        material_runtime->render_status();
    error_status.indicator = platform::SaveIndicator::error;
    error_status.error = persistence::SaveError::write_failed;
    if (!capture_poe2_scenario(run, result, Poe2Scenario::error,
            *material_runtime->session(),
            material_runtime->session()->snapshot(), error_status,
            material_inventory, assets)) {
        return false;
    }

    if (!select_material_for_equipment(material_inventory, *material_runtime,
            items::MaterialId::transmute, resolution)) {
        return false;
    }
    std::array<FrameObservation, 2U> transmute_tab_frames{};
    for (FrameObservation& frame : transmute_tab_frames) {
        if (!render_poe2_inventory_frame(run, *material_runtime->session(),
                material_runtime->session()->snapshot(),
                material_runtime->render_status(), material_inventory, assets,
                resolution, nullptr, equipment_tab, equipment_tab, &frame)
                || frame.probe_digest == 0U || !frame.gold_outline
                || frame.probe_digest == material_unselected_tab.probe_digest) {
            return false;
        }
    }
    if (transmute_tab_frames[0].probe_digest
            != transmute_tab_frames[1].probe_digest) {
        return false;
    }
    if (material_inventory.process_input(*material_runtime,
            material_runtime->session()->snapshot(),
            hover(center(target_cell)))) {
        return false;
    }
    FrameObservation rejected_target_before{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            material_runtime->session()->snapshot(),
            material_runtime->render_status(), material_inventory, assets,
            resolution, nullptr, target_cell, target_cell,
            &rejected_target_before)
            || rejected_target_before.probe_digest == 0U
            || rejected_target_before.gold_outline
            || rejected_target_before.cyan_outline) {
        return false;
    }
    const auto rejected_before = material_runtime->session()->snapshot();
    const std::uint64_t transmute_before = material_runtime->item_state()
        ->materials[items::material_index(items::MaterialId::transmute)];
    const bool rejected_commit = material_inventory.process_input(
        *material_runtime, rejected_before, click(center(target_cell)));
    const auto rejected_after = material_runtime->session()->snapshot();
    if (rejected_commit
            || rejected_after.commit_generation
                != rejected_before.commit_generation
            || material_runtime->item_state()->materials[
                items::material_index(items::MaterialId::transmute)]
                != transmute_before) {
        return false;
    }
    if (!capture_poe2_scenario(run, result,
            Poe2Scenario::material_rejected,
            *material_runtime->session(), rejected_after,
            material_runtime->render_status(), material_inventory, assets,
            target_cell, target_cell)) {
        return false;
    }
    const FrameObservation& rejected_target_after = result.captures[
        poe2_scenario_index(Poe2Scenario::material_rejected)].frame;
    if (rejected_target_after.probe_digest
                != rejected_target_before.probe_digest
            || rejected_target_after.gold_outline
            || rejected_target_after.cyan_outline) {
        return false;
    }
    FrameObservation rejected_tab_after{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            rejected_after, material_runtime->render_status(),
            material_inventory, assets, resolution, nullptr,
            equipment_tab, equipment_tab, &rejected_tab_after)
            || rejected_tab_after.probe_digest
                != transmute_tab_frames.back().probe_digest
            || !rejected_tab_after.gold_outline) {
        return false;
    }
    if (material_inventory.process_input(*material_runtime, rejected_after,
            right_click({1.0F, 1.0F}))) {
        return false;
    }
    FrameObservation rejected_unselected_target{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            rejected_after, material_runtime->render_status(),
            material_inventory, assets, resolution, nullptr,
            target_cell, target_cell, &rejected_unselected_target)
            || rejected_unselected_target.probe_digest
                != material_unselected_target.probe_digest
            || rejected_unselected_target.gold_outline
            || rejected_unselected_target.cyan_outline) {
        return false;
    }
    FrameObservation rejected_unselected_tab{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            rejected_after, material_runtime->render_status(),
            material_inventory, assets, resolution, nullptr,
            equipment_tab, equipment_tab, &rejected_unselected_tab)
            || rejected_unselected_tab.probe_digest
                != material_unselected_tab.probe_digest
            || rejected_unselected_tab.gold_outline) {
        return false;
    }

    if (!select_material_for_equipment(material_inventory, *material_runtime,
            items::MaterialId::chaos, resolution)) {
        return false;
    }
    FrameObservation material_context{};
    if (!capture_poe2_scenario(run, result, Poe2Scenario::material_target,
            *material_runtime->session(),
            material_runtime->session()->snapshot(),
            material_runtime->render_status(), material_inventory, assets,
            {}, target_cell)) {
        return false;
    }
    material_context = result.captures[poe2_scenario_index(
        Poe2Scenario::material_target)].frame;
    if (!material_context.gold_outline) return false;
    FrameObservation material_tab_context{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            material_runtime->session()->snapshot(),
            material_runtime->render_status(), material_inventory, assets,
            resolution, nullptr, {}, equipment_tab,
            &material_tab_context)
            || !material_tab_context.gold_outline) {
        return false;
    }

    if (!material_inventory.process_input(*material_runtime,
            material_runtime->session()->snapshot(),
            click(center(target_cell)))) {
        return false;
    }
    platform::DungeonRenderStatus saving_status =
        material_runtime->render_status();
    saving_status.indicator = platform::SaveIndicator::saving;
    platform::InventoryRenderer saving_inventory{};
    saving_inventory.open(*material_runtime->session(),
        material_runtime->session()->snapshot());
    if (!capture_poe2_scenario(run, result, Poe2Scenario::saving,
            *material_runtime->session(),
            material_runtime->session()->snapshot(), saving_status,
            saving_inventory, assets, {}, target_cell)) {
        return false;
    }
    if (!settle_pending_save(*material_runtime)) return false;
    const auto saved_snapshot = material_runtime->session()->snapshot();
    const platform::DungeonRenderStatus saved_status =
        material_runtime->render_status();
    if (material_runtime->item_state()->materials[
            items::material_index(items::MaterialId::chaos)] != 1U) {
        return false;
    }
    if (!capture_poe2_scenario(run, result,
            Poe2Scenario::material_accepted,
            *material_runtime->session(), saved_snapshot, saved_status,
            material_inventory, assets, {}, target_cell)) {
        return false;
    }
    const FrameObservation saved_first = result.captures[
        poe2_scenario_index(Poe2Scenario::material_accepted)].frame;
    if (!saved_first.gold_outline
            || !capture_poe2_scenario(run, result, Poe2Scenario::saved,
                *material_runtime->session(), saved_snapshot, saved_status,
                material_inventory, assets, {}, target_cell)) {
        return false;
    }
    std::array<FrameObservation,
        platform::kInventoryTransientStatusFrameCount> saved_frames{};
    saved_frames[0U] = saved_first;
    saved_frames[1U] = result.captures[
        poe2_scenario_index(Poe2Scenario::saved)].frame;
    for (std::size_t frame = 2U; frame < saved_frames.size(); ++frame) {
        if (!render_poe2_inventory_frame(run, *material_runtime->session(),
                saved_snapshot, saved_status, material_inventory, assets,
                resolution, nullptr, {}, {}, &saved_frames[frame])) {
            return false;
        }
    }
    FrameObservation saved_after{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            saved_snapshot, saved_status, material_inventory, assets,
            resolution, nullptr, {}, {}, &saved_after)) {
        return false;
    }
    FrameObservation saved_not_replayed{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            saved_snapshot, saved_status, material_inventory, assets,
            resolution, nullptr, {}, {}, &saved_not_replayed)) {
        return false;
    }
    const bool saved_frames_stable = std::all_of(saved_frames.begin(),
        saved_frames.end(), [&](const FrameObservation& frame) noexcept {
            return frame.valid && frame.status_digest
                == saved_first.status_digest;
        });
    const bool saved_trace = saved_frames_stable
        && saved_first.status_digest != 0U
        && saved_first.status_digest != material_context.status_digest
        && saved_after.status_digest == material_context.status_digest
        && saved_not_replayed.status_digest == material_context.status_digest
        && material_runtime->session()->snapshot().commit_generation
            == saved_snapshot.commit_generation;
    if (!saved_trace) return false;

    if (!material_inventory.process_input(*material_runtime,
            material_runtime->session()->snapshot(),
            click(center(target_cell)))
            || !settle_pending_save(*material_runtime)) {
        return false;
    }
    const auto exhausted_snapshot = material_runtime->session()->snapshot();
    const auto exhausted_status = material_runtime->render_status();
    if (material_runtime->item_state()->materials[
            items::material_index(items::MaterialId::chaos)] != 0U) {
        return false;
    }
    if (material_inventory.process_input(*material_runtime,
            exhausted_snapshot, hover(center(shell.status_bar)))) {
        return false;
    }
    for (std::uint16_t frame{};
            frame < platform::kInventoryTransientStatusFrameCount; ++frame) {
        if (!render_poe2_inventory_frame(run, *material_runtime->session(),
                exhausted_snapshot, exhausted_status, material_inventory,
                assets, resolution, nullptr, {}, {}, nullptr)) {
            return false;
        }
    }
    if (!capture_poe2_scenario(run, result,
            Poe2Scenario::material_exhausted,
            *material_runtime->session(), exhausted_snapshot,
            exhausted_status, material_inventory, assets, {}, target_cell)) {
        return false;
    }
    const FrameObservation exhausted_context = result.captures[
        poe2_scenario_index(Poe2Scenario::material_exhausted)].frame;
    if (exhausted_context.gold_outline) return false;

    if (!select_material_for_equipment(material_inventory, *material_runtime,
            items::MaterialId::coupon_12, resolution)
            || !material_inventory.process_input(*material_runtime,
                material_runtime->session()->snapshot(),
                click(center(target_cell)))
            || !settle_pending_save(*material_runtime)) {
        return false;
    }
    const auto receipt_snapshot = material_runtime->session()->snapshot();
    const auto receipt_status = material_runtime->render_status();
    if (!receipt_snapshot.reinforcement_receipt.valid
            || !receipt_snapshot.reinforcement_receipt.coupon
            || receipt_snapshot.reinforcement_receipt.commit_generation == 0U) {
        return false;
    }
    if (material_inventory.process_input(*material_runtime,
            receipt_snapshot, hover(center(shell.status_bar)))) {
        return false;
    }
    if (!capture_poe2_scenario(run, result, Poe2Scenario::receipt,
            *material_runtime->session(), receipt_snapshot, receipt_status,
            material_inventory, assets, {}, target_cell)) {
        return false;
    }
    const FrameObservation receipt_first = result.captures[
        poe2_scenario_index(Poe2Scenario::receipt)].frame;
    std::array<FrameObservation,
        platform::kInventoryTransientStatusFrameCount> receipt_frames{};
    receipt_frames[0U] = receipt_first;
    for (std::size_t frame = 1U; frame < receipt_frames.size(); ++frame) {
        if (!render_poe2_inventory_frame(run, *material_runtime->session(),
                receipt_snapshot, receipt_status, material_inventory, assets,
                resolution, nullptr, {}, {}, &receipt_frames[frame])) {
            return false;
        }
    }
    FrameObservation receipt_after{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            receipt_snapshot, receipt_status, material_inventory, assets,
            resolution, nullptr, {}, {}, &receipt_after)) {
        return false;
    }
    FrameObservation receipt_not_replayed{};
    if (!render_poe2_inventory_frame(run, *material_runtime->session(),
            receipt_snapshot, receipt_status, material_inventory, assets,
            resolution, nullptr, {}, {}, &receipt_not_replayed)) {
        return false;
    }
    const bool receipt_frames_stable = std::all_of(receipt_frames.begin(),
        receipt_frames.end(), [&](const FrameObservation& frame) noexcept {
            return frame.valid && frame.status_digest
                == receipt_first.status_digest;
        });
    const bool receipt_trace = receipt_frames_stable
        && receipt_first.status_digest != 0U
        && receipt_first.status_digest != exhausted_context.status_digest
        && receipt_after.status_digest == exhausted_context.status_digest
        && receipt_not_replayed.status_digest
            == exhausted_context.status_digest
        && material_runtime->session()->snapshot().commit_generation
            == receipt_snapshot.commit_generation;
    result.status_transition_trace = saved_trace && receipt_trace;
    if (!result.status_transition_trace) return false;

    result.passed = std::all_of(result.captures.begin(),
        result.captures.end(), [](const Poe2CaptureResult& capture) noexcept {
            return capture.exported && capture.frame.valid;
        }) && result.tooltip_transition_trace
        && result.tooltip_comparison_trace
        && result.status_transition_trace;
    return result.passed;
}

[[nodiscard]] bool legacy_scenario_passed(
    const ScenarioResult& result) noexcept {
    return result.ground_material_visible && result.real_material_pickup
        && result.bag_selection && result.craft && result.coupon
        && result.confirmation_visible && result.destroyed
        && result.restarted;
}

bool write_state(const std::filesystem::path& run,
    const ScenarioResult& result,
    const Poe2ValidationResult& poe2) noexcept {
    const bool tooltip_transition_trace = std::all_of(
        poe2.resolutions.begin(), poe2.resolutions.end(),
        [](const Poe2ResolutionResult& resolution) noexcept {
            return resolution.tooltip_transition_trace;
        });
    const bool tooltip_comparison_trace = std::all_of(
        poe2.resolutions.begin(), poe2.resolutions.end(),
        [](const Poe2ResolutionResult& resolution) noexcept {
            return resolution.tooltip_comparison_trace;
        });
    const bool status_transition_trace = std::all_of(
        poe2.resolutions.begin(), poe2.resolutions.end(),
        [](const Poe2ResolutionResult& resolution) noexcept {
            return resolution.status_transition_trace;
        });
    std::ofstream output(run / "stage16-loot-reinforcement-state.txt",
        std::ios::out | std::ios::trunc);
    output << "result=" << (legacy_scenario_passed(result) && poe2.passed
            ? "pass" : "fail") << '\n'
        << "production_runtime=pass\n"
        << "root_filter=production_plan_nearest_material_drop"
        << " root=" << result.root_seed
        << " spawn_ordinal=" << result.first_drop_ordinal
        << " spawn_distance=" << result.first_spawn_distance << '\n'
        << "save_store_restart=" << (result.restarted ? "pass" : "fail") << '\n'
        << "ground_material_world_render=" << (result.ground_material_visible ? "pass" : "fail")
        << " spawn_distance=" << result.ground_spawn_distance << '\n'
        << "material_pickup=" << (result.real_material_pickup ? "pass" : "fail")
        << " generation=" << result.pickup_generation
        << " material=" << static_cast<unsigned>(result.picked) << '\n'
        << "inventory_material_selection=" << (result.bag_selection ? "pass" : "fail") << '\n'
        << "chaos_craft=" << (result.craft ? "committed" : "fail") << '\n'
        << "coupon_12=" << (result.coupon ? "committed" : "fail") << '\n'
        << "production_confirmation_ui="
        << (result.confirmation_visible ? "visible" : "fail") << '\n'
        << "reinforcement_12_to_13="
        << (result.destroyed ? "failed_destroyed" : "fail") << '\n'
        << "confirmation_screenshot=stage16-confirmation-1280x720.png\n"
        << "ground_screenshot=stage16-ground-material-1280x720.png\n"
        << "complete_screenshot=stage16-complete-1280x720.png\n"
        << "poe2_inventory_result=" << (poe2.passed ? "pass" : "fail")
        << '\n'
        << "tooltip_transition_trace="
        << (tooltip_transition_trace ? "pass" : "fail") << '\n'
        << "tooltip_comparison_trace="
        << (tooltip_comparison_trace ? "pass" : "fail") << '\n'
        << "status_transition_trace="
        << (status_transition_trace ? "pass" : "fail") << '\n';
    for (const Poe2ResolutionResult& resolution : poe2.resolutions) {
        output << "poe2_resolution=" << resolution.resolution.width << 'x'
            << resolution.resolution.height << " result="
            << (resolution.passed ? "pass" : "fail") << '\n';
        for (std::size_t index{}; index < resolution.captures.size(); ++index) {
            const Poe2CaptureResult& capture = resolution.captures[index];
            const bool readback = capture.frame.valid
                && capture.frame.width == resolution.resolution.width
                && capture.frame.height == resolution.resolution.height;
            output << "poe2_capture="
                << poe2_capture_name(kPoe2InventoryScenarios[index],
                    resolution.resolution)
                << " scenario=" << kPoe2InventoryScenarios[index]
                << " resolution=" << resolution.resolution.width << 'x'
                << resolution.resolution.height
                << " export=" << (capture.exported ? "pass" : "fail")
                << " png_readback=" << (readback ? "pass" : "fail")
                << " png_width=" << capture.frame.width
                << " png_height=" << capture.frame.height << '\n';
        }
    }
    return static_cast<bool>(output);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || argv[1] == nullptr) return 2;
    std::filesystem::path run{};
    if (!prepare_evidence(std::filesystem::absolute(argv[1]), run)) return 3;
    InitWindow(kLegacyResolution.width, kLegacyResolution.height,
        "Stage 16 Production UI Validation");
    if (!IsWindowReady()) return 4;
    if (!ChangeDirectory(GetApplicationDirectory())) {
        CloseWindow();
        return 5;
    }
    platform::SharedUiFont shared_ui_font{};
    if (!shared_ui_font.initialize()) {
        CloseWindow();
        return 6;
    }
    const Font borrowed_font = shared_ui_font.font();
    const std::size_t borrowed_atlas_bytes = shared_ui_font.atlas_bytes();
    if (!shared_ui_font.initialize()
            || shared_ui_font.font().texture.id != borrowed_font.texture.id
            || shared_ui_font.font().glyphs != borrowed_font.glyphs
            || shared_ui_font.atlas_bytes() != borrowed_atlas_bytes
            || shared_ui_font.active_instance_count() != 1U
            || shared_ui_font.total_atlas_bytes() != borrowed_atlas_bytes) {
        shared_ui_font.shutdown();
        CloseWindow();
        return 7;
    }
    if (!set_exact_window_resolution(kLegacyResolution)) {
        shared_ui_font.shutdown();
        CloseWindow();
        return 8;
    }
    const auto scenario = run_production_scenario(run, shared_ui_font);
    Poe2ValidationResult poe2{};
    if (scenario.has_value()) {
        poe2.passed = true;
        for (std::size_t index{}; index < kPoe2Resolutions.size(); ++index) {
            const bool resolution_passed = run_poe2_inventory_scenarios(
                run, shared_ui_font, kPoe2Resolutions[index],
                poe2.resolutions[index]);
            poe2.passed = poe2.passed && resolution_passed;
        }
    }
    const bool written = scenario.has_value()
        && write_state(run, *scenario, poe2);
    shared_ui_font.shutdown();
    CloseWindow();
    if (!scenario.has_value()) return 1;
    const bool passed = written && legacy_scenario_passed(*scenario)
        && poe2.passed;
    std::cout << "stage16 raylib production scenario="
              << (passed ? "PASS" : "FAIL") << '\n';
    return passed ? 0 : 1;
}

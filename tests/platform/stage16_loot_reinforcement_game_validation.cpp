#include "combat_renderer.hpp"
#include "control_hints.hpp"
#include "combat/monster_affix_generation.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/encounter_director.hpp"
#include "dungeon/material_loot.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon_runtime.hpp"
#include "host_input.hpp"
#include "inventory_renderer.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"
#include "items/material_catalog.hpp"
#include "persistence/save_store.hpp"

#include <raylib.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <optional>
#include <string>

namespace {

namespace combat = arpg::combat;
namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace persistence = arpg::persistence;
namespace platform = arpg::platform;

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

bool exists(const items::ItemOwnershipState& state,
    std::uint64_t id) noexcept {
    return std::any_of(state.items.begin(), state.items.end(),
        [&](const items::ItemInstance& item) { return item.id == id; });
}

std::optional<dungeon::DungeonRunState> prepared_state(
    std::uint64_t& selected_root, std::uint64_t& target_id,
    std::uint16_t& first_drop_ordinal, float& first_spawn_distance) noexcept {
    for (std::uint64_t root = 1U; root <= 8192U; ++root) {
        const auto built = dungeon::make_initial_run_state(root, dungeon::DungeonRules{});
        if (built.fault != dungeon::DungeonFault::none) continue;
        const dungeon::EncounterBuildRequest request{
            built.state.current_room.seed,
            built.state.current_room.depth,
            built.state.current_room.ecology,
            built.state.current_room.entry,
            built.state.current_room.has_hole,
            12U,
        };
        const auto plan = dungeon::build_encounter_plan(
            request, dungeon::DungeonRules{}.encounter);
        if (plan.fault != dungeon::DungeonFault::none) continue;
        if (plan.plan.wave_count == 0U || plan.plan.waves[0].spawn_count == 0U) {
            continue;
        }
        const combat::MonsterSpawnSpec* first_spawn = nullptr;
        float closest_squared = 0.0F;
        const auto& first_wave = plan.plan.waves[0];
        for (std::size_t index = 0U; index < first_wave.spawn_count; ++index) {
            const auto& spawn = first_wave.spawns[index];
            const float squared = spawn.position.x * spawn.position.x
                + spawn.position.y * spawn.position.y;
            if (first_spawn == nullptr || squared < closest_squared) {
                first_spawn = &spawn;
                closest_squared = squared;
            }
        }
        if (first_spawn == nullptr) continue;
        const std::uint16_t score =
            combat::monster_affix_danger_score(first_spawn->affixes);
        if (!dungeon::roll_material_drop(built.state.current_room.seed,
                first_spawn->spawn_ordinal, built.state.current_room.depth,
                score).has_value()) continue;
        for (std::uint64_t offset = 0U; offset < 256U; ++offset) {
            const std::uint64_t candidate_id = 0x1601U + offset * 2U;
            const auto target = make_item(candidate_id, 1U, items::ItemRarity::rare);
            const auto weapon = make_item(candidate_id + 1U, 8U, items::ItemRarity::rare);
            const auto helmet = make_item(candidate_id + 2U, 10U, items::ItemRarity::rare);
            const auto chest = make_item(candidate_id + 3U, 12U, items::ItemRarity::rare);
            const auto gloves = make_item(candidate_id + 4U, 14U, items::ItemRarity::rare);
            const auto boots = make_item(candidate_id + 5U, 16U, items::ItemRarity::rare);
            const auto accessory = make_item(candidate_id + 6U, 18U, items::ItemRarity::rare);
            if (!target || !weapon || !helmet || !chest || !gloves || !boots || !accessory) {
                continue;
            }
            auto state = built.state;
            auto strong = *weapon;
            // A legal end-game weapon keeps the integration drive focused on the
            // drop transaction instead of turning the acceptance case into a
            // low-damage survival test.  The tested target remains an independent
            // rare +12 item and still takes the real destruction path.
            strong.reinforcement = 15U;
            auto guarded_helmet = *helmet;
            auto guarded_chest = *chest;
            auto guarded_gloves = *gloves;
            auto guarded_boots = *boots;
            auto guarded_accessory = *accessory;
            guarded_helmet.reinforcement = 15U;
            guarded_chest.reinforcement = 15U;
            guarded_gloves.reinforcement = 15U;
            guarded_boots.reinforcement = 15U;
            guarded_accessory.reinforcement = 15U;
            state.item_ownership.items = {strong, *target, guarded_helmet,
                guarded_chest, guarded_gloves, guarded_boots, guarded_accessory};
            state.item_ownership.equipment.equipped_ids[
                static_cast<std::size_t>(items::ItemSlot::weapon)] = strong.id;
            state.item_ownership.equipment.equipped_ids[
                static_cast<std::size_t>(items::ItemSlot::helmet)] = guarded_helmet.id;
            state.item_ownership.equipment.equipped_ids[
                static_cast<std::size_t>(items::ItemSlot::chest)] = guarded_chest.id;
            state.item_ownership.equipment.equipped_ids[
                static_cast<std::size_t>(items::ItemSlot::gloves)] = guarded_gloves.id;
            state.item_ownership.equipment.equipped_ids[
                static_cast<std::size_t>(items::ItemSlot::boots)] = guarded_boots.id;
            state.item_ownership.equipment.equipped_ids[
                static_cast<std::size_t>(items::ItemSlot::accessory)] = guarded_accessory.id;
            state.item_ownership.next_item_sequence = candidate_id + 7U;
            state.item_ownership.materials[items::material_index(items::MaterialId::chaos)] = 1U;
            state.item_ownership.materials[items::material_index(items::MaterialId::coupon_12)] = 1U;
            state.item_ownership.materials[items::material_index(items::MaterialId::reinforcement_stone)] = 1U;
            auto reinforcement_state = state;
            reinforcement_state.item_ownership.items[1U].reinforcement = 12U;
            reinforcement_state.commit_generation += 3U;
            dungeon::DungeonSession probe{dungeon::DungeonRules{}, reinforcement_state};
            if (probe.request_reinforcement(candidate_id) != dungeon::RequestResult::accepted) continue;
            const auto pending = probe.pending_save();
            if (!pending || !pending->reinforcement_receipt.has_value()
                    || pending->reinforcement_receipt->success
                    || !pending->reinforcement_receipt->destroyed) continue;
            target_id = candidate_id;
            selected_root = root;
            first_drop_ordinal = first_spawn->spawn_ordinal;
            first_spawn_distance = std::sqrt(closest_squared);
            return state;
        }
    }
    return std::nullopt;
}

combat::MovementInput toward(combat::Vec3 from, combat::Vec3 to) noexcept {
    combat::MovementInput result{};
    const float dx = to.x - from.x;
    const float dy = to.y - from.y;
    result.x = dx > 0.35F ? 1 : (dx < -0.35F ? -1 : 0);
    result.y = dy > 0.25F ? 1 : (dy < -0.25F ? -1 : 0);
    return result;
}

const combat::MonsterSnapshot* nearest(const combat::CombatSnapshot& snapshot) noexcept {
    const combat::MonsterSnapshot* result = nullptr;
    float distance = 0.0F;
    for (const auto& monster : snapshot.monsters) {
        if (!monster.active || monster.hp <= 0) continue;
        const float dx = monster.position.x - snapshot.player.position.x;
        const float dy = monster.position.y - snapshot.player.position.y;
        const float next = dx * dx + dy * dy;
        if (result == nullptr || next < distance) {
            result = &monster;
            distance = next;
        }
    }
    return result;
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

bool render_world_capture(const std::filesystem::path& run,
    platform::DungeonRuntime& runtime, const char* name) noexcept {
    const auto start = std::filesystem::file_time_type::clock::now()
        - std::chrono::seconds(1);
    const auto room = std::make_unique<platform::CombatRenderer>();
    if (!room->initialize_resources() || runtime.session() == nullptr) return false;
    const auto current = std::make_unique<dungeon::DungeonSnapshot>(
        runtime.session()->snapshot());
    const platform::CombatFeedback feedback{};
    const platform::ControlHints hints{};
    room->observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        *current, *current, runtime.render_status(), hints, 1.0F / 60.0F, false);
    BeginDrawing();
    ClearBackground(BLACK);
    static_cast<void>(room->draw(*current, *current, runtime.render_status(), 1.0F,
        false, feedback, false));
    EndDrawing();
    const auto image = run / name;
    const auto relative = std::filesystem::relative(image,
        std::filesystem::current_path()).generic_string();
    TakeScreenshot(relative.c_str());
    room->shutdown_resources();
    std::error_code error{};
    const Image decoded = LoadImage(image.string().c_str());
    const bool valid = decoded.data != nullptr && decoded.width == 1280
        && decoded.height == 720 && image_has_rendered_content(decoded);
    if (decoded.data != nullptr) UnloadImage(decoded);
    const bool regular = std::filesystem::is_regular_file(image, error) && !error;
    error.clear();
    const bool large = std::filesystem::file_size(image, error) > 4096U && !error;
    error.clear();
    const bool fresh = std::filesystem::last_write_time(image, error) >= start && !error;
    return valid && regular && large && fresh;
}

bool drive_real_material_pickup(const std::filesystem::path& run,
    platform::DungeonRuntime& runtime, ScenarioResult& result) noexcept {
    std::int8_t retreat_direction{};
    for (int tick = 0; tick < 30000; ++tick) {
        const auto* const session = runtime.session();
        if (session == nullptr) return false;
        const auto snapshot = std::make_unique<dungeon::DungeonSnapshot>(session->snapshot());
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
                    result.ground_material_visible = render_world_capture(run, runtime,
                        "stage16-ground-material-1280x720.png");
                    if (!result.ground_material_visible) return false;
                }
                movement = toward(snapshot->combat->player.position,
                    snapshot->ground_materials[0].position);
            } else if (const auto* const target = nearest(*snapshot->combat)) {
                const float dx = target->position.x - snapshot->combat->player.position.x;
                const float dy = target->position.y - snapshot->combat->player.position.y;
                const float horizontal = std::abs(dx);
                const bool target_on_right = dx >= 0.0F;
                const combat::Facing desired_facing = target_on_right
                    ? combat::Facing::right : combat::Facing::left;
                if (snapshot->combat->player.active_attack != combat::AttackId::none) {
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
                    static_cast<void>(runtime.session()->queue_action(combat::Action::light));
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
                    movement = {retreat_direction, 0};
                } else if (horizontal > 2.55F) {
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
        if (runtime.state() != platform::DungeonRuntimeState::running) return false;
    }
    return false;
}

platform::HostFrameInput click(Vector2 point) noexcept {
    platform::HostFrameInput input{};
    input.mouse_left_pressed = true;
    input.mouse_position = point;
    return input;
}

Vector2 center(Rectangle rectangle) noexcept {
    return {rectangle.x + rectangle.width * 0.5F,
        rectangle.y + rectangle.height * 0.5F};
}

Vector2 first_inventory_cell() noexcept {
    const platform::InventoryLayout layout = platform::inventory_layout(1280, 720);
    return {layout.grid.x + 24.0F, layout.grid.y + 100.0F};
}

bool process_item_action(platform::InventoryRenderer& inventory,
    platform::DungeonRuntime& runtime, items::MaterialId material,
    std::uint64_t expected_item, bool& action_result) noexcept {
    const auto* const session = runtime.session();
    if (session == nullptr) return false;
    auto snapshot = session->snapshot();
    const auto bag = platform::material_bag_layout(1280, 720);
    if (!inventory.process_input(runtime, snapshot,
            click(center(bag.slots[items::material_index(material)])))) {
        // Selecting a material is intentionally a UI-only click.
    }
    snapshot = runtime.session()->snapshot();
    const bool committed = inventory.process_input(runtime, snapshot,
        click(first_inventory_cell()));
    action_result = committed;
    if (runtime.item_state() == nullptr || !exists(*runtime.item_state(), expected_item)) {
        return false;
    }
    const std::size_t index = items::material_index(material);
    return committed && index < items::kMaterialCount
        && runtime.item_state()->materials[index] == 0U;
}

bool render_inventory_capture(const std::filesystem::path& run,
    platform::DungeonRuntime& runtime, platform::InventoryRenderer& inventory,
    const char* name) noexcept {
    const auto start = std::filesystem::file_time_type::clock::now()
        - std::chrono::seconds(1);
    const auto room = std::make_unique<platform::CombatRenderer>();
    if (!room->initialize_resources() || runtime.session() == nullptr) return false;
    const auto current = std::make_unique<dungeon::DungeonSnapshot>(
        runtime.session()->snapshot());
    const platform::CombatFeedback feedback{};
    const platform::ControlHints hints{};
    room->observe_presented_hud_frame(platform::HudPresentedFrame::normal,
        *current, *current, runtime.render_status(), hints, 1.0F / 60.0F, false);
    BeginDrawing();
    ClearBackground(BLACK);
    static_cast<void>(room->draw(*current, *current, runtime.render_status(), 1.0F,
        false, feedback, false));
    inventory.draw(*runtime.session(), *current, runtime.render_status(),
        room->hud_font(), room->hud_font_ready());
    EndDrawing();
    const auto image = run / name;
    const auto relative = std::filesystem::relative(image,
        std::filesystem::current_path()).generic_string();
    TakeScreenshot(relative.c_str());
    room->shutdown_resources();
    std::error_code error{};
    const Image decoded = LoadImage(image.string().c_str());
    const bool valid = decoded.data != nullptr && decoded.width == 1280
        && decoded.height == 720 && image_has_rendered_content(decoded);
    if (decoded.data != nullptr) UnloadImage(decoded);
    const bool regular = std::filesystem::is_regular_file(image, error) && !error;
    error.clear();
    const bool large = std::filesystem::file_size(image, error) > 4096U && !error;
    error.clear();
    const bool fresh = std::filesystem::last_write_time(image, error) >= start && !error;
    return valid && regular && large && fresh;
}

std::optional<ScenarioResult> run_production_scenario(
    const std::filesystem::path& run) noexcept {
    std::uint64_t target_id = 0x1601U;
    std::uint64_t root{};
    std::uint16_t first_drop_ordinal{};
    float first_spawn_distance{};
    const auto state = prepared_state(root, target_id, first_drop_ordinal,
        first_spawn_distance);
    if (!state.has_value()) return std::nullopt;
    const auto save_directory = run / "save";
    persistence::SaveStore store{{save_directory}};
    const auto initial = store.commit(*state);
    if (initial.state != persistence::SaveCommitState::committed) return std::nullopt;

    platform::DungeonRuntimeConfig config{};
    config.save.directory = save_directory;
    config.new_run_seed = root;
    const auto runtime = std::make_unique<platform::DungeonRuntime>(config);
    if (!runtime->initialize() || runtime->state()
            != platform::DungeonRuntimeState::running) return std::nullopt;
    ScenarioResult result{};
    result.root_seed = root;
    result.first_drop_ordinal = first_drop_ordinal;
    result.first_spawn_distance = first_spawn_distance;
    if (!drive_real_material_pickup(run, *runtime, result) || runtime->item_state() == nullptr) {
        return std::nullopt;
    }

    const auto inventory = std::make_unique<platform::InventoryRenderer>();
    inventory->open(*runtime->session(), runtime->session()->snapshot());
    if (!process_item_action(*inventory, *runtime, items::MaterialId::chaos,
            target_id, result.craft) || !result.craft) return std::nullopt;
    if (!process_item_action(*inventory, *runtime, items::MaterialId::coupon_12,
            target_id, result.coupon) || !result.coupon) return std::nullopt;

    const auto bag = platform::material_bag_layout(1280, 720);
    static_cast<void>(inventory->process_input(*runtime, runtime->session()->snapshot(),
        click(center(bag.slots[items::material_index(
            items::MaterialId::reinforcement_stone)]))));
    static_cast<void>(inventory->process_input(*runtime, runtime->session()->snapshot(),
        click(first_inventory_cell())));
    const auto confirmation = platform::reinforcement_confirmation_layout(1280, 720);
    result.confirmation_visible = render_inventory_capture(run, *runtime, *inventory,
        "stage16-confirmation-1280x720.png");
    if (!result.confirmation_visible) return std::nullopt;
    const bool confirmation_committed = inventory->process_input(*runtime,
        runtime->session()->snapshot(), click(center(confirmation.confirm)));
    result.destroyed = runtime->item_state() != nullptr
        && !exists(*runtime->item_state(), target_id);
    if (!confirmation_committed || !result.destroyed) return std::nullopt;
    if (!render_inventory_capture(run, *runtime, *inventory,
            "stage16-complete-1280x720.png")) return std::nullopt;

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

bool write_state(const std::filesystem::path& run,
    const ScenarioResult& result) noexcept {
    std::ofstream output(run / "stage16-loot-reinforcement-state.txt",
        std::ios::out | std::ios::trunc);
    output << "result=" << (result.ground_material_visible && result.real_material_pickup && result.bag_selection
        && result.craft && result.coupon && result.confirmation_visible
        && result.destroyed && result.restarted ? "pass" : "fail") << '\n'
        << "production_runtime=pass\n"
        << "root_filter=first_wave_nearest_monster_material_drop"
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
        << "complete_screenshot=stage16-complete-1280x720.png\n";
    return static_cast<bool>(output);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || argv[1] == nullptr) return 2;
    std::filesystem::path run{};
    if (!prepare_evidence(std::filesystem::absolute(argv[1]), run)) return 3;
    InitWindow(1280, 720, "Stage 16 Production UI Validation");
    if (!IsWindowReady()) return 4;
    const auto scenario = run_production_scenario(run);
    const bool written = scenario.has_value() && write_state(run, *scenario);
    CloseWindow();
    if (!scenario.has_value()) return 1;
    const bool passed = written && scenario->ground_material_visible && scenario->real_material_pickup
        && scenario->bag_selection && scenario->craft && scenario->coupon
        && scenario->confirmation_visible && scenario->destroyed
        && scenario->restarted;
    std::cout << "stage16 raylib production scenario="
              << (passed ? "PASS" : "FAIL") << '\n';
    return passed ? 0 : 1;
}

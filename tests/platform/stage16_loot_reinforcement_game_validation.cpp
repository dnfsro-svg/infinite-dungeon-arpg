#include "combat_renderer.hpp"
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
#include "items/item_catalog.hpp"
#include "items/item_crafting.hpp"
#include "items/item_generation.hpp"
#include "items/material_catalog.hpp"
#include "persistence/save_store.hpp"
#include "stage10_validation_build.hpp"

#include <raylib.h>
#include <rlgl.h>

#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <memory>
#include <new>
#include <optional>
#include <string>
#include <thread>

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
    const auto image = run / name;
    const bool exported = export_flushed_frame(image);
    EndDrawing();
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
    return exported && valid && regular && large && fresh;
}

bool drive_real_material_pickup(const std::filesystem::path& run,
    platform::DungeonRuntime& runtime, ScenarioResult& result) noexcept {
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
                    result.ground_material_visible = render_world_capture(run, runtime,
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

Vector2 center(Rectangle rectangle) noexcept {
    return {rectangle.x + rectangle.width * 0.5F,
        rectangle.y + rectangle.height * 0.5F};
}

Vector2 first_inventory_cell() noexcept {
    const platform::InventoryLayout layout = platform::inventory_layout(1280, 720);
    return {layout.grid.x + 24.0F, layout.grid.y + 100.0F};
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

bool process_item_action(platform::InventoryRenderer& inventory,
    platform::DungeonRuntime& runtime, items::MaterialId material,
    std::uint64_t expected_item, bool& action_result) noexcept {
    const auto* const session = runtime.session();
    if (session == nullptr || !settle_pending_save(runtime)) return false;
    const std::size_t index = items::material_index(material);
    const std::uint64_t before = runtime.item_state() != nullptr
            && index < items::kMaterialCount
        ? runtime.item_state()->materials[index] : 0U;
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
        room->material_pack(), room->hud_font(), room->hud_font_ready());
    const auto image = run / name;
    const bool exported = export_flushed_frame(image);
    EndDrawing();
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
    return exported && valid && regular && large && fresh;
}

std::optional<ScenarioResult> run_production_scenario(
    const std::filesystem::path& run) noexcept {
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
    const auto runtime = std::make_unique<platform::DungeonRuntime>(config);
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
    if (!drive_real_material_pickup(run, *runtime, result)
            || runtime->item_state() == nullptr
            || !settle_pending_save(*runtime)) {
        std::cerr << "stage16 failure=drive_real_material_pickup"
                  << " item_state=" << (runtime->item_state() != nullptr ? 1 : 0)
                  << '\n';
        return std::nullopt;
    }

    const auto inventory = std::make_unique<platform::InventoryRenderer>();
    inventory->open(*runtime->session(), runtime->session()->snapshot());
    if (!process_item_action(*inventory, *runtime, items::MaterialId::chaos,
            target_id, result.craft) || !result.craft) {
        std::cerr << "stage16 failure=chaos_craft\n";
        return std::nullopt;
    }
    if (!process_item_action(*inventory, *runtime, items::MaterialId::coupon_12,
            target_id, result.coupon) || !result.coupon) {
        std::cerr << "stage16 failure=coupon_12\n";
        return std::nullopt;
    }

    const auto bag = platform::material_bag_layout(1280, 720);
    static_cast<void>(inventory->process_input(*runtime, runtime->session()->snapshot(),
        click(center(bag.slots[items::material_index(
            items::MaterialId::reinforcement_stone)]))));
    static_cast<void>(inventory->process_input(*runtime, runtime->session()->snapshot(),
        click(first_inventory_cell())));
    const auto confirmation = platform::reinforcement_confirmation_layout(1280, 720);
    result.confirmation_visible = render_inventory_capture(run, *runtime, *inventory,
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
    if (!render_inventory_capture(run, *runtime, *inventory,
            "stage16-complete-1280x720.png")) {
        std::cerr << "stage16 failure=complete_capture\n";
        return std::nullopt;
    }

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

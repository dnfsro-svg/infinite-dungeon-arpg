#include "test_framework.hpp"
#include "allocation_probe.hpp"

#include "../dungeon/dungeon_test_support.hpp"
#include "dungeon_runtime.hpp"

#include <array>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;
namespace platform = arpg::platform;
namespace combat = arpg::combat;
namespace items = arpg::items;

items::ItemInstance normal_item(std::uint64_t id,
    std::uint8_t base_id = 1U) noexcept {
    items::ItemInstance item{};
    item.id = id;
    item.base_id = base_id;
    item.rarity = items::ItemRarity::normal;
    item.item_level = 1U;
    item.required_level = 1U;
    return item;
}

struct TempDirectory final {
    std::filesystem::path path{};

    TempDirectory() noexcept {
        std::error_code error;
        path = std::filesystem::temp_directory_path(error)
            / "arpg_task10_dungeon_runtime";
        path /= std::to_string(static_cast<unsigned long long>(
            std::hash<std::string>{}(path.string() + std::to_string(
                reinterpret_cast<std::uintptr_t>(this)))));
        std::filesystem::remove_all(path, error);
        std::filesystem::create_directories(path, error);
    }

    ~TempDirectory() noexcept {
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

struct FaultContext final {
    persistence::SaveFaultPoint point{};
    bool enabled{};
};

bool fail_when_enabled(persistence::SaveFaultPoint point,
    void* opaque) noexcept {
    const auto* const context = static_cast<const FaultContext*>(opaque);
    return context != nullptr && context->enabled && context->point == point;
}

std::optional<std::uint64_t> provider_seed(void* context) noexcept {
    return context == nullptr ? std::nullopt
                              : std::optional<std::uint64_t>{
                                    *static_cast<std::uint64_t*>(context)};
}

platform::DungeonRuntimeConfig config_for(const TempDirectory& directory,
    std::uint64_t seed = 8U) noexcept {
    platform::DungeonRuntimeConfig config{};
    config.save.directory = directory.path;
    config.new_run_seed = seed;
    return config;
}

void drain(dungeon::DungeonSession& session) noexcept {
    while (session.try_pop_event().has_value()) {
    }
    while (session.try_pop_combat_event().has_value()) {
    }
}

bool clear_and_await(dungeon::DungeonSession& session) noexcept {
    for (int tick = 0; tick < 4096; ++tick) {
        const auto snapshot = session.snapshot();
        if (snapshot.phase == dungeon::RoomPhase::awaiting_exit) {
            return true;
        }
        if (snapshot.phase == dungeon::RoomPhase::cleared) {
            session.tick({});
            drain(session);
            continue;
        }
        if (snapshot.phase == dungeon::RoomPhase::combat) {
            arpg::test::force_defeat_current_wave(session);
        }
        session.tick({});
        drain(session);
    }
    return false;
}

bool drive_door_pending(dungeon::DungeonSession& session) noexcept {
    if (!clear_and_await(session)) {
        return false;
    }
    combat::MovementInput movement{1, 0};
    for (int tick = 0; tick < 512; ++tick) {
        const auto snapshot = session.snapshot();
        if (snapshot.phase == dungeon::RoomPhase::committing) {
            return session.pending_transition().has_value();
        }
        if (!snapshot.combat.has_value()) {
            return false;
        }
        movement.y = snapshot.combat->player.position.y > 0.1F ? -1
            : (snapshot.combat->player.position.y < -0.1F ? 1 : 0);
        session.tick(movement);
        drain(session);
    }
    return false;
}

arpg::test::Failure empty_directory_commits_seeded_generation_one_before_running() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.session() != nullptr);
    ARPG_REQUIRE(runtime.session()->snapshot().root_seed == 8U);
    ARPG_REQUIRE(runtime.session()->snapshot().commit_generation == 1U);
    ARPG_REQUIRE(runtime.session()->snapshot().depth == 1U);
    ARPG_REQUIRE(runtime.session()->snapshot().floor_room_index == 1U);
    ARPG_REQUIRE(runtime.session()->snapshot().ecology
        == dungeon::DungeonElement::water);
    ARPG_REQUIRE((runtime.session()->snapshot().biases
        == std::array<std::uint32_t, 4>{{0U, 0U, 0U, 0U}}));
    ARPG_REQUIRE((runtime.session()->snapshot().exits_open
        == std::array<bool, 4>{{false, false, false, false}}));
    persistence::SaveStore store(config.save);
    const auto loaded = store.load();
    ARPG_REQUIRE(loaded.state == persistence::SaveLoadState::ready);
    ARPG_REQUIRE(loaded.checkpoint.root_seed == 8U);
    ARPG_REQUIRE(loaded.checkpoint.commit_generation == 1U);
    return {};
}

arpg::test::Failure valid_save_ignores_new_run_seed_override() noexcept {
    TempDirectory directory;
    auto first = config_for(directory, 8U);
    platform::DungeonRuntime original(first);
    ARPG_REQUIRE(original.initialize());
    const auto original_seed = original.session()->snapshot().root_seed;
    auto second = config_for(directory, 999U);
    platform::DungeonRuntime resumed(second);
    ARPG_REQUIRE(resumed.initialize());
    ARPG_REQUIRE(resumed.session() != nullptr);
    ARPG_REQUIRE(resumed.session()->snapshot().root_seed == original_seed);
    return {};
}

arpg::test::Failure committed_pending_transition_maps_verified_state_and_saved_indicator() noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    const auto expected = runtime.session()->pending_transition();
    ARPG_REQUIRE(expected.has_value());
    runtime.service_pending_transition();
    const auto snapshot = runtime.session()->snapshot();
    ARPG_REQUIRE(snapshot.phase == dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(snapshot.commit_generation == expected->next_state.commit_generation);
    ARPG_REQUIRE(snapshot.room_seed == expected->next_state.current_room.seed);
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::saved);
    return {};
}

arpg::test::Failure committed_passive_save_survives_runtime_restart() noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    runtime.service_pending_save();
    const auto saved = runtime.session()->snapshot();
    ARPG_REQUIRE(saved.passive_tree.allocated_bits == ((1ULL << 0U) | (1ULL << 8U)));
    ARPG_REQUIRE(!saved.passive_save_pending);
    const auto generation = saved.commit_generation;
    runtime.service_pending_save();
    ARPG_REQUIRE(runtime.session()->snapshot().commit_generation == generation);
    platform::DungeonRuntime resumed(config_for(directory, 999U));
    ARPG_REQUIRE(resumed.initialize());
    ARPG_REQUIRE(resumed.session()->snapshot().passive_tree.allocated_bits
        == saved.passive_tree.allocated_bits);
    return {};
}

arpg::test::Failure committed_route_and_refund_survive_runtime_restart() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    persistence::SaveStore seed_store(config.save);
    auto initial = dungeon::make_initial_run_state(8U, config.rules).state;
    initial.progression = {4U, 0U, 3U, 3U};
    ARPG_REQUIRE(seed_store.commit(initial).state
        == persistence::SaveCommitState::committed);

    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    const auto before = runtime.session()->snapshot();
    for (const std::uint8_t node : {std::uint8_t{8U}, std::uint8_t{9U},
            std::uint8_t{10U}}) {
        ARPG_REQUIRE(runtime.session()->request_passive_allocation(node));
        runtime.service_pending_save();
        ARPG_REQUIRE(!runtime.session()->snapshot().passive_save_pending);
        ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::saved);
    }
    ARPG_REQUIRE(runtime.session()->request_passive_refund(10U));
    runtime.service_pending_save();
    const auto saved = runtime.session()->snapshot();
    constexpr std::uint64_t kExpectedBits = (1ULL << 0U) | (1ULL << 8U)
        | (1ULL << 9U);
    ARPG_REQUIRE(saved.passive_tree.allocated_bits == kExpectedBits);
    ARPG_REQUIRE(saved.progression.unspent_passive_points
        == before.progression.unspent_passive_points - 2U);
    ARPG_REQUIRE(!saved.passive_save_pending);

    platform::DungeonRuntime resumed(config_for(directory, 999U));
    ARPG_REQUIRE(resumed.initialize());
    const auto restored = resumed.session()->snapshot();
    ARPG_REQUIRE(restored.passive_tree.allocated_bits == kExpectedBits);
    ARPG_REQUIRE(restored.progression.unspent_passive_points
        == saved.progression.unspent_passive_points);
    ARPG_REQUIRE(!restored.passive_save_pending);
    return {};
}

arpg::test::Failure passive_pre_publish_failure_keeps_old_tree_and_retryable_runtime() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto config = config_for(directory);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    const auto before = runtime.session()->snapshot();
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    fault.enabled = true;
    runtime.service_pending_save();
    const auto after = runtime.session()->snapshot();
    ARPG_REQUIRE(after.passive_tree.allocated_bits == before.passive_tree.allocated_bits);
    ARPG_REQUIRE(after.commit_generation == before.commit_generation);
    ARPG_REQUIRE(after.phase == dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(!after.passive_save_pending);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::error);
    platform::DungeonRuntime resumed(config_for(directory, 999U));
    ARPG_REQUIRE(resumed.initialize());
    const auto restored = resumed.session()->snapshot();
    ARPG_REQUIRE(restored.passive_tree.allocated_bits
        == before.passive_tree.allocated_bits);
    ARPG_REQUIRE(restored.commit_generation == before.commit_generation);
    return {};
}

arpg::test::Failure indeterminate_passive_save_faults_runtime() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::after_publish, false};
    auto config = config_for(directory);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    fault.enabled = true;
    runtime.service_pending_save();
    ARPG_REQUIRE(runtime.session()->snapshot().phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    return {};
}

arpg::test::Failure passive_pending_rejects_door_and_descent_requests() noexcept {
    TempDirectory directory;
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(clear_and_await(*runtime.session()));
    ARPG_REQUIRE(runtime.session()->request_passive_allocation(8U));
    const auto pending = runtime.session()->pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->kind == dungeon::PendingSaveKind::passive_tree);
    ARPG_REQUIRE(!runtime.session()->request_descent(true));
    const auto before_door = runtime.session()->snapshot();
    arpg::test::attempt_exit(*runtime.session(), dungeon::ExitDirection::right);
    const auto snapshot = runtime.session()->snapshot();
    ARPG_REQUIRE(snapshot.phase == dungeon::RoomPhase::committing);
    ARPG_REQUIRE(snapshot.passive_save_pending);
    ARPG_REQUIRE(!snapshot.has_pending_transition);
    ARPG_REQUIRE(snapshot.diagnostics.rejected_exit_count
        == before_door.diagnostics.rejected_exit_count + 1U);
    return {};
}

arpg::test::Failure pre_publish_not_committed_maps_to_retryable_error() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::before_publish, false};
    auto config = config_for(directory);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    fault.enabled = true;
    runtime.service_pending_transition();
    ARPG_REQUIRE(runtime.session()->snapshot().phase
        == dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::error);
    fault.enabled = false;
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    runtime.service_pending_transition();
    ARPG_REQUIRE(runtime.session()->snapshot().phase
        == dungeon::RoomPhase::transitioning);
    return {};
}

arpg::test::Failure indeterminate_maps_to_faulted_runtime_and_blocks_selection() noexcept {
    TempDirectory directory;
    FaultContext fault{persistence::SaveFaultPoint::after_publish, false};
    auto config = config_for(directory);
    config.save.fault_hook = &fail_when_enabled;
    config.save.fault_context = &fault;
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    fault.enabled = true;
    runtime.service_pending_transition();
    ARPG_REQUIRE(runtime.session()->snapshot().phase == dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
    ARPG_REQUIRE(!runtime.session()->request_descent(true));
    return {};
}

arpg::test::Failure dual_slot_corruption_requires_recovery_then_archives_new_run() noexcept {
    TempDirectory directory;
    const char invalid[] = "bad";
    std::ofstream(directory.path / "run_a.sav", std::ios::binary).write(invalid, 3);
    std::ofstream(directory.path / "run_b.sav", std::ios::binary).write(invalid, 3);
    platform::DungeonRuntime runtime(config_for(directory));
    ARPG_REQUIRE(!runtime.initialize());
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::recovery_required);
    ARPG_REQUIRE(runtime.session() == nullptr);
    ARPG_REQUIRE(runtime.item_state() == nullptr);
    ARPG_REQUIRE(runtime.request_pickup(0U) == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_equip(1U) == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_unequip(items::ItemSlot::weapon)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_recipe({{1U, 2U, 3U}})
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.recover_with_new_run());
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.session() != nullptr);
    ARPG_REQUIRE(runtime.session()->snapshot().commit_generation == 1U);
    return {};
}

arpg::test::Failure single_slot_corruption_recovers_and_subsequent_saves_alternate() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    persistence::SaveStore store(config.save);
    const auto initial = dungeon::make_initial_run_state(8U, config.rules).state;
    ARPG_REQUIRE(store.commit(initial).state == persistence::SaveCommitState::committed);
    const auto active = store.load().active_slot;
    const auto bad_slot = active == persistence::SaveSlot::a ? "run_b.sav" : "run_a.sav";
    const char invalid[] = "bad";
    std::ofstream(directory.path / bad_slot, std::ios::binary).write(invalid, 3);
    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::recovered);
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    runtime.service_pending_transition();
    const auto first_slot = runtime.render_status().active_slot;
    runtime.session()->tick({});
    ARPG_REQUIRE(drive_door_pending(*runtime.session()));
    runtime.service_pending_transition();
    ARPG_REQUIRE(runtime.render_status().active_slot != first_slot);
    return {};
}

arpg::test::Failure invalid_rules_fault_without_creating_save_or_session() noexcept {
    for (const int variant : {0, 1, 2}) {
        TempDirectory directory;
        auto config = config_for(directory);
        if (variant == 0) {
            config.rules.rules_version = 2U;
        } else if (variant == 1) {
            config.rules.hole_threshold = 10001U;
        } else {
            config.rules.base_weights[0] = 0U;
        }
        std::uint64_t provided = 17U;
        config.seed_provider = &provider_seed;
        config.seed_context = &provided;
        platform::DungeonRuntime runtime(config);
        ARPG_REQUIRE(!runtime.initialize());
        ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::faulted);
        ARPG_REQUIRE(runtime.session() == nullptr);
        ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_a.sav"));
        ARPG_REQUIRE(!std::filesystem::exists(directory.path / "run_b.sav"));
    }
    return {};
}

bool same_item(const items::ItemInstance& left,
    const items::ItemInstance& right) noexcept {
    return std::memcmp(&left, &right, sizeof(items::ItemInstance)) == 0;
}

bool same_ownership(const items::ItemOwnershipState& left,
    const items::ItemOwnershipState& right) noexcept {
    if (left.items.size() != right.items.size()
            || left.equipment.equipped_ids != right.equipment.equipped_ids
            || left.claimed_drop_bits != right.claimed_drop_bits
            || left.next_item_sequence != right.next_item_sequence) {
        return false;
    }
    for (std::size_t index = 0U; index < left.items.size(); ++index) {
        if (!same_item(left.items[index], right.items[index])) return false;
    }
    return true;
}

bool same_build(const combat::PlayerCombatBuild& left,
    const combat::PlayerCombatBuild& right) noexcept {
    const auto& a = left.values;
    const auto& b = right.values;
    return left.weapon_physical == right.weapon_physical
        && left.local_attack_speed_bp == right.local_attack_speed_bp
        && a.flat_damage == b.flat_damage
        && a.damage_increased == b.damage_increased
        && a.damage_reduction == b.damage_reduction
        && a.damage_reduction_cap_bonus == b.damage_reduction_cap_bonus
        && a.armor == b.armor && a.evasion == b.evasion
        && a.melee_damage == b.melee_damage
        && a.max_health == b.max_health
        && a.max_health_more == b.max_health_more
        && a.max_barrier == b.max_barrier
        && a.damage_taken == b.damage_taken
        && a.movement_speed == b.movement_speed
        && a.attack_speed == b.attack_speed
        && a.impulse_scale == b.impulse_scale
        && a.jump_speed == b.jump_speed
        && a.air_control == b.air_control && a.valid == b.valid;
}

enum class ItemRequestKind : std::uint8_t {
    pickup,
    equip,
    unequip,
    recipe,
};

dungeon::DungeonRunState item_state_for(ItemRequestKind kind) {
    auto state = dungeon::make_initial_run_state(0x81818181U, {}).state;
    state.item_ownership.next_item_sequence = 9U;
    if (kind == ItemRequestKind::equip) {
        state.item_ownership.items.push_back(normal_item(101U));
    } else if (kind == ItemRequestKind::unequip) {
        state.item_ownership.items.push_back(normal_item(201U));
        state.item_ownership.equipment.equipped_ids[0] = 201U;
    } else if (kind == ItemRequestKind::recipe) {
        state.item_ownership.items.push_back(normal_item(301U));
        state.item_ownership.items.push_back(normal_item(302U));
        state.item_ownership.items.push_back(normal_item(303U));
    }
    return state;
}

dungeon::RequestResult issue_item_request(platform::DungeonRuntime& runtime,
    ItemRequestKind kind) noexcept {
    switch (kind) {
    case ItemRequestKind::pickup:
        return runtime.request_pickup(0U);
    case ItemRequestKind::equip:
        return runtime.request_equip(101U);
    case ItemRequestKind::unequip:
        return runtime.request_unequip(items::ItemSlot::weapon);
    case ItemRequestKind::recipe:
        return runtime.request_recipe({{301U, 302U, 303U}});
    }
    return dungeon::RequestResult::rejected;
}

bool install_pickup_if_needed(platform::DungeonRuntime& runtime,
    ItemRequestKind kind) noexcept {
    if (kind != ItemRequestKind::pickup) return true;
    const auto snapshot = runtime.session()->snapshot();
    if (!snapshot.combat.has_value()) return false;
    arpg::test::install_ground_item(*runtime.session(), 0U,
        normal_item(401U, 2U), snapshot.combat->player.position);
    return runtime.session()->snapshot().ground_item_count == 1U;
}

arpg::test::Failure runtime_exposes_narrow_item_requests_and_stable_item_view() noexcept {
    TempDirectory directory;
    auto config = config_for(directory);
    auto initial = dungeon::make_initial_run_state(8U, config.rules).state;
    initial.item_ownership.items.push_back(normal_item(101U));
    initial.item_ownership.next_item_sequence = 102U;
    persistence::SaveStore seed_store(config.save);
    ARPG_REQUIRE(seed_store.commit(initial).state
        == persistence::SaveCommitState::committed);

    platform::DungeonRuntime runtime(config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(runtime.item_state() != nullptr);
    ARPG_REQUIRE(runtime.item_state() == &runtime.session()->item_state());
    ARPG_REQUIRE(runtime.item_state()->items.size() == 1U);
    ARPG_REQUIRE(runtime.request_equip(101U) == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(runtime.request_pickup(0U) == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_unequip(items::ItemSlot::weapon)
        == dungeon::RequestResult::rejected);
    ARPG_REQUIRE(runtime.request_recipe({{101U, 102U, 103U}})
        == dungeon::RequestResult::rejected);
    return {};
}

arpg::test::Failure large_inventory_snapshot_and_views_do_not_allocate() noexcept {
    constexpr std::size_t kItemCount = 65535U;
    auto state = dungeon::make_initial_run_state(0x65535U, {}).state;
    state.item_ownership.items.reserve(kItemCount);
    for (std::size_t index = 0U; index < kItemCount; ++index) {
        state.item_ownership.items.push_back(normal_item(index + 1U));
    }
    state.item_ownership.next_item_sequence = kItemCount + 1U;
    dungeon::DungeonSession session{{}, std::move(state)};
    platform::DungeonRuntime* no_runtime = nullptr;
    ARPG_REQUIRE(session.snapshot().inventory_count == kItemCount);
    ARPG_REQUIRE(session.pending_save_view() == nullptr);

    const std::uint64_t before = arpg::test::allocation_count();
    for (int repetition = 0; repetition < 8; ++repetition) {
        const auto snapshot = session.snapshot();
        ARPG_REQUIRE(snapshot.inventory_count == kItemCount);
        ARPG_REQUIRE(snapshot.ground_item_count == 0U);
        ARPG_REQUIRE(session.item_state().items.size() == kItemCount);
        ARPG_REQUIRE(session.pending_save_view() == nullptr);
        ARPG_REQUIRE(no_runtime == nullptr);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);

    ARPG_REQUIRE(session.request_equip(1U)
        == dungeon::RequestResult::accepted);
    ARPG_REQUIRE(session.pending_save_view() != nullptr);
    const std::uint64_t pending_before = arpg::test::allocation_count();
    for (int repetition = 0; repetition < 8; ++repetition) {
        const dungeon::PendingSave* const pending = session.pending_save_view();
        ARPG_REQUIRE(pending != nullptr);
        ARPG_REQUIRE(pending->next_state.item_ownership.items.size()
            == kItemCount);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == pending_before);
    return {};
}

arpg::test::Failure generic_service_adds_no_large_state_copies() noexcept {
    constexpr std::size_t kItemCount = 65535U;
    auto initial = dungeon::make_initial_run_state(0xC0FFEEU, {}).state;
    initial.item_ownership.items.reserve(kItemCount);
    for (std::size_t index = 0U; index < kItemCount; ++index) {
        initial.item_ownership.items.push_back(normal_item(index + 1U));
    }
    initial.item_ownership.next_item_sequence = kItemCount + 1U;

    TempDirectory runtime_directory;
    TempDirectory direct_directory;
    auto runtime_config = config_for(runtime_directory);
    auto direct_config = config_for(direct_directory);
    persistence::SaveStore runtime_seed(runtime_config.save);
    persistence::SaveStore direct_store(direct_config.save);
    ARPG_REQUIRE(runtime_seed.commit(initial).state
        == persistence::SaveCommitState::committed);
    ARPG_REQUIRE(direct_store.commit(initial).state
        == persistence::SaveCommitState::committed);

    platform::DungeonRuntime runtime(runtime_config);
    ARPG_REQUIRE(runtime.initialize());
    ARPG_REQUIRE(runtime.request_equip(1U) == dungeon::RequestResult::accepted);
    const dungeon::PendingSave* const pending =
        runtime.session()->pending_save_view();
    ARPG_REQUIRE(pending != nullptr);
    const dungeon::DungeonRunState expected = pending->next_state;

    const std::uint64_t direct_before = arpg::test::allocation_count();
    const persistence::SaveCommitResult direct = direct_store.commit(expected);
    const std::uint64_t direct_allocations =
        arpg::test::allocation_count() - direct_before;
    ARPG_REQUIRE(direct.state == persistence::SaveCommitState::committed);

    const std::uint64_t runtime_before = arpg::test::allocation_count();
    runtime.service_pending_save();
    const std::uint64_t runtime_allocations =
        arpg::test::allocation_count() - runtime_before;
    ARPG_REQUIRE(runtime.state() == platform::DungeonRuntimeState::running);
    ARPG_REQUIRE(runtime.render_status().indicator == platform::SaveIndicator::saved);
    ARPG_REQUIRE(runtime_allocations == direct_allocations);
    ARPG_REQUIRE(same_ownership(*runtime.item_state(), expected.item_ownership));
    return {};
}

arpg::test::Failure item_request_fault_matrix_is_atomic_and_restart_consistent() noexcept {
    constexpr std::array<ItemRequestKind, 4> kKinds{{
        ItemRequestKind::pickup,
        ItemRequestKind::equip,
        ItemRequestKind::unequip,
        ItemRequestKind::recipe,
    }};
    constexpr std::array<persistence::SaveCommitState, 3> kDispositions{{
        persistence::SaveCommitState::committed,
        persistence::SaveCommitState::not_committed,
        persistence::SaveCommitState::indeterminate,
    }};
    for (const ItemRequestKind kind : kKinds) {
        for (const persistence::SaveCommitState disposition : kDispositions) {
            TempDirectory directory;
            FaultContext fault{
                disposition == persistence::SaveCommitState::not_committed
                    ? persistence::SaveFaultPoint::before_publish
                    : persistence::SaveFaultPoint::after_publish,
                false,
            };
            auto config = config_for(directory);
            config.save.fault_hook = &fail_when_enabled;
            config.save.fault_context = &fault;
            persistence::SaveStore seed_store(config.save);
            const auto initial = item_state_for(kind);
            ARPG_REQUIRE(seed_store.commit(initial).state
                == persistence::SaveCommitState::committed);

            platform::DungeonRuntime runtime(config);
            ARPG_REQUIRE(runtime.initialize());
            ARPG_REQUIRE(install_pickup_if_needed(runtime, kind));
            const auto before_snapshot = runtime.session()->snapshot();
            const items::ItemOwnershipState before_items = *runtime.item_state();
            const auto before_build = arpg::test::player_build(*runtime.session());
            ARPG_REQUIRE(issue_item_request(runtime, kind)
                == dungeon::RequestResult::accepted);
            const dungeon::PendingSave* const pending =
                runtime.session()->pending_save_view();
            ARPG_REQUIRE(pending != nullptr);
            const auto expected_generation = pending->expected_generation;
            const items::ItemOwnershipState expected_items =
                pending->next_state.item_ownership;
            dungeon::DungeonSession expected_session{{}, pending->next_state};
            const auto expected_build = arpg::test::player_build(expected_session);

            ARPG_REQUIRE(runtime.request_pickup(0U)
                == dungeon::RequestResult::rejected);
            ARPG_REQUIRE(runtime.request_equip(101U)
                == dungeon::RequestResult::rejected);
            ARPG_REQUIRE(runtime.request_unequip(items::ItemSlot::weapon)
                == dungeon::RequestResult::rejected);
            ARPG_REQUIRE(runtime.request_recipe({{301U, 302U, 303U}})
                == dungeon::RequestResult::rejected);
            ARPG_REQUIRE(!runtime.session()->request_descent(true));
            arpg::test::attempt_exit(
                *runtime.session(), dungeon::ExitDirection::right);
            runtime.session()->reset_current_room();
            ARPG_REQUIRE(!runtime.session()->queue_action(combat::Action::light));
            ARPG_REQUIRE(runtime.session()->snapshot().phase
                == dungeon::RoomPhase::committing);
            ARPG_REQUIRE(runtime.session()->snapshot().commit_generation
                == before_snapshot.commit_generation);
            ARPG_REQUIRE(runtime.session()->snapshot().ground_item_count
                == before_snapshot.ground_item_count);
            ARPG_REQUIRE(same_ownership(*runtime.item_state(), before_items));

            fault.enabled = disposition != persistence::SaveCommitState::committed;
            runtime.service_pending_save();
            const auto after_snapshot = runtime.session()->snapshot();
            const bool committed =
                disposition == persistence::SaveCommitState::committed;
            const bool indeterminate =
                disposition == persistence::SaveCommitState::indeterminate;
            ARPG_REQUIRE(runtime.render_status().indicator
                == (committed ? platform::SaveIndicator::saved
                              : platform::SaveIndicator::error));
            ARPG_REQUIRE(runtime.state()
                == (indeterminate ? platform::DungeonRuntimeState::faulted
                                  : platform::DungeonRuntimeState::running));
            ARPG_REQUIRE(after_snapshot.phase
                == (indeterminate ? dungeon::RoomPhase::faulted
                                  : dungeon::RoomPhase::locked));
            ARPG_REQUIRE(after_snapshot.commit_generation
                == (committed ? expected_generation
                              : before_snapshot.commit_generation));
            ARPG_REQUIRE(same_ownership(*runtime.item_state(),
                committed ? expected_items : before_items));
            ARPG_REQUIRE(after_snapshot.ground_item_count
                == (committed && kind == ItemRequestKind::pickup
                    ? 0U : before_snapshot.ground_item_count));
            ARPG_REQUIRE(same_build(
                arpg::test::player_build(*runtime.session()),
                committed ? expected_build : before_build));
            if (indeterminate) {
                ARPG_REQUIRE(issue_item_request(runtime, kind)
                    == dungeon::RequestResult::rejected);
            }

            auto restart_config = config_for(directory, 999U);
            persistence::SaveStore disk_store(restart_config.save);
            const auto disk = disk_store.load();
            ARPG_REQUIRE(disk.state == persistence::SaveLoadState::ready);
            platform::DungeonRuntime restarted(restart_config);
            ARPG_REQUIRE(restarted.initialize());
            ARPG_REQUIRE(restarted.session()->snapshot().commit_generation
                == disk.checkpoint.commit_generation);
            ARPG_REQUIRE(same_ownership(
                *restarted.item_state(), disk.checkpoint.item_ownership));
            ARPG_REQUIRE(same_build(arpg::test::player_build(*restarted.session()),
                arpg::test::player_build(dungeon::DungeonSession{
                    {}, disk.checkpoint})));
            if (committed) {
                ARPG_REQUIRE(same_ownership(*restarted.item_state(), expected_items));
            } else if (!indeterminate) {
                ARPG_REQUIRE(same_ownership(*restarted.item_state(), before_items));
            } else {
                ARPG_REQUIRE(same_ownership(*restarted.item_state(), before_items)
                    || same_ownership(*restarted.item_state(), expected_items));
            }
        }
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"empty directory commits seeded generation one before running", &empty_directory_commits_seeded_generation_one_before_running},
    {"valid save ignores new run seed override", &valid_save_ignores_new_run_seed_override},
    {"committed pending transition maps verified state and saved indicator", &committed_pending_transition_maps_verified_state_and_saved_indicator},
    {"committed passive save survives runtime restart", &committed_passive_save_survives_runtime_restart},
    {"committed route and refund survive runtime restart", &committed_route_and_refund_survive_runtime_restart},
    {"passive pre publish failure keeps old tree and retryable runtime", &passive_pre_publish_failure_keeps_old_tree_and_retryable_runtime},
    {"indeterminate passive save faults runtime", &indeterminate_passive_save_faults_runtime},
    {"passive pending rejects door and descent requests", &passive_pending_rejects_door_and_descent_requests},
    {"pre publish not committed maps to retryable error", &pre_publish_not_committed_maps_to_retryable_error},
    {"indeterminate maps to faulted runtime and blocks selection", &indeterminate_maps_to_faulted_runtime_and_blocks_selection},
    {"dual slot corruption requires recovery then archives new run", &dual_slot_corruption_requires_recovery_then_archives_new_run},
    {"single slot corruption recovers and subsequent saves alternate", &single_slot_corruption_recovers_and_subsequent_saves_alternate},
    {"invalid rules fault without creating save or session", &invalid_rules_fault_without_creating_save_or_session},
    {"runtime exposes narrow item requests and stable item view", &runtime_exposes_narrow_item_requests_and_stable_item_view},
    {"large inventory snapshot and views do not allocate", &large_inventory_snapshot_and_views_do_not_allocate},
    {"generic service adds no large state copies", &generic_service_adds_no_large_state_copies},
    {"item request fault matrix is atomic and restart consistent", &item_request_fault_matrix_is_atomic_and_restart_consistent},
};

}  // namespace

arpg::test::TestSuite dungeon_runtime_suite() noexcept {
    return arpg::test::make_suite("dungeon_runtime", kCases);
}

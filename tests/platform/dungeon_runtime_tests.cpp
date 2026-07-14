#include "test_framework.hpp"

#include "../dungeon/dungeon_test_support.hpp"
#include "dungeon_runtime.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <string>

namespace {

namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;
namespace platform = arpg::platform;
namespace combat = arpg::combat;

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

constexpr arpg::test::TestCase kCases[] = {
    {"empty directory commits seeded generation one before running", &empty_directory_commits_seeded_generation_one_before_running},
    {"valid save ignores new run seed override", &valid_save_ignores_new_run_seed_override},
    {"committed pending transition maps verified state and saved indicator", &committed_pending_transition_maps_verified_state_and_saved_indicator},
    {"pre publish not committed maps to retryable error", &pre_publish_not_committed_maps_to_retryable_error},
    {"indeterminate maps to faulted runtime and blocks selection", &indeterminate_maps_to_faulted_runtime_and_blocks_selection},
    {"dual slot corruption requires recovery then archives new run", &dual_slot_corruption_requires_recovery_then_archives_new_run},
    {"single slot corruption recovers and subsequent saves alternate", &single_slot_corruption_recovers_and_subsequent_saves_alternate},
    {"invalid rules fault without creating save or session", &invalid_rules_fault_without_creating_save_or_session},
};

}  // namespace

arpg::test::TestSuite dungeon_runtime_suite() noexcept {
    return arpg::test::make_suite("dungeon_runtime", kCases);
}

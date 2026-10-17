#include "abyss/abyss_rules.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon/room_generation.hpp"
#include "persistence/save_store.hpp"

#include <array>
#include <filesystem>
#include <iostream>
#include <optional>

namespace {

namespace dungeon = arpg::dungeon;
namespace persistence = arpg::persistence;

struct PathResult final {
    dungeon::checkpoint::DeathCheckpoint death{};
    dungeon::checkpoint::DungeonRunState continued{};
};

std::filesystem::path fresh_directory(const std::filesystem::path& root,
    const char* name) {
    const auto directory = root / name;
    std::error_code error;
    std::filesystem::remove_all(directory, error);
    if (error) return {};
    std::filesystem::create_directories(directory, error);
    return error ? std::filesystem::path{} : directory;
}

bool resolve_with_store(dungeon::DungeonSession& session,
    persistence::SaveStore& store) {
    const dungeon::PendingSave* const pending = session.pending_save_view();
    if (pending == nullptr) return false;
    const auto kind = pending->kind;
    auto saved = store.commit(pending->next_state);
    dungeon::SaveDisposition disposition = dungeon::SaveDisposition::indeterminate;
    if (saved.state == persistence::SaveCommitState::committed) {
        disposition = dungeon::SaveDisposition::committed;
    } else if (saved.state == persistence::SaveCommitState::not_committed) {
        disposition = dungeon::SaveDisposition::not_committed;
    }
    session.resolve_pending_save({disposition,
        saved.verified_state.commit_generation,
        std::move(saved.verified_state), kind});
    return disposition == dungeon::SaveDisposition::committed
        && session.snapshot().phase != dungeon::RoomPhase::faulted;
}

std::optional<dungeon::checkpoint::DungeonRunState> load(
    persistence::SaveStore& store) {
    auto loaded = store.load();
    if (loaded.state != persistence::SaveLoadState::ready) return std::nullopt;
    return std::move(loaded.checkpoint);
}

bool wait_for_real_death(dungeon::DungeonSession& session) {
    for (int tick = 0; tick < 30000; ++tick) {
        session.tick({});
        while (session.try_pop_combat_event().has_value()) {}
        while (session.try_pop_event().has_value()) {}
        const auto* pending = session.pending_save_view();
        if (pending != nullptr) {
            return pending->kind == dungeon::PendingSaveKind::death_retreat
                && pending->death_snapshot.has_value()
                && pending->next_state.death.lifecycle
                    == dungeon::checkpoint::DeathLifecycle::pending_continue;
        }
        if (session.snapshot().phase == dungeon::RoomPhase::faulted) return false;
    }
    return false;
}

std::optional<PathResult> exercise_path(const std::filesystem::path& directory,
    dungeon::checkpoint::DungeonRunState start) {
    persistence::SaveStore store({directory});
    const auto seeded = store.commit(start);
    if (seeded.state != persistence::SaveCommitState::committed) return std::nullopt;
    dungeon::DungeonSession session{{}, seeded.verified_state};
    if (session.pending_save_view() != nullptr && !resolve_with_store(session, store)) {
        return std::nullopt;
    }
    if (!wait_for_real_death(session)) return std::nullopt;
    const auto* death_pending = session.pending_save_view();
    if (death_pending == nullptr) return std::nullopt;
    const auto death = death_pending->next_state.death;
    if (!resolve_with_store(session, store)) return std::nullopt;

    auto pending_load = load(store);
    if (!pending_load.has_value()
            || pending_load->death.lifecycle
                != dungeon::checkpoint::DeathLifecycle::pending_continue) {
        return std::nullopt;
    }
    dungeon::DungeonSession restarted{{}, *pending_load};
    const auto recap = restarted.snapshot();
    if (!recap.death.has_value() || !recap.death->can_continue
            || recap.death->checkpoint.target_room.seed != death.target_room.seed
            || restarted.request_death_continue()
                != dungeon::RequestResult::accepted
            || !resolve_with_store(restarted, store)) {
        return std::nullopt;
    }
    auto continued = load(store);
    if (!continued.has_value()
            || continued->death.lifecycle
                != dungeon::checkpoint::DeathLifecycle::none) {
        return std::nullopt;
    }
    return PathResult{death, std::move(*continued)};
}

std::optional<dungeon::checkpoint::DungeonRunState> abyss_start_state() {
    constexpr std::array<dungeon::ExitDirection, 4> directions{{
        dungeon::ExitDirection::up, dungeon::ExitDirection::down,
        dungeon::ExitDirection::left, dungeon::ExitDirection::right}};
    for (std::uint64_t seed = 1U; seed < 100000U; ++seed) {
        const auto initial = dungeon::make_initial_run_state(seed, {});
        if (initial.fault != dungeon::DungeonFault::none) continue;
        const auto doors = dungeon::preview_abyss_doors(initial.state.current_room);
        for (std::size_t index = 0; index < doors.size(); ++index) {
            if (!doors[index]) continue;
            const auto target = dungeon::make_door_transition(
                initial.state, directions[index], {});
            if (target.fault == dungeon::DungeonFault::none
                    && target.state.current_room.is_abyss) return target.state;
        }
    }
    return std::nullopt;
}

}  // namespace

int main(int argc, char** argv) {
    if (argc < 1 || argv == nullptr || argv[0] == nullptr) return 2;
    const auto root = std::filesystem::absolute(argv[0]).parent_path()
        / "stage11-death-fixture";
    const auto initial = dungeon::make_initial_run_state(0x11A001U, {});
    if (initial.fault != dungeon::DungeonFault::none) return 3;

    const auto normal = exercise_path(fresh_directory(root, "normal"), initial.state);
    if (!normal.has_value() || normal->death.target_room.depth != 1U
            || normal->continued.current_room.depth != 1U) return 4;

    const auto deep_state = dungeon::make_descent_transition(initial.state, {});
    if (deep_state.fault != dungeon::DungeonFault::none
            || deep_state.state.current_room.depth != 2U) return 5;
    const auto deep = exercise_path(fresh_directory(root, "deep"), deep_state.state);
    if (!deep.has_value() || deep->death.target_room.depth != 1U
            || deep->continued.current_room.depth != 1U) return 6;

    const auto abyss_state = abyss_start_state();
    if (!abyss_state.has_value()) return 7;
    const auto abyss = exercise_path(fresh_directory(root, "abyss"), *abyss_state);
    if (!abyss.has_value() || !abyss->death.death_was_abyss
            || !abyss->continued.last_abyss_resolution.valid) return 8;

    std::cout << "normal_death=PASS pending_restart=PASS deep_continue=PASS "
                 "floor_one_continue=PASS abyss_death=PASS\n";
    return 0;
}

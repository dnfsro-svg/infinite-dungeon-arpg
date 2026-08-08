#include "dungeon/dungeon_progression.hpp"
#include "dungeon/encounter_director.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstdio>

namespace {

constexpr std::uint64_t kOffset = 14695981039346656037ULL;
constexpr std::uint64_t kPrime = 1099511628211ULL;

void append(std::uint64_t& hash, std::uint64_t value) {
    for (std::size_t byte = 0; byte < sizeof(value); ++byte) {
        hash ^= value & 0xFFULL;
        hash *= kPrime;
        value >>= 8U;
    }
}

void print_entry(std::size_t index, const arpg::dungeon::checkpoint::DungeonRunState& state,
    const arpg::dungeon::RoomEncounterPlan& plan, std::uint64_t next_seed) {
    std::printf("entry=%zu depth=%llu ecology=%u monsters=", index,
        static_cast<unsigned long long>(state.current_room.depth),
        static_cast<unsigned>(state.current_room.ecology));
    for (std::size_t wave = 0; wave < plan.wave_count; ++wave) {
        for (std::size_t spawn = 0; spawn < plan.waves[wave].spawn_count; ++spawn) {
            std::printf("%u%s", static_cast<unsigned>(plan.waves[wave].spawns[spawn].id),
                wave + 1U == plan.wave_count && spawn + 1U == plan.waves[wave].spawn_count ? "" : ",");
        }
    }
    std::printf(" count=");
    unsigned count = 0U;
    for (std::size_t wave = 0; wave < plan.wave_count; ++wave) count += plan.waves[wave].spawn_count;
    std::printf("%u hole=%u abyss=%u next_seed=0x%llx\n", count,
        state.current_room.has_hole ? 1U : 0U, state.current_room.is_abyss ? 1U : 0U,
        static_cast<unsigned long long>(next_seed));
}

}  // namespace

int main() {
    using namespace arpg::dungeon;
    constexpr std::array<checkpoint::ExitDirection, 4> route{{
        checkpoint::ExitDirection::up, checkpoint::ExitDirection::right,
        checkpoint::ExitDirection::down, checkpoint::ExitDirection::left}};
    const DungeonRules rules{};
    auto built = make_initial_run_state(0x6d5a56da1234ULL, rules);
    if (built.fault != DungeonFault::none) return 1;
    auto state = built.state;
    std::uint64_t hash = kOffset;
    for (std::size_t index = 0; index < 256U; ++index) {
        const auto plan = build_encounter_plan(state.current_room.seed,
            state.current_room.depth, state.current_room.ecology, rules.encounter);
        const auto next = make_door_transition(state, route[index % route.size()], rules);
        if (plan.fault != DungeonFault::none || next.fault != DungeonFault::none) return 2;
        append(hash, state.current_room.depth);
        append(hash, static_cast<std::uint64_t>(state.current_room.ecology));
        unsigned count = 0U;
        for (std::size_t wave = 0; wave < plan.plan.wave_count; ++wave) {
            for (std::size_t spawn = 0; spawn < plan.plan.waves[wave].spawn_count; ++spawn) {
                append(hash, static_cast<std::uint64_t>(plan.plan.waves[wave].spawns[spawn].id));
                ++count;
            }
        }
        append(hash, count);
        append(hash, state.current_room.has_hole ? 1U : 0U);
        append(hash, state.current_room.is_abyss ? 1U : 0U);
        append(hash, next.state.current_room.seed);
        if (index == 0U || index == 127U || index == 255U) print_entry(index, state, plan.plan, next.state.current_room.seed);
        state = next.state;
    }
    std::printf("trace_hash=0x%llx\n", static_cast<unsigned long long>(hash));
    return 0;
}

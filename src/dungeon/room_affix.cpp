#include "dungeon/room_affix.hpp"

#include "core/deterministic_rng.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::dungeon {
namespace {

constexpr std::array<RoomDensityDefinition, 3> kDefinitions{{
    {RoomDensityAffix::crowded, 50U, 12U, 16U},
    {RoomDensityAffix::dense, 35U, 17U, 22U},
    {RoomDensityAffix::horde, 15U, 23U, 30U},
}};

constexpr std::uint64_t kAffixDomain = 0x524F4F4D5F414646ULL;
constexpr std::uint64_t kCountDomain = 0x524F4F4D5F434E54ULL;

[[nodiscard]] const RoomDensityDefinition& select_definition(
    std::uint64_t room_seed) noexcept {
    auto stream = core::DeterministicRng::derive_stream(room_seed, kAffixDomain);
    const std::uint64_t selection = stream.next_bounded(100U).value_or(0U);
    std::uint64_t upper_bound = 0U;
    for (const RoomDensityDefinition& definition : kDefinitions) {
        upper_bound += definition.weight;
        if (selection < upper_bound) return definition;
    }
    return kDefinitions.back();
}

}  // namespace

const RoomDensityDefinition* room_density_definition(
    RoomDensityAffix affix) noexcept {
    const std::size_t index = static_cast<std::size_t>(affix);
    if (index >= kDefinitions.size()) return nullptr;
    return &kDefinitions[index];
}

RoomDensityRoll roll_room_density(
    std::uint64_t room_seed,
    bool is_abyss) noexcept {
    const RoomDensityDefinition& definition = select_definition(room_seed);
    auto stream = core::DeterministicRng::derive_stream(room_seed, kCountDomain);
    const std::uint64_t span = static_cast<std::uint64_t>(
        definition.maximum - definition.minimum + 1U);
    const std::uint8_t base_count = static_cast<std::uint8_t>(
        definition.minimum + stream.next_bounded(span).value_or(0U));
    const std::uint8_t monster_count = is_abyss
        ? static_cast<std::uint8_t>((base_count * 3U + 1U) / 2U)
        : base_count;
    return {definition.id, base_count, monster_count};
}

}  // namespace arpg::dungeon

#include "dungeon/room_affix.hpp"

#include "core/deterministic_rng.hpp"
#include "core/gameplay_limits.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::dungeon {
namespace {

constexpr std::array<RoomDensityDefinition, 3U> kDefinitions{{
    {RoomDensityAffix::crowded, 50U, 300U, 400U},
    {RoomDensityAffix::dense, 35U, 425U, 550U},
    {RoomDensityAffix::horde, 15U, 575U, 750U},
}};

constexpr std::uint16_t kReferenceScale = 25U;
constexpr std::uint64_t kAffixDomain = 0x524F4F4D5F414646ULL;
constexpr std::uint64_t kCountDomain = 0x524F4F4D5F434E54ULL;

static_assert((750U * 3U + 1U) / 2U == 1125U);
static_assert(1125U <= limits::kRoomMonsterCapacity);

[[nodiscard]] const RoomDensityDefinition& select_definition(
    std::uint64_t room_seed) noexcept {
    auto stream = core::DeterministicRng::derive_stream(
        room_seed, kAffixDomain);
    const std::uint64_t selection =
        stream.next_bounded(100U).value_or(0U);
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
    return index < kDefinitions.size() ? &kDefinitions[index] : nullptr;
}

RoomDensityRoll roll_room_density(
    std::uint64_t room_seed,
    bool abyss) noexcept {
    const RoomDensityDefinition& definition = select_definition(room_seed);
    auto stream = core::DeterministicRng::derive_stream(
        room_seed, kCountDomain);
    const std::uint64_t reference_span =
        (definition.maximum - definition.minimum) / kReferenceScale + 1U;
    const std::uint16_t base_count = static_cast<std::uint16_t>(
        definition.minimum + stream.next_bounded(reference_span).value_or(0U)
            * kReferenceScale);
    const std::uint16_t total_count = abyss
        ? static_cast<std::uint16_t>(
            (static_cast<std::uint32_t>(base_count) * 3U + 1U) / 2U)
        : base_count;
    return {definition.id, base_count, total_count};
}

}  // namespace arpg::dungeon

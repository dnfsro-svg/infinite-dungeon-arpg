#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::modifiers {

using FixedValue = std::int64_t;
using ModifierId = std::uint32_t;
inline constexpr FixedValue kFixedOne = 10000;

enum class StatId : std::uint16_t {
    impulse_scale,
    shield,
    melee_damage,
    fire_flat_damage,
    water_flat_damage,
    lightning_flat_damage,
    chaos_flat_damage,
    fire_damage,
    water_damage,
    lightning_damage,
    chaos_damage,
    fire_resistance,
    water_resistance,
    lightning_resistance,
    chaos_resistance,
    max_health,
    max_health_more,
    max_barrier,
    damage_taken,
    move_speed,
    attack_speed,
    jump_speed,
    air_control,
    count,
};

enum class ModifierOperation : std::uint8_t {
    flat,
    increased,
    more,
    conversion,
};

enum class ModifierTag : std::uint8_t {
    light_target = 0,
    heavy_target = 1,
    hit = 2,
    hurt = 3,
};

constexpr std::uint64_t tag(ModifierTag value) noexcept {
    return std::uint64_t{1U} << static_cast<std::uint8_t>(value);
}

struct Modifier final {
    ModifierId id{};
    StatId stat{};
    ModifierOperation operation{};
    FixedValue value{};
    std::uint64_t required_tags{};
    std::uint64_t forbidden_tags{};
    std::uint64_t required_conditions{};
    std::uint16_t priority{};
    StatId conversion_target{};
};

struct ModifierContext final {
    std::uint64_t tags{};
    std::uint64_t conditions{};
};

struct ModifierSpan final {
    const Modifier* data{};
    std::size_t size{};

    constexpr ModifierSpan() noexcept = default;
    constexpr ModifierSpan(const Modifier* values, std::size_t count) noexcept
        : data(values), size(count) {}
    template <std::size_t N>
    constexpr ModifierSpan(const std::array<Modifier, N>& values) noexcept
        : data(values.data()), size(N) {}
};

struct StatBounds final {
    FixedValue minimum{};
    FixedValue maximum{};
};

struct StatEvaluation final {
    FixedValue value{};
    bool valid{};
    bool duplicate_id{};
};

using StatValues = std::array<FixedValue,
    static_cast<std::size_t>(StatId::count)>;

struct ConversionResult final {
    StatValues values{};
    bool valid{};
    FixedValue truncated_basis_points{};
};

}  // namespace arpg::modifiers

#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

enum class VisualEffectKind : std::uint8_t {
    spark,
    dust,
    damage_number,
    weapon_trail,
};

struct VisualEffect final {
    bool active{};
    VisualEffectKind kind{};
    combat::Vec3 position{};
    float age_seconds{};
    float lifetime_seconds{};
    int value{};
};

struct CameraOffset final {
    float x{};
    float y{};
};

class CombatFeedback final {
public:
    static constexpr std::size_t kCapacity = 256;

    void consume(const combat::CombatEvent& event) noexcept;
    void update(float frame_seconds) noexcept;
    void clear() noexcept;
    [[nodiscard]] bool try_spawn(const VisualEffect& effect) noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
    [[nodiscard]] std::uint32_t dropped_count() const noexcept;
    [[nodiscard]] float shake_amplitude() const noexcept;
    [[nodiscard]] CameraOffset camera_offset() const noexcept;
    [[nodiscard]] float target_flash_seconds(
        std::size_t target_index) const noexcept;
    [[nodiscard]] const std::array<VisualEffect, kCapacity>& effects()
        const noexcept;

private:
    std::array<VisualEffect, kCapacity> effects_{};
    std::array<float, combat::kDummyCount> flash_seconds_{};
    float shake_amplitude_{};
    float shake_time_{};
    float shake_phase_{};
    std::uint32_t dropped_count_{};
};

}  // namespace arpg::platform

#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::checkpoint {
struct PlayerDamageHistoryCheckpoint;
}

namespace arpg::combat {

inline constexpr std::size_t kPlayerDamageHistoryTicks = 300U;

struct PlayerDamageHistoryCheckpoint final {
    std::array<std::array<std::uint64_t, modifiers::kDamageTypeCount>,
        kPlayerDamageHistoryTicks> buckets{};
    std::uint64_t active_tick{};
    bool initialized{};
};

class PlayerDamageHistory final {
public:
    void begin_tick(std::uint64_t tick) noexcept;
    void record(const ResolvedPlayerDamage& damage) noexcept;
    [[nodiscard]] std::array<std::uint64_t,
        modifiers::kDamageTypeCount> totals() const noexcept;
    void capture_checkpoint(PlayerDamageHistoryCheckpoint& out) const noexcept;
    void capture_checkpoint(
        checkpoint::PlayerDamageHistoryCheckpoint& out) const noexcept;
    [[nodiscard]] bool restore_checkpoint(
        const PlayerDamageHistoryCheckpoint& checkpoint) noexcept;
    [[nodiscard]] bool restore_checkpoint(
        const checkpoint::PlayerDamageHistoryCheckpoint& checkpoint) noexcept;
    [[nodiscard]] std::uint64_t active_tick() const noexcept;
    [[nodiscard]] bool initialized() const noexcept;

private:
    std::array<std::array<std::uint64_t, modifiers::kDamageTypeCount>,
        kPlayerDamageHistoryTicks> buckets_{};
    std::uint64_t active_tick_{};
    bool initialized_{};
};

}  // namespace arpg::combat

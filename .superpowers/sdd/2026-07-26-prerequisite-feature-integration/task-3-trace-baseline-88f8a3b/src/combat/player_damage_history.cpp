#include "combat/player_damage_history.hpp"

#include <limits>

namespace arpg::combat {

namespace {

std::uint64_t saturating_add_u64(
    std::uint64_t left, std::uint64_t right) noexcept {
    const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
    return left > maximum - right ? maximum : left + right;
}

}  // namespace

void PlayerDamageHistory::begin_tick(std::uint64_t tick) noexcept {
    if (!initialized_) {
        initialized_ = true;
        active_tick_ = tick;
        buckets_[tick % kPlayerDamageHistoryTicks].fill(0U);
        return;
    }
    if (tick == active_tick_) return;
    if (tick < active_tick_ || tick - active_tick_ >= kPlayerDamageHistoryTicks) {
        for (auto& bucket : buckets_) bucket.fill(0U);
        active_tick_ = tick;
        return;
    }

    const std::uint64_t elapsed = tick - active_tick_;
    for (std::uint64_t offset = 1U; offset <= elapsed; ++offset) {
        buckets_[(active_tick_ + offset) % kPlayerDamageHistoryTicks].fill(0U);
    }
    active_tick_ = tick;
}

void PlayerDamageHistory::record(const ResolvedPlayerDamage& damage) noexcept {
    if (!initialized_) begin_tick(0U);
    auto& bucket = buckets_[active_tick_ % kPlayerDamageHistoryTicks];
    for (std::size_t index = 0; index < bucket.size(); ++index) {
        bucket[index] = saturating_add_u64(bucket[index], damage.by_type[index]);
    }
}

std::array<std::uint64_t, modifiers::kDamageTypeCount>
PlayerDamageHistory::totals() const noexcept {
    std::array<std::uint64_t, modifiers::kDamageTypeCount> result{};
    for (const auto& bucket : buckets_) {
        for (std::size_t index = 0; index < bucket.size(); ++index) {
            result[index] = saturating_add_u64(result[index], bucket[index]);
        }
    }
    return result;
}

}  // namespace arpg::combat

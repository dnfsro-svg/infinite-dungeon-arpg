#include "combat/player_damage_history.hpp"

#include "checkpoint/room_combat_checkpoint.hpp"

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

void PlayerDamageHistory::capture_checkpoint(
    checkpoint::PlayerDamageHistoryCheckpoint& out) const noexcept {
    static_assert(kPlayerDamageHistoryTicks
        == checkpoint::kPlayerDamageHistoryTicks);
    static_assert(modifiers::kDamageTypeCount
        == checkpoint::kDamageTypeCount);
    out.buckets = buckets_;
    out.active_tick = active_tick_;
    out.initialized = initialized_;
}

bool PlayerDamageHistory::restore_checkpoint(
    const checkpoint::PlayerDamageHistoryCheckpoint& checkpoint) noexcept {
    if (!checkpoint.initialized) {
        if (checkpoint.active_tick != 0U) return false;
        for (const auto& bucket : checkpoint.buckets) {
            for (const std::uint64_t value : bucket) {
                if (value != 0U) return false;
            }
        }
    } else {
        std::array<std::uint64_t, modifiers::kDamageTypeCount> totals{};
        const auto maximum = (std::numeric_limits<std::uint64_t>::max)();
        for (const auto& bucket : checkpoint.buckets) {
            for (std::size_t index = 0U; index < bucket.size(); ++index) {
                if (totals[index] > maximum - bucket[index]) return false;
                totals[index] += bucket[index];
            }
        }
    }
    buckets_ = checkpoint.buckets;
    active_tick_ = checkpoint.active_tick;
    initialized_ = checkpoint.initialized;
    return true;
}

std::uint64_t PlayerDamageHistory::active_tick() const noexcept {
    return active_tick_;
}

bool PlayerDamageHistory::initialized() const noexcept {
    return initialized_;
}

}  // namespace arpg::combat

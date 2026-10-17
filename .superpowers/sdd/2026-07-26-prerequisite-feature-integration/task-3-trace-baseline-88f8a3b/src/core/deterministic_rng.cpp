#include "core/deterministic_rng.hpp"

#include <cstdint>

namespace arpg::core {
namespace {

[[nodiscard]] std::uint64_t rotate_left(
    std::uint64_t value,
    unsigned int shift) noexcept {
    return (value << shift) | (value >> (64U - shift));
}

[[nodiscard]] std::uint64_t splitmix64_next(
    std::uint64_t& state) noexcept {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state;
    value =
        (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value =
        (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

}  // namespace

DeterministicRng::DeterministicRng(std::uint64_t seed) noexcept {
    std::uint64_t splitmix_state = seed;
    for (auto& value : state_) {
        value = splitmix64_next(splitmix_state);
    }

    if ((state_[0] | state_[1] | state_[2] | state_[3]) == 0) {
        state_[0] = 0x9E3779B97F4A7C15ULL;
    }
}

std::uint64_t DeterministicRng::next_u64() noexcept {
    const std::uint64_t result =
        rotate_left(state_[1] * 5ULL, 7U) * 9ULL;
    const std::uint64_t shifted = state_[1] << 17U;

    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= shifted;
    state_[3] = rotate_left(state_[3], 45U);
    return result;
}

std::optional<std::uint64_t> DeterministicRng::next_bounded(
    std::uint64_t bound) noexcept {
    if (bound == 0U) {
        return std::nullopt;
    }
    const std::uint64_t threshold =
        (std::uint64_t{0} - bound) % bound;
    for (;;) {
        const std::uint64_t value = next_u64();
        if (value >= threshold) {
            return value % bound;
        }
    }
}

DeterministicRng DeterministicRng::derive_stream(
    std::uint64_t root_seed,
    std::uint64_t stream_id) noexcept {
    std::uint64_t id_state = stream_id;
    const std::uint64_t mixed_id = splitmix64_next(id_state);
    std::uint64_t outer_state = root_seed ^ mixed_id;
    const std::uint64_t child_seed =
        splitmix64_next(outer_state);
    return DeterministicRng{child_seed};
}

}  // namespace arpg::core

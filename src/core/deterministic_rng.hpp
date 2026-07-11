#pragma once

#include <array>
#include <cstdint>

namespace arpg::core {

class DeterministicRng final {
public:
    explicit DeterministicRng(std::uint64_t seed) noexcept;

    [[nodiscard]] std::uint64_t next_u64() noexcept;

    [[nodiscard]] static DeterministicRng derive_stream(
        std::uint64_t root_seed,
        std::uint64_t stream_id) noexcept;

private:
    std::array<std::uint64_t, 4> state_{};
};

}  // namespace arpg::core

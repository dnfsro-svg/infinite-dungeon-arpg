#pragma once

#include <array>
#include <cstdint>
#include <optional>

namespace arpg::core {

class DeterministicRng final {
public:
    using State = std::array<std::uint64_t, 4>;

    explicit DeterministicRng(std::uint64_t seed) noexcept;

    [[nodiscard]] std::uint64_t next_u64() noexcept;

    [[nodiscard]] std::optional<std::uint64_t> next_bounded(
        std::uint64_t bound) noexcept;

    [[nodiscard]] static DeterministicRng derive_stream(
        std::uint64_t root_seed,
        std::uint64_t stream_id) noexcept;

    [[nodiscard]] State export_state() const noexcept;
    [[nodiscard]] bool import_state(const State& state) noexcept;

private:
    State state_{};
};

}  // namespace arpg::core

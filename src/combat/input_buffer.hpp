#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::combat {

enum class Action : std::uint8_t {
    light,
    jump,
    launcher,
};

class InputBuffer final {
public:
    static constexpr std::size_t kCapacity = 32;
    static constexpr std::uint8_t kLifetimeTicks = 8;

    [[nodiscard]] bool push(Action action) noexcept;
    [[nodiscard]] bool consume(Action action) noexcept;
    void age(bool paused) noexcept;
    void clear() noexcept;
    void reset_diagnostics() noexcept;

    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::uint32_t expired_count() const noexcept;
    [[nodiscard]] std::uint32_t overflow_count() const noexcept;

private:
    struct Entry final {
        Action action{};
        std::uint8_t remaining_ticks{};
    };

    std::array<Entry, kCapacity> entries_{};
    std::size_t size_{};
    std::uint32_t expired_count_{};
    std::uint32_t overflow_count_{};
};

}  // namespace arpg::combat

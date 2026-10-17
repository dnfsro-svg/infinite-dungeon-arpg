#pragma once

#include <cstdint>

namespace arpg::core {

struct FixedStepFrame final {
    std::uint32_t steps{};
    std::uint64_t total_ticks{};
    double interpolation_alpha{};
    double dropped_seconds{};
    std::uint64_t invalid_input_count{};
};

class FixedStepRunner final {
public:
    static constexpr double kStepSeconds = 1.0 / 60.0;
    static constexpr std::uint32_t kMaxStepsPerFrame = 8;

    [[nodiscard]] FixedStepFrame advance(double frame_seconds) noexcept;
    void clear_accumulator() noexcept;

private:
    double accumulator_seconds_{};
    double dropped_seconds_{};
    std::uint64_t total_ticks_{};
    std::uint64_t invalid_input_count_{};
};

}  // namespace arpg::core

#pragma once

#include "host_validation_stage10_11.hpp"
#include "host_validation_stage11b.hpp"
#include "host_validation_stage11c.hpp"
#include "host_validation_stage11d.hpp"
#include "host_validation_stage17.hpp"

namespace arpg::platform {

class HostValidationRuntime;
struct PhysicalKeySnapshot;

struct HostValidationStates final {
    host_validation::Stage10ValidationState stage10{};
    host_validation::Stage11ValidationState stage11{};
    host_validation::Stage11BValidationState stage11b{};
    host_validation::Stage11CHudValidationState stage11c{};
    host_validation::Stage11DLootValidationState stage11d{};
    host_validation::Stage17SkillStonesValidationState stage17{};
};

// Temporary Task 7A transition seam. Task 7B/7C remove these direct state
// references as fixed-step and presentation ownership moves behind the facade.
struct HostValidationStateAccess final {
    [[nodiscard]] static host_validation::Stage10ValidationState& stage10(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static host_validation::Stage11ValidationState& stage11(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static host_validation::Stage11BValidationState& stage11b(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static host_validation::Stage11CHudValidationState& stage11c(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static host_validation::Stage11DLootValidationState& stage11d(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static host_validation::Stage17SkillStonesValidationState& stage17(
        HostValidationRuntime&) noexcept;
    [[nodiscard]] static const PhysicalKeySnapshot& death_input_snapshot(
        const HostValidationRuntime&) noexcept;
};

}  // namespace arpg::platform

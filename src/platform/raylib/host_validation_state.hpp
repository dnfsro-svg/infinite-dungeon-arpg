#pragma once

#include "host_validation_stage10_11.hpp"
#include "host_validation_stage11b.hpp"
#include "host_validation_stage11c.hpp"
#include "host_validation_stage11d.hpp"
#include "host_validation_stage17.hpp"

namespace arpg::platform {

struct HostValidationStates final {
    host_validation::Stage10ValidationState stage10{};
    host_validation::Stage11ValidationState stage11{};
    host_validation::Stage11BValidationState stage11b{};
    host_validation::Stage11CHudValidationState stage11c{};
    host_validation::Stage11DLootValidationState stage11d{};
    host_validation::Stage17SkillStonesValidationState stage17{};
};

}  // namespace arpg::platform

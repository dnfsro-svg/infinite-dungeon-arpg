#pragma once

#include "combat/combat_types.hpp"

namespace arpg::combat {

[[nodiscard]] const MonsterDefinition* monster_definition(
    MonsterId id) noexcept;
[[nodiscard]] bool monster_catalog_valid() noexcept;

}  // namespace arpg::combat

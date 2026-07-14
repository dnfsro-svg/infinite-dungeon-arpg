#pragma once

#include "passives/passive_tree_types.hpp"

#include <array>

namespace arpg::passives {

[[nodiscard]] const std::array<PassiveNode, kPassiveNodeCount>&
passive_nodes() noexcept;
[[nodiscard]] bool catalog_is_valid() noexcept;

}  // namespace arpg::passives

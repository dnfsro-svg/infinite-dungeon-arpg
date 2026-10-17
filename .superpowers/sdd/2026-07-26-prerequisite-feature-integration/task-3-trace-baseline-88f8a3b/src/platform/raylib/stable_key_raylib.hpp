#pragma once

#include "platform/settings/settings_types.hpp"

#include <optional>

namespace arpg::platform {

[[nodiscard]] int raylib_key(settings::StableKey key) noexcept;
[[nodiscard]] std::optional<settings::StableKey> stable_key_from_raylib(
    int key) noexcept;
[[nodiscard]] const char* stable_key_label(settings::StableKey key) noexcept;

}  // namespace arpg::platform

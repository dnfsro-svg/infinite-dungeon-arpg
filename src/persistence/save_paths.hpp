#pragma once

#include <cstdint>
#include <filesystem>
#include <optional>

namespace arpg::persistence {

[[nodiscard]] std::optional<std::filesystem::path>
default_save_directory() noexcept;

[[nodiscard]] std::optional<std::uint64_t>
system_root_seed() noexcept;

}  // namespace arpg::persistence

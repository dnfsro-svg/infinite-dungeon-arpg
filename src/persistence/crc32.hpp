#pragma once

#include <cstddef>
#include <cstdint>

namespace arpg::persistence {

[[nodiscard]] std::uint32_t crc32(
    const std::uint8_t* bytes, std::size_t size) noexcept;

}  // namespace arpg::persistence

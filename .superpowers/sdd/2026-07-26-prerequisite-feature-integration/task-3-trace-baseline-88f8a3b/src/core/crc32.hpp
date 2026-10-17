#pragma once

#include <cstddef>
#include <cstdint>

namespace arpg::core {

[[nodiscard]] std::uint32_t crc32(
    const std::uint8_t* bytes, std::size_t size) noexcept;

[[nodiscard]] std::uint32_t crc32_update(
    std::uint32_t state,
    const std::uint8_t* bytes,
    std::size_t size) noexcept;

}  // namespace arpg::core

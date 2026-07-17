#pragma once

#include "core/crc32.hpp"

namespace arpg::persistence {

[[nodiscard]] inline std::uint32_t crc32(
    const std::uint8_t* bytes, std::size_t size) noexcept {
    return core::crc32(bytes, size);
}

[[nodiscard]] inline std::uint32_t crc32_update(
    std::uint32_t state,
    const std::uint8_t* bytes,
    std::size_t size) noexcept {
    return core::crc32_update(state, bytes, size);
}

}  // namespace arpg::persistence

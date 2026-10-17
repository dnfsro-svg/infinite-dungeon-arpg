#include "core/crc32.hpp"

namespace arpg::core {

std::uint32_t crc32_update(std::uint32_t state,
    const std::uint8_t* bytes, std::size_t size) noexcept {
    if (bytes == nullptr || size == 0U)
        return state;
    std::uint32_t crc = state ^ 0xFFFFFFFFU;

    for (std::size_t index = 0; index < size; ++index) {
        crc ^= bytes[index];
        for (int bit = 0; bit < 8; ++bit) {
            const std::uint32_t mask =
                0U - static_cast<std::uint32_t>(crc & 1U);
            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
        }
    }
    return crc ^ 0xFFFFFFFFU;
}

std::uint32_t crc32(
    const std::uint8_t* bytes, std::size_t size) noexcept {
    return crc32_update(0U, bytes, size);
}

}  // namespace arpg::core

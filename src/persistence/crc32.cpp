#include "persistence/crc32.hpp"

namespace arpg::persistence {

std::uint32_t crc32(
    const std::uint8_t* bytes, std::size_t size) noexcept {
    std::uint32_t crc = 0xFFFFFFFFU;
    if (bytes == nullptr) {
        return crc ^ 0xFFFFFFFFU;
    }

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

}  // namespace arpg::persistence

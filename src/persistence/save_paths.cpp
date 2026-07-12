#include "persistence/save_paths.hpp"

#include <cstdlib>
#include <random>

namespace arpg::persistence {

std::optional<std::filesystem::path> default_save_directory() noexcept {
    try {
#if defined(_MSC_VER)
        char* owned_local_app_data = nullptr;
        std::size_t owned_size = 0U;
        if (_dupenv_s(&owned_local_app_data, &owned_size, "LOCALAPPDATA") != 0) {
            return std::nullopt;
        }
        const char* local_app_data = owned_local_app_data;
#else
        const char* local_app_data = std::getenv("LOCALAPPDATA");
#endif
        if (local_app_data == nullptr || *local_app_data == '\0') {
#if defined(_MSC_VER)
            std::free(owned_local_app_data);
#endif
            return std::nullopt;
        }
#if defined(_MSC_VER)
        const auto result = std::filesystem::path(local_app_data)
            / "InfiniteDungeon" / "save";
        std::free(owned_local_app_data);
        return result;
#else
        return std::filesystem::path(local_app_data)
            / "InfiniteDungeon" / "save";
#endif
    } catch (...) {
        return std::nullopt;
    }
}

std::optional<std::uint64_t> system_root_seed() noexcept {
    try {
        std::random_device device;
        const auto high = static_cast<std::uint64_t>(device());
        const auto low = static_cast<std::uint64_t>(device());
        return (high << 32U) | low;
    } catch (...) {
        return std::nullopt;
    }
}

}  // namespace arpg::persistence

#include "host_launch_options.hpp"

#include <charconv>
#include <cstring>
#include <system_error>

namespace arpg::platform {
namespace {

bool equals(const char* value, const char* expected) noexcept {
    return value != nullptr && std::strcmp(value, expected) == 0;
}

bool is_option(const char* value) noexcept {
    return value != nullptr && value[0] == '-' && value[1] == '-';
}

bool parse_seed(const char* value, std::uint64_t& seed) noexcept {
    if (value == nullptr || value[0] == '\0' || value[0] == '-'
        || value[0] == '+') {
        return false;
    }

    int base = 10;
    const char* first = value;
    if (value[0] == '0' && (value[1] == 'x' || value[1] == 'X')) {
        base = 16;
        first += 2;
    }
    if (*first == '\0') {
        return false;
    }

    const char* const last = value + std::strlen(value);
    const auto parsed = std::from_chars(first, last, seed, base);
    return parsed.ec == std::errc{} && parsed.ptr == last;
}

HostArgumentResult error_result(HostArgumentError error) noexcept {
    return HostArgumentResult{error, {}};
}

}  // namespace

HostArgumentResult parse_host_arguments(
    int argc,
    const char* const* argv) noexcept {
    if (argc <= 1) {
        return {};
    }
    if (argv == nullptr) {
        return error_result(HostArgumentError::missing_value);
    }

    HostArgumentResult result{};
    bool seed_seen = false;
    bool save_directory_seen = false;
    bool settings_directory_seen = false;
    for (int index = 1; index < argc; ++index) {
        const char* const argument = argv[index];
        if (equals(argument, "--seed")) {
            if (seed_seen) {
                return error_result(HostArgumentError::duplicate_option);
            }
            seed_seen = true;
            if (index + 1 >= argc || argv[index + 1] == nullptr
                || is_option(argv[index + 1])) {
                return error_result(HostArgumentError::missing_value);
            }
            std::uint64_t seed{};
            if (!parse_seed(argv[++index], seed)) {
                return error_result(HostArgumentError::invalid_seed);
            }
            result.options.new_run_seed = seed;
            continue;
        }

        if (equals(argument, "--save-dir")) {
            if (save_directory_seen) {
                return error_result(HostArgumentError::duplicate_option);
            }
            save_directory_seen = true;
            if (index + 1 >= argc || argv[index + 1] == nullptr
                || is_option(argv[index + 1])) {
                return error_result(HostArgumentError::missing_value);
            }
            try {
                result.options.save_directory = std::filesystem::absolute(
                    std::filesystem::path{argv[++index]});
            } catch (const std::filesystem::filesystem_error&) {
                return error_result(HostArgumentError::invalid_seed);
            } catch (...) {
                return error_result(HostArgumentError::invalid_seed);
            }
            continue;
        }

        if (equals(argument, "--settings-dir")) {
            if (settings_directory_seen) {
                return error_result(HostArgumentError::duplicate_option);
            }
            settings_directory_seen = true;
            if (index + 1 >= argc || argv[index + 1] == nullptr
                || is_option(argv[index + 1])) {
                return error_result(HostArgumentError::missing_value);
            }
            try {
                result.options.settings_directory = std::filesystem::absolute(
                    std::filesystem::path{argv[++index]});
            } catch (...) {
                return error_result(HostArgumentError::invalid_seed);
            }
            continue;
        }

        return error_result(HostArgumentError::unknown_option);
    }
    return result;
}

}  // namespace arpg::platform

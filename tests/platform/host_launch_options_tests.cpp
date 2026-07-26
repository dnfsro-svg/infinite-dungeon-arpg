#include "test_framework.hpp"

#include "host_launch_options.hpp"
#include "raylib_host.hpp"

#include <cstdio>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <functional>
#include <string>
#include <utility>

namespace {

using arpg::platform::HostArgumentError;
using arpg::platform::HostArgumentResult;

HostArgumentResult parse(int argc, const char* const* argv) noexcept {
    return arpg::platform::parse_host_arguments(argc, argv);
}

struct CurrentDirectoryGuard final {
    std::filesystem::path original{};
    bool valid{};

    CurrentDirectoryGuard() noexcept {
        std::error_code error;
        original = std::filesystem::current_path(error);
        valid = !error;
    }

    ~CurrentDirectoryGuard() noexcept {
        if (!valid) {
            return;
        }
        std::error_code error;
        std::filesystem::current_path(original, error);
    }
};

struct TemporaryDirectory final {
    std::filesystem::path path{};
    bool valid{};
    bool owns_path{};

    TemporaryDirectory() noexcept {
        std::error_code error;
        const std::filesystem::path temporary_root =
            std::filesystem::temp_directory_path(error);
        if (error) {
            return;
        }
        path = temporary_root / ("arpg_host_launch_options_tests_"
            + std::to_string(static_cast<unsigned long long>(
                std::hash<std::string>{}(std::to_string(
                    reinterpret_cast<std::uintptr_t>(this))))));
        owns_path = std::filesystem::create_directories(path, error);
        valid = !error && owns_path;
    }

    explicit TemporaryDirectory(const std::filesystem::path& requested_path)
        noexcept : path(requested_path) {
        std::error_code error;
        owns_path = std::filesystem::create_directories(path, error);
        valid = !error && owns_path;
    }

    ~TemporaryDirectory() noexcept {
        if (!owns_path) {
            return;
        }
        std::error_code error;
        std::filesystem::remove_all(path, error);
    }
};

arpg::test::Failure no_arguments_leave_options_empty() noexcept {
    const char* const argv[] = {"arpg"};
    const HostArgumentResult result = parse(1, argv);
    ARPG_REQUIRE(result.error == HostArgumentError::none);
    ARPG_REQUIRE(!result.options.save_directory.has_value());
    ARPG_REQUIRE(!result.options.settings_directory.has_value());
    ARPG_REQUIRE(!result.options.screenshot_directory.has_value());
    ARPG_REQUIRE(!result.options.new_run_seed.has_value());
    return {};
}

arpg::test::Failure production_config_maps_options_and_continues_death()
    noexcept {
    arpg::platform::HostLaunchOptions options{};
    options.save_directory = "save";
    options.settings_directory = "settings";
    options.screenshot_directory = "screenshots";
    options.new_run_seed = 42U;

    const auto config = arpg::platform::make_production_host_config(
        std::move(options));
    ARPG_REQUIRE(config.save_directory == std::filesystem::path{"save"});
    ARPG_REQUIRE(config.settings_directory
        == std::filesystem::path{"settings"});
    ARPG_REQUIRE(config.screenshot_directory
        == std::filesystem::path{"screenshots"});
    ARPG_REQUIRE(config.new_run_seed == 42U);
    ARPG_REQUIRE(config.continue_pending_death_on_launch);
    return {};
}

arpg::test::Failure screenshot_directory_is_absolute_and_distinct() noexcept {
    const char* const argv[] = {"arpg", "--screenshot-dir", "capture evidence"};
    const HostArgumentResult result = parse(3, argv);
    ARPG_REQUIRE(result.error == HostArgumentError::none);
    ARPG_REQUIRE(result.options.screenshot_directory.has_value());
    ARPG_REQUIRE(result.options.screenshot_directory->is_absolute());
    ARPG_REQUIRE(result.options.screenshot_directory->filename()
        == "capture evidence");
    return {};
}

arpg::test::Failure settings_directory_is_frozen_and_isolated_from_save()
    noexcept {
    const char* const argv[] = {
        "arpg", "--save-dir", "character saves",
        "--settings-dir", "settings only"};
    const HostArgumentResult result = parse(5, argv);
    ARPG_REQUIRE(result.error == HostArgumentError::none);
    ARPG_REQUIRE(result.options.save_directory.has_value());
    ARPG_REQUIRE(result.options.settings_directory.has_value());
    ARPG_REQUIRE(result.options.save_directory->is_absolute());
    ARPG_REQUIRE(result.options.settings_directory->is_absolute());
    ARPG_REQUIRE(*result.options.save_directory
        != *result.options.settings_directory);
    ARPG_REQUIRE(result.options.settings_directory->filename()
        == "settings only");
    return {};
}

arpg::test::Failure decimal_and_hex_seeds_parse_to_same_value() noexcept {
    const char* const decimal_argv[] = {"arpg", "--seed", "8"};
    const char* const hex_argv[] = {"arpg", "--seed", "0x0000000000000008"};
    const HostArgumentResult decimal = parse(3, decimal_argv);
    const HostArgumentResult hex = parse(3, hex_argv);
    ARPG_REQUIRE(decimal.error == HostArgumentError::none);
    ARPG_REQUIRE(hex.error == HostArgumentError::none);
    ARPG_REQUIRE(decimal.options.new_run_seed.has_value());
    ARPG_REQUIRE(hex.options.new_run_seed.has_value());
    ARPG_REQUIRE(*decimal.options.new_run_seed == 8U);
    ARPG_REQUIRE(*hex.options.new_run_seed == 8U);
    return {};
}

arpg::test::Failure save_directory_with_spaces_is_frozen_absolute() noexcept {
    std::error_code error;
    std::filesystem::path original_directory;
    {
        TemporaryDirectory alternate_directory;
        CurrentDirectoryGuard working_directory;
        ARPG_REQUIRE(working_directory.valid);
        ARPG_REQUIRE(alternate_directory.valid);
        original_directory = working_directory.original;
        const char* const argv[] = {"arpg", "--save-dir", "slot saves"};
        const HostArgumentResult result = parse(3, argv);
        ARPG_REQUIRE(result.error == HostArgumentError::none);
        ARPG_REQUIRE(result.options.save_directory.has_value());
        ARPG_REQUIRE(result.options.save_directory->is_absolute());
        ARPG_REQUIRE(result.options.save_directory->filename() == "slot saves");
        const auto frozen = *result.options.save_directory;

        std::filesystem::current_path(alternate_directory.path, error);
        ARPG_REQUIRE(!error);
        ARPG_REQUIRE(std::filesystem::current_path(error)
            == alternate_directory.path);
        ARPG_REQUIRE(!error);
        ARPG_REQUIRE(frozen == *result.options.save_directory);
        ARPG_REQUIRE(frozen == working_directory.original / "slot saves");
    }
    ARPG_REQUIRE(std::filesystem::current_path(error) == original_directory);
    ARPG_REQUIRE(!error);
    return {};
}

arpg::test::Failure failed_temporary_directory_never_removes_unowned_path()
    noexcept {
    TemporaryDirectory parent;
    ARPG_REQUIRE(parent.valid);
    const std::filesystem::path sentinel = parent.path / "unowned_sentinel";
    const std::string sentinel_text = sentinel.string();
    std::FILE* sentinel_file{};
    ARPG_REQUIRE(::fopen_s(&sentinel_file, sentinel_text.c_str(), "wb") == 0);
    ARPG_REQUIRE(sentinel_file != nullptr);
    ARPG_REQUIRE(std::fputs("sentinel", sentinel_file) >= 0);
    ARPG_REQUIRE(std::fclose(sentinel_file) == 0);
    std::error_code error;
    ARPG_REQUIRE(std::filesystem::is_regular_file(sentinel, error));
    ARPG_REQUIRE(!error);
    {
        TemporaryDirectory failed(sentinel);
        ARPG_REQUIRE(!failed.valid);
        ARPG_REQUIRE(std::filesystem::is_regular_file(sentinel, error));
        ARPG_REQUIRE(!error);
    }
    ARPG_REQUIRE(std::filesystem::is_regular_file(sentinel, error));
    ARPG_REQUIRE(!error);
    return {};
}

arpg::test::Failure duplicate_and_unknown_options_are_rejected() noexcept {
    const char* const duplicate_seed[] = {
        "arpg", "--seed", "1", "--seed", "2"};
    const char* const duplicate_directory[] = {
        "arpg", "--save-dir", "one", "--save-dir", "two"};
    const char* const unknown[] = {"arpg", "--seed=8"};
    const char* const duplicate_settings_directory[] = {
        "arpg", "--settings-dir", "one", "--settings-dir", "two"};
    const char* const duplicate_screenshot_directory[] = {
        "arpg", "--screenshot-dir", "one", "--screenshot-dir", "two"};
    ARPG_REQUIRE(parse(5, duplicate_seed).error
        == HostArgumentError::duplicate_option);
    ARPG_REQUIRE(parse(5, duplicate_directory).error
        == HostArgumentError::duplicate_option);
    ARPG_REQUIRE(parse(2, unknown).error
        == HostArgumentError::unknown_option);
    ARPG_REQUIRE(parse(5, duplicate_settings_directory).error
        == HostArgumentError::duplicate_option);
    ARPG_REQUIRE(parse(5, duplicate_screenshot_directory).error
        == HostArgumentError::duplicate_option);
    return {};
}

arpg::test::Failure missing_and_invalid_seed_values_are_rejected() noexcept {
    const char* const missing[] = {"arpg", "--seed"};
    const char* const null_directory[] = {"arpg", "--save-dir", nullptr};
    const char* const missing_screenshot_directory[] = {"arpg", "--screenshot-dir"};
    const char* const negative[] = {"arpg", "--seed", "-1"};
    const char* const overflow[] = {
        "arpg", "--seed", "18446744073709551616"};
    const char* const trailing[] = {"arpg", "--seed", "8x"};
    ARPG_REQUIRE(parse(2, missing).error == HostArgumentError::missing_value);
    ARPG_REQUIRE(parse(3, null_directory).error
        == HostArgumentError::missing_value);
    ARPG_REQUIRE(parse(2, missing_screenshot_directory).error
        == HostArgumentError::missing_value);
    ARPG_REQUIRE(parse(3, negative).error == HostArgumentError::invalid_seed);
    ARPG_REQUIRE(parse(3, overflow).error == HostArgumentError::invalid_seed);
    ARPG_REQUIRE(parse(3, trailing).error == HostArgumentError::invalid_seed);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"no arguments", &no_arguments_leave_options_empty},
    {"production config continues death",
        &production_config_maps_options_and_continues_death},
    {"decimal and hexadecimal seed", &decimal_and_hex_seeds_parse_to_same_value},
    {"absolute save directory", &save_directory_with_spaces_is_frozen_absolute},
    {"isolated settings directory", &settings_directory_is_frozen_and_isolated_from_save},
    {"isolated screenshot directory", &screenshot_directory_is_absolute_and_distinct},
    {"failed temporary directory cleanup", &failed_temporary_directory_never_removes_unowned_path},
    {"duplicate and unknown options", &duplicate_and_unknown_options_are_rejected},
    {"missing and invalid seed", &missing_and_invalid_seed_values_are_rejected},
};

}  // namespace

arpg::test::TestSuite host_launch_options_suite() noexcept {
    return arpg::test::make_suite("host_launch_options", kCases);
}

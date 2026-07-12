#include "test_framework.hpp"

#include "host_launch_options.hpp"

#include <cstring>
#include <filesystem>

namespace {

using arpg::platform::HostArgumentError;
using arpg::platform::HostArgumentResult;

HostArgumentResult parse(int argc, const char* const* argv) noexcept {
    return arpg::platform::parse_host_arguments(argc, argv);
}

arpg::test::Failure no_arguments_leave_options_empty() noexcept {
    const char* const argv[] = {"arpg"};
    const HostArgumentResult result = parse(1, argv);
    ARPG_REQUIRE(result.error == HostArgumentError::none);
    ARPG_REQUIRE(!result.options.save_directory.has_value());
    ARPG_REQUIRE(!result.options.new_run_seed.has_value());
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
    const char* const argv[] = {"arpg", "--save-dir", "slot saves"};
    const HostArgumentResult result = parse(3, argv);
    ARPG_REQUIRE(result.error == HostArgumentError::none);
    ARPG_REQUIRE(result.options.save_directory.has_value());
    ARPG_REQUIRE(result.options.save_directory->is_absolute());
    ARPG_REQUIRE(result.options.save_directory->filename() == "slot saves");
    return {};
}

arpg::test::Failure duplicate_and_unknown_options_are_rejected() noexcept {
    const char* const duplicate_seed[] = {
        "arpg", "--seed", "1", "--seed", "2"};
    const char* const duplicate_directory[] = {
        "arpg", "--save-dir", "one", "--save-dir", "two"};
    const char* const unknown[] = {"arpg", "--seed=8"};
    ARPG_REQUIRE(parse(5, duplicate_seed).error
        == HostArgumentError::duplicate_option);
    ARPG_REQUIRE(parse(5, duplicate_directory).error
        == HostArgumentError::duplicate_option);
    ARPG_REQUIRE(parse(2, unknown).error
        == HostArgumentError::unknown_option);
    return {};
}

arpg::test::Failure missing_and_invalid_seed_values_are_rejected() noexcept {
    const char* const missing[] = {"arpg", "--seed"};
    const char* const null_directory[] = {"arpg", "--save-dir", nullptr};
    const char* const negative[] = {"arpg", "--seed", "-1"};
    const char* const overflow[] = {
        "arpg", "--seed", "18446744073709551616"};
    const char* const trailing[] = {"arpg", "--seed", "8x"};
    ARPG_REQUIRE(parse(2, missing).error == HostArgumentError::missing_value);
    ARPG_REQUIRE(parse(3, null_directory).error
        == HostArgumentError::missing_value);
    ARPG_REQUIRE(parse(3, negative).error == HostArgumentError::invalid_seed);
    ARPG_REQUIRE(parse(3, overflow).error == HostArgumentError::invalid_seed);
    ARPG_REQUIRE(parse(3, trailing).error == HostArgumentError::invalid_seed);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"no arguments", &no_arguments_leave_options_empty},
    {"decimal and hexadecimal seed", &decimal_and_hex_seeds_parse_to_same_value},
    {"absolute save directory", &save_directory_with_spaces_is_frozen_absolute},
    {"duplicate and unknown options", &duplicate_and_unknown_options_are_rejected},
    {"missing and invalid seed", &missing_and_invalid_seed_values_are_rejected},
};

}  // namespace

arpg::test::TestSuite host_launch_options_suite() noexcept {
    return arpg::test::make_suite("host_launch_options", kCases);
}

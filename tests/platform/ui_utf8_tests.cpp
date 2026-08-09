#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "ui_utf8.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

namespace platform = arpg::platform;

arpg::test::Failure strict_validation_accepts_utf8_boundaries_and_rejects_invalid_forms()
    noexcept {
    ARPG_REQUIRE(platform::ui_utf8_is_valid("\x7F"));
    ARPG_REQUIRE(platform::ui_utf8_is_valid("\xC2\x80"));
    ARPG_REQUIRE(platform::ui_utf8_is_valid("\xE0\xA0\x80"));
    ARPG_REQUIRE(platform::ui_utf8_is_valid("\xF0\x90\x80\x80"));
    ARPG_REQUIRE(platform::ui_utf8_is_valid("\xF4\x8F\xBF\xBF"));

    ARPG_REQUIRE(!platform::ui_utf8_is_valid("\xC0\x80"));
    ARPG_REQUIRE(!platform::ui_utf8_is_valid("\xC1\xBF"));
    ARPG_REQUIRE(!platform::ui_utf8_is_valid("\xE0\x80\x80"));
    ARPG_REQUIRE(!platform::ui_utf8_is_valid("\xF0\x80\x80\x80"));
    ARPG_REQUIRE(!platform::ui_utf8_is_valid("\x80"));
    ARPG_REQUIRE(!platform::ui_utf8_is_valid("\x80\xBF"));
    ARPG_REQUIRE(!platform::ui_utf8_is_valid("\xED\xA0\x80"));
    ARPG_REQUIRE(!platform::ui_utf8_is_valid("\xF4\x90\x80\x80"));
    ARPG_REQUIRE(!platform::ui_utf8_is_valid("\xE8\xAE"));
    return {};
}

arpg::test::Failure copy_replaces_each_invalid_input_byte_without_losing_recoverable_ascii()
    noexcept {
    char buffer[8]{};
    const auto mixed = platform::ui_utf8_copy(
        buffer, sizeof(buffer), "\xE8\x28\xA1");
    ARPG_REQUIRE(std::strcmp(buffer, "?(?") == 0);
    ARPG_REQUIRE(mixed.bytes_written == 3U);
    ARPG_REQUIRE(!mixed.input_was_valid);
    ARPG_REQUIRE(!mixed.truncated);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(buffer));

    const auto incomplete = platform::ui_utf8_copy(
        buffer, sizeof(buffer), "\xE8\xAE");
    ARPG_REQUIRE(std::strcmp(buffer, "??") == 0);
    ARPG_REQUIRE(incomplete.bytes_written == 2U);
    ARPG_REQUIRE(!incomplete.input_was_valid);
    ARPG_REQUIRE(!incomplete.truncated);
    return {};
}

arpg::test::Failure copy_never_splits_one_to_four_byte_code_points_at_capacity_boundaries()
    noexcept {
    constexpr char kText[] = "A\xC2\xA2\xE4\xB8\xAD\xF0\x9F\x98\x80";
    char buffer[11]{};

    const auto one = platform::ui_utf8_copy(buffer, 2U, kText);
    ARPG_REQUIRE(std::strcmp(buffer, "A") == 0);
    ARPG_REQUIRE(one.bytes_written == 1U);
    ARPG_REQUIRE(one.truncated);

    const auto two = platform::ui_utf8_copy(buffer, 4U, kText);
    ARPG_REQUIRE(std::strcmp(buffer, "A\xC2\xA2") == 0);
    ARPG_REQUIRE(two.bytes_written == 3U);
    ARPG_REQUIRE(two.truncated);

    const auto three = platform::ui_utf8_copy(buffer, 7U, kText);
    ARPG_REQUIRE(std::strcmp(buffer, "A\xC2\xA2\xE4\xB8\xAD") == 0);
    ARPG_REQUIRE(three.bytes_written == 6U);
    ARPG_REQUIRE(three.truncated);

    const auto four = platform::ui_utf8_copy(buffer, sizeof(buffer), kText);
    ARPG_REQUIRE(std::strcmp(buffer, kText) == 0);
    ARPG_REQUIRE(four.bytes_written == 10U);
    ARPG_REQUIRE(!four.truncated);
    return {};
}

arpg::test::Failure ellipsis_preserves_chinese_utf8_or_returns_empty_when_it_cannot_fit()
    noexcept {
    char buffer[7]{};
    const auto result = platform::ui_utf8_fit_with_ellipsis(
        buffer, sizeof(buffer), u8"设置界面");
    ARPG_REQUIRE(result.truncated);
    ARPG_REQUIRE(result.input_was_valid);
    ARPG_REQUIRE(result.bytes_written == 6U);
    ARPG_REQUIRE(std::strcmp(buffer, u8"设…") == 0);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(buffer));
    ARPG_REQUIRE(buffer[sizeof(buffer) - 1U] == '\0');

    const auto too_small = platform::ui_utf8_fit_with_ellipsis(
        buffer, 3U, u8"设置界面");
    ARPG_REQUIRE(too_small.truncated);
    ARPG_REQUIRE(too_small.bytes_written == 0U);
    ARPG_REQUIRE(buffer[0] == '\0');
    return {};
}

arpg::test::Failure empty_or_nul_only_destinations_remain_safely_terminated()
    noexcept {
    char destination{'x'};
    const auto empty = platform::ui_utf8_copy(nullptr, 0U, u8"设置");
    ARPG_REQUIRE(empty.bytes_written == 0U);
    ARPG_REQUIRE(empty.input_was_valid);
    ARPG_REQUIRE(empty.truncated);

    const auto nul_only = platform::ui_utf8_copy(
        &destination, 1U, u8"设置");
    ARPG_REQUIRE(nul_only.bytes_written == 0U);
    ARPG_REQUIRE(nul_only.input_was_valid);
    ARPG_REQUIRE(nul_only.truncated);
    ARPG_REQUIRE(destination == '\0');

    const auto null_destination = platform::ui_utf8_format(
        nullptr, 1U, "%s", "A");
    ARPG_REQUIRE(null_destination.bytes_written == 0U);
    ARPG_REQUIRE(null_destination.input_was_valid);
    ARPG_REQUIRE(null_destination.truncated);
    return {};
}

arpg::test::Failure format_repairs_partial_tail_and_remains_allocation_free()
    noexcept {
    char buffer[8]{};
    const auto repaired = platform::ui_utf8_format(
        buffer, sizeof(buffer), "%s", u8"设置界面");
    ARPG_REQUIRE(std::strcmp(buffer, u8"设置") == 0);
    ARPG_REQUIRE(repaired.bytes_written == 6U);
    ARPG_REQUIRE(repaired.input_was_valid);
    ARPG_REQUIRE(repaired.truncated);
    ARPG_REQUIRE(platform::ui_utf8_is_valid(buffer));
    ARPG_REQUIRE(buffer[sizeof(buffer) - 1U] == '\0');

    static_cast<void>(platform::ui_utf8_format(
        buffer, sizeof(buffer), u8"第%u项", 1U));
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t index{}; index < 10000U; ++index) {
        const auto formatted = platform::ui_utf8_format(
            buffer, sizeof(buffer), u8"第%u项", static_cast<unsigned>(index));
        ARPG_REQUIRE(formatted.input_was_valid);
        ARPG_REQUIRE(platform::ui_utf8_is_valid(buffer));
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"strict UTF-8 validation boundaries",
        &strict_validation_accepts_utf8_boundaries_and_rejects_invalid_forms},
    {"invalid byte recovery", 
        &copy_replaces_each_invalid_input_byte_without_losing_recoverable_ascii},
    {"complete code point copy boundaries",
        &copy_never_splits_one_to_four_byte_code_points_at_capacity_boundaries},
    {"Chinese ellipsis truncation",
        &ellipsis_preserves_chinese_utf8_or_returns_empty_when_it_cannot_fit},
    {"empty and NUL-only destinations",
        &empty_or_nul_only_destinations_remain_safely_terminated},
    {"format repair and zero allocation",
        &format_repairs_partial_tail_and_remains_allocation_free},
};

}  // namespace

arpg::test::TestSuite ui_utf8_suite() noexcept {
    return arpg::test::make_suite("ui_utf8", kCases);
}

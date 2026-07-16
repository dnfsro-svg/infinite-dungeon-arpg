#include "test_framework.hpp"

#include "modifiers/player_modifier_values.hpp"

#include <array>
#include <cstdint>
#include <limits>

namespace {

using namespace arpg::modifiers;

static_assert(sizeof(FixedValue) == sizeof(std::int64_t));

constexpr StatBounds kWideBounds{-1000000000LL, 1000000000LL};

arpg::test::Failure empty_modifier_list_is_identity() noexcept {
    const StatEvaluation result = evaluate_stat(
        1000000, StatId::impulse_scale, {}, {}, kWideBounds);
    ARPG_REQUIRE(result.valid);
    ARPG_REQUIRE(result.value == 1000000);
    return {};
}

arpg::test::Failure operation_order_is_flat_increased_then_more() noexcept {
    const std::array<Modifier, 3> values{{
        {3U, StatId::impulse_scale, ModifierOperation::more, 12000},
        {1U, StatId::impulse_scale, ModifierOperation::flat, 200000},
        {2U, StatId::impulse_scale, ModifierOperation::increased, 5000},
    }};
    const StatEvaluation result = evaluate_stat(
        1000000, StatId::impulse_scale, values, {}, kWideBounds);
    ARPG_REQUIRE(result.valid);
    ARPG_REQUIRE(result.value == 2160000);

    return {};
}

arpg::test::Failure addition_overflow_is_invalid() noexcept {
    constexpr FixedValue kMaximum = (std::numeric_limits<FixedValue>::max)();
    const std::array<Modifier, 1> values{{
        {4U, StatId::impulse_scale, ModifierOperation::flat, 1},
    }};
    const StatBounds full_range{
        (std::numeric_limits<FixedValue>::min)(), kMaximum};
    const auto result = evaluate_stat(
        kMaximum, StatId::impulse_scale, values, {}, full_range);
    ARPG_REQUIRE(!result.valid);
    return {};
}

arpg::test::Failure player_modifier_capacity_is_256() noexcept {
    std::array<Modifier, 256> accepted{};
    for (std::size_t index = 0; index < accepted.size(); ++index) {
        accepted[index] = Modifier{static_cast<ModifierId>(index + 1U),
            StatId::impulse_scale, ModifierOperation::flat, 0};
    }
    const auto at_capacity = evaluate_stat(
        1, StatId::impulse_scale, accepted, {}, kWideBounds);
    ARPG_REQUIRE(at_capacity.valid);

    std::array<Modifier, 257> rejected{};
    for (std::size_t index = 0; index < rejected.size(); ++index) {
        rejected[index] = Modifier{static_cast<ModifierId>(index + 1U),
            StatId::impulse_scale, ModifierOperation::flat, 0};
    }
    const auto over_capacity = evaluate_stat(
        1, StatId::impulse_scale, rejected, {}, kWideBounds);
    ARPG_REQUIRE(!over_capacity.valid);
    return {};
}

arpg::test::Failure rating_curve_hits_all_anchors() noexcept {
    ARPG_REQUIRE(rating_to_basis_points(0) == 0);
    ARPG_REQUIRE(rating_to_basis_points(100) == 2000);
    ARPG_REQUIRE(rating_to_basis_points(1000) == 4000);
    ARPG_REQUIRE(rating_to_basis_points(10000) == 6000);
    ARPG_REQUIRE(rating_to_basis_points(100000) == 8000);
    ARPG_REQUIRE(rating_to_basis_points(1000000) == 9900);
    ARPG_REQUIRE(rating_to_basis_points(10000000) == 9990);
    return {};
}

arpg::test::Failure rating_curve_is_monotonic_within_each_segment() noexcept {
    constexpr std::array<std::int64_t, 7> ratings{{
        0, 100, 1000, 10000, 100000, 1000000, 10000000}};
    for (std::size_t index = 1; index < ratings.size(); ++index) {
        const std::int64_t first = ratings[index - 1U];
        const std::int64_t last = ratings[index];
        const std::int64_t middle = first + (last - first) / 2;
        ARPG_REQUIRE(rating_to_basis_points(first)
            <= rating_to_basis_points(middle));
        ARPG_REQUIRE(rating_to_basis_points(middle)
            <= rating_to_basis_points(last));
    }
    return {};
}

arpg::test::Failure rating_curve_handles_negative_and_tail_values() noexcept {
    ARPG_REQUIRE(rating_to_basis_points(-1) == 0);
    ARPG_REQUIRE(rating_to_basis_points(10000001) == 9990);
    ARPG_REQUIRE(rating_to_basis_points(
        (std::numeric_limits<std::int64_t>::max)()) == 9999);
    ARPG_REQUIRE(rating_to_basis_points(
        (std::numeric_limits<std::int64_t>::max)()) < 10000);
    return {};
}

arpg::test::Failure conditions_and_forbidden_tags_gate_modifiers() noexcept {
    Modifier light_more{1U, StatId::impulse_scale,
        ModifierOperation::more, 12500};
    light_more.required_tags = tag(ModifierTag::light_target);
    Modifier blocked{2U, StatId::impulse_scale,
        ModifierOperation::flat, 9990000};
    blocked.forbidden_tags = tag(ModifierTag::light_target);
    const std::array<Modifier, 2> values{{blocked, light_more}};
    ModifierContext context{};
    context.tags = tag(ModifierTag::light_target);
    const StatEvaluation result = evaluate_stat(
        1000000, StatId::impulse_scale, values, context, kWideBounds);
    ARPG_REQUIRE(result.valid);
    ARPG_REQUIRE(result.value == 1250000);
    return {};
}

arpg::test::Failure insertion_order_does_not_change_result() noexcept {
    const Modifier a{10U, StatId::impulse_scale,
        ModifierOperation::more, 11000};
    const Modifier b{5U, StatId::impulse_scale,
        ModifierOperation::more, 12000};
    const std::array<Modifier, 2> first{{a, b}};
    const std::array<Modifier, 2> second{{b, a}};
    const auto left = evaluate_stat(
        1000000, StatId::impulse_scale, first, {}, kWideBounds);
    const auto right = evaluate_stat(
        1000000, StatId::impulse_scale, second, {}, kWideBounds);
    ARPG_REQUIRE(left.valid && right.valid);
    ARPG_REQUIRE(left.value == right.value);
    ARPG_REQUIRE(left.value == 1320000);
    return {};
}

arpg::test::Failure duplicate_ids_are_rejected() noexcept {
    const std::array<Modifier, 2> values{{
        {7U, StatId::impulse_scale, ModifierOperation::flat, 1},
        {7U, StatId::impulse_scale, ModifierOperation::flat, 2},
    }};
    const auto result = evaluate_stat(
        1000000, StatId::impulse_scale, values, {}, kWideBounds);
    ARPG_REQUIRE(!result.valid);
    ARPG_REQUIRE(result.duplicate_id);
    return {};
}

arpg::test::Failure conversion_is_capped_at_one_hundred_percent() noexcept {
    Modifier first{1U, StatId::impulse_scale,
        ModifierOperation::conversion, 4000};
    first.conversion_target = StatId::shield;
    first.priority = 1U;
    Modifier second{2U, StatId::impulse_scale,
        ModifierOperation::conversion, 8000};
    second.conversion_target = StatId::shield;
    second.priority = 2U;
    const std::array<Modifier, 2> modifiers{{second, first}};
    StatValues values{};
    values[static_cast<std::size_t>(StatId::impulse_scale)] = 1000000;
    const auto result = evaluate_conversions(values, modifiers, {});
    ARPG_REQUIRE(result.valid);
    ARPG_REQUIRE(result.values[
        static_cast<std::size_t>(StatId::impulse_scale)] == 0);
    ARPG_REQUIRE(result.values[
        static_cast<std::size_t>(StatId::shield)] == 1000000);
    ARPG_REQUIRE(result.truncated_basis_points == 2000);
    return {};
}

arpg::test::Failure converted_value_does_not_convert_again() noexcept {
    Modifier to_shield{1U, StatId::impulse_scale,
        ModifierOperation::conversion, 10000};
    to_shield.conversion_target = StatId::shield;
    Modifier back{2U, StatId::shield,
        ModifierOperation::conversion, 10000};
    back.conversion_target = StatId::impulse_scale;
    const std::array<Modifier, 2> modifiers{{to_shield, back}};
    StatValues values{};
    values[static_cast<std::size_t>(StatId::impulse_scale)] = 1000000;
    const auto result = evaluate_conversions(values, modifiers, {});
    ARPG_REQUIRE(result.valid);
    ARPG_REQUIRE(result.values[
        static_cast<std::size_t>(StatId::impulse_scale)] == 0);
    ARPG_REQUIRE(result.values[
        static_cast<std::size_t>(StatId::shield)] == 1000000);
    return {};
}

arpg::test::Failure minimum_int64_conversion_is_invalid() noexcept {
    Modifier conversion{3U, StatId::impulse_scale,
        ModifierOperation::conversion, kFixedOne};
    conversion.conversion_target = StatId::shield;
    const std::array<Modifier, 1> modifiers{{conversion}};
    StatValues values{};
    values[static_cast<std::size_t>(StatId::impulse_scale)] =
        (std::numeric_limits<std::int64_t>::min)();

    const ConversionResult result = evaluate_conversions(values, modifiers, {});
    ARPG_REQUIRE(!result.valid);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"identity", &empty_modifier_list_is_identity},
    {"operation order", &operation_order_is_flat_increased_then_more},
    {"addition overflow", &addition_overflow_is_invalid},
    {"player modifier capacity", &player_modifier_capacity_is_256},
    {"rating curve anchors", &rating_curve_hits_all_anchors},
    {"rating curve segment monotonicity",
        &rating_curve_is_monotonic_within_each_segment},
    {"rating curve tail", &rating_curve_handles_negative_and_tail_values},
    {"context gates", &conditions_and_forbidden_tags_gate_modifiers},
    {"order independence", &insertion_order_does_not_change_result},
    {"duplicate IDs", &duplicate_ids_are_rejected},
    {"conversion cap", &conversion_is_capped_at_one_hundred_percent},
    {"single conversion pass", &converted_value_does_not_convert_again},
    {"minimum int64 conversion", &minimum_int64_conversion_is_invalid},
};

}  // namespace

arpg::test::TestSuite modifier_math_suite() noexcept {
    return arpg::test::make_suite("modifier_math", kCases);
}

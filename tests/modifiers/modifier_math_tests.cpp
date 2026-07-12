#include "test_framework.hpp"

#include "modifiers/modifier_math.hpp"

#include <array>

namespace {

using namespace arpg::modifiers;

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

constexpr arpg::test::TestCase kCases[] = {
    {"identity", &empty_modifier_list_is_identity},
    {"operation order", &operation_order_is_flat_increased_then_more},
    {"context gates", &conditions_and_forbidden_tags_gate_modifiers},
    {"order independence", &insertion_order_does_not_change_result},
    {"duplicate IDs", &duplicate_ids_are_rejected},
    {"conversion cap", &conversion_is_capped_at_one_hundred_percent},
    {"single conversion pass", &converted_value_does_not_convert_again},
};

}  // namespace

arpg::test::TestSuite modifier_math_suite() noexcept {
    return arpg::test::make_suite("modifier_math", kCases);
}

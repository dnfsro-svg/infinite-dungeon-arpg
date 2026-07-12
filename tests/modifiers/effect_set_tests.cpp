#include "test_framework.hpp"

#include "modifiers/effect_set.hpp"

namespace {

using namespace arpg::modifiers;

EffectDefinition effect(EffectId id, int ticks,
    RefreshRule rule = RefreshRule::refresh_duration) noexcept {
    EffectDefinition value{};
    value.id = id;
    value.duration_ticks = ticks;
    value.refresh_rule = rule;
    value.max_stacks = 1;
    return value;
}

arpg::test::Failure capacity_is_fixed_and_reported() noexcept {
    EffectSet set;
    for (EffectId id = 1; id <= EffectSet::capacity(); ++id) {
        ARPG_REQUIRE(set.apply(effect(id, 10)) == ApplyResult::applied);
    }
    ARPG_REQUIRE(set.apply(effect(99, 10)) == ApplyResult::capacity_rejected);
    ARPG_REQUIRE(set.diagnostics().effect_overflows == 1);
    return {};
}

arpg::test::Failure refresh_restores_duration_without_duplication() noexcept {
    EffectSet set;
    ARPG_REQUIRE(set.apply(effect(4, 3)) == ApplyResult::applied);
    set.tick();
    ARPG_REQUIRE(set.apply(effect(4, 5)) == ApplyResult::refreshed);
    ARPG_REQUIRE(set.active_count() == 1);
    ARPG_REQUIRE(set.remaining_ticks(4) == 5);
    return {};
}

arpg::test::Failure add_stack_respects_maximum() noexcept {
    EffectSet set;
    auto value = effect(7, 10, RefreshRule::add_stack);
    value.max_stacks = 2;
    ARPG_REQUIRE(set.apply(value) == ApplyResult::applied);
    ARPG_REQUIRE(set.apply(value) == ApplyResult::stacked);
    ARPG_REQUIRE(set.apply(value) == ApplyResult::rejected);
    ARPG_REQUIRE(set.stack_count(7) == 2);
    return {};
}

arpg::test::Failure expiry_emits_deterministic_command() noexcept {
    EffectSet set;
    auto value = effect(3, 1);
    value.on_expire = {EffectCommandKind::clear_shield, 42};
    ARPG_REQUIRE(set.apply(value) == ApplyResult::applied);
    set.tick();
    EffectCommand command{};
    ARPG_REQUIRE(set.pop_command(command));
    ARPG_REQUIRE(command.kind == EffectCommandKind::clear_shield);
    ARPG_REQUIRE(command.value == 42);
    ARPG_REQUIRE(command.effect_id == 3);
    return {};
}

arpg::test::Failure command_overflow_is_bounded_and_reported() noexcept {
    EffectSet set;
    auto value = effect(1, 20);
    value.on_refresh = {EffectCommandKind::set_shield, 10};
    ARPG_REQUIRE(set.apply(value) == ApplyResult::applied);
    for (std::size_t index = 0; index < EffectSet::command_capacity() + 2; ++index) {
        ARPG_REQUIRE(set.apply(value) == ApplyResult::refreshed);
    }
    ARPG_REQUIRE(set.queued_command_count() == EffectSet::command_capacity());
    ARPG_REQUIRE(set.diagnostics().command_overflows == 2);
    return {};
}

arpg::test::Failure active_modifiers_are_exposed_in_slot_order() noexcept {
    EffectSet set;
    auto first = effect(8, 10);
    first.has_modifier = true;
    first.modifier = {80U, StatId::shield, ModifierOperation::flat, 20};
    auto second = effect(2, 10);
    second.has_modifier = true;
    second.modifier = {20U, StatId::shield, ModifierOperation::flat, 10};
    ARPG_REQUIRE(set.apply(first) == ApplyResult::applied);
    ARPG_REQUIRE(set.apply(second) == ApplyResult::applied);
    std::array<Modifier, EffectSet::kCapacity> modifiers{};
    ARPG_REQUIRE(set.copy_modifiers(modifiers) == 2);
    ARPG_REQUIRE(modifiers[0].id == 80U);
    ARPG_REQUIRE(modifiers[1].id == 20U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"fixed capacity", &capacity_is_fixed_and_reported},
    {"refresh duration", &refresh_restores_duration_without_duplication},
    {"stack cap", &add_stack_respects_maximum},
    {"expiry trigger", &expiry_emits_deterministic_command},
    {"command overflow", &command_overflow_is_bounded_and_reported},
    {"active modifiers", &active_modifiers_are_exposed_in_slot_order},
};

}  // namespace

arpg::test::TestSuite effect_set_suite() noexcept {
    return arpg::test::make_suite("effect_set", kCases);
}

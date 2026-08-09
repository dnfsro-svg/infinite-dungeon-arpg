#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "control_hints.hpp"
#include "platform/settings/settings_types.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

namespace platform = arpg::platform;
namespace settings = arpg::settings;

arpg::test::Failure revision_zero_initializes_all_structured_bindings()
    noexcept {
    const settings::SettingsData values = settings::default_settings();
    ARPG_REQUIRE(values.revision == 0U);
    platform::ControlHints hints{};

    platform::refresh_control_hints(hints, values);

    ARPG_REQUIRE(hints.initialized);
    ARPG_REQUIRE(hints.revision == 0U);
    ARPG_REQUIRE(hints.bindings == values.bindings);
    for (std::size_t index = 0U; index < hints.bindings.size(); ++index) {
        const auto action = static_cast<settings::SettingAction>(index);
        ARPG_REQUIRE(platform::control_hint_binding(hints, action)
            == values.bindings[index]);
    }
    return {};
}

arpg::test::Failure rebound_action_queries_return_current_key_labels()
    noexcept {
    settings::SettingsData values = settings::default_settings();
    ARPG_REQUIRE(settings::assign_or_swap(values,
        settings::SettingAction::interact, settings::StableKey::q));
    ARPG_REQUIRE(settings::assign_or_swap(values,
        settings::SettingAction::inventory, settings::StableKey::o));
    ARPG_REQUIRE(settings::assign_or_swap(values,
        settings::SettingAction::passive_tree, settings::StableKey::t));
    values.revision = 7U;
    platform::ControlHints hints{};

    platform::refresh_control_hints(hints, values);

    ARPG_REQUIRE(platform::control_hint_binding(
        hints, settings::SettingAction::interact) == settings::StableKey::q);
    ARPG_REQUIRE(std::strcmp(platform::control_hint_key_label(
        hints, settings::SettingAction::interact), "Q") == 0);
    ARPG_REQUIRE(std::strcmp(platform::control_hint_key_label(
        hints, settings::SettingAction::inventory), "O") == 0);
    ARPG_REQUIRE(std::strcmp(platform::control_hint_key_label(
        hints, settings::SettingAction::passive_tree), "T") == 0);
    ARPG_REQUIRE(hints.revision == 7U);
    return {};
}

arpg::test::Failure same_revision_preserves_the_complete_cache() noexcept {
    settings::SettingsData values = settings::default_settings();
    values.revision = 3U;
    platform::ControlHints hints{};
    platform::refresh_control_hints(hints, values);
    const platform::ControlHints original = hints;

    ARPG_REQUIRE(settings::assign_or_swap(values,
        settings::SettingAction::jump, settings::StableKey::q));
    platform::refresh_control_hints(hints, values);

    ARPG_REQUIRE(hints.bindings == original.bindings);
    ARPG_REQUIRE(hints.revision == original.revision);
    ARPG_REQUIRE(hints.initialized == original.initialized);
    return {};
}

arpg::test::Failure new_revision_replaces_the_complete_cache() noexcept {
    settings::SettingsData values = settings::default_settings();
    values.revision = 3U;
    platform::ControlHints hints{};
    platform::refresh_control_hints(hints, values);
    const auto old_jump = platform::control_hint_binding(
        hints, settings::SettingAction::jump);

    ARPG_REQUIRE(settings::assign_or_swap(values,
        settings::SettingAction::jump, settings::StableKey::q));
    values.revision = 4U;
    platform::refresh_control_hints(hints, values);

    ARPG_REQUIRE(hints.revision == 4U);
    ARPG_REQUIRE(hints.bindings == values.bindings);
    ARPG_REQUIRE(platform::control_hint_binding(
        hints, settings::SettingAction::jump) == settings::StableKey::q);
    ARPG_REQUIRE(platform::control_hint_binding(
        hints, settings::SettingAction::jump) != old_jump);
    return {};
}

arpg::test::Failure invalid_actions_return_safe_unknown_values() noexcept {
    platform::ControlHints hints{};
    ARPG_REQUIRE(platform::control_hint_binding(
        hints, settings::SettingAction::interact) == settings::StableKey::count);
    ARPG_REQUIRE(std::strcmp(platform::control_hint_key_label(
        hints, settings::SettingAction::interact), u8"未知") == 0);
    platform::refresh_control_hints(hints, settings::default_settings());
    const auto invalid = static_cast<settings::SettingAction>(255U);

    ARPG_REQUIRE(platform::control_hint_binding(hints, invalid)
        == settings::StableKey::count);
    ARPG_REQUIRE(std::strcmp(
        platform::control_hint_key_label(hints, invalid), u8"未知") == 0);
    return {};
}

arpg::test::Failure unchanged_refresh_and_queries_allocate_nothing() noexcept {
    settings::SettingsData values = settings::default_settings();
    values.revision = 91U;
    platform::ControlHints hints{};
    platform::refresh_control_hints(hints, values);
    const std::uint64_t before = arpg::test::allocation_count();
    for (int iteration = 0; iteration < 10000; ++iteration) {
        platform::refresh_control_hints(hints, values);
        ARPG_REQUIRE(platform::control_hint_binding(
            hints, settings::SettingAction::interact)
            == settings::StableKey::e);
        ARPG_REQUIRE(platform::control_hint_key_label(
            hints, settings::SettingAction::interact)[0] == 'E');
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"revision zero initializes structured bindings",
        &revision_zero_initializes_all_structured_bindings},
    {"rebound action queries", &rebound_action_queries_return_current_key_labels},
    {"same revision preserves cache", &same_revision_preserves_the_complete_cache},
    {"new revision replaces cache", &new_revision_replaces_the_complete_cache},
    {"invalid actions are safe", &invalid_actions_return_safe_unknown_values},
    {"unchanged refresh and queries no allocation",
        &unchanged_refresh_and_queries_allocate_nothing},
};

}  // namespace

arpg::test::TestSuite control_hints_suite() noexcept {
    return arpg::test::make_suite("control_hints", kCases);
}

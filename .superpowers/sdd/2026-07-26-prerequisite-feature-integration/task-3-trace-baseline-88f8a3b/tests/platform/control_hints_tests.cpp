#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "control_hints.hpp"
#include "platform/settings/settings_types.hpp"

#include <cstring>
#include <cstdint>

namespace {

namespace platform = arpg::platform;
namespace settings = arpg::settings;

[[nodiscard]] bool contains(const char* text, const char* needle) noexcept {
    return std::strstr(text, needle) != nullptr;
}

arpg::test::Failure defaults_render_every_configurable_action() noexcept {
    const settings::SettingsData values = settings::default_settings();
    platform::ControlHints hints{};
    platform::refresh_control_hints(hints, values);

    ARPG_REQUIRE(std::strcmp(hints.primary.data(),
        "W Move Up  S Move Down  A Move Left  D Move Right") == 0);
    ARPG_REQUIRE(std::strcmp(hints.secondary.data(),
        "J Light Attack  K Jump  L Launcher  E Interact  I Inventory  P Passive Tree  F1 Debug  F12 Screenshot  Esc Pause") == 0);
    ARPG_REQUIRE(hints.revision == values.revision);
    return {};
}

arpg::test::Failure arbitrary_swaps_refresh_the_current_key_labels() noexcept {
    settings::SettingsData values = settings::default_settings();
    ARPG_REQUIRE(settings::assign_or_swap(values,
        settings::SettingAction::light_attack, settings::StableKey::q));
    ARPG_REQUIRE(settings::assign_or_swap(values,
        settings::SettingAction::move_right, settings::StableKey::j));
    values.revision = 7U;

    platform::ControlHints hints{};
    platform::refresh_control_hints(hints, values);

    ARPG_REQUIRE(contains(hints.primary.data(), "J Move Right"));
    ARPG_REQUIRE(contains(hints.secondary.data(), "Q Light Attack"));
    ARPG_REQUIRE(!contains(hints.secondary.data(), "J Light Attack"));
    ARPG_REQUIRE(hints.revision == 7U);
    return {};
}

arpg::test::Failure unchanged_revision_does_not_rebuild_and_new_revision_does() noexcept {
    settings::SettingsData values = settings::default_settings();
    values.revision = 3U;
    platform::ControlHints hints{};
    platform::refresh_control_hints(hints, values);
    const platform::ControlHints original = hints;

    ARPG_REQUIRE(settings::assign_or_swap(values,
        settings::SettingAction::jump, settings::StableKey::q));
    platform::refresh_control_hints(hints, values);
    ARPG_REQUIRE(std::memcmp(&hints, &original, sizeof(hints)) == 0);

    values.revision = 4U;
    platform::refresh_control_hints(hints, values);
    ARPG_REQUIRE(hints.revision == 4U);
    ARPG_REQUIRE(contains(hints.secondary.data(), "Q Jump"));
    ARPG_REQUIRE(!contains(hints.secondary.data(), "K Jump"));
    return {};
}

arpg::test::Failure buffers_are_terminated_and_keep_global_labels_fixed() noexcept {
    settings::SettingsData values = settings::default_settings();
    values.revision = 11U;
    platform::ControlHints hints{};
    platform::refresh_control_hints(hints, values);

    ARPG_REQUIRE(hints.primary.back() == '\0');
    ARPG_REQUIRE(hints.secondary.back() == '\0');
    ARPG_REQUIRE(contains(hints.secondary.data(), "F1 Debug"));
    ARPG_REQUIRE(contains(hints.secondary.data(), "F12 Screenshot"));
    ARPG_REQUIRE(contains(hints.secondary.data(), "Esc Pause"));
    return {};
}

arpg::test::Failure all_action_labels_are_present_in_the_cached_text() noexcept {
    const settings::SettingsData values = settings::default_settings();
    platform::ControlHints hints{};
    platform::refresh_control_hints(hints, values);

    for (std::size_t index = 0U;
            index < static_cast<std::size_t>(settings::SettingAction::count);
            ++index) {
        const auto action = static_cast<settings::SettingAction>(index);
        const char* label = settings::action_label(action);
        ARPG_REQUIRE(contains(hints.primary.data(), label)
            || contains(hints.secondary.data(), label));
    }
    return {};
}

arpg::test::Failure unchanged_refresh_performs_no_heap_allocation() noexcept {
    settings::SettingsData values = settings::default_settings();
    values.revision = 91U;
    platform::ControlHints hints{};
    platform::refresh_control_hints(hints, values);
    const std::uint64_t before = arpg::test::allocation_count();
    for (int iteration = 0; iteration < 10000; ++iteration) {
        platform::refresh_control_hints(hints, values);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"defaults render every action", &defaults_render_every_configurable_action},
    {"swaps refresh current labels", &arbitrary_swaps_refresh_the_current_key_labels},
    {"revision refresh policy", &unchanged_revision_does_not_rebuild_and_new_revision_does},
    {"buffers and global labels", &buffers_are_terminated_and_keep_global_labels_fixed},
    {"all action labels", &all_action_labels_are_present_in_the_cached_text},
    {"unchanged refresh no allocation", &unchanged_refresh_performs_no_heap_allocation},
};

}  // namespace

arpg::test::TestSuite control_hints_suite() noexcept {
    return arpg::test::make_suite("control_hints", kCases);
}

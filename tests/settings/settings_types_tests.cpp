#include "test_framework.hpp"

#include "platform/settings/settings_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <cstring>

namespace {

using arpg::settings::SettingAction;
using arpg::settings::SettingsData;
using arpg::settings::SettingsValidationError;
using arpg::settings::StableKey;
using arpg::settings::WindowMode;

constexpr std::array<SettingAction, 10> actions{
    SettingAction::move_up,
    SettingAction::move_down,
    SettingAction::move_left,
    SettingAction::move_right,
    SettingAction::light_attack,
    SettingAction::jump,
    SettingAction::launcher,
    SettingAction::interact,
    SettingAction::inventory,
    SettingAction::passive_tree};

[[nodiscard]] bool same_settings(
    const SettingsData& lhs, const SettingsData& rhs) noexcept {
    return lhs.master_sfx_percent == rhs.master_sfx_percent &&
        lhs.window_mode == rhs.window_mode &&
        lhs.vsync_enabled == rhs.vsync_enabled &&
        lhs.bindings == rhs.bindings &&
        lhs.revision == rhs.revision;
}

arpg::test::Failure defaults_are_stable() noexcept {
    const SettingsData settings = arpg::settings::default_settings();
    const std::array<StableKey, 10> expected{
        StableKey::w, StableKey::s, StableKey::a, StableKey::d, StableKey::j,
        StableKey::k, StableKey::l, StableKey::e, StableKey::i, StableKey::p};

    ARPG_REQUIRE(settings.master_sfx_percent == 100U);
    ARPG_REQUIRE(settings.window_mode == WindowMode::windowed);
    ARPG_REQUIRE(settings.vsync_enabled);
    ARPG_REQUIRE(settings.revision == 0U);
    ARPG_REQUIRE(settings.bindings == expected);
    for (std::size_t index = 0; index < actions.size(); ++index) {
        ARPG_REQUIRE(arpg::settings::binding_for(settings, actions[index]) == expected[index]);
    }
    ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
    return {};
}

arpg::test::Failure action_labels_are_present_and_distinct() noexcept {
    for (std::size_t lhs = 0; lhs < actions.size(); ++lhs) {
        const char* label = arpg::settings::action_label(actions[lhs]);
        ARPG_REQUIRE(label != nullptr);
        ARPG_REQUIRE(label[0] != '\0');
        for (std::size_t rhs = lhs + 1U; rhs < actions.size(); ++rhs) {
            ARPG_REQUIRE(std::strcmp(label, arpg::settings::action_label(actions[rhs])) != 0);
        }
    }
    return {};
}

arpg::test::Failure rejects_volume_out_of_range() noexcept {
    SettingsData settings = arpg::settings::default_settings();
    settings.master_sfx_percent = 101U;
    ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
        SettingsValidationError::volume_range);
    return {};
}

arpg::test::Failure rejects_volume_off_step() noexcept {
    SettingsData settings = arpg::settings::default_settings();
    settings.master_sfx_percent = 99U;
    ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
        SettingsValidationError::volume_step);
    return {};
}

arpg::test::Failure rejects_invalid_window_mode() noexcept {
    SettingsData settings = arpg::settings::default_settings();
    settings.window_mode = static_cast<WindowMode>(2U);
    ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
        SettingsValidationError::window_mode);
    return {};
}

arpg::test::Failure rejects_invalid_and_duplicate_keys() noexcept {
    SettingsData settings = arpg::settings::default_settings();
    settings.bindings[0] = StableKey::count;
    ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
        SettingsValidationError::key_range);

    settings = arpg::settings::default_settings();
    settings.bindings[0] = settings.bindings[1];
    ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
        SettingsValidationError::duplicate_key);
    return {};
}

arpg::test::Failure rejects_reserved_v_in_every_binding() noexcept {
    for (std::size_t index = 0; index < actions.size(); ++index) {
        SettingsData settings = arpg::settings::default_settings();
        settings.bindings[index] = StableKey::v;
        ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
            SettingsValidationError::reserved_key);
    }
    return {};
}

arpg::test::Failure assigns_an_unoccupied_key() noexcept {
    SettingsData settings = arpg::settings::default_settings();
    ARPG_REQUIRE(arpg::settings::assign_or_swap(
        settings, SettingAction::move_up, StableKey::z));
    ARPG_REQUIRE(arpg::settings::binding_for(settings, SettingAction::move_up) == StableKey::z);
    ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
    return {};
}

arpg::test::Failure swaps_an_occupied_key() noexcept {
    SettingsData settings = arpg::settings::default_settings();
    ARPG_REQUIRE(arpg::settings::assign_or_swap(
        settings, SettingAction::move_up, StableKey::s));
    ARPG_REQUIRE(arpg::settings::binding_for(settings, SettingAction::move_up) == StableKey::s);
    ARPG_REQUIRE(arpg::settings::binding_for(settings, SettingAction::move_down) == StableKey::w);
    ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
    return {};
}

arpg::test::Failure reserved_v_assignment_is_rejected_without_mutation() noexcept {
    SettingsData settings = arpg::settings::default_settings();
    settings.master_sfx_percent = 75U;
    settings.window_mode = WindowMode::fullscreen;
    settings.vsync_enabled = false;
    settings.revision = 42U;
    const SettingsData before = settings;

    ARPG_REQUIRE(!arpg::settings::assign_or_swap(
        settings, SettingAction::light_attack, StableKey::v));
    ARPG_REQUIRE(same_settings(settings, before));
    return {};
}

arpg::test::Failure repeated_swaps_preserve_exact_permutation() noexcept {
    SettingsData settings = arpg::settings::default_settings();
    for (std::size_t iteration = 0; iteration < 1000U; ++iteration) {
        const std::size_t target = (iteration * 7U + 3U) % actions.size();
        const std::size_t owner =
            (target + 1U + (iteration * 3U) % (actions.size() - 1U)) % actions.size();
        ARPG_REQUIRE(owner != target);

        const SettingsData before = settings;
        const StableKey target_old_key = before.bindings[target];
        const StableKey candidate = before.bindings[owner];
        ARPG_REQUIRE(candidate != StableKey::v);
        ARPG_REQUIRE(arpg::settings::assign_or_swap(settings, actions[target], candidate));
        ARPG_REQUIRE(settings.bindings[target] == candidate);
        ARPG_REQUIRE(settings.bindings[owner] == target_old_key);
        for (std::size_t index = 0; index < actions.size(); ++index) {
            if (index != target && index != owner) {
                ARPG_REQUIRE(settings.bindings[index] == before.bindings[index]);
            }
        }
        ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
    }
    return {};
}

constexpr arpg::test::TestCase cases[] = {
    {"defaults are stable", defaults_are_stable},
    {"action labels are present and distinct", action_labels_are_present_and_distinct},
    {"volume outside range is rejected", rejects_volume_out_of_range},
    {"volume outside five-percent step is rejected", rejects_volume_off_step},
    {"invalid window mode is rejected", rejects_invalid_window_mode},
    {"invalid and duplicate keys are rejected", rejects_invalid_and_duplicate_keys},
    {"reserved V is rejected in every binding", rejects_reserved_v_in_every_binding},
    {"unoccupied key assignment succeeds", assigns_an_unoccupied_key},
    {"occupied key assignment swaps owners", swaps_an_occupied_key},
    {"reserved V assignment leaves settings unchanged", reserved_v_assignment_is_rejected_without_mutation},
    {"one thousand occupied-key swaps preserve exact permutation", repeated_swaps_preserve_exact_permutation}};

}  // namespace

arpg::test::TestSuite settings_types_suite() noexcept {
    return arpg::test::make_suite("settings.types", cases);
}

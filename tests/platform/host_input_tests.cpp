#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "host_input.hpp"
#include "stable_key_raylib.hpp"

#include "dungeon/dungeon_session.hpp"
#include "platform/settings/settings_types.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstring>

namespace {

using namespace arpg;

constexpr std::size_t kStableKeyCount =
    static_cast<std::size_t>(settings::StableKey::count);

constexpr std::array<int, kStableKeyCount> kExpectedRaylibKeys{{
    KEY_A, KEY_B, KEY_C, KEY_D, KEY_E, KEY_F, KEY_G, KEY_H, KEY_I,
    KEY_J, KEY_K, KEY_L, KEY_M, KEY_N, KEY_O, KEY_P, KEY_Q, KEY_R,
    KEY_S, KEY_T, KEY_U, KEY_V, KEY_W, KEY_X, KEY_Y, KEY_Z,
    KEY_ZERO, KEY_ONE, KEY_TWO, KEY_THREE, KEY_FOUR, KEY_FIVE,
    KEY_SIX, KEY_SEVEN, KEY_EIGHT, KEY_NINE,
    KEY_UP, KEY_DOWN, KEY_LEFT, KEY_RIGHT, KEY_SPACE,
    KEY_LEFT_SHIFT, KEY_RIGHT_SHIFT, KEY_LEFT_CONTROL, KEY_RIGHT_CONTROL,
}};

constexpr std::array<const char*, kStableKeyCount> kExpectedLabels{{
    "A", "B", "C", "D", "E", "F", "G", "H", "I", "J", "K", "L",
    "M", "N", "O", "P", "Q", "R", "S", "T", "U", "V", "W", "X",
    "Y", "Z", "0", "1", "2", "3", "4", "5", "6", "7", "8", "9",
    "Up", "Down", "Left", "Right", "Space", "Left Shift", "Right Shift",
    "Left Ctrl", "Right Ctrl",
}};

[[nodiscard]] constexpr std::size_t key_index(settings::StableKey key) noexcept {
    return static_cast<std::size_t>(key);
}

void press(platform::PhysicalKeySnapshot& snapshot,
    settings::StableKey key) noexcept {
    snapshot.pressed[key_index(key)] = true;
}

void hold(platform::PhysicalKeySnapshot& snapshot,
    settings::StableKey key) noexcept {
    snapshot.down[key_index(key)] = true;
}

test::Failure adapter_round_trips_all_45_keys_with_unique_codes_and_labels() noexcept {
    for (std::size_t index = 0U; index < kStableKeyCount; ++index) {
        const auto key = static_cast<settings::StableKey>(index);
        const int code = platform::raylib_key(key);
        ARPG_REQUIRE(code == kExpectedRaylibKeys[index]);
        const auto round_trip = platform::stable_key_from_raylib(code);
        ARPG_REQUIRE(round_trip.has_value());
        ARPG_REQUIRE(*round_trip == key);
        ARPG_REQUIRE(std::strcmp(
            platform::stable_key_label(key), kExpectedLabels[index]) == 0);
        for (std::size_t earlier = 0U; earlier < index; ++earlier) {
            ARPG_REQUIRE(code != kExpectedRaylibKeys[earlier]);
            ARPG_REQUIRE(std::strcmp(
                platform::stable_key_label(key),
                platform::stable_key_label(
                    static_cast<settings::StableKey>(earlier))) != 0);
        }
    }
    return {};
}

test::Failure adapter_rejects_unknown_values() noexcept {
    ARPG_REQUIRE(platform::raylib_key(settings::StableKey::count) == KEY_NULL);
    ARPG_REQUIRE(!platform::stable_key_from_raylib(KEY_NULL).has_value());
    ARPG_REQUIRE(!platform::stable_key_from_raylib(KEY_ESCAPE).has_value());
    ARPG_REQUIRE(std::strcmp(
        platform::stable_key_label(settings::StableKey::count), "Unknown") == 0);
    return {};
}

struct QueryLog final {
    std::array<unsigned, 512> pressed{};
    std::array<unsigned, 512> down{};
    unsigned mouse_pressed{};
    unsigned mouse_position{};
    unsigned focus_lost{};
};

bool query_pressed(void* context, int key) noexcept {
    auto& log = *static_cast<QueryLog*>(context);
    ++log.pressed[static_cast<std::size_t>(key)];
    return key == KEY_V || key == KEY_R || key == KEY_N;
}

bool query_down(void* context, int key) noexcept {
    auto& log = *static_cast<QueryLog*>(context);
    ++log.down[static_cast<std::size_t>(key)];
    return key == KEY_W;
}

bool query_mouse_pressed(void* context) noexcept {
    ++static_cast<QueryLog*>(context)->mouse_pressed;
    return true;
}

Vector2 query_mouse_position(void* context) noexcept {
    ++static_cast<QueryLog*>(context)->mouse_position;
    return {123.0F, 456.0F};
}

bool query_focus_lost(void* context) noexcept {
    ++static_cast<QueryLog*>(context)->focus_lost;
    return true;
}

test::Failure sampler_queries_every_stable_key_once_per_state() noexcept {
    QueryLog log{};
    const platform::PhysicalKeySource source{
        &log,
        &query_pressed,
        &query_down,
        &query_mouse_pressed,
        &query_mouse_position,
        &query_focus_lost,
    };
    const platform::PhysicalKeySnapshot snapshot =
        platform::sample_physical_keys(source);

    for (std::size_t index = 0U; index < kStableKeyCount; ++index) {
        const int code = kExpectedRaylibKeys[index];
        ARPG_REQUIRE(log.pressed[static_cast<std::size_t>(code)] == 1U);
        ARPG_REQUIRE(log.down[static_cast<std::size_t>(code)] == 1U);
    }
    ARPG_REQUIRE(log.pressed[KEY_ESCAPE] == 1U);
    ARPG_REQUIRE(log.pressed[KEY_ENTER] == 1U);
    ARPG_REQUIRE(log.pressed[KEY_F1] == 1U);
    ARPG_REQUIRE(log.pressed[KEY_F12] == 1U);
    ARPG_REQUIRE(log.pressed[KEY_V] == 1U);
    ARPG_REQUIRE(log.pressed[KEY_R] == 1U);
    ARPG_REQUIRE(log.pressed[KEY_N] == 1U);
    ARPG_REQUIRE(log.mouse_pressed == 1U);
    ARPG_REQUIRE(log.mouse_position == 1U);
    ARPG_REQUIRE(log.focus_lost == 1U);
    ARPG_REQUIRE(snapshot.v);
    ARPG_REQUIRE(snapshot.mouse_left);
    ARPG_REQUIRE(snapshot.focus_lost);
    ARPG_REQUIRE(snapshot.mouse_position.x == 123.0F);
    ARPG_REQUIRE(snapshot.mouse_position.y == 456.0F);
    return {};
}

test::Failure default_bindings_map_movement_actions_and_overlays() noexcept {
    const settings::SettingsData settings = settings::default_settings();
    platform::PhysicalKeySnapshot snapshot{};
    hold(snapshot, settings::StableKey::w);
    hold(snapshot, settings::StableKey::d);
    press(snapshot, settings::StableKey::j);
    press(snapshot, settings::StableKey::k);
    press(snapshot, settings::StableKey::l);
    press(snapshot, settings::StableKey::e);
    press(snapshot, settings::StableKey::i);
    press(snapshot, settings::StableKey::p);

    const platform::HostFrameInput input =
        platform::map_host_frame_input(settings, snapshot);
    ARPG_REQUIRE(input.movement.x == 1);
    ARPG_REQUIRE(input.movement.y == -1);
    ARPG_REQUIRE(input.keys.movement);
    ARPG_REQUIRE(input.combat_actions[0]);
    ARPG_REQUIRE(input.combat_actions[1]);
    ARPG_REQUIRE(input.combat_actions[2]);
    ARPG_REQUIRE(input.keys.attack);
    ARPG_REQUIRE(input.keys.e);
    ARPG_REQUIRE(input.keys.inventory);
    ARPG_REQUIRE(input.keys.passives);
    return {};
}

test::Failure swapped_bindings_drive_the_new_logical_owners() noexcept {
    settings::SettingsData settings = settings::default_settings();
    ARPG_REQUIRE(settings::assign_or_swap(
        settings, settings::SettingAction::light_attack, settings::StableKey::d));
    ARPG_REQUIRE(settings::binding_for(
        settings, settings::SettingAction::move_right) == settings::StableKey::j);
    ARPG_REQUIRE(settings::validate_settings(settings)
        == settings::SettingsValidationError::none);

    platform::PhysicalKeySnapshot snapshot{};
    press(snapshot, settings::StableKey::d);
    hold(snapshot, settings::StableKey::j);
    const platform::HostFrameInput input =
        platform::map_host_frame_input(settings, snapshot);
    ARPG_REQUIRE(input.combat_actions[0]);
    ARPG_REQUIRE(!input.combat_actions[1]);
    ARPG_REQUIRE(!input.combat_actions[2]);
    ARPG_REQUIRE(input.movement.x == 1);

    settings = settings::default_settings();
    ARPG_REQUIRE(settings::assign_or_swap(
        settings, settings::SettingAction::interact, settings::StableKey::i));
    snapshot = {};
    press(snapshot, settings::StableKey::i);
    auto overlay_input = platform::map_host_frame_input(settings, snapshot);
    ARPG_REQUIRE(overlay_input.keys.e);
    ARPG_REQUIRE(!overlay_input.keys.inventory);
    snapshot = {};
    press(snapshot, settings::StableKey::e);
    overlay_input = platform::map_host_frame_input(settings, snapshot);
    ARPG_REQUIRE(!overlay_input.keys.e);
    ARPG_REQUIRE(overlay_input.keys.inventory);

    settings = settings::default_settings();
    ARPG_REQUIRE(settings::assign_or_swap(
        settings, settings::SettingAction::inventory, settings::StableKey::p));
    snapshot = {};
    press(snapshot, settings::StableKey::p);
    overlay_input = platform::map_host_frame_input(settings, snapshot);
    ARPG_REQUIRE(overlay_input.keys.inventory);
    ARPG_REQUIRE(!overlay_input.keys.passives);
    snapshot = {};
    press(snapshot, settings::StableKey::i);
    overlay_input = platform::map_host_frame_input(settings, snapshot);
    ARPG_REQUIRE(!overlay_input.keys.inventory);
    ARPG_REQUIRE(overlay_input.keys.passives);
    return {};
}

test::Failure movement_uses_down_and_actions_use_pressed() noexcept {
    const settings::SettingsData settings = settings::default_settings();
    platform::PhysicalKeySnapshot snapshot{};
    press(snapshot, settings::StableKey::w);
    hold(snapshot, settings::StableKey::j);
    platform::HostFrameInput input =
        platform::map_host_frame_input(settings, snapshot);
    ARPG_REQUIRE(input.movement.x == 0);
    ARPG_REQUIRE(input.movement.y == 0);
    ARPG_REQUIRE(!input.keys.movement);
    ARPG_REQUIRE(!input.combat_actions[0]);
    ARPG_REQUIRE(!input.keys.attack);

    snapshot = {};
    hold(snapshot, settings::StableKey::w);
    press(snapshot, settings::StableKey::j);
    input = platform::map_host_frame_input(settings, snapshot);
    ARPG_REQUIRE(input.movement.y == -1);
    ARPG_REQUIRE(input.combat_actions[0]);
    return {};
}

test::Failure global_keys_and_mouse_are_binding_independent() noexcept {
    settings::SettingsData settings = settings::default_settings();
    ARPG_REQUIRE(settings::assign_or_swap(
        settings, settings::SettingAction::interact, settings::StableKey::q));
    platform::PhysicalKeySnapshot snapshot{};
    snapshot.escape = true;
    snapshot.enter = true;
    snapshot.f1 = true;
    snapshot.f12 = true;
    snapshot.v = true;
    snapshot.mouse_left = true;
    snapshot.focus_lost = true;
    snapshot.mouse_position = {17.0F, 29.0F};

    const platform::HostFrameInput input =
        platform::map_host_frame_input(settings, snapshot);
    ARPG_REQUIRE(input.keys.escape);
    ARPG_REQUIRE(input.keys.enter);
    ARPG_REQUIRE(input.keys.f1);
    ARPG_REQUIRE(input.keys.f12);
    ARPG_REQUIRE(input.keys.v);
    ARPG_REQUIRE(input.keys.mouse_gameplay);
    ARPG_REQUIRE(input.keys.focus_lost);
    ARPG_REQUIRE(input.mouse_position.x == 17.0F);
    ARPG_REQUIRE(input.mouse_position.y == 29.0F);
    ARPG_REQUIRE(!input.keys.e);
    return {};
}

test::Failure valid_r_binding_wins_over_legacy_room_reset() noexcept {
    platform::PhysicalKeySnapshot snapshot{};
    press(snapshot, settings::StableKey::r);
    hold(snapshot, settings::StableKey::r);

    const platform::HostFrameInput legacy = platform::map_host_frame_input(
        settings::default_settings(), snapshot);
    ARPG_REQUIRE(legacy.keys.reset);

    settings::SettingsData rebound = settings::default_settings();
    ARPG_REQUIRE(settings::assign_or_swap(
        rebound, settings::SettingAction::move_up, settings::StableKey::r));
    ARPG_REQUIRE(settings::validate_settings(rebound)
        == settings::SettingsValidationError::none);
    const platform::HostFrameInput bound =
        platform::map_host_frame_input(rebound, snapshot);
    ARPG_REQUIRE(bound.movement.y == -1);
    ARPG_REQUIRE(!bound.keys.reset);
    return {};
}

test::Failure recovery_n_uses_the_stable_snapshot_edge() noexcept {
    platform::PhysicalKeySnapshot snapshot{};
    press(snapshot, settings::StableKey::n);
    const platform::HostFrameInput input = platform::map_host_frame_input(
        settings::default_settings(), snapshot);
    ARPG_REQUIRE(input.keys.recovery);
    return {};
}

test::Failure validated_swap_never_emits_two_combat_actions() noexcept {
    settings::SettingsData settings = settings::default_settings();
    ARPG_REQUIRE(settings::assign_or_swap(
        settings, settings::SettingAction::light_attack, settings::StableKey::k));
    ARPG_REQUIRE(settings::validate_settings(settings)
        == settings::SettingsValidationError::none);
    platform::PhysicalKeySnapshot snapshot{};
    press(snapshot, settings::StableKey::k);
    const platform::HostFrameInput input =
        platform::map_host_frame_input(settings, snapshot);
    const unsigned action_count = static_cast<unsigned>(input.combat_actions[0])
        + static_cast<unsigned>(input.combat_actions[1])
        + static_cast<unsigned>(input.combat_actions[2]);
    ARPG_REQUIRE(action_count == 1U);
    ARPG_REQUIRE(input.combat_actions[0]);
    return {};
}

test::Failure mapper_performs_no_heap_allocations() noexcept {
    const settings::SettingsData settings = settings::default_settings();
    platform::PhysicalKeySnapshot snapshot{};
    press(snapshot, settings::StableKey::j);
    hold(snapshot, settings::StableKey::w);
    const std::uint64_t before = test::allocation_count();
    for (int iteration = 0; iteration < 1000; ++iteration) {
        const platform::HostFrameInput input =
            platform::map_host_frame_input(settings, snapshot);
        ARPG_REQUIRE(input.combat_actions[0]);
    }
    ARPG_REQUIRE(test::allocation_count() == before);
    return {};
}

test::Failure submit_forwards_each_combat_action_to_the_session() noexcept {
    {
        dungeon::DungeonSession session{};
        session.tick({});
        platform::HostFrameInput input{};
        input.combat_actions[0] = true;
        platform::submit_frame_actions(session, input);
        session.tick({});
        ARPG_REQUIRE(session.snapshot().combat.has_value());
        ARPG_REQUIRE(session.snapshot().combat->player.active_attack
            == combat::AttackId::j1);
    }
    {
        dungeon::DungeonSession session{};
        session.tick({});
        platform::HostFrameInput input{};
        input.combat_actions[1] = true;
        platform::submit_frame_actions(session, input);
        session.tick({});
        ARPG_REQUIRE(session.snapshot().combat.has_value());
        ARPG_REQUIRE(session.snapshot().combat->player.state
            == combat::PlayerState::jump_rise);
    }
    {
        dungeon::DungeonSession session{};
        session.tick({});
        platform::HostFrameInput input{};
        input.combat_actions[2] = true;
        platform::submit_frame_actions(session, input);
        session.tick({});
        ARPG_REQUIRE(session.snapshot().combat.has_value());
        ARPG_REQUIRE(session.snapshot().combat->player.active_attack
            == combat::AttackId::launcher);
    }
    return {};
}

constexpr test::TestCase kCases[] = {
    {"45 stable keys round-trip with labels", &adapter_round_trips_all_45_keys_with_unique_codes_and_labels},
    {"unknown adapter values rejected", &adapter_rejects_unknown_values},
    {"stable keys sampled once per state", &sampler_queries_every_stable_key_once_per_state},
    {"default logical bindings", &default_bindings_map_movement_actions_and_overlays},
    {"swapped logical bindings", &swapped_bindings_drive_the_new_logical_owners},
    {"movement down and action pressed", &movement_uses_down_and_actions_use_pressed},
    {"global input binding independent", &global_keys_and_mouse_are_binding_independent},
    {"R binding wins over reset", &valid_r_binding_wins_over_legacy_room_reset},
    {"N recovery from stable edge", &recovery_n_uses_the_stable_snapshot_edge},
    {"validated binding emits one combat action", &validated_swap_never_emits_two_combat_actions},
    {"mapper has zero allocations", &mapper_performs_no_heap_allocations},
    {"submit forwards combat actions", &submit_forwards_each_combat_action_to_the_session},
};

}  // namespace

arpg::test::TestSuite host_input_suite() noexcept {
    return arpg::test::make_suite("host_input", kCases);
}

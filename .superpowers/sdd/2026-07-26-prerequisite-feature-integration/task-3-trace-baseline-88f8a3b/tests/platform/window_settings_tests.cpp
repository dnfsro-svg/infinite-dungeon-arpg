#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "window_settings.hpp"

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace {

using arpg::platform::LiveSettingsResult;
using arpg::platform::WindowSettingsBackend;
using arpg::settings::SettingsData;
using arpg::settings::WindowMode;

enum class BackendEventKind : std::uint8_t {
    set_volume,
    set_window_mode,
    read_window_mode,
    set_vsync,
    read_vsync
};

struct BackendEvent final {
    BackendEventKind kind{};
    std::uint8_t volume{};
    WindowMode mode{WindowMode::windowed};
    bool enabled{};
};

struct FakeBackend final {
    std::uint8_t volume{100U};
    WindowMode mode{WindowMode::windowed};
    bool vsync{true};
    std::array<BackendEvent, 32U> events{};
    std::array<bool, 32U> fail_on_event{};
    std::size_t event_count{};
    bool mismatch_mode_readback{};
    bool mismatch_vsync_readback{};

    [[nodiscard]] bool record(BackendEvent event) noexcept {
        const std::size_t index = event_count;
        if (index < events.size()) events[index] = event;
        ++event_count;
        return index >= fail_on_event.size() || !fail_on_event[index];
    }
};

bool set_volume(void* context, std::uint8_t percent) noexcept {
    auto& fake = *static_cast<FakeBackend*>(context);
    fake.volume = percent;
    return fake.record({BackendEventKind::set_volume, percent});
}

bool set_window_mode(void* context, WindowMode mode) noexcept {
    auto& fake = *static_cast<FakeBackend*>(context);
    BackendEvent event{};
    event.kind = BackendEventKind::set_window_mode;
    event.mode = mode;
    fake.mode = mode;
    return fake.record(event);
}

bool set_vsync(void* context, bool enabled) noexcept {
    auto& fake = *static_cast<FakeBackend*>(context);
    BackendEvent event{};
    event.kind = BackendEventKind::set_vsync;
    event.enabled = enabled;
    fake.vsync = enabled;
    return fake.record(event);
}

WindowMode window_mode(void* context) noexcept {
    auto& fake = *static_cast<FakeBackend*>(context);
    BackendEvent event{};
    event.kind = BackendEventKind::read_window_mode;
    static_cast<void>(fake.record(event));
    if (!fake.mismatch_mode_readback) return fake.mode;
    return fake.mode == WindowMode::windowed
        ? WindowMode::fullscreen
        : WindowMode::windowed;
}

bool vsync(void* context) noexcept {
    auto& fake = *static_cast<FakeBackend*>(context);
    BackendEvent event{};
    event.kind = BackendEventKind::read_vsync;
    static_cast<void>(fake.record(event));
    return fake.mismatch_vsync_readback ? !fake.vsync : fake.vsync;
}

[[nodiscard]] WindowSettingsBackend backend_for(FakeBackend& fake) noexcept {
    return {&fake, &set_volume, &set_window_mode, &set_vsync,
        &window_mode, &vsync};
}

[[nodiscard]] SettingsData live_settings(
    std::uint8_t volume,
    WindowMode mode,
    bool vsync_enabled) noexcept {
    SettingsData settings = arpg::settings::default_settings();
    settings.master_sfx_percent = volume;
    settings.window_mode = mode;
    settings.vsync_enabled = vsync_enabled;
    return settings;
}

arpg::test::Failure invalid_snapshots_never_call_backend() noexcept {
    FakeBackend fake{};
    SettingsData valid = arpg::settings::default_settings();
    SettingsData invalid = valid;
    invalid.master_sfx_percent = 101U;

    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        invalid, valid, backend_for(fake)) == LiveSettingsResult::invalid);
    ARPG_REQUIRE(fake.event_count == 0U);
    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        valid, invalid, backend_for(fake)) == LiveSettingsResult::invalid);
    ARPG_REQUIRE(fake.event_count == 0U);
    return {};
}

arpg::test::Failure valid_noop_accepts_empty_backend() noexcept {
    SettingsData current = arpg::settings::default_settings();
    SettingsData desired = current;
    desired.revision = 19U;
    desired.bindings[0] = current.bindings[1];
    desired.bindings[1] = current.bindings[0];

    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        current, desired, {}) == LiveSettingsResult::applied);
    return {};
}

arpg::test::Failure changed_fields_apply_in_stable_order() noexcept {
    FakeBackend fake{};
    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);

    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        current, desired, backend_for(fake)) == LiveSettingsResult::applied);
    ARPG_REQUIRE(fake.event_count == 5U);
    ARPG_REQUIRE(fake.events[0].kind == BackendEventKind::set_volume);
    ARPG_REQUIRE(fake.events[0].volume == 0U);
    ARPG_REQUIRE(fake.events[1].kind == BackendEventKind::set_window_mode);
    ARPG_REQUIRE(fake.events[1].mode == WindowMode::fullscreen);
    ARPG_REQUIRE(fake.events[2].kind == BackendEventKind::read_window_mode);
    ARPG_REQUIRE(fake.events[3].kind == BackendEventKind::set_vsync);
    ARPG_REQUIRE(!fake.events[3].enabled);
    ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::read_vsync);
    ARPG_REQUIRE(fake.volume == 0U);
    ARPG_REQUIRE(fake.mode == WindowMode::fullscreen);
    ARPG_REQUIRE(!fake.vsync);
    return {};
}

arpg::test::Failure volume_endpoints_and_fraction_are_exact() noexcept {
    ARPG_REQUIRE(arpg::platform::master_volume_fraction(0U) == 0.0F);
    ARPG_REQUIRE(arpg::platform::master_volume_fraction(55U)
        == 55.0F / 100.0F);
    ARPG_REQUIRE(arpg::platform::master_volume_fraction(100U) == 1.0F);

    for (const std::uint8_t endpoint : {std::uint8_t{0U}, std::uint8_t{100U}}) {
        FakeBackend fake{};
        const std::uint8_t current_volume = endpoint == 0U ? 100U : 0U;
        fake.volume = current_volume;
        const SettingsData current = live_settings(
            current_volume, WindowMode::windowed, true);
        const SettingsData desired = live_settings(
            endpoint, WindowMode::windowed, true);
        ARPG_REQUIRE(arpg::platform::apply_live_settings(
            current, desired, backend_for(fake)) == LiveSettingsResult::applied);
        ARPG_REQUIRE(fake.event_count == 1U);
        ARPG_REQUIRE(fake.events[0].kind == BackendEventKind::set_volume);
        ARPG_REQUIRE(fake.events[0].volume == endpoint);
    }
    return {};
}

arpg::test::Failure setter_failures_restore_completed_fields() noexcept {
    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);

    for (std::size_t failed_event : {0U, 1U, 3U}) {
        FakeBackend fake{};
        fake.fail_on_event[failed_event] = true;
        ARPG_REQUIRE(arpg::platform::apply_live_settings(
            current, desired, backend_for(fake))
            == LiveSettingsResult::backend_failed);
        if (failed_event == 0U) {
            ARPG_REQUIRE(fake.event_count == 2U);
            ARPG_REQUIRE(fake.events[1].kind == BackendEventKind::set_volume);
            ARPG_REQUIRE(fake.events[1].volume == 100U);
        } else if (failed_event == 1U) {
            ARPG_REQUIRE(fake.event_count == 4U);
            ARPG_REQUIRE(fake.events[2].kind == BackendEventKind::set_window_mode);
            ARPG_REQUIRE(fake.events[2].mode == WindowMode::windowed);
            ARPG_REQUIRE(fake.events[3].kind == BackendEventKind::set_volume);
            ARPG_REQUIRE(fake.events[3].volume == 100U);
        } else {
            ARPG_REQUIRE(fake.event_count == 7U);
            ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::set_vsync);
            ARPG_REQUIRE(fake.events[4].enabled);
            ARPG_REQUIRE(fake.events[5].kind == BackendEventKind::set_window_mode);
            ARPG_REQUIRE(fake.events[5].mode == WindowMode::windowed);
            ARPG_REQUIRE(fake.events[6].kind == BackendEventKind::set_volume);
            ARPG_REQUIRE(fake.events[6].volume == 100U);
        }
        ARPG_REQUIRE(fake.volume == 100U);
        ARPG_REQUIRE(fake.mode == WindowMode::windowed);
        ARPG_REQUIRE(fake.vsync);
    }
    return {};
}

arpg::test::Failure missing_callbacks_fail_before_backend_calls() noexcept {
    FakeBackend fake{};
    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);
    WindowSettingsBackend backend = backend_for(fake);

    backend.set_volume = nullptr;
    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        current, desired, backend) == LiveSettingsResult::backend_failed);
    ARPG_REQUIRE(fake.event_count == 0U);

    backend = backend_for(fake);
    backend.set_window_mode = nullptr;
    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        current, desired, backend) == LiveSettingsResult::backend_failed);
    ARPG_REQUIRE(fake.event_count == 0U);

    backend = backend_for(fake);
    backend.window_mode = nullptr;
    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        current, desired, backend) == LiveSettingsResult::backend_failed);
    ARPG_REQUIRE(fake.event_count == 0U);

    backend = backend_for(fake);
    backend.set_vsync = nullptr;
    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        current, desired, backend) == LiveSettingsResult::backend_failed);
    ARPG_REQUIRE(fake.event_count == 0U);

    backend = backend_for(fake);
    backend.vsync = nullptr;
    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        current, desired, backend) == LiveSettingsResult::backend_failed);
    ARPG_REQUIRE(fake.event_count == 0U);
    return {};
}

arpg::test::Failure mode_readback_mismatch_restores_mode_then_volume() noexcept {
    FakeBackend fake{};
    fake.mismatch_mode_readback = true;
    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, true);

    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        current, desired, backend_for(fake))
        == LiveSettingsResult::readback_failed);
    ARPG_REQUIRE(fake.event_count == 5U);
    ARPG_REQUIRE(fake.events[2].kind == BackendEventKind::read_window_mode);
    ARPG_REQUIRE(fake.events[3].kind == BackendEventKind::set_window_mode);
    ARPG_REQUIRE(fake.events[3].mode == WindowMode::windowed);
    ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::set_volume);
    ARPG_REQUIRE(fake.events[4].volume == 100U);
    ARPG_REQUIRE(fake.volume == 100U);
    ARPG_REQUIRE(fake.mode == WindowMode::windowed);
    return {};
}

arpg::test::Failure vsync_readback_mismatch_restores_reverse_order() noexcept {
    FakeBackend fake{};
    fake.mismatch_vsync_readback = true;
    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);

    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        current, desired, backend_for(fake))
        == LiveSettingsResult::readback_failed);
    ARPG_REQUIRE(fake.event_count == 8U);
    ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::read_vsync);
    ARPG_REQUIRE(fake.events[5].kind == BackendEventKind::set_vsync);
    ARPG_REQUIRE(fake.events[5].enabled);
    ARPG_REQUIRE(fake.events[6].kind == BackendEventKind::set_window_mode);
    ARPG_REQUIRE(fake.events[6].mode == WindowMode::windowed);
    ARPG_REQUIRE(fake.events[7].kind == BackendEventKind::set_volume);
    ARPG_REQUIRE(fake.events[7].volume == 100U);
    ARPG_REQUIRE(fake.volume == 100U);
    ARPG_REQUIRE(fake.mode == WindowMode::windowed);
    ARPG_REQUIRE(fake.vsync);
    return {};
}

arpg::test::Failure rollback_uses_the_same_transaction_path() noexcept {
    FakeBackend fake{};
    fake.volume = 0U;
    fake.mode = WindowMode::fullscreen;
    fake.vsync = false;
    const SettingsData previewed = live_settings(0U, WindowMode::fullscreen, false);
    const SettingsData committed = live_settings(100U, WindowMode::windowed, true);

    ARPG_REQUIRE(arpg::platform::rollback_live_settings(
        previewed, committed, backend_for(fake)) == LiveSettingsResult::applied);
    ARPG_REQUIRE(fake.event_count == 5U);
    ARPG_REQUIRE(fake.events[0].kind == BackendEventKind::set_volume);
    ARPG_REQUIRE(fake.events[0].volume == 100U);
    ARPG_REQUIRE(fake.events[1].kind == BackendEventKind::set_window_mode);
    ARPG_REQUIRE(fake.events[1].mode == WindowMode::windowed);
    ARPG_REQUIRE(fake.events[2].kind == BackendEventKind::read_window_mode);
    ARPG_REQUIRE(fake.events[3].kind == BackendEventKind::set_vsync);
    ARPG_REQUIRE(fake.events[3].enabled);
    ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::read_vsync);
    return {};
}

arpg::test::Failure rollback_failures_do_not_replace_original_error() noexcept {
    FakeBackend fake{};
    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);
    fake.fail_on_event[3] = true;
    fake.fail_on_event[4] = true;

    ARPG_REQUIRE(arpg::platform::apply_live_settings(
        current, desired, backend_for(fake))
        == LiveSettingsResult::backend_failed);
    ARPG_REQUIRE(fake.event_count == 7U);
    ARPG_REQUIRE(fake.events[4].kind == BackendEventKind::set_vsync);
    ARPG_REQUIRE(fake.events[5].kind == BackendEventKind::set_window_mode);
    ARPG_REQUIRE(fake.events[6].kind == BackendEventKind::set_volume);
    ARPG_REQUIRE(fake.volume == 100U);
    ARPG_REQUIRE(fake.mode == WindowMode::windowed);
    ARPG_REQUIRE(fake.vsync);
    return {};
}

arpg::test::Failure initial_flags_cover_all_mode_vsync_combinations() noexcept {
    for (const WindowMode mode : {WindowMode::windowed, WindowMode::fullscreen}) {
        for (const bool vsync_enabled : {false, true}) {
            const SettingsData settings = live_settings(100U, mode, vsync_enabled);
            const unsigned int flags = arpg::platform::initial_window_flags(settings);
            unsigned int expected = FLAG_WINDOW_RESIZABLE;
            if (mode == WindowMode::fullscreen) expected |= FLAG_FULLSCREEN_MODE;
            if (vsync_enabled) expected |= FLAG_VSYNC_HINT;
            ARPG_REQUIRE(flags == expected);
        }
    }
    return {};
}

arpg::test::Failure orchestration_does_not_allocate() noexcept {
    FakeBackend fake{};
    const SettingsData current = live_settings(100U, WindowMode::windowed, true);
    const SettingsData desired = live_settings(0U, WindowMode::fullscreen, false);
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 1000U; ++iteration) {
        fake.event_count = 0U;
        ARPG_REQUIRE(arpg::platform::apply_live_settings(
            current, desired, backend_for(fake)) == LiveSettingsResult::applied);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"invalid snapshots", &invalid_snapshots_never_call_backend},
    {"valid noop", &valid_noop_accepts_empty_backend},
    {"stable apply order", &changed_fields_apply_in_stable_order},
    {"volume endpoints and fraction", &volume_endpoints_and_fraction_are_exact},
    {"setter failures rollback", &setter_failures_restore_completed_fields},
    {"missing callbacks", &missing_callbacks_fail_before_backend_calls},
    {"mode readback mismatch", &mode_readback_mismatch_restores_mode_then_volume},
    {"vsync readback mismatch", &vsync_readback_mismatch_restores_reverse_order},
    {"rollback transaction", &rollback_uses_the_same_transaction_path},
    {"rollback failure tolerance", &rollback_failures_do_not_replace_original_error},
    {"initial flags", &initial_flags_cover_all_mode_vsync_combinations},
    {"no allocation", &orchestration_does_not_allocate},
};

}  // namespace

arpg::test::TestSuite window_settings_suite() noexcept {
    return arpg::test::make_suite("window_settings", kCases);
}

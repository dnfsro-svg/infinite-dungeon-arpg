#include "test_framework.hpp"

#include "host_window_lifetime.hpp"
#include "raylib_host.hpp"

#include "platform/settings/settings_types.hpp"

#include <array>
#include <cstddef>
#include <type_traits>

namespace {

namespace platform = arpg::platform;
namespace settings = arpg::settings;

static_assert(std::is_same_v<
    decltype(platform::HostWindowBackend::set_config_flags),
    void (*)(unsigned int) noexcept>);
static_assert(std::is_same_v<
    decltype(platform::HostWindowBackend::init_window),
    void (*)(int, int, const char*) noexcept>);
static_assert(std::is_same_v<
    decltype(platform::HostWindowBackend::is_window_ready),
    bool (*)() noexcept>);
static_assert(std::is_same_v<
    decltype(platform::HostWindowBackend::close_window),
    void (*)() noexcept>);

enum class BackendEvent : unsigned char {
    set_flags,
    init_window,
    query_ready,
    close_window,
};

struct CountingBackend final {
    bool is_ready{};
    unsigned int flags{};
    int width{};
    int height{};
    const char* title{};
    std::array<BackendEvent, 4U> events{};
    std::size_t event_count{};
    std::size_t close_count{};
};

CountingBackend* active_backend{};

void record(BackendEvent event) noexcept {
    active_backend->events[active_backend->event_count++] = event;
}

void set_config_flags(unsigned int flags) noexcept {
    active_backend->flags = flags;
    record(BackendEvent::set_flags);
}

void init_window(int width, int height, const char* title) noexcept {
    active_backend->width = width;
    active_backend->height = height;
    active_backend->title = title;
    record(BackendEvent::init_window);
}

bool is_window_ready() noexcept {
    record(BackendEvent::query_ready);
    return active_backend->is_ready;
}

void close_window() noexcept {
    ++active_backend->close_count;
    record(BackendEvent::close_window);
}

[[nodiscard]] platform::HostWindowBackend backend_for(
    CountingBackend& backend) noexcept {
    active_backend = &backend;
    return {&set_config_flags, &init_window, &is_window_ready, &close_window};
}

[[nodiscard]] platform::RaylibHostConfig configured_host() noexcept {
    platform::RaylibHostConfig config{};
    config.window_width = 960;
    config.window_height = 540;
    config.window_title = "lifetime test";
    return config;
}

arpg::test::Failure failed_initialization_leaves_window_unowned() noexcept {
    CountingBackend backend{};
    const settings::SettingsData settings = settings::default_settings();
    platform::HostWindowLifetime lifetime{backend_for(backend)};

    ARPG_REQUIRE(!lifetime.initialize(configured_host(), settings));
    ARPG_REQUIRE(!lifetime.ready());
    ARPG_REQUIRE(backend.event_count == 3U);
    ARPG_REQUIRE(backend.events[0] == BackendEvent::set_flags);
    ARPG_REQUIRE(backend.events[1] == BackendEvent::init_window);
    ARPG_REQUIRE(backend.events[2] == BackendEvent::query_ready);
    ARPG_REQUIRE(backend.close_count == 0U);
    return {};
}

arpg::test::Failure incomplete_backend_is_rejected_before_any_callback()
    noexcept {
    const settings::SettingsData settings = settings::default_settings();
    for (std::size_t missing = 0U; missing < 4U; ++missing) {
        CountingBackend backend{true};
        platform::HostWindowBackend callbacks = backend_for(backend);
        if (missing == 0U) callbacks.set_config_flags = nullptr;
        if (missing == 1U) callbacks.init_window = nullptr;
        if (missing == 2U) callbacks.is_window_ready = nullptr;
        if (missing == 3U) callbacks.close_window = nullptr;
        platform::HostWindowLifetime lifetime{callbacks};

        ARPG_REQUIRE(!lifetime.initialize(configured_host(), settings));
        ARPG_REQUIRE(!lifetime.ready());
        ARPG_REQUIRE(backend.event_count == 0U);
        ARPG_REQUIRE(backend.close_count == 0U);
        lifetime.close();
        ARPG_REQUIRE(backend.event_count == 0U);
    }
    return {};
}

arpg::test::Failure explicit_close_is_idempotent_after_ready_initialization()
    noexcept {
    CountingBackend backend{true};
    const settings::SettingsData settings = settings::default_settings();
    platform::HostWindowLifetime lifetime{backend_for(backend)};

    ARPG_REQUIRE(lifetime.initialize(configured_host(), settings));
    ARPG_REQUIRE(lifetime.ready());
    ARPG_REQUIRE(backend.width == 960);
    ARPG_REQUIRE(backend.height == 540);
    ARPG_REQUIRE(backend.title == configured_host().window_title);
    lifetime.close();
    lifetime.close();
    ARPG_REQUIRE(!lifetime.ready());
    ARPG_REQUIRE(backend.close_count == 1U);
    ARPG_REQUIRE(backend.event_count == 4U);
    ARPG_REQUIRE(backend.events[3] == BackendEvent::close_window);
    return {};
}

void initialize_then_return(CountingBackend& backend) noexcept {
    const settings::SettingsData settings = settings::default_settings();
    platform::HostWindowLifetime lifetime{backend_for(backend)};
    static_cast<void>(lifetime.initialize(configured_host(), settings));
}

void initialize_then_throw(CountingBackend& backend) {
    const settings::SettingsData settings = settings::default_settings();
    platform::HostWindowLifetime lifetime{backend_for(backend)};
    if (!lifetime.initialize(configured_host(), settings)) throw 1;
    throw 2;
}

arpg::test::Failure destructor_closes_ready_window_on_early_return_and_exception()
    noexcept {
    CountingBackend early_return_backend{true};
    initialize_then_return(early_return_backend);
    ARPG_REQUIRE(early_return_backend.close_count == 1U);

    CountingBackend exception_backend{true};
    try {
        initialize_then_throw(exception_backend);
    } catch (int error) {
        ARPG_REQUIRE(error == 2);
    }
    ARPG_REQUIRE(exception_backend.close_count == 1U);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"failed initialization leaves window unowned",
        &failed_initialization_leaves_window_unowned},
    {"incomplete backend is rejected before any callback",
        &incomplete_backend_is_rejected_before_any_callback},
    {"explicit close is idempotent after ready initialization",
        &explicit_close_is_idempotent_after_ready_initialization},
    {"destructor closes ready window on early return and exception",
        &destructor_closes_ready_window_on_early_return_and_exception},
};

}  // namespace

arpg::test::TestSuite host_window_lifetime_suite() noexcept {
    return arpg::test::make_suite("host_window_lifetime", kCases);
}

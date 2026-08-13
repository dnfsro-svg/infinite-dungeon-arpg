#include "test_framework.hpp"

#include "host_settings_runtime.hpp"
#include "pause_menu_state.hpp"
#include "platform/settings/settings_codec.hpp"
#include "platform/settings/settings_store.hpp"
#include "raylib_host.hpp"

#include <cstddef>
#include <cstdint>
#include <cstring>
#include <filesystem>
#include <type_traits>
#include <vector>

namespace {

namespace platform = arpg::platform;
namespace settings = arpg::settings;

static_assert(std::is_aggregate_v<platform::HostSettingsRuntime>);
static_assert(std::is_standard_layout_v<platform::HostSettingsRuntime>);
static_assert(std::is_trivially_copyable_v<platform::HostSettingsRuntime>);

struct MemorySettingsFiles final {
    std::filesystem::path path{};
    std::vector<std::uint8_t> bytes{};
    bool fail_replace{};
    std::size_t replacements{};
    bool write_lock_held{};
};

bool memory_read(void* context, const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes) {
    auto& files = *static_cast<MemorySettingsFiles*>(context);
    if (files.bytes.empty() || files.path != path) return false;
    bytes = files.bytes;
    return true;
}

bool memory_replace(void* context, const std::filesystem::path& path,
    const std::uint8_t* bytes, std::size_t size) {
    auto& files = *static_cast<MemorySettingsFiles*>(context);
    if (files.fail_replace || bytes == nullptr) return false;
    ++files.replacements;
    files.path = path;
    files.bytes.assign(bytes, bytes + size);
    return true;
}

settings::SettingsWriteLockResult memory_acquire_write_lock(
    void* context, const std::filesystem::path&) noexcept {
    auto& files = *static_cast<MemorySettingsFiles*>(context);
    if (files.write_lock_held) {
        return {settings::SettingsWriteLockStatus::busy, nullptr};
    }
    files.write_lock_held = true;
    return {settings::SettingsWriteLockStatus::acquired, &files};
}

void memory_release_write_lock(void* context, void* token) noexcept {
    auto& files = *static_cast<MemorySettingsFiles*>(context);
    if (token == &files) files.write_lock_held = false;
}

[[nodiscard]] settings::SettingsStore memory_store(
    MemorySettingsFiles& files) {
    return settings::SettingsStore{"memory-settings",
        {&files, &memory_read, &memory_replace,
            &memory_acquire_write_lock, &memory_release_write_lock}};
}

void put_memory_settings(MemorySettingsFiles& files,
    const settings::SettingsData& value) {
    files.path = "memory-settings/settings-a.bin";
    const auto encoded = settings::encode_settings(value);
    files.bytes.assign(encoded.begin(), encoded.end());
}

struct FakeBackend final {
    std::uint8_t volume{100U};
    settings::WindowMode mode{settings::WindowMode::windowed};
    bool vsync{true};
    std::size_t fail_volume_call_a{};
    std::size_t fail_volume_call_b{};
    std::size_t volume_calls{};
    std::size_t mode_calls{};
    std::size_t vsync_calls{};
};

bool set_volume(void* context, std::uint8_t volume) noexcept {
    auto& fake = *static_cast<FakeBackend*>(context);
    fake.volume = volume;
    ++fake.volume_calls;
    return fake.volume_calls != fake.fail_volume_call_a
        && fake.volume_calls != fake.fail_volume_call_b;
}

bool set_window_mode(void* context, settings::WindowMode mode) noexcept {
    auto& fake = *static_cast<FakeBackend*>(context);
    fake.mode = mode;
    ++fake.mode_calls;
    return true;
}

bool set_vsync(void* context, bool enabled) noexcept {
    auto& fake = *static_cast<FakeBackend*>(context);
    fake.vsync = enabled;
    ++fake.vsync_calls;
    return true;
}

settings::WindowMode window_mode(void* context) noexcept {
    return static_cast<FakeBackend*>(context)->mode;
}

bool vsync(void* context) noexcept {
    return static_cast<FakeBackend*>(context)->vsync;
}

[[nodiscard]] platform::WindowSettingsBackend backend_for(
    FakeBackend& fake) noexcept {
    return {&fake, &set_volume, &set_window_mode, &set_vsync,
        &window_mode, &vsync};
}

[[nodiscard]] bool same_settings(const settings::SettingsData& lhs,
    const settings::SettingsData& rhs) noexcept {
    return lhs.master_sfx_percent == rhs.master_sfx_percent
        && lhs.sfx_percent == rhs.sfx_percent
        && lhs.music_percent == rhs.music_percent
        && lhs.ambience_percent == rhs.ambience_percent
        && lhs.ui_percent == rhs.ui_percent
        && lhs.window_mode == rhs.window_mode
        && lhs.vsync_enabled == rhs.vsync_enabled
        && lhs.loot_filter_mode == rhs.loot_filter_mode
        && lhs.bindings == rhs.bindings
        && lhs.revision == rhs.revision;
}

[[nodiscard]] bool same_message(const char* lhs, const char* rhs) noexcept {
    return lhs == nullptr || rhs == nullptr
        ? lhs == rhs
        : std::strcmp(lhs, rhs) == 0;
}

[[nodiscard]] bool same_menu(const platform::PauseMenuState& lhs,
    const platform::PauseMenuState& rhs) noexcept {
    return lhs.screen == rhs.screen
        && lhs.selected_row == rhs.selected_row
        && lhs.capture_action == rhs.capture_action
        && same_settings(lhs.committed, rhs.committed)
        && same_settings(lhs.draft, rhs.draft)
        && same_message(lhs.message, rhs.message);
}

[[nodiscard]] settings::SettingsData changed_settings(
    settings::SettingsData value) noexcept {
    value.master_sfx_percent = 75U;
    value.sfx_percent = 65U;
    value.music_percent = 55U;
    value.ambience_percent = 45U;
    value.ui_percent = 35U;
    value.window_mode = settings::WindowMode::fullscreen;
    value.vsync_enabled = false;
    value.loot_filter_mode = settings::LootFilterMode::rare_only;
    static_cast<void>(settings::assign_or_swap(value,
        settings::SettingAction::light_attack, settings::StableKey::q));
    return value;
}

[[nodiscard]] platform::HostSettingsRuntime make_runtime(
    platform::HostSettingsNotice* notice,
    platform::PauseMenuState& pause_menu,
    settings::SettingsData& live,
    settings::SettingsData& input,
    const settings::SettingsStore& store,
    platform::WindowSettingsBackend backend = {}) noexcept {
    return {notice, &pause_menu, &live, &input, &store, backend};
}

arpg::test::Failure recovery_notice_is_consumed_once_on_settings_entry()
    noexcept {
    constexpr settings::SettingsLoadStatus kStatuses[] = {
        settings::SettingsLoadStatus::loaded,
        settings::SettingsLoadStatus::defaults_missing,
        settings::SettingsLoadStatus::recovered_single_slot,
        settings::SettingsLoadStatus::defaults_corrupt,
    };
    for (const settings::SettingsLoadStatus status : kStatuses) {
        const platform::HostSettingsNotice candidate =
            platform::make_host_settings_notice(status);
        ARPG_REQUIRE(candidate.recovered_defaults_pending
            == (status == settings::SettingsLoadStatus::defaults_corrupt));
    }

    MemorySettingsFiles files{};
    const settings::SettingsStore store = memory_store(files);
    platform::HostSettingsNotice notice = platform::make_host_settings_notice(
        settings::SettingsLoadStatus::defaults_corrupt);
    platform::PauseMenuState menu{};
    menu.screen = platform::PauseScreen::root;
    settings::SettingsData live = settings::default_settings();
    settings::SettingsData input = live;
    platform::HostSettingsRuntime runtime = make_runtime(
        &notice, menu, live, input, store);

    runtime.consume_notice(platform::PauseScreen::root);
    ARPG_REQUIRE(notice.recovered_defaults_pending);
    ARPG_REQUIRE(menu.message == nullptr);
    menu.screen = platform::PauseScreen::settings;
    runtime.consume_notice(platform::PauseScreen::closed);
    ARPG_REQUIRE(notice.recovered_defaults_pending);
    ARPG_REQUIRE(menu.message == nullptr);
    runtime.consume_notice(platform::PauseScreen::root);
    ARPG_REQUIRE(!notice.recovered_defaults_pending);
    ARPG_REQUIRE(menu.message != nullptr);
    ARPG_REQUIRE(std::strcmp(menu.message, u8"设置已恢复默认值") == 0);
    menu.message = nullptr;
    runtime.consume_notice(platform::PauseScreen::root);
    ARPG_REQUIRE(menu.message == nullptr);
    return {};
}

arpg::test::Failure preview_updates_live_fields_but_delays_loot_commit()
    noexcept {
    MemorySettingsFiles files{};
    const settings::SettingsStore store = memory_store(files);
    FakeBackend backend{};
    platform::PauseMenuState menu{};
    menu.screen = platform::PauseScreen::settings;
    menu.committed = settings::default_settings();
    menu.draft = changed_settings(menu.committed);
    menu.draft.revision = 41U;
    settings::SettingsData live = menu.committed;
    settings::SettingsData input = menu.committed;
    const settings::SettingsData committed_before = menu.committed;
    const settings::SettingsData draft_before = menu.draft;
    const settings::SettingsData input_before = input;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store, backend_for(backend));

    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::preview, false));
    settings::SettingsData expected = menu.draft;
    expected.loot_filter_mode = menu.committed.loot_filter_mode;
    ARPG_REQUIRE(same_settings(live, expected));
    ARPG_REQUIRE(same_settings(menu.committed, committed_before));
    ARPG_REQUIRE(same_settings(menu.draft, draft_before));
    ARPG_REQUIRE(same_settings(input, input_before));
    ARPG_REQUIRE(menu.message == nullptr);
    ARPG_REQUIRE(backend.volume == expected.master_sfx_percent);
    ARPG_REQUIRE(backend.mode == expected.window_mode);
    ARPG_REQUIRE(backend.vsync == expected.vsync_enabled);
    ARPG_REQUIRE(platform::renderer_loot_filter_mode(
        menu.screen, live, menu.draft) == menu.draft.loot_filter_mode);
    return {};
}

arpg::test::Failure preview_failure_preserves_live_and_reports_message()
    noexcept {
    MemorySettingsFiles files{};
    const settings::SettingsStore store = memory_store(files);
    FakeBackend backend{};
    backend.fail_volume_call_a = 1U;
    platform::PauseMenuState menu{};
    menu.committed = settings::default_settings();
    menu.draft = changed_settings(menu.committed);
    settings::SettingsData live = menu.committed;
    settings::SettingsData input = menu.committed;
    const settings::SettingsData live_before = live;
    const settings::SettingsData committed_before = menu.committed;
    const settings::SettingsData draft_before = menu.draft;
    const settings::SettingsData input_before = input;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store, backend_for(backend));

    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::preview, false));
    ARPG_REQUIRE(same_settings(live, live_before));
    ARPG_REQUIRE(same_settings(menu.committed, committed_before));
    ARPG_REQUIRE(same_settings(menu.draft, draft_before));
    ARPG_REQUIRE(same_settings(input, input_before));
    ARPG_REQUIRE(menu.message != nullptr);
    ARPG_REQUIRE(std::strcmp(menu.message, "Live preview failed") == 0);
    return {};
}

arpg::test::Failure apply_commits_every_field_and_advances_revision() noexcept {
    MemorySettingsFiles files{};
    const settings::SettingsStore store = memory_store(files);
    FakeBackend backend{};
    platform::PauseMenuState menu{};
    menu.committed = settings::default_settings();
    menu.committed.revision = 7U;
    menu.draft = changed_settings(menu.committed);
    menu.draft.revision = 999U;
    put_memory_settings(files, menu.committed);
    settings::SettingsData live = menu.committed;
    settings::SettingsData input = menu.committed;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store, backend_for(backend));

    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::apply, false));
    settings::SettingsData expected = changed_settings(menu.committed);
    expected.revision = 8U;
    ARPG_REQUIRE(same_settings(menu.committed, expected));
    ARPG_REQUIRE(same_settings(menu.draft, expected));
    ARPG_REQUIRE(same_settings(live, expected));
    ARPG_REQUIRE(same_settings(input, expected));
    ARPG_REQUIRE(menu.message != nullptr);
    ARPG_REQUIRE(std::strcmp(menu.message, "Settings saved") == 0);
    return {};
}

arpg::test::Failure save_failure_rolls_back_live_and_reports_retry()
    noexcept {
    MemorySettingsFiles files{};
    files.fail_replace = true;
    const settings::SettingsStore store = memory_store(files);
    FakeBackend backend{};
    platform::PauseMenuState menu{};
    menu.committed = settings::default_settings();
    menu.draft = changed_settings(menu.committed);
    settings::SettingsData live = menu.committed;
    settings::SettingsData input = menu.committed;
    const settings::SettingsData committed = menu.committed;
    const settings::SettingsData edited = menu.draft;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store, backend_for(backend));

    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::apply, false));
    ARPG_REQUIRE(same_settings(live, committed));
    ARPG_REQUIRE(same_settings(input, committed));
    ARPG_REQUIRE(same_settings(menu.committed, committed));
    settings::SettingsData expected_draft = edited;
    expected_draft.loot_filter_mode = committed.loot_filter_mode;
    ARPG_REQUIRE(same_settings(menu.draft, expected_draft));
    ARPG_REQUIRE(menu.message != nullptr);
    ARPG_REQUIRE(std::strcmp(menu.message,
        "Settings save failed; retry") == 0);
    return {};
}

arpg::test::Failure stale_apply_reloads_external_settings() noexcept {
    MemorySettingsFiles files{};
    settings::SettingsData external = settings::default_settings();
    external.revision = 1U;
    external.master_sfx_percent = 40U;
    external.window_mode = settings::WindowMode::fullscreen;
    external.vsync_enabled = false;
    put_memory_settings(files, external);
    const settings::SettingsStore store = memory_store(files);

    FakeBackend backend{};
    platform::PauseMenuState menu{};
    menu.screen = platform::PauseScreen::settings;
    menu.committed = settings::default_settings();
    menu.draft = changed_settings(menu.committed);
    settings::SettingsData live = menu.committed;
    settings::SettingsData input = menu.committed;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store, backend_for(backend));

    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::apply, false));
    ARPG_REQUIRE(files.replacements == 0U);
    ARPG_REQUIRE(same_settings(menu.committed, external));
    ARPG_REQUIRE(same_settings(menu.draft, external));
    ARPG_REQUIRE(same_settings(live, external));
    ARPG_REQUIRE(same_settings(input, external));
    ARPG_REQUIRE(backend.volume == external.master_sfx_percent);
    ARPG_REQUIRE(backend.mode == external.window_mode);
    ARPG_REQUIRE(backend.vsync == external.vsync_enabled);
    ARPG_REQUIRE(menu.message != nullptr);
    ARPG_REQUIRE(std::strcmp(menu.message,
        "Settings changed in another game instance; reloaded") == 0);
    return {};
}

arpg::test::Failure busy_and_corrupt_apply_report_actionable_messages()
    noexcept {
    {
        MemorySettingsFiles files{};
        files.write_lock_held = true;
        const settings::SettingsStore store = memory_store(files);
        FakeBackend backend{};
        platform::PauseMenuState menu{};
        menu.committed = settings::default_settings();
        menu.draft = changed_settings(menu.committed);
        const settings::SettingsData edited = menu.draft;
        settings::SettingsData live = menu.committed;
        settings::SettingsData input = menu.committed;
        platform::HostSettingsRuntime runtime = make_runtime(
            nullptr, menu, live, input, store, backend_for(backend));

        ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::apply, false));
        ARPG_REQUIRE(same_settings(menu.committed,
            settings::default_settings()));
        ARPG_REQUIRE(same_settings(live, menu.committed));
        ARPG_REQUIRE(same_settings(input, menu.committed));
        settings::SettingsData expected_draft = edited;
        expected_draft.loot_filter_mode = menu.committed.loot_filter_mode;
        ARPG_REQUIRE(same_settings(menu.draft, expected_draft));
        ARPG_REQUIRE(menu.message != nullptr);
        ARPG_REQUIRE(std::strcmp(menu.message,
            "Settings save busy in another game instance; try again") == 0);
    }
    {
        MemorySettingsFiles files{};
        files.path = "memory-settings/settings-a.bin";
        files.bytes = {0x01U};
        const settings::SettingsStore store = memory_store(files);
        FakeBackend backend{};
        platform::PauseMenuState menu{};
        menu.committed = settings::default_settings();
        menu.draft = changed_settings(menu.committed);
        settings::SettingsData live = menu.committed;
        settings::SettingsData input = menu.committed;
        platform::HostSettingsRuntime runtime = make_runtime(
            nullptr, menu, live, input, store, backend_for(backend));

        ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::apply, false));
        ARPG_REQUIRE(same_settings(menu.draft, menu.committed));
        ARPG_REQUIRE(same_settings(live, menu.committed));
        ARPG_REQUIRE(same_settings(input, menu.committed));
        ARPG_REQUIRE(menu.message != nullptr);
        ARPG_REQUIRE(std::strcmp(menu.message,
            "Settings storage conflict; restart to recover") == 0);
    }
    return {};
}

arpg::test::Failure rollback_failure_preserves_live_and_reports_message()
    noexcept {
    MemorySettingsFiles files{};
    const settings::SettingsStore store = memory_store(files);
    FakeBackend backend{};
    backend.fail_volume_call_a = 1U;
    platform::PauseMenuState menu{};
    menu.committed = settings::default_settings();
    menu.draft = changed_settings(menu.committed);
    settings::SettingsData live = changed_settings(menu.committed);
    settings::SettingsData input = menu.committed;
    const settings::SettingsData previewed = live;
    const settings::SettingsData committed_before = menu.committed;
    const settings::SettingsData draft_before = menu.draft;
    const settings::SettingsData input_before = input;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store, backend_for(backend));

    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::rollback, false));
    ARPG_REQUIRE(same_settings(live, previewed));
    ARPG_REQUIRE(same_settings(menu.committed, committed_before));
    ARPG_REQUIRE(same_settings(menu.draft, draft_before));
    ARPG_REQUIRE(same_settings(input, input_before));
    ARPG_REQUIRE(menu.message != nullptr);
    ARPG_REQUIRE(std::strcmp(menu.message, "Settings rollback failed") == 0);
    return {};
}

arpg::test::Failure renderer_filter_uses_draft_only_on_settings_screen()
    noexcept {
    settings::SettingsData live = settings::default_settings();
    settings::SettingsData draft = live;
    draft.loot_filter_mode = settings::LootFilterMode::rare_only;
    constexpr platform::PauseScreen kCommittedScreens[] = {
        platform::PauseScreen::closed,
        platform::PauseScreen::root,
        platform::PauseScreen::capture_binding,
        platform::PauseScreen::quit_confirm,
    };
    ARPG_REQUIRE(platform::renderer_loot_filter_mode(
        platform::PauseScreen::settings, live, draft)
        == draft.loot_filter_mode);
    for (const platform::PauseScreen screen : kCommittedScreens) {
        ARPG_REQUIRE(platform::renderer_loot_filter_mode(screen, live, draft)
            == live.loot_filter_mode);
    }
    ARPG_REQUIRE(platform::loot_pickup_policy(live.loot_filter_mode)
            .minimum_rarity
        != platform::loot_pickup_policy(draft.loot_filter_mode)
            .minimum_rarity);
    return {};
}

arpg::test::Failure apply_preview_failure_rolls_back_and_reports_preview()
    noexcept {
    MemorySettingsFiles files{};
    const settings::SettingsStore store = memory_store(files);
    FakeBackend backend{};
    backend.fail_volume_call_a = 1U;
    platform::PauseMenuState menu{};
    menu.committed = settings::default_settings();
    menu.draft = changed_settings(menu.committed);
    const settings::SettingsData edited = menu.draft;
    const settings::SettingsData committed_before = menu.committed;
    settings::SettingsData live = menu.committed;
    settings::SettingsData input = menu.committed;
    const settings::SettingsData input_before = input;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store, backend_for(backend));

    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::apply, false));
    ARPG_REQUIRE(same_settings(live, menu.committed));
    ARPG_REQUIRE(same_settings(menu.committed, committed_before));
    ARPG_REQUIRE(same_settings(input, input_before));
    settings::SettingsData expected_draft = edited;
    expected_draft.loot_filter_mode = menu.committed.loot_filter_mode;
    ARPG_REQUIRE(same_settings(menu.draft, expected_draft));
    ARPG_REQUIRE(std::strcmp(menu.message, "Live preview failed") == 0);
    return {};
}

arpg::test::Failure apply_preview_and_rollback_failure_reports_rollback()
    noexcept {
    MemorySettingsFiles files{};
    const settings::SettingsStore store = memory_store(files);
    FakeBackend backend{};
    backend.fail_volume_call_a = 1U;
    backend.fail_volume_call_b = 3U;
    platform::PauseMenuState menu{};
    menu.committed = settings::default_settings();
    menu.draft = changed_settings(menu.committed);
    menu.draft.master_sfx_percent = 50U;
    settings::SettingsData live = changed_settings(menu.committed);
    backend.volume = live.master_sfx_percent;
    backend.mode = live.window_mode;
    backend.vsync = live.vsync_enabled;
    settings::SettingsData input = menu.committed;
    const settings::SettingsData committed_before = menu.committed;
    const settings::SettingsData input_before = input;
    settings::SettingsData expected_draft = menu.draft;
    expected_draft.loot_filter_mode = menu.committed.loot_filter_mode;
    settings::SettingsData expected_live = live;
    expected_live.loot_filter_mode = menu.committed.loot_filter_mode;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store, backend_for(backend));

    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::apply, false));
    ARPG_REQUIRE(same_settings(live, expected_live));
    ARPG_REQUIRE(same_settings(menu.committed, committed_before));
    ARPG_REQUIRE(same_settings(menu.draft, expected_draft));
    ARPG_REQUIRE(same_settings(input, input_before));
    ARPG_REQUIRE(std::strcmp(menu.message, "Settings rollback failed") == 0);
    return {};
}

arpg::test::Failure apply_save_and_rollback_failure_keeps_previewed_live()
    noexcept {
    MemorySettingsFiles files{};
    files.fail_replace = true;
    const settings::SettingsStore store = memory_store(files);
    FakeBackend backend{};
    backend.fail_volume_call_a = 2U;
    platform::PauseMenuState menu{};
    menu.committed = settings::default_settings();
    menu.draft = changed_settings(menu.committed);
    settings::SettingsData live = menu.committed;
    settings::SettingsData input = menu.committed;
    const settings::SettingsData committed_before = menu.committed;
    const settings::SettingsData input_before = input;
    settings::SettingsData expected_draft = menu.draft;
    expected_draft.loot_filter_mode = menu.committed.loot_filter_mode;
    settings::SettingsData expected_live = menu.draft;
    expected_live.loot_filter_mode = menu.committed.loot_filter_mode;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store, backend_for(backend));

    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::apply, false));
    ARPG_REQUIRE(same_settings(live, expected_live));
    ARPG_REQUIRE(same_settings(menu.committed, committed_before));
    ARPG_REQUIRE(same_settings(menu.draft, expected_draft));
    ARPG_REQUIRE(same_settings(input, input_before));
    ARPG_REQUIRE(std::strcmp(menu.message, "Settings rollback failed") == 0);
    return {};
}

arpg::test::Failure rollback_success_restores_only_live_settings() noexcept {
    MemorySettingsFiles files{};
    const settings::SettingsStore store = memory_store(files);
    FakeBackend backend{};
    platform::PauseMenuState menu{};
    menu.committed = settings::default_settings();
    menu.draft = changed_settings(menu.committed);
    menu.message = "unchanged";
    const settings::SettingsData draft_before = menu.draft;
    settings::SettingsData live = changed_settings(menu.committed);
    backend.volume = live.master_sfx_percent;
    backend.mode = live.window_mode;
    backend.vsync = live.vsync_enabled;
    settings::SettingsData input = menu.committed;
    const settings::SettingsData input_before = input;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store, backend_for(backend));

    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::rollback, false));
    ARPG_REQUIRE(same_settings(live, menu.committed));
    ARPG_REQUIRE(same_settings(menu.draft, draft_before));
    ARPG_REQUIRE(same_settings(input, input_before));
    ARPG_REQUIRE(std::strcmp(menu.message, "unchanged") == 0);
    return {};
}

arpg::test::Failure legacy_apply_wrapper_matches_runtime_field_for_field()
    noexcept {
    MemorySettingsFiles runtime_files{};
    MemorySettingsFiles wrapper_files{};
    const settings::SettingsStore runtime_store = memory_store(runtime_files);
    const settings::SettingsStore wrapper_store = memory_store(wrapper_files);
    FakeBackend runtime_backend{};
    FakeBackend wrapper_backend{};
    platform::PauseMenuState runtime_menu{};
    runtime_menu.committed = settings::default_settings();
    runtime_menu.committed.revision = 3U;
    runtime_menu.draft = changed_settings(runtime_menu.committed);
    platform::PauseMenuState wrapper_menu = runtime_menu;
    put_memory_settings(runtime_files, runtime_menu.committed);
    put_memory_settings(wrapper_files, wrapper_menu.committed);
    settings::SettingsData runtime_live = runtime_menu.committed;
    settings::SettingsData runtime_input = runtime_menu.committed;
    settings::SettingsData wrapper_live = wrapper_menu.committed;
    settings::SettingsData wrapper_input = wrapper_menu.committed;
    platform::HostSettingsRuntime runtime = make_runtime(nullptr, runtime_menu,
        runtime_live, runtime_input, runtime_store,
        backend_for(runtime_backend));

    const bool runtime_exit = runtime.settle(
        platform::PauseCommand::apply, true);
    const bool wrapper_exit = platform::settle_host_pause_command(
        platform::PauseCommand::apply, true, wrapper_menu, wrapper_live,
        wrapper_input, wrapper_store, backend_for(wrapper_backend));
    ARPG_REQUIRE(runtime_exit == wrapper_exit);
    ARPG_REQUIRE(same_settings(runtime_menu.committed,
        wrapper_menu.committed));
    ARPG_REQUIRE(same_settings(runtime_menu.draft, wrapper_menu.draft));
    ARPG_REQUIRE(same_settings(runtime_live, wrapper_live));
    ARPG_REQUIRE(same_settings(runtime_input, wrapper_input));
    ARPG_REQUIRE(std::strcmp(runtime_menu.message, wrapper_menu.message) == 0);
    ARPG_REQUIRE(runtime_backend.volume == wrapper_backend.volume);
    ARPG_REQUIRE(runtime_backend.mode == wrapper_backend.mode);
    ARPG_REQUIRE(runtime_backend.vsync == wrapper_backend.vsync);
    ARPG_REQUIRE(runtime_files.bytes == wrapper_files.bytes);

    constexpr platform::PauseCommand kCommands[] = {
        platform::PauseCommand::preview,
        platform::PauseCommand::rollback,
        platform::PauseCommand::quit,
    };
    for (const platform::PauseCommand command : kCommands) {
        MemorySettingsFiles direct_files{};
        MemorySettingsFiles legacy_files{};
        const settings::SettingsStore direct_store = memory_store(direct_files);
        const settings::SettingsStore legacy_store = memory_store(legacy_files);
        FakeBackend direct_backend{};
        FakeBackend legacy_backend{};
        platform::PauseMenuState direct_menu{};
        direct_menu.screen = platform::PauseScreen::settings;
        direct_menu.committed = settings::default_settings();
        direct_menu.draft = changed_settings(direct_menu.committed);
        direct_menu.message = command == platform::PauseCommand::preview
            ? "stale" : "unchanged";
        settings::SettingsData direct_live =
            command == platform::PauseCommand::rollback
                ? changed_settings(direct_menu.committed)
                : direct_menu.committed;
        direct_backend.volume = direct_live.master_sfx_percent;
        direct_backend.mode = direct_live.window_mode;
        direct_backend.vsync = direct_live.vsync_enabled;
        platform::PauseMenuState legacy_menu = direct_menu;
        settings::SettingsData direct_input = direct_menu.committed;
        settings::SettingsData legacy_live = direct_live;
        settings::SettingsData legacy_input = direct_input;
        legacy_backend = direct_backend;
        platform::HostSettingsRuntime direct = make_runtime(nullptr,
            direct_menu, direct_live, direct_input, direct_store,
            backend_for(direct_backend));

        const bool direct_exit = direct.settle(command, false);
        const bool legacy_exit = platform::settle_host_pause_command(
            command, false, legacy_menu, legacy_live, legacy_input,
            legacy_store, backend_for(legacy_backend));
        ARPG_REQUIRE(direct_exit == legacy_exit);
        ARPG_REQUIRE(same_menu(direct_menu, legacy_menu));
        ARPG_REQUIRE(same_settings(direct_live, legacy_live));
        ARPG_REQUIRE(same_settings(direct_input, legacy_input));
        ARPG_REQUIRE(direct_backend.volume == legacy_backend.volume);
        ARPG_REQUIRE(direct_backend.mode == legacy_backend.mode);
        ARPG_REQUIRE(direct_backend.vsync == legacy_backend.vsync);
        ARPG_REQUIRE(direct_files.bytes == legacy_files.bytes);
    }

    platform::HostSettingsNotice direct_notice =
        platform::make_host_settings_notice(
            settings::SettingsLoadStatus::defaults_corrupt);
    platform::HostSettingsNotice legacy_notice = direct_notice;
    platform::PauseMenuState direct_notice_menu{};
    direct_notice_menu.screen = platform::PauseScreen::settings;
    platform::PauseMenuState legacy_notice_menu = direct_notice_menu;
    settings::SettingsData notice_live = settings::default_settings();
    settings::SettingsData notice_input = notice_live;
    platform::HostSettingsRuntime notice_runtime = make_runtime(
        &direct_notice, direct_notice_menu, notice_live, notice_input,
        runtime_store);
    notice_runtime.consume_notice(platform::PauseScreen::root);
    platform::consume_host_settings_notice(legacy_notice,
        platform::PauseScreen::root, legacy_notice_menu);
    ARPG_REQUIRE(direct_notice.recovered_defaults_pending
        == legacy_notice.recovered_defaults_pending);
    ARPG_REQUIRE(same_menu(direct_notice_menu, legacy_notice_menu));
    return {};
}

arpg::test::Failure quit_and_window_close_requests_are_preserved() noexcept {
    MemorySettingsFiles files{};
    const settings::SettingsStore store = memory_store(files);
    platform::PauseMenuState menu{};
    menu.screen = platform::PauseScreen::settings;
    menu.committed = settings::default_settings();
    menu.draft = changed_settings(menu.committed);
    menu.message = "unchanged";
    settings::SettingsData live = changed_settings(menu.committed);
    settings::SettingsData input = menu.committed;
    const platform::PauseMenuState menu_before = menu;
    const settings::SettingsData live_before = live;
    const settings::SettingsData input_before = input;
    platform::HostSettingsRuntime runtime = make_runtime(
        nullptr, menu, live, input, store);

    ARPG_REQUIRE(runtime.settle(platform::PauseCommand::none, true));
    ARPG_REQUIRE(!runtime.settle(platform::PauseCommand::resume, false));
    ARPG_REQUIRE(runtime.settle(platform::PauseCommand::quit, false));
    ARPG_REQUIRE(same_menu(menu, menu_before));
    ARPG_REQUIRE(same_settings(live, live_before));
    ARPG_REQUIRE(same_settings(input, input_before));
    ARPG_REQUIRE(files.bytes.empty());
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"recovery notice is consumed once",
        &recovery_notice_is_consumed_once_on_settings_entry},
    {"preview updates live but delays loot commit",
        &preview_updates_live_fields_but_delays_loot_commit},
    {"preview failure reports message",
        &preview_failure_preserves_live_and_reports_message},
    {"Apply commits fields and revision",
        &apply_commits_every_field_and_advances_revision},
    {"save failure rolls back and reports retry",
        &save_failure_rolls_back_live_and_reports_retry},
    {"stale Apply reloads external settings",
        &stale_apply_reloads_external_settings},
    {"busy and corrupt Apply report actionable messages",
        &busy_and_corrupt_apply_report_actionable_messages},
    {"rollback failure reports message",
        &rollback_failure_preserves_live_and_reports_message},
    {"renderer filter previews only on settings",
        &renderer_filter_uses_draft_only_on_settings_screen},
    {"Apply preview failure rolls back",
        &apply_preview_failure_rolls_back_and_reports_preview},
    {"Apply preview and rollback failure reports rollback",
        &apply_preview_and_rollback_failure_reports_rollback},
    {"Apply save and rollback failure keeps preview",
        &apply_save_and_rollback_failure_keeps_previewed_live},
    {"rollback success restores only live settings",
        &rollback_success_restores_only_live_settings},
    {"legacy Apply wrapper matches runtime",
        &legacy_apply_wrapper_matches_runtime_field_for_field},
    {"quit and close requests are preserved",
        &quit_and_window_close_requests_are_preserved},
};

}  // namespace

arpg::test::TestSuite host_settings_runtime_suite() noexcept {
    return arpg::test::make_suite("host_settings_runtime", kCases);
}

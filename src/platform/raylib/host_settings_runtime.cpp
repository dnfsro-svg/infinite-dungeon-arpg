#include "host_settings_runtime.hpp"

#include "pause_menu_state.hpp"
#include "platform/settings/settings_store.hpp"
#include "raylib_host.hpp"

#include <cassert>

namespace arpg::platform {
namespace {

constexpr char kSettingsPreviewFailed[] = "Live preview failed";
constexpr char kSettingsSaveFailed[] = "Settings save failed; retry";
constexpr char kSettingsSaveBusy[] =
    "Settings save busy in another game instance; try again";
constexpr char kSettingsRollbackFailed[] = "Settings rollback failed";
constexpr char kSettingsSaved[] = "Settings saved";
constexpr char kSettingsReloadedElsewhere[] =
    "Settings changed in another game instance; reloaded";
constexpr char kSettingsStorageConflict[] =
    "Settings storage conflict; restart to recover";
constexpr char kSettingsRecoveredDefaults[] = u8"设置已恢复默认值";

}  // namespace

HostSettingsNotice make_host_settings_notice(
    settings::SettingsLoadStatus status) noexcept {
    return {status == settings::SettingsLoadStatus::defaults_corrupt};
}

settings::LootFilterMode renderer_loot_filter_mode(
    PauseScreen screen,
    const settings::SettingsData& live_settings,
    const settings::SettingsData& draft_settings) noexcept {
    return screen == PauseScreen::settings
        ? draft_settings.loot_filter_mode
        : live_settings.loot_filter_mode;
}

void HostSettingsRuntime::consume_notice(
    PauseScreen previous_screen) noexcept {
    assert(notice != nullptr && pause_menu != nullptr);
    if (!notice->recovered_defaults_pending
            || previous_screen != PauseScreen::root
            || pause_menu->screen != PauseScreen::settings) {
        return;
    }
    pause_menu->message = kSettingsRecoveredDefaults;
    notice->recovered_defaults_pending = false;
}

bool HostSettingsRuntime::settle(
    PauseCommand command,
    bool window_close_requested) {
    assert(pause_menu != nullptr && live != nullptr && input != nullptr
        && store != nullptr);
    switch (command) {
    case PauseCommand::none:
        break;
    case PauseCommand::preview: {
        settings::SettingsData preview_settings = pause_menu->draft;
        preview_settings.loot_filter_mode =
            pause_menu->committed.loot_filter_mode;
        const LiveSettingsResult result = apply_live_settings(
            *live, preview_settings, backend);
        if (result == LiveSettingsResult::applied) {
            *live = preview_settings;
            pause_menu->message = nullptr;
        } else {
            pause_menu->message = kSettingsPreviewFailed;
        }
        break;
    }
    case PauseCommand::apply: {
        settings::SettingsData preview_settings = pause_menu->draft;
        preview_settings.loot_filter_mode =
            pause_menu->committed.loot_filter_mode;
        const LiveSettingsResult preview = apply_live_settings(
            *live, preview_settings, backend);
        if (preview != LiveSettingsResult::applied) {
            const LiveSettingsResult rollback = rollback_live_settings(
                *live, pause_menu->committed, backend);
            this->live->loot_filter_mode =
                this->pause_menu->committed.loot_filter_mode;
            pause_menu->draft.loot_filter_mode =
                pause_menu->committed.loot_filter_mode;
            if (rollback == LiveSettingsResult::applied) {
                *live = pause_menu->committed;
                pause_menu->message = kSettingsPreviewFailed;
            } else {
                pause_menu->message = kSettingsRollbackFailed;
            }
            break;
        }
        const settings::SettingsData previewed = preview_settings;
        *live = previewed;
        settings::SettingsData save_draft = pause_menu->draft;
        save_draft.revision = pause_menu->committed.revision;
        const settings::SettingsSaveResult saved = store->save(
            pause_menu->committed, save_draft);
        if (saved.status == settings::SettingsSaveStatus::committed) {
            pause_menu->committed = saved.settings;
            pause_menu->draft = saved.settings;
            *live = saved.settings;
            *input = saved.settings;
            pause_menu->message = kSettingsSaved;
            break;
        }
        if (saved.status == settings::SettingsSaveStatus::busy) {
            const LiveSettingsResult rollback = rollback_live_settings(
                previewed, pause_menu->committed, backend);
            this->live->loot_filter_mode =
                this->pause_menu->committed.loot_filter_mode;
            pause_menu->draft.loot_filter_mode =
                pause_menu->committed.loot_filter_mode;
            if (rollback == LiveSettingsResult::applied) {
                *live = pause_menu->committed;
                pause_menu->message = kSettingsSaveBusy;
            } else {
                pause_menu->message = kSettingsRollbackFailed;
            }
            break;
        }
        if (saved.status == settings::SettingsSaveStatus::storage_conflict) {
            const LiveSettingsResult rollback = rollback_live_settings(
                previewed, pause_menu->committed, backend);
            this->live->loot_filter_mode =
                this->pause_menu->committed.loot_filter_mode;
            pause_menu->draft = pause_menu->committed;
            if (rollback == LiveSettingsResult::applied) {
                *live = pause_menu->committed;
                pause_menu->message = kSettingsStorageConflict;
            } else {
                pause_menu->message = kSettingsRollbackFailed;
            }
            break;
        }
        if (saved.status == settings::SettingsSaveStatus::stale_revision) {
            const settings::SettingsLoadResult reloaded = store->load();
            const bool loadable =
                reloaded.status == settings::SettingsLoadStatus::loaded
                || reloaded.status
                    == settings::SettingsLoadStatus::recovered_single_slot;
            if (loadable && apply_live_settings(
                    previewed, reloaded.settings, backend)
                    == LiveSettingsResult::applied) {
                pause_menu->committed = reloaded.settings;
                pause_menu->draft = reloaded.settings;
                *live = reloaded.settings;
                *input = reloaded.settings;
                pause_menu->message = kSettingsReloadedElsewhere;
                break;
            }

            const LiveSettingsResult rollback = rollback_live_settings(
                previewed, pause_menu->committed, backend);
            this->live->loot_filter_mode =
                this->pause_menu->committed.loot_filter_mode;
            pause_menu->draft = pause_menu->committed;
            if (rollback == LiveSettingsResult::applied) {
                *live = pause_menu->committed;
                pause_menu->message = kSettingsStorageConflict;
            } else {
                pause_menu->message = kSettingsRollbackFailed;
            }
            break;
        }
        const LiveSettingsResult rollback = rollback_live_settings(
            previewed, pause_menu->committed, backend);
        this->live->loot_filter_mode =
            this->pause_menu->committed.loot_filter_mode;
        pause_menu->draft.loot_filter_mode =
            pause_menu->committed.loot_filter_mode;
        if (rollback == LiveSettingsResult::applied) {
            *live = pause_menu->committed;
            pause_menu->message = kSettingsSaveFailed;
        } else {
            pause_menu->message = kSettingsRollbackFailed;
        }
        break;
    }
    case PauseCommand::rollback: {
        const LiveSettingsResult result = rollback_live_settings(
            *live, pause_menu->committed, backend);
        if (result == LiveSettingsResult::applied) {
            *live = pause_menu->committed;
        } else {
            pause_menu->message = kSettingsRollbackFailed;
        }
        break;
    }
    case PauseCommand::resume:
        break;
    case PauseCommand::quit:
        return true;
    }
    return window_close_requested;
}

void consume_host_settings_notice(
    HostSettingsNotice& notice,
    PauseScreen previous_screen,
    PauseMenuState& pause_menu) noexcept {
    HostSettingsRuntime runtime{&notice, &pause_menu};
    runtime.consume_notice(previous_screen);
}

bool settle_host_pause_command(
    PauseCommand command,
    bool window_close_requested,
    PauseMenuState& pause_menu,
    settings::SettingsData& live_settings,
    settings::SettingsData& input_settings,
    const settings::SettingsStore& settings_store,
    WindowSettingsBackend settings_backend) {
    HostSettingsRuntime runtime{nullptr, &pause_menu, &live_settings,
        &input_settings, &settings_store, settings_backend};
    return runtime.settle(command, window_close_requested);
}

}  // namespace arpg::platform

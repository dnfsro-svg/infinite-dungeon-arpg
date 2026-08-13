#include "test_framework.hpp"

#include "platform/settings/settings_codec.hpp"
#include "platform/settings/settings_store.hpp"

#include <cstddef>
#include <cstdint>
#include <filesystem>
#include <limits>
#include <new>
#include <stdexcept>
#include <string>
#include <unordered_map>
#include <utility>
#include <vector>

namespace {

using arpg::settings::SettingsData;
using arpg::settings::SettingsFileOps;
using arpg::settings::SettingsLoadStatus;
using arpg::settings::SettingsSaveStatus;
using arpg::settings::SettingsStore;

struct FakeFiles final {
    enum class ReplaceFault : std::uint8_t { none, write, publish };
    enum class ReadFault : std::uint8_t {
        none,
        bad_alloc_after_replace,
        runtime_error_after_replace
    };

    std::unordered_map<std::string, std::vector<std::uint8_t>> files{};
    std::vector<std::string> reads{};
    std::vector<std::string> replacements{};
    ReplaceFault replace_fault{ReplaceFault::none};
    ReadFault read_fault{ReadFault::none};
    bool fail_readback{};
    bool corrupt_readback{};
    bool replacement_completed{};
    bool write_lock_held{};
    std::size_t write_lock_acquires{};
    std::size_t write_lock_releases{};
};

[[nodiscard]] std::string file_name(const std::filesystem::path& path) {
    return path.filename().string();
}

bool fake_read(
    void* context,
    const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes) {
    auto& fake = *static_cast<FakeFiles*>(context);
    const std::string name = file_name(path);
    fake.reads.push_back(name);
    if (fake.replacement_completed) {
        if (fake.read_fault == FakeFiles::ReadFault::bad_alloc_after_replace) {
            throw std::bad_alloc{};
        }
        if (fake.read_fault == FakeFiles::ReadFault::runtime_error_after_replace) {
            throw std::runtime_error{"injected readback failure"};
        }
    }
    if (fake.replacement_completed && fake.fail_readback) {
        return false;
    }
    const auto found = fake.files.find(name);
    if (found == fake.files.end()) {
        return false;
    }
    bytes = found->second;
    if (fake.replacement_completed && fake.corrupt_readback && !bytes.empty()) {
        bytes.back() ^= 0x01U;
    }
    return true;
}

bool fake_replace(
    void* context,
    const std::filesystem::path& path,
    const std::uint8_t* bytes,
    std::size_t size) {
    auto& fake = *static_cast<FakeFiles*>(context);
    const std::string name = file_name(path);
    fake.replacements.push_back(name);
    if (fake.replace_fault == FakeFiles::ReplaceFault::write) {
        return false;
    }
    if (fake.replace_fault == FakeFiles::ReplaceFault::publish) {
        fake.files[name + ".tmp"] = std::vector<std::uint8_t>(bytes, bytes + size);
        return false;
    }
    fake.files[name] = std::vector<std::uint8_t>(bytes, bytes + size);
    fake.replacement_completed = true;
    return true;
}

arpg::settings::SettingsWriteLockResult fake_acquire_write_lock(
    void* context, const std::filesystem::path&) noexcept {
    auto& fake = *static_cast<FakeFiles*>(context);
    if (fake.write_lock_held) {
        return {arpg::settings::SettingsWriteLockStatus::busy, nullptr};
    }
    fake.write_lock_held = true;
    ++fake.write_lock_acquires;
    return {arpg::settings::SettingsWriteLockStatus::acquired, &fake};
}

void fake_release_write_lock(void* context, void* token) noexcept {
    auto& fake = *static_cast<FakeFiles*>(context);
    if (token != &fake) {
        return;
    }
    fake.write_lock_held = false;
    ++fake.write_lock_releases;
}

[[nodiscard]] SettingsFileOps fake_ops(FakeFiles& fake) noexcept {
    return {&fake, fake_read, fake_replace,
        fake_acquire_write_lock, fake_release_write_lock};
}

[[nodiscard]] SettingsData settings_at(
    std::uint64_t revision,
    std::uint8_t volume = 100U) noexcept {
    SettingsData settings = arpg::settings::default_settings();
    settings.revision = revision;
    settings.master_sfx_percent = volume;
    return settings;
}

void put(FakeFiles& fake, const char* name, const SettingsData& settings) {
    const auto bytes = arpg::settings::encode_settings(settings);
    fake.files[name] = {bytes.begin(), bytes.end()};
}

[[nodiscard]] bool same_settings(
    const SettingsData& lhs, const SettingsData& rhs) noexcept {
    return lhs.master_sfx_percent == rhs.master_sfx_percent &&
        lhs.sfx_percent == rhs.sfx_percent &&
        lhs.music_percent == rhs.music_percent &&
        lhs.ambience_percent == rhs.ambience_percent &&
        lhs.ui_percent == rhs.ui_percent &&
        lhs.window_mode == rhs.window_mode &&
        lhs.vsync_enabled == rhs.vsync_enabled &&
        lhs.loot_filter_mode == rhs.loot_filter_mode &&
        lhs.bindings == rhs.bindings &&
        lhs.revision == rhs.revision;
}

arpg::test::Failure missing_slots_load_defaults_without_writes() noexcept {
    FakeFiles fake{};
    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
    ARPG_REQUIRE(result.status == SettingsLoadStatus::defaults_missing);
    ARPG_REQUIRE(same_settings(result.settings, arpg::settings::default_settings()));
    ARPG_REQUIRE(fake.replacements.empty());
    ARPG_REQUIRE(fake.files.empty());
    return {};
}

arpg::test::Failure newest_slot_is_selected_in_either_position() noexcept {
    FakeFiles fake{};
    put(fake, "settings-a.bin", settings_at(7U, 70U));
    put(fake, "settings-b.bin", settings_at(8U, 80U));
    auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
    ARPG_REQUIRE(result.status == SettingsLoadStatus::loaded);
    ARPG_REQUIRE(result.settings.revision == 8U);
    ARPG_REQUIRE(result.settings.master_sfx_percent == 80U);

    put(fake, "settings-a.bin", settings_at(9U, 90U));
    result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
    ARPG_REQUIRE(result.status == SettingsLoadStatus::loaded);
    ARPG_REQUIRE(result.settings.revision == 9U);
    ARPG_REQUIRE(result.settings.master_sfx_percent == 90U);
    return {};
}

arpg::test::Failure revision_selection_does_not_treat_wrap_as_newer() noexcept {
    FakeFiles fake{};
    put(fake, "settings-a.bin", settings_at(
        std::numeric_limits<std::uint64_t>::max() - 1U, 95U));
    put(fake, "settings-b.bin", settings_at(5U, 50U));
    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
    ARPG_REQUIRE(result.status == SettingsLoadStatus::loaded);
    ARPG_REQUIRE(result.settings.revision ==
        std::numeric_limits<std::uint64_t>::max() - 1U);
    ARPG_REQUIRE(result.settings.master_sfx_percent == 95U);
    return {};
}

arpg::test::Failure one_valid_slot_recovers_without_repairing_on_load() noexcept {
    FakeFiles fake{};
    put(fake, "settings-b.bin", settings_at(3U, 75U));
    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
    ARPG_REQUIRE(result.status == SettingsLoadStatus::recovered_single_slot);
    ARPG_REQUIRE(result.settings.revision == 3U);
    ARPG_REQUIRE(fake.replacements.empty());
    ARPG_REQUIRE(fake.files.count("settings-a.bin") == 0U);
    return {};
}

arpg::test::Failure equal_revision_identical_slots_are_loadable() noexcept {
    FakeFiles fake{};
    const SettingsData settings = settings_at(11U, 85U);
    put(fake, "settings-a.bin", settings);
    put(fake, "settings-b.bin", settings);
    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
    ARPG_REQUIRE(result.status == SettingsLoadStatus::loaded);
    ARPG_REQUIRE(same_settings(result.settings, settings));
    return {};
}

arpg::test::Failure equal_revision_conflict_loads_corrupt_defaults() noexcept {
    FakeFiles fake{};
    const SettingsData slot_a = settings_at(11U, 85U);
    SettingsData slot_b = slot_a;
    slot_b.loot_filter_mode = arpg::settings::LootFilterMode::rare_only;
    put(fake, "settings-a.bin", slot_a);
    put(fake, "settings-b.bin", slot_b);
    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
    ARPG_REQUIRE(result.status == SettingsLoadStatus::defaults_corrupt);
    ARPG_REQUIRE(same_settings(result.settings, arpg::settings::default_settings()));
    ARPG_REQUIRE(fake.replacements.empty());
    return {};
}

arpg::test::Failure bad_crc_loads_corrupt_defaults_without_deleting() noexcept {
    FakeFiles fake{};
    put(fake, "settings-a.bin", settings_at(4U, 80U));
    fake.files["settings-a.bin"].back() ^= 0x01U;
    const auto before = fake.files["settings-a.bin"];
    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
    ARPG_REQUIRE(result.status == SettingsLoadStatus::defaults_corrupt);
    ARPG_REQUIRE(fake.files["settings-a.bin"] == before);
    ARPG_REQUIRE(fake.replacements.empty());
    return {};
}

arpg::test::Failure save_validates_revision_and_settings_before_io() noexcept {
    FakeFiles fake{};
    const SettingsData committed = settings_at(4U);
    SettingsData draft = committed;
    draft.revision = 3U;
    auto result = SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
    ARPG_REQUIRE(result.status == SettingsSaveStatus::stale_revision);
    ARPG_REQUIRE(same_settings(result.settings, committed));

    draft = committed;
    draft.master_sfx_percent = 99U;
    result = SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
    ARPG_REQUIRE(result.status == SettingsSaveStatus::invalid_settings);
    ARPG_REQUIRE(same_settings(result.settings, committed));
    ARPG_REQUIRE(fake.reads.empty());
    ARPG_REQUIRE(fake.replacements.empty());
    return {};
}

arpg::test::Failure save_rejects_revision_overflow_without_io() noexcept {
    FakeFiles fake{};
    const SettingsData committed = settings_at(
        std::numeric_limits<std::uint64_t>::max());
    auto result =
        SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, committed);
    ARPG_REQUIRE(result.status == SettingsSaveStatus::revision_overflow);
    ARPG_REQUIRE(same_settings(result.settings, committed));

    SettingsData invalid_draft = committed;
    invalid_draft.master_sfx_percent = 99U;
    result = SettingsStore{"C:/settings", fake_ops(fake)}.save(
        committed, invalid_draft);
    ARPG_REQUIRE(result.status == SettingsSaveStatus::revision_overflow);
    ARPG_REQUIRE(same_settings(result.settings, committed));
    ARPG_REQUIRE(fake.reads.empty());
    ARPG_REQUIRE(fake.replacements.empty());
    return {};
}

arpg::test::Failure save_writes_older_slot_and_returns_readback_value() noexcept {
    FakeFiles fake{};
    const SettingsData committed = settings_at(8U, 80U);
    put(fake, "settings-a.bin", settings_at(7U, 70U));
    put(fake, "settings-b.bin", committed);
    SettingsData draft = committed;
    draft.master_sfx_percent = 90U;
    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
    ARPG_REQUIRE(result.status == SettingsSaveStatus::committed);
    ARPG_REQUIRE(result.settings.revision == 9U);
    ARPG_REQUIRE(result.settings.master_sfx_percent == 90U);
    ARPG_REQUIRE(fake.replacements.size() == 1U);
    ARPG_REQUIRE(fake.replacements.front() == "settings-a.bin");
    const auto preserved = arpg::settings::decode_settings(
        fake.files["settings-b.bin"].data(), fake.files["settings-b.bin"].size());
    ARPG_REQUIRE(preserved.settings.revision == 8U);
    return {};
}

arpg::test::Failure stale_second_instance_does_not_create_conflicting_slots() noexcept {
    FakeFiles fake{};
    const SettingsStore first{"C:/settings", fake_ops(fake)};
    const SettingsStore second{"C:/settings", fake_ops(fake)};

    const auto first_loaded = first.load();
    const auto second_loaded = second.load();
    ARPG_REQUIRE(first_loaded.status == SettingsLoadStatus::defaults_missing);
    ARPG_REQUIRE(second_loaded.status == SettingsLoadStatus::defaults_missing);

    SettingsData first_draft = first_loaded.settings;
    first_draft.master_sfx_percent = 90U;
    const auto first_saved = first.save(first_loaded.settings, first_draft);
    ARPG_REQUIRE(first_saved.status == SettingsSaveStatus::committed);
    ARPG_REQUIRE(first_saved.settings.revision == 1U);
    ARPG_REQUIRE(first_saved.settings.master_sfx_percent == 90U);
    ARPG_REQUIRE(fake.replacements.size() == 1U);

    SettingsData stale_draft = second_loaded.settings;
    stale_draft.master_sfx_percent = 80U;
    const auto stale_saved = second.save(second_loaded.settings, stale_draft);
    ARPG_REQUIRE(stale_saved.status == SettingsSaveStatus::stale_revision);
    ARPG_REQUIRE(same_settings(stale_saved.settings, second_loaded.settings));
    ARPG_REQUIRE(fake.replacements.size() == 1U);
    ARPG_REQUIRE(fake.write_lock_acquires == 2U);
    ARPG_REQUIRE(fake.write_lock_releases == 2U);

    const auto reloaded = first.load();
    ARPG_REQUIRE(reloaded.status != SettingsLoadStatus::defaults_corrupt);
    ARPG_REQUIRE(same_settings(reloaded.settings, first_saved.settings));
    return {};
}

arpg::test::Failure missing_storage_rejects_nondefault_committed_settings() noexcept {
    FakeFiles fake{};
    const SettingsData committed = settings_at(0U, 90U);
    SettingsData draft = committed;
    draft.master_sfx_percent = 95U;

    const auto result =
        SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);

    ARPG_REQUIRE(result.status == SettingsSaveStatus::stale_revision);
    ARPG_REQUIRE(same_settings(result.settings, committed));
    ARPG_REQUIRE(fake.replacements.empty());
    ARPG_REQUIRE(fake.write_lock_acquires == 1U);
    ARPG_REQUIRE(fake.write_lock_releases == 1U);
    return {};
}

arpg::test::Failure corrupt_storage_is_not_overwritten_by_save() noexcept {
    FakeFiles fake{};
    const SettingsData slot_a = settings_at(1U, 85U);
    SettingsData slot_b = slot_a;
    slot_b.loot_filter_mode = arpg::settings::LootFilterMode::rare_only;
    put(fake, "settings-a.bin", slot_a);
    put(fake, "settings-b.bin", slot_b);
    const SettingsData committed = arpg::settings::default_settings();
    SettingsData draft = committed;
    draft.master_sfx_percent = 90U;

    const auto result =
        SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);

    ARPG_REQUIRE(result.status == SettingsSaveStatus::storage_conflict);
    ARPG_REQUIRE(same_settings(result.settings, committed));
    ARPG_REQUIRE(fake.replacements.empty());
    ARPG_REQUIRE(fake.write_lock_acquires == 1U);
    ARPG_REQUIRE(fake.write_lock_releases == 1U);
    return {};
}

arpg::test::Failure busy_write_lock_does_not_read_or_replace() noexcept {
    FakeFiles fake{};
    fake.write_lock_held = true;
    const SettingsData committed = arpg::settings::default_settings();
    SettingsData draft = committed;
    draft.master_sfx_percent = 90U;

    const auto result =
        SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);

    ARPG_REQUIRE(result.status == SettingsSaveStatus::busy);
    ARPG_REQUIRE(same_settings(result.settings, committed));
    ARPG_REQUIRE(fake.reads.empty());
    ARPG_REQUIRE(fake.replacements.empty());
    ARPG_REQUIRE(fake.write_lock_held);
    ARPG_REQUIRE(fake.write_lock_releases == 0U);
    return {};
}

arpg::test::Failure save_requires_complete_write_lock_protocol() noexcept {
    for (const bool missing_acquire : {false, true}) {
        FakeFiles fake{};
        SettingsFileOps ops = fake_ops(fake);
        if (missing_acquire) {
            ops.acquire_write_lock = nullptr;
        } else {
            ops.release_write_lock = nullptr;
        }
        const SettingsData committed = arpg::settings::default_settings();
        SettingsData draft = committed;
        draft.master_sfx_percent = 90U;

        const auto result = SettingsStore{"C:/settings", ops}.save(
            committed, draft);

        ARPG_REQUIRE(result.status == SettingsSaveStatus::write_failed);
        ARPG_REQUIRE(same_settings(result.settings, committed));
        ARPG_REQUIRE(fake.reads.empty());
        ARPG_REQUIRE(fake.replacements.empty());
    }
    return {};
}

arpg::test::Failure write_and_publish_failures_preserve_last_valid_slot() noexcept {
    for (const auto fault : {FakeFiles::ReplaceFault::write,
             FakeFiles::ReplaceFault::publish}) {
        FakeFiles fake{};
        fake.replace_fault = fault;
        const SettingsData committed = settings_at(8U, 80U);
        put(fake, "settings-a.bin", settings_at(7U, 70U));
        put(fake, "settings-b.bin", committed);
        const auto old_slot = fake.files["settings-b.bin"];
        SettingsData draft = committed;
        draft.master_sfx_percent = 90U;
        const auto result =
            SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
        ARPG_REQUIRE(result.status == SettingsSaveStatus::write_failed);
        ARPG_REQUIRE(same_settings(result.settings, committed));
        ARPG_REQUIRE(fake.files["settings-b.bin"] == old_slot);
        const auto reloaded = SettingsStore{"C:/settings", fake_ops(fake)}.load();
        ARPG_REQUIRE(reloaded.settings.revision == 8U);
        ARPG_REQUIRE(reloaded.settings.master_sfx_percent == 80U);
    }
    return {};
}

arpg::test::Failure readback_failure_does_not_publish_to_caller() noexcept {
    for (const bool corrupt : {false, true}) {
        FakeFiles fake{};
        fake.fail_readback = !corrupt;
        fake.corrupt_readback = corrupt;
        const SettingsData committed = settings_at(8U, 80U);
        put(fake, "settings-b.bin", committed);
        SettingsData draft = committed;
        draft.master_sfx_percent = 90U;
        const auto result =
            SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
        ARPG_REQUIRE(result.status == SettingsSaveStatus::readback_failed);
        ARPG_REQUIRE(same_settings(result.settings, committed));
        ARPG_REQUIRE(fake.files.count("settings-b.bin") == 1U);
    }
    return {};
}

arpg::test::Failure readback_exceptions_are_not_classified_as_write_failures() noexcept {
    for (const auto fault : {FakeFiles::ReadFault::bad_alloc_after_replace,
             FakeFiles::ReadFault::runtime_error_after_replace}) {
        FakeFiles fake{};
        fake.read_fault = fault;
        const SettingsData committed = settings_at(8U, 80U);
        put(fake, "settings-b.bin", committed);
        SettingsData draft = committed;
        draft.master_sfx_percent = 90U;

        const auto result =
            SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);

        ARPG_REQUIRE(result.status == SettingsSaveStatus::readback_failed);
        ARPG_REQUIRE(same_settings(result.settings, committed));
        ARPG_REQUIRE(fake.files.count("settings-a.bin") == 1U);
        const auto published = arpg::settings::decode_settings(
            fake.files["settings-a.bin"].data(), fake.files["settings-a.bin"].size());
        ARPG_REQUIRE(published.error == arpg::settings::SettingsCodecError::none);
        ARPG_REQUIRE(published.settings.revision == 9U);
        ARPG_REQUIRE(published.settings.master_sfx_percent == 90U);
    }
    return {};
}

arpg::test::Failure empty_directory_loads_corrupt_defaults_without_io() noexcept {
    FakeFiles fake{};

    const auto result = SettingsStore{
        std::filesystem::path{}, fake_ops(fake)}.load();

    ARPG_REQUIRE(result.status == SettingsLoadStatus::defaults_corrupt);
    ARPG_REQUIRE(same_settings(result.settings, arpg::settings::default_settings()));
    ARPG_REQUIRE(fake.reads.empty());
    ARPG_REQUIRE(fake.replacements.empty());
    return {};
}

arpg::test::Failure empty_directory_save_fails_without_io() noexcept {
    FakeFiles fake{};
    const SettingsData committed = settings_at(8U, 80U);
    SettingsData draft = committed;
    draft.master_sfx_percent = 90U;

    const auto result = SettingsStore{
        std::filesystem::path{}, fake_ops(fake)}.save(committed, draft);

    ARPG_REQUIRE(result.status == SettingsSaveStatus::write_failed);
    ARPG_REQUIRE(same_settings(result.settings, committed));
    ARPG_REQUIRE(fake.reads.empty());
    ARPG_REQUIRE(fake.replacements.empty());
    return {};
}

constexpr arpg::test::TestCase cases[] = {
    {"missing slots load defaults without writes", missing_slots_load_defaults_without_writes},
    {"newest A or B slot is selected", newest_slot_is_selected_in_either_position},
    {"revision selection does not treat wrap as newer", revision_selection_does_not_treat_wrap_as_newer},
    {"one valid slot recovers without repairing on load", one_valid_slot_recovers_without_repairing_on_load},
    {"equal revision identical slots are loadable", equal_revision_identical_slots_are_loadable},
    {"equal revision conflict loads corrupt defaults", equal_revision_conflict_loads_corrupt_defaults},
    {"bad CRC loads corrupt defaults without deleting", bad_crc_loads_corrupt_defaults_without_deleting},
    {"save validates revision and settings before I/O", save_validates_revision_and_settings_before_io},
    {"save rejects revision overflow without I/O", save_rejects_revision_overflow_without_io},
    {"save writes older slot and returns readback value", save_writes_older_slot_and_returns_readback_value},
    {"stale second instance does not create conflicting slots", stale_second_instance_does_not_create_conflicting_slots},
    {"missing storage rejects nondefault committed settings", missing_storage_rejects_nondefault_committed_settings},
    {"corrupt storage is not overwritten by save", corrupt_storage_is_not_overwritten_by_save},
    {"busy write lock does not read or replace", busy_write_lock_does_not_read_or_replace},
    {"save requires complete write lock protocol", save_requires_complete_write_lock_protocol},
    {"write and publish failures preserve last valid slot", write_and_publish_failures_preserve_last_valid_slot},
    {"readback failure does not publish to caller", readback_failure_does_not_publish_to_caller},
    {"readback exceptions are not classified as write failures", readback_exceptions_are_not_classified_as_write_failures},
    {"empty directory loads corrupt defaults without I/O", empty_directory_loads_corrupt_defaults_without_io},
    {"empty directory save fails without I/O", empty_directory_save_fails_without_io}};

}  // namespace

arpg::test::TestSuite settings_store_suite() noexcept {
    return arpg::test::make_suite("settings.store", cases);
}

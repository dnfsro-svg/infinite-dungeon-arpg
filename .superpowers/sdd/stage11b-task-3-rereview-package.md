# Review package: dab6010..538c212

## Commits
538c212 fix: classify settings store failures
bcc2e7d feat: persist settings in atomic dual slots

## Files changed
 src/platform/settings/CMakeLists.txt     |   3 +-
 src/platform/settings/settings_store.cpp | 269 ++++++++++++++++++++++
 src/platform/settings/settings_store.hpp |  64 ++++++
 tests/settings/CMakeLists.txt            |   3 +-
 tests/settings/settings_store_tests.cpp  | 382 +++++++++++++++++++++++++++++++
 tests/settings/settings_test_main.cpp    |   6 +-
 6 files changed, 723 insertions(+), 4 deletions(-)

## Diff
diff --git a/src/platform/settings/CMakeLists.txt b/src/platform/settings/CMakeLists.txt
index 1b2b9f7..50d4f60 100644
--- a/src/platform/settings/CMakeLists.txt
+++ b/src/platform/settings/CMakeLists.txt
@@ -1,8 +1,9 @@
 add_library(arpg_settings STATIC
     settings_types.cpp
-    settings_codec.cpp)
+    settings_codec.cpp
+    settings_store.cpp)
 
 target_include_directories(arpg_settings PUBLIC "${PROJECT_SOURCE_DIR}/src")
 target_compile_features(arpg_settings PUBLIC cxx_std_17)
 arpg_enable_project_warnings(arpg_settings)
 target_link_libraries(arpg_settings PRIVATE arpg_core)
diff --git a/src/platform/settings/settings_store.cpp b/src/platform/settings/settings_store.cpp
new file mode 100644
index 0000000..722d64e
--- /dev/null
+++ b/src/platform/settings/settings_store.cpp
@@ -0,0 +1,269 @@
+#include "platform/settings/settings_store.hpp"
+
+#include "platform/settings/settings_codec.hpp"
+
+#include <array>
+#include <cstddef>
+#include <cstdint>
+#include <filesystem>
+#include <fstream>
+#include <limits>
+#include <string>
+#include <utility>
+#include <vector>
+
+#ifdef _WIN32
+#define WIN32_LEAN_AND_MEAN
+#define NOMINMAX
+#include <Windows.h>
+#endif
+
+namespace arpg::settings {
+namespace {
+
+constexpr const char* slot_a_name = "settings-a.bin";
+constexpr const char* slot_b_name = "settings-b.bin";
+
+enum class SlotState : std::uint8_t { missing, invalid, valid };
+
+struct SlotRecord final {
+    SlotState state{SlotState::missing};
+    SettingsData settings{};
+};
+
+[[nodiscard]] bool same_settings(
+    const SettingsData& lhs, const SettingsData& rhs) noexcept {
+    return lhs.master_sfx_percent == rhs.master_sfx_percent &&
+        lhs.window_mode == rhs.window_mode &&
+        lhs.vsync_enabled == rhs.vsync_enabled &&
+        lhs.bindings == rhs.bindings &&
+        lhs.revision == rhs.revision;
+}
+
+[[nodiscard]] SlotRecord read_slot(
+    const SettingsFileOps& ops,
+    const std::filesystem::path& path) {
+    if (ops.read == nullptr) {
+        return {};
+    }
+    std::vector<std::uint8_t> bytes{};
+    if (!ops.read(ops.context, path, bytes)) {
+        return {};
+    }
+    const auto decoded = decode_settings(bytes.data(), bytes.size());
+    if (decoded.error != SettingsCodecError::none) {
+        return {SlotState::invalid, {}};
+    }
+    return {SlotState::valid, decoded.settings};
+}
+
+[[nodiscard]] bool is_valid(const SlotRecord& slot) noexcept {
+    return slot.state == SlotState::valid;
+}
+
+[[nodiscard]] SettingsLoadResult select_loaded(
+    const SlotRecord& a, const SlotRecord& b) noexcept {
+    const bool valid_a = is_valid(a);
+    const bool valid_b = is_valid(b);
+    if (valid_a && valid_b) {
+        if (a.settings.revision == b.settings.revision) {
+            if (!same_settings(a.settings, b.settings)) {
+                return {SettingsLoadStatus::defaults_corrupt, default_settings()};
+            }
+            return {SettingsLoadStatus::loaded, a.settings};
+        }
+        return {SettingsLoadStatus::loaded,
+            a.settings.revision > b.settings.revision ? a.settings : b.settings};
+    }
+    if (valid_a || valid_b) {
+        return {SettingsLoadStatus::recovered_single_slot,
+            valid_a ? a.settings : b.settings};
+    }
+    if (a.state == SlotState::missing && b.state == SlotState::missing) {
+        return {SettingsLoadStatus::defaults_missing, default_settings()};
+    }
+    return {SettingsLoadStatus::defaults_corrupt, default_settings()};
+}
+
+[[nodiscard]] const char* target_slot_name(
+    const SlotRecord& a, const SlotRecord& b) noexcept {
+    if (!is_valid(a)) {
+        return slot_a_name;
+    }
+    if (!is_valid(b)) {
+        return slot_b_name;
+    }
+    if (a.settings.revision <= b.settings.revision) {
+        return slot_a_name;
+    }
+    return slot_b_name;
+}
+
+bool native_read(
+    void*,
+    const std::filesystem::path& path,
+    std::vector<std::uint8_t>& bytes) {
+    std::ifstream stream(path, std::ios::binary | std::ios::ate);
+    if (!stream) {
+        return false;
+    }
+    const std::streampos end = stream.tellg();
+    if (end < 0) {
+        return false;
+    }
+    bytes.resize(static_cast<std::size_t>(end));
+    stream.seekg(0, std::ios::beg);
+    if (!bytes.empty()) {
+        stream.read(
+            reinterpret_cast<char*>(bytes.data()),
+            static_cast<std::streamsize>(bytes.size()));
+    }
+    return static_cast<bool>(stream);
+}
+
+[[nodiscard]] bool allowed_slot_path(const std::filesystem::path& path) {
+    const auto name = path.filename();
+    return name == slot_a_name || name == slot_b_name;
+}
+
+#ifdef _WIN32
+bool write_and_flush_temp(
+    const std::filesystem::path& temp,
+    const std::uint8_t* bytes,
+    std::size_t size) {
+    HANDLE file = CreateFileW(temp.c_str(), GENERIC_WRITE, 0, nullptr,
+        CREATE_ALWAYS, FILE_ATTRIBUTE_NORMAL, nullptr);
+    if (file == INVALID_HANDLE_VALUE) {
+        return false;
+    }
+    bool ok = true;
+    std::size_t offset = 0U;
+    while (ok && offset < size) {
+        const std::size_t remaining = size - offset;
+        const DWORD request = remaining > static_cast<std::size_t>(MAXDWORD)
+            ? MAXDWORD : static_cast<DWORD>(remaining);
+        DWORD written = 0U;
+        ok = WriteFile(file, bytes + offset, request, &written, nullptr) != FALSE &&
+            written == request;
+        offset += static_cast<std::size_t>(written);
+    }
+    if (ok) {
+        ok = FlushFileBuffers(file) != FALSE;
+    }
+    if (CloseHandle(file) == FALSE) {
+        ok = false;
+    }
+    return ok;
+}
+#endif
+
+bool native_replace(
+    void*,
+    const std::filesystem::path& path,
+    const std::uint8_t* bytes,
+    std::size_t size) {
+    if (!allowed_slot_path(path) || bytes == nullptr) {
+        return false;
+    }
+    std::error_code directory_error{};
+    std::filesystem::create_directories(path.parent_path(), directory_error);
+    if (directory_error) {
+        return false;
+    }
+    std::filesystem::path temp = path;
+    temp += ".tmp";
+#ifdef _WIN32
+    if (!write_and_flush_temp(temp, bytes, size)) {
+        return false;
+    }
+    return MoveFileExW(temp.c_str(), path.c_str(),
+        MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH) != FALSE;
+#else
+    std::ofstream stream(temp, std::ios::binary | std::ios::trunc);
+    if (!stream) {
+        return false;
+    }
+    stream.write(reinterpret_cast<const char*>(bytes),
+        static_cast<std::streamsize>(size));
+    stream.flush();
+    if (!stream) {
+        return false;
+    }
+    stream.close();
+    if (!stream) {
+        return false;
+    }
+    std::error_code replace_error{};
+    std::filesystem::rename(temp, path, replace_error);
+    return !replace_error;
+#endif
+}
+
+}  // namespace
+
+SettingsFileOps native_settings_file_ops() noexcept {
+    return {nullptr, native_read, native_replace};
+}
+
+SettingsStore::SettingsStore(
+    std::filesystem::path directory,
+    SettingsFileOps file_ops)
+    : directory_(std::move(directory)), file_ops_(file_ops) {}
+
+SettingsLoadResult SettingsStore::load() const {
+    if (directory_.empty()) {
+        return {SettingsLoadStatus::defaults_corrupt, default_settings()};
+    }
+    try {
+        const SlotRecord a = read_slot(file_ops_, directory_ / slot_a_name);
+        const SlotRecord b = read_slot(file_ops_, directory_ / slot_b_name);
+        return select_loaded(a, b);
+    } catch (...) {
+        return {SettingsLoadStatus::defaults_corrupt, default_settings()};
+    }
+}
+
+SettingsSaveResult SettingsStore::save(
+    const SettingsData& committed, SettingsData draft) const {
+    if (directory_.empty()) {
+        return {SettingsSaveStatus::write_failed, committed};
+    }
+    if (draft.revision != committed.revision) {
+        return {SettingsSaveStatus::stale_revision, committed};
+    }
+    if (committed.revision == std::numeric_limits<std::uint64_t>::max()) {
+        return {SettingsSaveStatus::revision_overflow, committed};
+    }
+    draft.revision = committed.revision + 1U;
+    if (validate_settings(draft) != SettingsValidationError::none) {
+        return {SettingsSaveStatus::invalid_settings, committed};
+    }
+
+    std::filesystem::path target{};
+    try {
+        const SlotRecord a = read_slot(file_ops_, directory_ / slot_a_name);
+        const SlotRecord b = read_slot(file_ops_, directory_ / slot_b_name);
+        target = directory_ / target_slot_name(a, b);
+        const auto encoded = encode_settings(draft);
+        if (file_ops_.replace == nullptr ||
+                !file_ops_.replace(file_ops_.context, target,
+                    encoded.data(), encoded.size())) {
+            return {SettingsSaveStatus::write_failed, committed};
+        }
+    } catch (...) {
+        return {SettingsSaveStatus::write_failed, committed};
+    }
+
+    try {
+        const SlotRecord published = read_slot(file_ops_, target);
+        if (!is_valid(published) ||
+                !same_settings(published.settings, draft)) {
+            return {SettingsSaveStatus::readback_failed, committed};
+        }
+        return {SettingsSaveStatus::committed, published.settings};
+    } catch (...) {
+        return {SettingsSaveStatus::readback_failed, committed};
+    }
+}
+
+}  // namespace arpg::settings
diff --git a/src/platform/settings/settings_store.hpp b/src/platform/settings/settings_store.hpp
new file mode 100644
index 0000000..d7da1b2
--- /dev/null
+++ b/src/platform/settings/settings_store.hpp
@@ -0,0 +1,64 @@
+#pragma once
+
+#include "platform/settings/settings_types.hpp"
+
+#include <cstddef>
+#include <cstdint>
+#include <filesystem>
+#include <vector>
+
+namespace arpg::settings {
+
+enum class SettingsLoadStatus : std::uint8_t {
+    loaded,
+    defaults_missing,
+    recovered_single_slot,
+    defaults_corrupt
+};
+
+enum class SettingsSaveStatus : std::uint8_t {
+    committed,
+    invalid_settings,
+    stale_revision,
+    revision_overflow,
+    write_failed,
+    readback_failed
+};
+
+struct SettingsLoadResult final {
+    SettingsLoadStatus status{SettingsLoadStatus::defaults_missing};
+    SettingsData settings{};
+};
+
+struct SettingsSaveResult final {
+    SettingsSaveStatus status{SettingsSaveStatus::write_failed};
+    SettingsData settings{};
+};
+
+struct SettingsFileOps final {
+    void* context{};
+    bool (*read)(void*, const std::filesystem::path&, std::vector<std::uint8_t>&){};
+    bool (*replace)(void*, const std::filesystem::path&,
+        const std::uint8_t*, std::size_t){};
+};
+
+[[nodiscard]] SettingsFileOps native_settings_file_ops() noexcept;
+
+class SettingsStore final {
+public:
+    // An empty directory disables all file I/O: load reports corrupt defaults
+    // and save reports write_failed while preserving the caller's committed value.
+    explicit SettingsStore(
+        std::filesystem::path directory,
+        SettingsFileOps file_ops = native_settings_file_ops());
+
+    [[nodiscard]] SettingsLoadResult load() const;
+    [[nodiscard]] SettingsSaveResult save(
+        const SettingsData& committed, SettingsData draft) const;
+
+private:
+    std::filesystem::path directory_{};
+    SettingsFileOps file_ops_{};
+};
+
+}  // namespace arpg::settings
diff --git a/tests/settings/CMakeLists.txt b/tests/settings/CMakeLists.txt
index 082699f..f8a99eb 100644
--- a/tests/settings/CMakeLists.txt
+++ b/tests/settings/CMakeLists.txt
@@ -1,14 +1,15 @@
 add_executable(arpg_settings_tests
     settings_test_main.cpp
     settings_types_tests.cpp
-    settings_codec_tests.cpp)
+    settings_codec_tests.cpp
+    settings_store_tests.cpp)
 
 target_include_directories(arpg_settings_tests PRIVATE
     "${PROJECT_SOURCE_DIR}/tests/core"
     "${PROJECT_SOURCE_DIR}/src")
 target_compile_features(arpg_settings_tests PRIVATE cxx_std_17)
 target_link_libraries(arpg_settings_tests PRIVATE arpg_settings)
 arpg_enable_project_warnings(arpg_settings_tests)
 
 add_test(NAME settings.units COMMAND arpg_settings_tests)
 set_tests_properties(settings.units PROPERTIES LABELS "headless;settings")
diff --git a/tests/settings/settings_store_tests.cpp b/tests/settings/settings_store_tests.cpp
new file mode 100644
index 0000000..bd14e3b
--- /dev/null
+++ b/tests/settings/settings_store_tests.cpp
@@ -0,0 +1,382 @@
+#include "test_framework.hpp"
+
+#include "platform/settings/settings_codec.hpp"
+#include "platform/settings/settings_store.hpp"
+
+#include <cstddef>
+#include <cstdint>
+#include <filesystem>
+#include <limits>
+#include <new>
+#include <stdexcept>
+#include <string>
+#include <unordered_map>
+#include <utility>
+#include <vector>
+
+namespace {
+
+using arpg::settings::SettingsData;
+using arpg::settings::SettingsFileOps;
+using arpg::settings::SettingsLoadStatus;
+using arpg::settings::SettingsSaveStatus;
+using arpg::settings::SettingsStore;
+
+struct FakeFiles final {
+    enum class ReplaceFault : std::uint8_t { none, write, publish };
+    enum class ReadFault : std::uint8_t {
+        none,
+        bad_alloc_after_replace,
+        runtime_error_after_replace
+    };
+
+    std::unordered_map<std::string, std::vector<std::uint8_t>> files{};
+    std::vector<std::string> reads{};
+    std::vector<std::string> replacements{};
+    ReplaceFault replace_fault{ReplaceFault::none};
+    ReadFault read_fault{ReadFault::none};
+    bool fail_readback{};
+    bool corrupt_readback{};
+    bool replacement_completed{};
+};
+
+[[nodiscard]] std::string file_name(const std::filesystem::path& path) {
+    return path.filename().string();
+}
+
+bool fake_read(
+    void* context,
+    const std::filesystem::path& path,
+    std::vector<std::uint8_t>& bytes) {
+    auto& fake = *static_cast<FakeFiles*>(context);
+    const std::string name = file_name(path);
+    fake.reads.push_back(name);
+    if (fake.replacement_completed) {
+        if (fake.read_fault == FakeFiles::ReadFault::bad_alloc_after_replace) {
+            throw std::bad_alloc{};
+        }
+        if (fake.read_fault == FakeFiles::ReadFault::runtime_error_after_replace) {
+            throw std::runtime_error{"injected readback failure"};
+        }
+    }
+    if (fake.replacement_completed && fake.fail_readback) {
+        return false;
+    }
+    const auto found = fake.files.find(name);
+    if (found == fake.files.end()) {
+        return false;
+    }
+    bytes = found->second;
+    if (fake.replacement_completed && fake.corrupt_readback && !bytes.empty()) {
+        bytes.back() ^= 0x01U;
+    }
+    return true;
+}
+
+bool fake_replace(
+    void* context,
+    const std::filesystem::path& path,
+    const std::uint8_t* bytes,
+    std::size_t size) {
+    auto& fake = *static_cast<FakeFiles*>(context);
+    const std::string name = file_name(path);
+    fake.replacements.push_back(name);
+    if (fake.replace_fault == FakeFiles::ReplaceFault::write) {
+        return false;
+    }
+    if (fake.replace_fault == FakeFiles::ReplaceFault::publish) {
+        fake.files[name + ".tmp"] = std::vector<std::uint8_t>(bytes, bytes + size);
+        return false;
+    }
+    fake.files[name] = std::vector<std::uint8_t>(bytes, bytes + size);
+    fake.replacement_completed = true;
+    return true;
+}
+
+[[nodiscard]] SettingsFileOps fake_ops(FakeFiles& fake) noexcept {
+    return {&fake, fake_read, fake_replace};
+}
+
+[[nodiscard]] SettingsData settings_at(
+    std::uint64_t revision,
+    std::uint8_t volume = 100U) noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    settings.revision = revision;
+    settings.master_sfx_percent = volume;
+    return settings;
+}
+
+void put(FakeFiles& fake, const char* name, const SettingsData& settings) {
+    const auto bytes = arpg::settings::encode_settings(settings);
+    fake.files[name] = {bytes.begin(), bytes.end()};
+}
+
+[[nodiscard]] bool same_settings(
+    const SettingsData& lhs, const SettingsData& rhs) noexcept {
+    return lhs.master_sfx_percent == rhs.master_sfx_percent &&
+        lhs.window_mode == rhs.window_mode &&
+        lhs.vsync_enabled == rhs.vsync_enabled &&
+        lhs.bindings == rhs.bindings &&
+        lhs.revision == rhs.revision;
+}
+
+arpg::test::Failure missing_slots_load_defaults_without_writes() noexcept {
+    FakeFiles fake{};
+    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
+    ARPG_REQUIRE(result.status == SettingsLoadStatus::defaults_missing);
+    ARPG_REQUIRE(same_settings(result.settings, arpg::settings::default_settings()));
+    ARPG_REQUIRE(fake.replacements.empty());
+    ARPG_REQUIRE(fake.files.empty());
+    return {};
+}
+
+arpg::test::Failure newest_slot_is_selected_in_either_position() noexcept {
+    FakeFiles fake{};
+    put(fake, "settings-a.bin", settings_at(7U, 70U));
+    put(fake, "settings-b.bin", settings_at(8U, 80U));
+    auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
+    ARPG_REQUIRE(result.status == SettingsLoadStatus::loaded);
+    ARPG_REQUIRE(result.settings.revision == 8U);
+    ARPG_REQUIRE(result.settings.master_sfx_percent == 80U);
+
+    put(fake, "settings-a.bin", settings_at(9U, 90U));
+    result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
+    ARPG_REQUIRE(result.status == SettingsLoadStatus::loaded);
+    ARPG_REQUIRE(result.settings.revision == 9U);
+    ARPG_REQUIRE(result.settings.master_sfx_percent == 90U);
+    return {};
+}
+
+arpg::test::Failure revision_selection_does_not_treat_wrap_as_newer() noexcept {
+    FakeFiles fake{};
+    put(fake, "settings-a.bin", settings_at(
+        std::numeric_limits<std::uint64_t>::max() - 1U, 95U));
+    put(fake, "settings-b.bin", settings_at(5U, 50U));
+    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
+    ARPG_REQUIRE(result.status == SettingsLoadStatus::loaded);
+    ARPG_REQUIRE(result.settings.revision ==
+        std::numeric_limits<std::uint64_t>::max() - 1U);
+    ARPG_REQUIRE(result.settings.master_sfx_percent == 95U);
+    return {};
+}
+
+arpg::test::Failure one_valid_slot_recovers_without_repairing_on_load() noexcept {
+    FakeFiles fake{};
+    put(fake, "settings-b.bin", settings_at(3U, 75U));
+    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
+    ARPG_REQUIRE(result.status == SettingsLoadStatus::recovered_single_slot);
+    ARPG_REQUIRE(result.settings.revision == 3U);
+    ARPG_REQUIRE(fake.replacements.empty());
+    ARPG_REQUIRE(fake.files.count("settings-a.bin") == 0U);
+    return {};
+}
+
+arpg::test::Failure equal_revision_identical_slots_are_loadable() noexcept {
+    FakeFiles fake{};
+    const SettingsData settings = settings_at(11U, 85U);
+    put(fake, "settings-a.bin", settings);
+    put(fake, "settings-b.bin", settings);
+    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
+    ARPG_REQUIRE(result.status == SettingsLoadStatus::loaded);
+    ARPG_REQUIRE(same_settings(result.settings, settings));
+    return {};
+}
+
+arpg::test::Failure equal_revision_conflict_loads_corrupt_defaults() noexcept {
+    FakeFiles fake{};
+    put(fake, "settings-a.bin", settings_at(11U, 85U));
+    put(fake, "settings-b.bin", settings_at(11U, 90U));
+    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
+    ARPG_REQUIRE(result.status == SettingsLoadStatus::defaults_corrupt);
+    ARPG_REQUIRE(same_settings(result.settings, arpg::settings::default_settings()));
+    ARPG_REQUIRE(fake.replacements.empty());
+    return {};
+}
+
+arpg::test::Failure bad_crc_loads_corrupt_defaults_without_deleting() noexcept {
+    FakeFiles fake{};
+    put(fake, "settings-a.bin", settings_at(4U, 80U));
+    fake.files["settings-a.bin"].back() ^= 0x01U;
+    const auto before = fake.files["settings-a.bin"];
+    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.load();
+    ARPG_REQUIRE(result.status == SettingsLoadStatus::defaults_corrupt);
+    ARPG_REQUIRE(fake.files["settings-a.bin"] == before);
+    ARPG_REQUIRE(fake.replacements.empty());
+    return {};
+}
+
+arpg::test::Failure save_validates_revision_and_settings_before_io() noexcept {
+    FakeFiles fake{};
+    const SettingsData committed = settings_at(4U);
+    SettingsData draft = committed;
+    draft.revision = 3U;
+    auto result = SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
+    ARPG_REQUIRE(result.status == SettingsSaveStatus::stale_revision);
+    ARPG_REQUIRE(same_settings(result.settings, committed));
+
+    draft = committed;
+    draft.master_sfx_percent = 99U;
+    result = SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
+    ARPG_REQUIRE(result.status == SettingsSaveStatus::invalid_settings);
+    ARPG_REQUIRE(same_settings(result.settings, committed));
+    ARPG_REQUIRE(fake.reads.empty());
+    ARPG_REQUIRE(fake.replacements.empty());
+    return {};
+}
+
+arpg::test::Failure save_rejects_revision_overflow_without_io() noexcept {
+    FakeFiles fake{};
+    const SettingsData committed = settings_at(
+        std::numeric_limits<std::uint64_t>::max());
+    auto result =
+        SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, committed);
+    ARPG_REQUIRE(result.status == SettingsSaveStatus::revision_overflow);
+    ARPG_REQUIRE(same_settings(result.settings, committed));
+
+    SettingsData invalid_draft = committed;
+    invalid_draft.master_sfx_percent = 99U;
+    result = SettingsStore{"C:/settings", fake_ops(fake)}.save(
+        committed, invalid_draft);
+    ARPG_REQUIRE(result.status == SettingsSaveStatus::revision_overflow);
+    ARPG_REQUIRE(same_settings(result.settings, committed));
+    ARPG_REQUIRE(fake.reads.empty());
+    ARPG_REQUIRE(fake.replacements.empty());
+    return {};
+}
+
+arpg::test::Failure save_writes_older_slot_and_returns_readback_value() noexcept {
+    FakeFiles fake{};
+    const SettingsData committed = settings_at(8U, 80U);
+    put(fake, "settings-a.bin", settings_at(7U, 70U));
+    put(fake, "settings-b.bin", committed);
+    SettingsData draft = committed;
+    draft.master_sfx_percent = 90U;
+    const auto result = SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
+    ARPG_REQUIRE(result.status == SettingsSaveStatus::committed);
+    ARPG_REQUIRE(result.settings.revision == 9U);
+    ARPG_REQUIRE(result.settings.master_sfx_percent == 90U);
+    ARPG_REQUIRE(fake.replacements.size() == 1U);
+    ARPG_REQUIRE(fake.replacements.front() == "settings-a.bin");
+    const auto preserved = arpg::settings::decode_settings(
+        fake.files["settings-b.bin"].data(), fake.files["settings-b.bin"].size());
+    ARPG_REQUIRE(preserved.settings.revision == 8U);
+    return {};
+}
+
+arpg::test::Failure write_and_publish_failures_preserve_last_valid_slot() noexcept {
+    for (const auto fault : {FakeFiles::ReplaceFault::write,
+             FakeFiles::ReplaceFault::publish}) {
+        FakeFiles fake{};
+        fake.replace_fault = fault;
+        const SettingsData committed = settings_at(8U, 80U);
+        put(fake, "settings-a.bin", settings_at(7U, 70U));
+        put(fake, "settings-b.bin", committed);
+        const auto old_slot = fake.files["settings-b.bin"];
+        SettingsData draft = committed;
+        draft.master_sfx_percent = 90U;
+        const auto result =
+            SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
+        ARPG_REQUIRE(result.status == SettingsSaveStatus::write_failed);
+        ARPG_REQUIRE(same_settings(result.settings, committed));
+        ARPG_REQUIRE(fake.files["settings-b.bin"] == old_slot);
+        const auto reloaded = SettingsStore{"C:/settings", fake_ops(fake)}.load();
+        ARPG_REQUIRE(reloaded.settings.revision == 8U);
+        ARPG_REQUIRE(reloaded.settings.master_sfx_percent == 80U);
+    }
+    return {};
+}
+
+arpg::test::Failure readback_failure_does_not_publish_to_caller() noexcept {
+    for (const bool corrupt : {false, true}) {
+        FakeFiles fake{};
+        fake.fail_readback = !corrupt;
+        fake.corrupt_readback = corrupt;
+        const SettingsData committed = settings_at(8U, 80U);
+        put(fake, "settings-b.bin", committed);
+        SettingsData draft = committed;
+        draft.master_sfx_percent = 90U;
+        const auto result =
+            SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
+        ARPG_REQUIRE(result.status == SettingsSaveStatus::readback_failed);
+        ARPG_REQUIRE(same_settings(result.settings, committed));
+        ARPG_REQUIRE(fake.files.count("settings-b.bin") == 1U);
+    }
+    return {};
+}
+
+arpg::test::Failure readback_exceptions_are_not_classified_as_write_failures() noexcept {
+    for (const auto fault : {FakeFiles::ReadFault::bad_alloc_after_replace,
+             FakeFiles::ReadFault::runtime_error_after_replace}) {
+        FakeFiles fake{};
+        fake.read_fault = fault;
+        const SettingsData committed = settings_at(8U, 80U);
+        put(fake, "settings-b.bin", committed);
+        SettingsData draft = committed;
+        draft.master_sfx_percent = 90U;
+
+        const auto result =
+            SettingsStore{"C:/settings", fake_ops(fake)}.save(committed, draft);
+
+        ARPG_REQUIRE(result.status == SettingsSaveStatus::readback_failed);
+        ARPG_REQUIRE(same_settings(result.settings, committed));
+        ARPG_REQUIRE(fake.files.count("settings-a.bin") == 1U);
+        const auto published = arpg::settings::decode_settings(
+            fake.files["settings-a.bin"].data(), fake.files["settings-a.bin"].size());
+        ARPG_REQUIRE(published.error == arpg::settings::SettingsCodecError::none);
+        ARPG_REQUIRE(published.settings.revision == 9U);
+        ARPG_REQUIRE(published.settings.master_sfx_percent == 90U);
+    }
+    return {};
+}
+
+arpg::test::Failure empty_directory_loads_corrupt_defaults_without_io() noexcept {
+    FakeFiles fake{};
+
+    const auto result = SettingsStore{
+        std::filesystem::path{}, fake_ops(fake)}.load();
+
+    ARPG_REQUIRE(result.status == SettingsLoadStatus::defaults_corrupt);
+    ARPG_REQUIRE(same_settings(result.settings, arpg::settings::default_settings()));
+    ARPG_REQUIRE(fake.reads.empty());
+    ARPG_REQUIRE(fake.replacements.empty());
+    return {};
+}
+
+arpg::test::Failure empty_directory_save_fails_without_io() noexcept {
+    FakeFiles fake{};
+    const SettingsData committed = settings_at(8U, 80U);
+    SettingsData draft = committed;
+    draft.master_sfx_percent = 90U;
+
+    const auto result = SettingsStore{
+        std::filesystem::path{}, fake_ops(fake)}.save(committed, draft);
+
+    ARPG_REQUIRE(result.status == SettingsSaveStatus::write_failed);
+    ARPG_REQUIRE(same_settings(result.settings, committed));
+    ARPG_REQUIRE(fake.reads.empty());
+    ARPG_REQUIRE(fake.replacements.empty());
+    return {};
+}
+
+constexpr arpg::test::TestCase cases[] = {
+    {"missing slots load defaults without writes", missing_slots_load_defaults_without_writes},
+    {"newest A or B slot is selected", newest_slot_is_selected_in_either_position},
+    {"revision selection does not treat wrap as newer", revision_selection_does_not_treat_wrap_as_newer},
+    {"one valid slot recovers without repairing on load", one_valid_slot_recovers_without_repairing_on_load},
+    {"equal revision identical slots are loadable", equal_revision_identical_slots_are_loadable},
+    {"equal revision conflict loads corrupt defaults", equal_revision_conflict_loads_corrupt_defaults},
+    {"bad CRC loads corrupt defaults without deleting", bad_crc_loads_corrupt_defaults_without_deleting},
+    {"save validates revision and settings before I/O", save_validates_revision_and_settings_before_io},
+    {"save rejects revision overflow without I/O", save_rejects_revision_overflow_without_io},
+    {"save writes older slot and returns readback value", save_writes_older_slot_and_returns_readback_value},
+    {"write and publish failures preserve last valid slot", write_and_publish_failures_preserve_last_valid_slot},
+    {"readback failure does not publish to caller", readback_failure_does_not_publish_to_caller},
+    {"readback exceptions are not classified as write failures", readback_exceptions_are_not_classified_as_write_failures},
+    {"empty directory loads corrupt defaults without I/O", empty_directory_loads_corrupt_defaults_without_io},
+    {"empty directory save fails without I/O", empty_directory_save_fails_without_io}};
+
+}  // namespace
+
+arpg::test::TestSuite settings_store_suite() noexcept {
+    return arpg::test::make_suite("settings.store", cases);
+}
diff --git a/tests/settings/settings_test_main.cpp b/tests/settings/settings_test_main.cpp
index e37e100..263004b 100644
--- a/tests/settings/settings_test_main.cpp
+++ b/tests/settings/settings_test_main.cpp
@@ -1,11 +1,13 @@
 #include "test_framework.hpp"
 
 arpg::test::TestSuite settings_types_suite() noexcept;
 arpg::test::TestSuite settings_codec_suite() noexcept;
+arpg::test::TestSuite settings_store_suite() noexcept;
 
 int main() {
     const arpg::test::TestSuite suites[] = {
         settings_types_suite(),
-        settings_codec_suite()};
-    return arpg::test::run_suites(suites, 17, "stage 11b task 2 settings codec");
+        settings_codec_suite(),
+        settings_store_suite()};
+    return arpg::test::run_suites(suites, 32, "stage 11b task 3 settings store");
 }

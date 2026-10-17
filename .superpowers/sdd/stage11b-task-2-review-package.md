# Review package: 19b04af..dab6010

## Commits
dab6010 feat: encode versioned settings records

## Files changed
 src/core/CMakeLists.txt                  |   3 +-
 src/{persistence => core}/crc32.cpp      |   6 +-
 src/core/crc32.hpp                       |  16 +++
 src/persistence/CMakeLists.txt           |   3 +-
 src/persistence/crc32.hpp                |  15 ++-
 src/platform/settings/CMakeLists.txt     |   4 +-
 src/platform/settings/settings_codec.cpp | 146 +++++++++++++++++++++++
 src/platform/settings/settings_codec.hpp |  35 ++++++
 tests/settings/CMakeLists.txt            |   3 +-
 tests/settings/settings_codec_tests.cpp  | 197 +++++++++++++++++++++++++++++++
 tests/settings/settings_test_main.cpp    |   7 +-
 11 files changed, 419 insertions(+), 16 deletions(-)

## Diff
diff --git a/src/core/CMakeLists.txt b/src/core/CMakeLists.txt
index 2783583..39c2c47 100644
--- a/src/core/CMakeLists.txt
+++ b/src/core/CMakeLists.txt
@@ -1,9 +1,10 @@
 add_library(arpg_core STATIC
     fixed_step.cpp
-    deterministic_rng.cpp)
+    deterministic_rng.cpp
+    crc32.cpp)
 
 target_include_directories(arpg_core
     PUBLIC "${PROJECT_SOURCE_DIR}/src")
 target_compile_features(arpg_core PUBLIC cxx_std_17)
 
 arpg_enable_project_warnings(arpg_core)
diff --git a/src/persistence/crc32.cpp b/src/core/crc32.cpp
similarity index 87%
rename from src/persistence/crc32.cpp
rename to src/core/crc32.cpp
index 1e403f9..29c9449 100644
--- a/src/persistence/crc32.cpp
+++ b/src/core/crc32.cpp
@@ -1,13 +1,13 @@
-#include "persistence/crc32.hpp"
+#include "core/crc32.hpp"
 
-namespace arpg::persistence {
+namespace arpg::core {
 
 std::uint32_t crc32_update(std::uint32_t state,
     const std::uint8_t* bytes, std::size_t size) noexcept {
     if (bytes == nullptr || size == 0U)
         return state;
     std::uint32_t crc = state ^ 0xFFFFFFFFU;
 
     for (std::size_t index = 0; index < size; ++index) {
         crc ^= bytes[index];
         for (int bit = 0; bit < 8; ++bit) {
@@ -17,11 +17,11 @@ std::uint32_t crc32_update(std::uint32_t state,
         }
     }
     return crc ^ 0xFFFFFFFFU;
 }
 
 std::uint32_t crc32(
     const std::uint8_t* bytes, std::size_t size) noexcept {
     return crc32_update(0U, bytes, size);
 }
 
-}  // namespace arpg::persistence
+}  // namespace arpg::core
diff --git a/src/core/crc32.hpp b/src/core/crc32.hpp
new file mode 100644
index 0000000..6c76ca6
--- /dev/null
+++ b/src/core/crc32.hpp
@@ -0,0 +1,16 @@
+#pragma once
+
+#include <cstddef>
+#include <cstdint>
+
+namespace arpg::core {
+
+[[nodiscard]] std::uint32_t crc32(
+    const std::uint8_t* bytes, std::size_t size) noexcept;
+
+[[nodiscard]] std::uint32_t crc32_update(
+    std::uint32_t state,
+    const std::uint8_t* bytes,
+    std::size_t size) noexcept;
+
+}  // namespace arpg::core
diff --git a/src/persistence/CMakeLists.txt b/src/persistence/CMakeLists.txt
index ac8c6b2..18f2c05 100644
--- a/src/persistence/CMakeLists.txt
+++ b/src/persistence/CMakeLists.txt
@@ -1,16 +1,15 @@
 add_library(arpg_persistence STATIC
-    crc32.cpp
     checkpoint_codec.cpp
     save_paths.cpp
     save_slot.cpp
     save_transaction.cpp
     save_recovery.cpp
     save_store.cpp)
 
 target_include_directories(arpg_persistence PUBLIC "${PROJECT_SOURCE_DIR}/src")
 target_compile_features(arpg_persistence PUBLIC cxx_std_17)
 if(MSVC)
     target_compile_options(arpg_persistence PUBLIC /EHsc)
 endif()
 arpg_enable_project_warnings(arpg_persistence)
-target_link_libraries(arpg_persistence PUBLIC arpg_progression arpg_passives PRIVATE arpg_items arpg_abyss)
+target_link_libraries(arpg_persistence PUBLIC arpg_progression arpg_passives PRIVATE arpg_items arpg_abyss arpg_core)
diff --git a/src/persistence/crc32.hpp b/src/persistence/crc32.hpp
index eae0d72..6c30237 100644
--- a/src/persistence/crc32.hpp
+++ b/src/persistence/crc32.hpp
@@ -1,16 +1,19 @@
 #pragma once
 
-#include <cstddef>
-#include <cstdint>
+#include "core/crc32.hpp"
 
 namespace arpg::persistence {
 
-[[nodiscard]] std::uint32_t crc32(
-    const std::uint8_t* bytes, std::size_t size) noexcept;
+[[nodiscard]] inline std::uint32_t crc32(
+    const std::uint8_t* bytes, std::size_t size) noexcept {
+    return core::crc32(bytes, size);
+}
 
-[[nodiscard]] std::uint32_t crc32_update(
+[[nodiscard]] inline std::uint32_t crc32_update(
     std::uint32_t state,
     const std::uint8_t* bytes,
-    std::size_t size) noexcept;
+    std::size_t size) noexcept {
+    return core::crc32_update(state, bytes, size);
+}
 
 }  // namespace arpg::persistence
diff --git a/src/platform/settings/CMakeLists.txt b/src/platform/settings/CMakeLists.txt
index 5799650..1b2b9f7 100644
--- a/src/platform/settings/CMakeLists.txt
+++ b/src/platform/settings/CMakeLists.txt
@@ -1,6 +1,8 @@
 add_library(arpg_settings STATIC
-    settings_types.cpp)
+    settings_types.cpp
+    settings_codec.cpp)
 
 target_include_directories(arpg_settings PUBLIC "${PROJECT_SOURCE_DIR}/src")
 target_compile_features(arpg_settings PUBLIC cxx_std_17)
 arpg_enable_project_warnings(arpg_settings)
+target_link_libraries(arpg_settings PRIVATE arpg_core)
diff --git a/src/platform/settings/settings_codec.cpp b/src/platform/settings/settings_codec.cpp
new file mode 100644
index 0000000..f2ba81e
--- /dev/null
+++ b/src/platform/settings/settings_codec.cpp
@@ -0,0 +1,146 @@
+#include "platform/settings/settings_codec.hpp"
+
+#include "core/crc32.hpp"
+
+#include <array>
+#include <cstddef>
+#include <cstdint>
+
+namespace arpg::settings {
+namespace {
+
+constexpr std::array<std::uint8_t, 8> magic{
+    'A', 'R', 'P', 'G', 'S', 'E', 'T', '1'};
+constexpr std::uint16_t format = 1U;
+constexpr std::uint16_t payload_size = 20U;
+constexpr std::size_t crc_covered_offset = 8U;
+constexpr std::size_t crc_covered_size = 32U;
+constexpr std::size_t crc_offset = 40U;
+
+void write_u16(
+    std::array<std::uint8_t, kSettingsEncodedSize>& bytes,
+    std::size_t offset,
+    std::uint16_t value) noexcept {
+    bytes[offset] = static_cast<std::uint8_t>(value & 0xFFU);
+    bytes[offset + 1U] = static_cast<std::uint8_t>((value >> 8U) & 0xFFU);
+}
+
+void write_u32(
+    std::array<std::uint8_t, kSettingsEncodedSize>& bytes,
+    std::size_t offset,
+    std::uint32_t value) noexcept {
+    for (std::size_t index = 0U; index < 4U; ++index) {
+        bytes[offset + index] = static_cast<std::uint8_t>(value & 0xFFU);
+        value >>= 8U;
+    }
+}
+
+void write_u64(
+    std::array<std::uint8_t, kSettingsEncodedSize>& bytes,
+    std::size_t offset,
+    std::uint64_t value) noexcept {
+    for (std::size_t index = 0U; index < 8U; ++index) {
+        bytes[offset + index] = static_cast<std::uint8_t>(value & 0xFFU);
+        value >>= 8U;
+    }
+}
+
+[[nodiscard]] std::uint16_t read_u16(
+    const std::uint8_t* bytes, std::size_t offset) noexcept {
+    return static_cast<std::uint16_t>(bytes[offset]) |
+        static_cast<std::uint16_t>(static_cast<std::uint16_t>(bytes[offset + 1U]) << 8U);
+}
+
+[[nodiscard]] std::uint32_t read_u32(
+    const std::uint8_t* bytes, std::size_t offset) noexcept {
+    std::uint32_t value = 0U;
+    for (std::size_t index = 0U; index < 4U; ++index) {
+        value |= static_cast<std::uint32_t>(bytes[offset + index]) << (index * 8U);
+    }
+    return value;
+}
+
+[[nodiscard]] std::uint64_t read_u64(
+    const std::uint8_t* bytes, std::size_t offset) noexcept {
+    std::uint64_t value = 0U;
+    for (std::size_t index = 0U; index < 8U; ++index) {
+        value |= static_cast<std::uint64_t>(bytes[offset + index]) << (index * 8U);
+    }
+    return value;
+}
+
+[[nodiscard]] SettingsDecodeResult decode_error(SettingsCodecError error) noexcept {
+    return {error, {}};
+}
+
+}  // namespace
+
+std::array<std::uint8_t, kSettingsEncodedSize>
+encode_settings(const SettingsData& settings) noexcept {
+    std::array<std::uint8_t, kSettingsEncodedSize> bytes{};
+    for (std::size_t index = 0U; index < magic.size(); ++index) {
+        bytes[index] = magic[index];
+    }
+    write_u16(bytes, 8U, format);
+    write_u16(bytes, 10U, payload_size);
+    write_u64(bytes, 12U, settings.revision);
+    bytes[20] = settings.master_sfx_percent;
+    bytes[21] = static_cast<std::uint8_t>(settings.window_mode);
+    bytes[22] = settings.vsync_enabled ? 1U : 0U;
+    for (std::size_t index = 0U; index < settings.bindings.size(); ++index) {
+        bytes[24U + index] = static_cast<std::uint8_t>(settings.bindings[index]);
+    }
+    const std::uint32_t checksum =
+        core::crc32(bytes.data() + crc_covered_offset, crc_covered_size);
+    write_u32(bytes, crc_offset, checksum);
+    return bytes;
+}
+
+SettingsDecodeResult decode_settings(
+    const std::uint8_t* bytes, std::size_t size) noexcept {
+    if (bytes == nullptr || size != kSettingsEncodedSize) {
+        return decode_error(SettingsCodecError::wrong_size);
+    }
+    for (std::size_t index = 0U; index < magic.size(); ++index) {
+        if (bytes[index] != magic[index]) {
+            return decode_error(SettingsCodecError::wrong_magic);
+        }
+    }
+    if (read_u16(bytes, 8U) != format) {
+        return decode_error(SettingsCodecError::wrong_format);
+    }
+    if (read_u16(bytes, 10U) != payload_size) {
+        return decode_error(SettingsCodecError::wrong_payload_size);
+    }
+    const std::uint32_t expected_crc =
+        core::crc32(bytes + crc_covered_offset, crc_covered_size);
+    if (read_u32(bytes, crc_offset) != expected_crc) {
+        return decode_error(SettingsCodecError::bad_crc);
+    }
+    if (bytes[23] != 0U) {
+        return decode_error(SettingsCodecError::reserved_nonzero);
+    }
+    for (std::size_t offset = 34U; offset < crc_offset; ++offset) {
+        if (bytes[offset] != 0U) {
+            return decode_error(SettingsCodecError::reserved_nonzero);
+        }
+    }
+    if (bytes[22] > 1U) {
+        return decode_error(SettingsCodecError::invalid_settings);
+    }
+
+    SettingsData settings{};
+    settings.revision = read_u64(bytes, 12U);
+    settings.master_sfx_percent = bytes[20];
+    settings.window_mode = static_cast<WindowMode>(bytes[21]);
+    settings.vsync_enabled = bytes[22] != 0U;
+    for (std::size_t index = 0U; index < settings.bindings.size(); ++index) {
+        settings.bindings[index] = static_cast<StableKey>(bytes[24U + index]);
+    }
+    if (validate_settings(settings) != SettingsValidationError::none) {
+        return decode_error(SettingsCodecError::invalid_settings);
+    }
+    return {SettingsCodecError::none, settings};
+}
+
+}  // namespace arpg::settings
diff --git a/src/platform/settings/settings_codec.hpp b/src/platform/settings/settings_codec.hpp
new file mode 100644
index 0000000..7815e80
--- /dev/null
+++ b/src/platform/settings/settings_codec.hpp
@@ -0,0 +1,35 @@
+#pragma once
+
+#include "platform/settings/settings_types.hpp"
+
+#include <array>
+#include <cstddef>
+#include <cstdint>
+
+namespace arpg::settings {
+
+inline constexpr std::size_t kSettingsEncodedSize = 44U;
+
+enum class SettingsCodecError : std::uint8_t {
+    none,
+    wrong_size,
+    wrong_magic,
+    wrong_format,
+    wrong_payload_size,
+    bad_crc,
+    reserved_nonzero,
+    invalid_settings
+};
+
+struct SettingsDecodeResult final {
+    SettingsCodecError error{SettingsCodecError::none};
+    SettingsData settings{};
+};
+
+[[nodiscard]] std::array<std::uint8_t, kSettingsEncodedSize>
+    encode_settings(const SettingsData& settings) noexcept;
+
+[[nodiscard]] SettingsDecodeResult decode_settings(
+    const std::uint8_t* bytes, std::size_t size) noexcept;
+
+}  // namespace arpg::settings
diff --git a/tests/settings/CMakeLists.txt b/tests/settings/CMakeLists.txt
index 4b60560..082699f 100644
--- a/tests/settings/CMakeLists.txt
+++ b/tests/settings/CMakeLists.txt
@@ -1,13 +1,14 @@
 add_executable(arpg_settings_tests
     settings_test_main.cpp
-    settings_types_tests.cpp)
+    settings_types_tests.cpp
+    settings_codec_tests.cpp)
 
 target_include_directories(arpg_settings_tests PRIVATE
     "${PROJECT_SOURCE_DIR}/tests/core"
     "${PROJECT_SOURCE_DIR}/src")
 target_compile_features(arpg_settings_tests PRIVATE cxx_std_17)
 target_link_libraries(arpg_settings_tests PRIVATE arpg_settings)
 arpg_enable_project_warnings(arpg_settings_tests)
 
 add_test(NAME settings.units COMMAND arpg_settings_tests)
 set_tests_properties(settings.units PROPERTIES LABELS "headless;settings")
diff --git a/tests/settings/settings_codec_tests.cpp b/tests/settings/settings_codec_tests.cpp
new file mode 100644
index 0000000..a9ee7e9
--- /dev/null
+++ b/tests/settings/settings_codec_tests.cpp
@@ -0,0 +1,197 @@
+#include "test_framework.hpp"
+
+#include "platform/settings/settings_codec.hpp"
+
+#include <array>
+#include <cstddef>
+#include <cstdint>
+#include <utility>
+
+namespace {
+
+using arpg::settings::SettingsCodecError;
+using arpg::settings::SettingsData;
+using arpg::settings::StableKey;
+using arpg::settings::WindowMode;
+
+constexpr std::size_t crc_offset = 40U;
+
+[[nodiscard]] std::uint32_t test_crc32(
+    const std::uint8_t* bytes, std::size_t size) noexcept {
+    std::uint32_t crc = 0xFFFFFFFFU;
+    for (std::size_t index = 0; index < size; ++index) {
+        crc ^= bytes[index];
+        for (int bit = 0; bit < 8; ++bit) {
+            const std::uint32_t mask =
+                0U - static_cast<std::uint32_t>(crc & 1U);
+            crc = (crc >> 1U) ^ (0xEDB88320U & mask);
+        }
+    }
+    return crc ^ 0xFFFFFFFFU;
+}
+
+void refresh_crc(
+    std::array<std::uint8_t, arpg::settings::kSettingsEncodedSize>& bytes) noexcept {
+    const std::uint32_t crc = test_crc32(bytes.data() + 8U, 32U);
+    bytes[40] = static_cast<std::uint8_t>(crc & 0xFFU);
+    bytes[41] = static_cast<std::uint8_t>((crc >> 8U) & 0xFFU);
+    bytes[42] = static_cast<std::uint8_t>((crc >> 16U) & 0xFFU);
+    bytes[43] = static_cast<std::uint8_t>((crc >> 24U) & 0xFFU);
+}
+
+[[nodiscard]] SettingsData golden_settings() noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    settings.master_sfx_percent = 95U;
+    settings.window_mode = WindowMode::fullscreen;
+    settings.vsync_enabled = false;
+    settings.revision = 0x0102030405060708ULL;
+    return settings;
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
+arpg::test::Failure fixed_layout_matches_golden_bytes() noexcept {
+    static_assert(arpg::settings::kSettingsEncodedSize == 44U,
+        "settings record format 1 must remain exactly 44 bytes");
+    const auto encoded = arpg::settings::encode_settings(golden_settings());
+    constexpr std::array<std::uint8_t, 44> expected{
+        0x41, 0x52, 0x50, 0x47, 0x53, 0x45, 0x54, 0x31,
+        0x01, 0x00, 0x14, 0x00,
+        0x08, 0x07, 0x06, 0x05, 0x04, 0x03, 0x02, 0x01,
+        0x5F, 0x01, 0x00, 0x00,
+        0x16, 0x12, 0x00, 0x03, 0x09, 0x0A, 0x0B, 0x04, 0x08, 0x0F,
+        0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
+        0xE1, 0xE9, 0x38, 0x1C};
+    ARPG_REQUIRE(encoded == expected);
+    return {};
+}
+
+arpg::test::Failure valid_records_round_trip() noexcept {
+    const SettingsData expected = golden_settings();
+    const auto encoded = arpg::settings::encode_settings(expected);
+    const auto decoded = arpg::settings::decode_settings(encoded.data(), encoded.size());
+    ARPG_REQUIRE(decoded.error == SettingsCodecError::none);
+    ARPG_REQUIRE(same_settings(decoded.settings, expected));
+    return {};
+}
+
+arpg::test::Failure null_truncated_and_trailing_records_are_rejected() noexcept {
+    const auto encoded = arpg::settings::encode_settings(golden_settings());
+    ARPG_REQUIRE(arpg::settings::decode_settings(nullptr, encoded.size()).error ==
+        SettingsCodecError::wrong_size);
+    for (std::size_t size = 0U; size < encoded.size(); ++size) {
+        ARPG_REQUIRE(arpg::settings::decode_settings(encoded.data(), size).error ==
+            SettingsCodecError::wrong_size);
+    }
+    ARPG_REQUIRE(arpg::settings::decode_settings(encoded.data(), encoded.size() + 1U).error ==
+        SettingsCodecError::wrong_size);
+    return {};
+}
+
+arpg::test::Failure every_single_byte_mutation_is_rejected() noexcept {
+    const auto encoded = arpg::settings::encode_settings(golden_settings());
+    for (std::size_t offset = 0U; offset < encoded.size(); ++offset) {
+        auto mutated = encoded;
+        mutated[offset] ^= 0x80U;
+        ARPG_REQUIRE(arpg::settings::decode_settings(mutated.data(), mutated.size()).error !=
+            SettingsCodecError::none);
+    }
+    return {};
+}
+
+arpg::test::Failure header_and_crc_errors_are_classified() noexcept {
+    const auto valid = arpg::settings::encode_settings(golden_settings());
+
+    auto bytes = valid;
+    bytes[0] ^= 0x01U;
+    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
+        SettingsCodecError::wrong_magic);
+
+    bytes = valid;
+    bytes[8] = 2U;
+    refresh_crc(bytes);
+    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
+        SettingsCodecError::wrong_format);
+
+    bytes = valid;
+    bytes[10] = 19U;
+    refresh_crc(bytes);
+    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
+        SettingsCodecError::wrong_payload_size);
+
+    bytes = valid;
+    bytes[crc_offset] ^= 0x01U;
+    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
+        SettingsCodecError::bad_crc);
+    return {};
+}
+
+arpg::test::Failure every_reserved_byte_must_be_zero() noexcept {
+    const auto valid = arpg::settings::encode_settings(golden_settings());
+    constexpr std::array<std::size_t, 7> reserved_offsets{23U, 34U, 35U, 36U, 37U, 38U, 39U};
+    for (const std::size_t offset : reserved_offsets) {
+        auto bytes = valid;
+        bytes[offset] = 1U;
+        refresh_crc(bytes);
+        ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
+            SettingsCodecError::reserved_nonzero);
+    }
+    return {};
+}
+
+arpg::test::Failure invalid_scalar_fields_are_rejected() noexcept {
+    const auto valid = arpg::settings::encode_settings(golden_settings());
+    constexpr std::array<std::pair<std::size_t, std::uint8_t>, 4> mutations{{
+        {20U, static_cast<std::uint8_t>(101U)},
+        {20U, static_cast<std::uint8_t>(99U)},
+        {21U, static_cast<std::uint8_t>(2U)},
+        {22U, static_cast<std::uint8_t>(2U)}}};
+    for (const auto& mutation : mutations) {
+        auto bytes = valid;
+        bytes[mutation.first] = mutation.second;
+        refresh_crc(bytes);
+        ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
+            SettingsCodecError::invalid_settings);
+    }
+    return {};
+}
+
+arpg::test::Failure invalid_and_duplicate_bindings_are_rejected() noexcept {
+    const auto valid = arpg::settings::encode_settings(golden_settings());
+
+    auto bytes = valid;
+    bytes[24] = static_cast<std::uint8_t>(StableKey::count);
+    refresh_crc(bytes);
+    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
+        SettingsCodecError::invalid_settings);
+
+    bytes = valid;
+    bytes[24] = bytes[25];
+    refresh_crc(bytes);
+    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
+        SettingsCodecError::invalid_settings);
+    return {};
+}
+
+constexpr arpg::test::TestCase cases[] = {
+    {"fixed layout matches golden bytes and CRC", fixed_layout_matches_golden_bytes},
+    {"valid records round trip", valid_records_round_trip},
+    {"null, truncated, and trailing records are rejected", null_truncated_and_trailing_records_are_rejected},
+    {"all 44 single-byte mutations are rejected", every_single_byte_mutation_is_rejected},
+    {"header and CRC errors are classified", header_and_crc_errors_are_classified},
+    {"every reserved byte must be zero", every_reserved_byte_must_be_zero},
+    {"invalid scalar fields are rejected", invalid_scalar_fields_are_rejected},
+    {"invalid and duplicate bindings are rejected", invalid_and_duplicate_bindings_are_rejected}};
+
+}  // namespace
+
+arpg::test::TestSuite settings_codec_suite() noexcept {
+    return arpg::test::make_suite("settings.codec", cases);
+}
diff --git a/tests/settings/settings_test_main.cpp b/tests/settings/settings_test_main.cpp
index e5f11ef..e37e100 100644
--- a/tests/settings/settings_test_main.cpp
+++ b/tests/settings/settings_test_main.cpp
@@ -1,8 +1,11 @@
 #include "test_framework.hpp"
 
 arpg::test::TestSuite settings_types_suite() noexcept;
+arpg::test::TestSuite settings_codec_suite() noexcept;
 
 int main() {
-    const arpg::test::TestSuite suites[] = {settings_types_suite()};
-    return arpg::test::run_suites(suites, 9, "stage 11b task 1 settings");
+    const arpg::test::TestSuite suites[] = {
+        settings_types_suite(),
+        settings_codec_suite()};
+    return arpg::test::run_suites(suites, 17, "stage 11b task 2 settings codec");
 }

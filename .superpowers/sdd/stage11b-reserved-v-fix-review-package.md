# Review package: 538c212..8ef1438

## Commits
8ef1438 fix: reserve screenshot key from gameplay bindings

## Files changed
 .../plans/2026-07-17-stage11b-settings-input.md    |  4 +-
 .../2026-07-17-stage11b-settings-input-design.md   |  2 +-
 src/platform/settings/settings_types.cpp           |  5 +-
 src/platform/settings/settings_types.hpp           |  1 +
 tests/settings/settings_codec_tests.cpp            | 12 ++++-
 tests/settings/settings_test_main.cpp              |  2 +-
 tests/settings/settings_types_tests.cpp            | 54 ++++++++++++++++++++--
 7 files changed, 71 insertions(+), 9 deletions(-)

## Diff
diff --git a/docs/superpowers/plans/2026-07-17-stage11b-settings-input.md b/docs/superpowers/plans/2026-07-17-stage11b-settings-input.md
index 4455cd6..b20925d 100644
--- a/docs/superpowers/plans/2026-07-17-stage11b-settings-input.md
+++ b/docs/superpowers/plans/2026-07-17-stage11b-settings-input.md
@@ -73,21 +73,21 @@ enum class StableKey : std::uint8_t {
 };
 enum class WindowMode : std::uint8_t { windowed, fullscreen };
 struct SettingsData final {
     std::uint8_t master_sfx_percent{100};
     WindowMode window_mode{WindowMode::windowed};
     bool vsync_enabled{true};
     std::array<StableKey, static_cast<std::size_t>(SettingAction::count)> bindings{};
     std::uint64_t revision{};
 };
 enum class SettingsValidationError : std::uint8_t {
-    none, volume_range, volume_step, window_mode, key_range, duplicate_key
+    none, volume_range, volume_step, window_mode, key_range, reserved_key, duplicate_key
 };
 [[nodiscard]] SettingsData default_settings() noexcept;
 [[nodiscard]] SettingsValidationError validate_settings(const SettingsData&) noexcept;
 [[nodiscard]] StableKey binding_for(const SettingsData&, SettingAction) noexcept;
 [[nodiscard]] bool assign_or_swap(SettingsData&, SettingAction, StableKey) noexcept;
 [[nodiscard]] const char* action_label(SettingAction) noexcept;
 }
 ```
 
 - Default action order and keys are exactly `W/S/A/D/J/K/L/E/I/P`.
@@ -102,21 +102,21 @@ Run:
 
 ```powershell
 cmake --preset windows-msvc-debug
 cmake --build --preset windows-msvc-debug --target arpg_settings_tests
 ```
 
 Expected: compilation fails because `settings_types.hpp` and `arpg_settings` do not exist.
 
 - [ ] **Step 3: Implement the minimal stable schema and validation**
 
-Use a fixed default array and an O(10²) duplicate scan; this runs only at load/apply and needs no dynamic allocation. `assign_or_swap` must find the owner of the new key and swap its old key into that owner.
+Use a fixed default array and an O(10²) duplicate scan; this runs only at load/apply and needs no dynamic allocation. The stable vocabulary retains `V` so the raylib adapter can sample the global screenshot command, but gameplay settings validation and `assign_or_swap` must reject `StableKey::v`. `assign_or_swap` must find the owner of any other new key and swap its old key into that owner.
 
 - [ ] **Step 4: Run focused and boundary regression**
 
 Run:
 
 ```powershell
 cmake --build --preset windows-msvc-debug --target arpg_settings_tests
 ctest --test-dir out/build/windows-msvc-debug -R "^settings.units$" --output-on-failure
 ctest --test-dir out/build/windows-msvc-debug -R "^(core.units|combat.units|platform.module_boundary)$" --output-on-failure
 ```
diff --git a/docs/superpowers/specs/2026-07-17-stage11b-settings-input-design.md b/docs/superpowers/specs/2026-07-17-stage11b-settings-input-design.md
index fe894e2..a298684 100644
--- a/docs/superpowers/specs/2026-07-17-stage11b-settings-input-design.md
+++ b/docs/superpowers/specs/2026-07-17-stage11b-settings-input-design.md
@@ -67,21 +67,21 @@ Stage 11-B 在已验收的 Stage 11-A `918f2e9` 基线上，实现不改变战
 
 稳定键枚举只接受：
 
 - `A`～`Z`；
 - `0`～`9`；
 - 上、下、左、右方向键；
 - `Space`；
 - 左/右 `Shift`；
 - 左/右 `Ctrl`。
 
-不保存 raylib 的整数常量。平台适配层维护稳定键与 raylib `KeyboardKey` 的完整双向表及显示名称。未知键、保留键、重复动作、重复键或枚举越界均使整个设置记录无效。
+不保存 raylib 的整数常量。平台适配层维护稳定键与 raylib `KeyboardKey` 的完整双向表及显示名称。稳定键词汇表保留 `V` 仅供全局截图命令采样；它不能出现在十项玩法绑定中。未知键、保留键、重复动作、重复键或枚举越界均使整个设置记录无效。
 
 ### 4.3 其他字段
 
 `SettingsData` 包含：
 
 - `std::uint8_t master_sfx_percent`，范围 `0..100` 且必须为 `5` 的倍数；
 - `WindowMode window_mode`：`windowed` 或 `fullscreen`；
 - `bool vsync_enabled`；
 - 十项 `StableKey` 绑定；
 - `std::uint64_t revision`，每次成功应用加一，溢出时拒绝保存并显示错误。
diff --git a/src/platform/settings/settings_types.cpp b/src/platform/settings/settings_types.cpp
index 121bf23..c08b08c 100644
--- a/src/platform/settings/settings_types.cpp
+++ b/src/platform/settings/settings_types.cpp
@@ -57,37 +57,40 @@ SettingsValidationError validate_settings(const SettingsData& settings) noexcept
     }
     if (settings.window_mode != WindowMode::windowed &&
         settings.window_mode != WindowMode::fullscreen) {
         return SettingsValidationError::window_mode;
     }
 
     for (std::size_t index = 0; index < settings.bindings.size(); ++index) {
         if (!valid_key(settings.bindings[index])) {
             return SettingsValidationError::key_range;
         }
+        if (settings.bindings[index] == StableKey::v) {
+            return SettingsValidationError::reserved_key;
+        }
         for (std::size_t other = index + 1U; other < settings.bindings.size(); ++other) {
             if (settings.bindings[index] == settings.bindings[other]) {
                 return SettingsValidationError::duplicate_key;
             }
         }
     }
     return SettingsValidationError::none;
 }
 
 StableKey binding_for(const SettingsData& settings, SettingAction action) noexcept {
     const std::size_t index = action_index(action);
     return index < settings.bindings.size() ? settings.bindings[index] : StableKey::count;
 }
 
 bool assign_or_swap(SettingsData& settings, SettingAction action, StableKey key) noexcept {
     const std::size_t target = action_index(action);
-    if (target >= settings.bindings.size() || !valid_key(key)) {
+    if (target >= settings.bindings.size() || !valid_key(key) || key == StableKey::v) {
         return false;
     }
 
     const StableKey old_key = settings.bindings[target];
     for (std::size_t owner = 0; owner < settings.bindings.size(); ++owner) {
         if (owner != target && settings.bindings[owner] == key) {
             settings.bindings[owner] = old_key;
             break;
         }
     }
diff --git a/src/platform/settings/settings_types.hpp b/src/platform/settings/settings_types.hpp
index e5b649b..ad4067f 100644
--- a/src/platform/settings/settings_types.hpp
+++ b/src/platform/settings/settings_types.hpp
@@ -81,20 +81,21 @@ struct SettingsData final {
     std::array<StableKey, static_cast<std::size_t>(SettingAction::count)> bindings{};
     std::uint64_t revision{};
 };
 
 enum class SettingsValidationError : std::uint8_t {
     none,
     volume_range,
     volume_step,
     window_mode,
     key_range,
+    reserved_key,
     duplicate_key
 };
 
 [[nodiscard]] SettingsData default_settings() noexcept;
 [[nodiscard]] SettingsValidationError validate_settings(const SettingsData& settings) noexcept;
 [[nodiscard]] StableKey binding_for(const SettingsData& settings, SettingAction action) noexcept;
 [[nodiscard]] bool assign_or_swap(
     SettingsData& settings,
     SettingAction action,
     StableKey key) noexcept;
diff --git a/tests/settings/settings_codec_tests.cpp b/tests/settings/settings_codec_tests.cpp
index a9ee7e9..df57333 100644
--- a/tests/settings/settings_codec_tests.cpp
+++ b/tests/settings/settings_codec_tests.cpp
@@ -173,25 +173,35 @@ arpg::test::Failure invalid_and_duplicate_bindings_are_rejected() noexcept {
         SettingsCodecError::invalid_settings);
 
     bytes = valid;
     bytes[24] = bytes[25];
     refresh_crc(bytes);
     ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
         SettingsCodecError::invalid_settings);
     return {};
 }
 
+arpg::test::Failure reserved_v_binding_is_rejected_after_crc_refresh() noexcept {
+    auto bytes = arpg::settings::encode_settings(golden_settings());
+    bytes[24] = static_cast<std::uint8_t>(StableKey::v);
+    refresh_crc(bytes);
+    ARPG_REQUIRE(arpg::settings::decode_settings(bytes.data(), bytes.size()).error ==
+        SettingsCodecError::invalid_settings);
+    return {};
+}
+
 constexpr arpg::test::TestCase cases[] = {
     {"fixed layout matches golden bytes and CRC", fixed_layout_matches_golden_bytes},
     {"valid records round trip", valid_records_round_trip},
     {"null, truncated, and trailing records are rejected", null_truncated_and_trailing_records_are_rejected},
     {"all 44 single-byte mutations are rejected", every_single_byte_mutation_is_rejected},
     {"header and CRC errors are classified", header_and_crc_errors_are_classified},
     {"every reserved byte must be zero", every_reserved_byte_must_be_zero},
     {"invalid scalar fields are rejected", invalid_scalar_fields_are_rejected},
-    {"invalid and duplicate bindings are rejected", invalid_and_duplicate_bindings_are_rejected}};
+    {"invalid and duplicate bindings are rejected", invalid_and_duplicate_bindings_are_rejected},
+    {"reserved V binding with valid CRC is rejected", reserved_v_binding_is_rejected_after_crc_refresh}};
 
 }  // namespace
 
 arpg::test::TestSuite settings_codec_suite() noexcept {
     return arpg::test::make_suite("settings.codec", cases);
 }
diff --git a/tests/settings/settings_test_main.cpp b/tests/settings/settings_test_main.cpp
index 263004b..120de91 100644
--- a/tests/settings/settings_test_main.cpp
+++ b/tests/settings/settings_test_main.cpp
@@ -2,12 +2,12 @@
 
 arpg::test::TestSuite settings_types_suite() noexcept;
 arpg::test::TestSuite settings_codec_suite() noexcept;
 arpg::test::TestSuite settings_store_suite() noexcept;
 
 int main() {
     const arpg::test::TestSuite suites[] = {
         settings_types_suite(),
         settings_codec_suite(),
         settings_store_suite()};
-    return arpg::test::run_suites(suites, 32, "stage 11b task 3 settings store");
+    return arpg::test::run_suites(suites, 35, "stage 11b reserved V invariant");
 }
diff --git a/tests/settings/settings_types_tests.cpp b/tests/settings/settings_types_tests.cpp
index 8d26cb3..a059c65 100644
--- a/tests/settings/settings_types_tests.cpp
+++ b/tests/settings/settings_types_tests.cpp
@@ -20,20 +20,29 @@ constexpr std::array<SettingAction, 10> actions{
     SettingAction::move_down,
     SettingAction::move_left,
     SettingAction::move_right,
     SettingAction::light_attack,
     SettingAction::jump,
     SettingAction::launcher,
     SettingAction::interact,
     SettingAction::inventory,
     SettingAction::passive_tree};
 
+[[nodiscard]] bool same_settings(
+    const SettingsData& lhs, const SettingsData& rhs) noexcept {
+    return lhs.master_sfx_percent == rhs.master_sfx_percent &&
+        lhs.window_mode == rhs.window_mode &&
+        lhs.vsync_enabled == rhs.vsync_enabled &&
+        lhs.bindings == rhs.bindings &&
+        lhs.revision == rhs.revision;
+}
+
 arpg::test::Failure defaults_are_stable() noexcept {
     const SettingsData settings = arpg::settings::default_settings();
     const std::array<StableKey, 10> expected{
         StableKey::w, StableKey::s, StableKey::a, StableKey::d, StableKey::j,
         StableKey::k, StableKey::l, StableKey::e, StableKey::i, StableKey::p};
 
     ARPG_REQUIRE(settings.master_sfx_percent == 100U);
     ARPG_REQUIRE(settings.window_mode == WindowMode::windowed);
     ARPG_REQUIRE(settings.vsync_enabled);
     ARPG_REQUIRE(settings.revision == 0U);
@@ -87,62 +96,101 @@ arpg::test::Failure rejects_invalid_and_duplicate_keys() noexcept {
     ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
         SettingsValidationError::key_range);
 
     settings = arpg::settings::default_settings();
     settings.bindings[0] = settings.bindings[1];
     ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
         SettingsValidationError::duplicate_key);
     return {};
 }
 
+arpg::test::Failure rejects_reserved_v_in_every_binding() noexcept {
+    for (std::size_t index = 0; index < actions.size(); ++index) {
+        SettingsData settings = arpg::settings::default_settings();
+        settings.bindings[index] = StableKey::v;
+        ARPG_REQUIRE(arpg::settings::validate_settings(settings) ==
+            SettingsValidationError::reserved_key);
+    }
+    return {};
+}
+
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
 
+arpg::test::Failure reserved_v_assignment_is_rejected_without_mutation() noexcept {
+    SettingsData settings = arpg::settings::default_settings();
+    settings.master_sfx_percent = 75U;
+    settings.window_mode = WindowMode::fullscreen;
+    settings.vsync_enabled = false;
+    settings.revision = 42U;
+    const SettingsData before = settings;
+
+    ARPG_REQUIRE(!arpg::settings::assign_or_swap(
+        settings, SettingAction::light_attack, StableKey::v));
+    ARPG_REQUIRE(same_settings(settings, before));
+    return {};
+}
+
 arpg::test::Failure repeated_swaps_preserve_uniqueness() noexcept {
+    constexpr std::array<StableKey, 44> assignable_keys{
+        StableKey::a, StableKey::b, StableKey::c, StableKey::d, StableKey::e,
+        StableKey::f, StableKey::g, StableKey::h, StableKey::i, StableKey::j,
+        StableKey::k, StableKey::l, StableKey::m, StableKey::n, StableKey::o,
+        StableKey::p, StableKey::q, StableKey::r, StableKey::s, StableKey::t,
+        StableKey::u, StableKey::w, StableKey::x, StableKey::y, StableKey::z,
+        StableKey::digit_0, StableKey::digit_1, StableKey::digit_2,
+        StableKey::digit_3, StableKey::digit_4, StableKey::digit_5,
+        StableKey::digit_6, StableKey::digit_7, StableKey::digit_8,
+        StableKey::digit_9, StableKey::arrow_up, StableKey::arrow_down,
+        StableKey::arrow_left, StableKey::arrow_right, StableKey::space,
+        StableKey::left_shift, StableKey::right_shift,
+        StableKey::left_control, StableKey::right_control};
     SettingsData settings = arpg::settings::default_settings();
     for (std::size_t iteration = 0; iteration < 1000U; ++iteration) {
         const std::size_t target = (iteration * 7U + 3U) % actions.size();
-        const std::size_t owner = (iteration * 3U + 1U) % actions.size();
-        const StableKey occupied = arpg::settings::binding_for(settings, actions[owner]);
-        ARPG_REQUIRE(arpg::settings::assign_or_swap(settings, actions[target], occupied));
+        const StableKey candidate = assignable_keys[(iteration * 3U + 1U) % assignable_keys.size()];
+        ARPG_REQUIRE(candidate != StableKey::v);
+        ARPG_REQUIRE(arpg::settings::assign_or_swap(settings, actions[target], candidate));
         ARPG_REQUIRE(arpg::settings::validate_settings(settings) == SettingsValidationError::none);
         for (std::size_t lhs = 0; lhs < actions.size(); ++lhs) {
             for (std::size_t rhs = lhs + 1U; rhs < actions.size(); ++rhs) {
                 ARPG_REQUIRE(settings.bindings[lhs] != settings.bindings[rhs]);
             }
         }
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
+    {"reserved V is rejected in every binding", rejects_reserved_v_in_every_binding},
     {"unoccupied key assignment succeeds", assigns_an_unoccupied_key},
     {"occupied key assignment swaps owners", swaps_an_occupied_key},
+    {"reserved V assignment leaves settings unchanged", reserved_v_assignment_is_rejected_without_mutation},
     {"one thousand swaps preserve uniqueness", repeated_swaps_preserve_uniqueness}};
 
 }  // namespace
 
 arpg::test::TestSuite settings_types_suite() noexcept {
     return arpg::test::make_suite("settings.types", cases);
 }

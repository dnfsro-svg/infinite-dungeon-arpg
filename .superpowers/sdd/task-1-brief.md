### Task 1: Settings Model and V1-to-V2 Codec

**Files:**
- Modify: `src/platform/settings/settings_types.hpp`
- Modify: `src/platform/settings/settings_types.cpp`
- Modify: `src/platform/settings/settings_codec.cpp`
- Modify: `tests/settings/settings_types_tests.cpp`
- Modify: `tests/settings/settings_codec_tests.cpp`
- Modify: `tests/settings/settings_store_tests.cpp`
- Modify: `tests/settings/settings_stress_tests.cpp`

**Interfaces:**
- Produces: `enum class LootFilterMode : std::uint8_t { show_all, magic_or_better, rare_only }`.
- Produces: `SettingsData::loot_filter_mode` defaulting to `show_all`.
- Produces: `const char* loot_filter_label(LootFilterMode) noexcept`.
- Preserves: `kSettingsEncodedSize == 44U`; V2 uses byte 23 and V1 decodes to `show_all`.

- [ ] **Step 1: Write failing model and codec tests**

Add tests that require all three modes to validate, require an out-of-range mode to return a new `SettingsValidationError::loot_filter_mode`, require defaults to use `show_all`, require encoded bytes 8-9 to hold format `2`, and require byte 23 to round-trip `rare_only`. Build a real V1 record by changing the format bytes to `1`, setting byte 23 to zero, and refreshing CRC; assert it decodes to `show_all`.

```cpp
SettingsData values = arpg::settings::default_settings();
ARPG_REQUIRE(values.loot_filter_mode == LootFilterMode::show_all);
values.loot_filter_mode = LootFilterMode::rare_only;
const auto encoded = arpg::settings::encode_settings(values);
ARPG_REQUIRE(encoded[8] == 2U && encoded[9] == 0U);
ARPG_REQUIRE(encoded[23] == static_cast<std::uint8_t>(LootFilterMode::rare_only));
const auto decoded = arpg::settings::decode_settings(encoded.data(), encoded.size());
ARPG_REQUIRE(decoded.error == SettingsCodecError::none);
ARPG_REQUIRE(decoded.settings.loot_filter_mode == LootFilterMode::rare_only);
```

- [ ] **Step 2: Run the settings suite and verify RED**

Run:

```powershell
cmake --build out/build/windows-msvc-debug --target arpg_settings_tests arpg_settings_stress_tests
$env:ARPG_TEST_FILTER='settings.'; .\out\build\windows-msvc-debug\bin\arpg_settings_tests.exe
```

Expected: compilation fails because `LootFilterMode` and `loot_filter_mode` do not exist.

- [ ] **Step 3: Implement the model and dual-format codec**

Define the enum and field, add range validation and stable labels. Encode format 2 with byte 23. Decode formats 1 and 2: V1 still rejects nonzero byte 23 and migrates to `show_all`; V2 interprets byte 23 and validates it. Keep bytes 34-39 reserved and keep the same CRC coverage.

```cpp
enum class LootFilterMode : std::uint8_t {
    show_all,
    magic_or_better,
    rare_only,
};

struct SettingsData final {
    std::uint8_t master_sfx_percent{100};
    WindowMode window_mode{WindowMode::windowed};
    bool vsync_enabled{true};
    LootFilterMode loot_filter_mode{LootFilterMode::show_all};
    std::array<StableKey, static_cast<std::size_t>(SettingAction::count)> bindings{};
    std::uint64_t revision{};
};
```

- [ ] **Step 4: Update store/stress equality helpers and verify GREEN**

Every test helper comparing `SettingsData` must include `loot_filter_mode`. Extend the 1000-cycle stress trace to rotate through all three modes and verify mixed V1/V2 slots choose the highest legal revision. Rebuild and run both settings executables; expected all tests pass and allocation counts remain zero.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/settings tests/settings
git commit -m "feat: persist loot filter settings"
```


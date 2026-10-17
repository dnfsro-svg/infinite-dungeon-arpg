# Stage 11-D Task 1 Report — Settings Model and V1-to-V2 Codec

## Status

DONE.

## Implementation

- Added `LootFilterMode : std::uint8_t` with `show_all`, `magic_or_better`, and `rare_only`.
- Added `SettingsData::loot_filter_mode`, defaulting to `show_all`, validation error `loot_filter_mode`, and stable labels: `Show All`, `Magic or Better`, `Rare Only`.
- Kept `kSettingsEncodedSize == 44U`, payload size 20, reserved bytes 34–39, and CRC coverage unchanged.
- Encoder now writes format V2 at bytes 8–9 and the filter mode at byte 23.
- Decoder accepts V1 and V2. V1 still requires byte 23 to be zero and migrates it to `show_all`; V2 validates byte 23 through `validate_settings`.
- Extended settings equality helpers and the 1,000-cycle trace to carry all three modes. The stress test also verifies that a legal higher-revision V2 slot wins over a legal V1 slot.
- Brief file-list correction authorized by the parent: added `src/platform/settings/settings_store.cpp` so equal-revision slots differing only by filter mode are treated as a conflict rather than as identical records.

## TDD evidence

1. RED — after model/codec tests were added, the MSVC build failed with missing `LootFilterMode`, `SettingsData::loot_filter_mode`, and `loot_filter_label` symbols.
2. GREEN — after the minimal model and dual-format codec implementation, the focused suite passed: `35 cases, 0 failures`; stress passed: `3 cases, 0 failures`.
3. RED — the adjusted store test created equal-revision V2 slots differing only by `loot_filter_mode`; it failed at `result.status == SettingsLoadStatus::defaults_corrupt` because production `same_settings` omitted the field.
4. GREEN — after adding the field to production `same_settings`, the same focused suite and stress executable both passed.

## Test commands and results

```powershell
cmake --build out/build/windows-msvc-debug --target arpg_settings_tests arpg_settings_stress_tests -- -j1
$env:ARPG_TEST_FILTER='settings.'; .\out\build\windows-msvc-debug\bin\arpg_settings_tests.exe
$env:ARPG_TEST_FILTER='stage11b.settings_stress'; .\out\build\windows-msvc-debug\bin\arpg_settings_stress_tests.exe
```

The commands were executed through the installed VS 2022 x64 developer environment because the base shell lacked C++ standard-library include paths. Final result: settings `35/35`, stress `3/3`, including the existing zero-allocation assertion.

## Modified files

- `src/platform/settings/settings_types.hpp`
- `src/platform/settings/settings_types.cpp`
- `src/platform/settings/settings_codec.cpp`
- `src/platform/settings/settings_store.cpp` (authorized correction)
- `tests/settings/settings_types_tests.cpp`
- `tests/settings/settings_codec_tests.cpp`
- `tests/settings/settings_store_tests.cpp`
- `tests/settings/settings_stress_tests.cpp`

## Self-review and concerns

- `git diff --check` passed; review confirmed the record remains 44 bytes, V1 rejects nonzero byte 23, V2 validates its range, and V6 save slots are only fingerprinted by the existing stress test and remain unchanged.
- No remaining functional concern. Git reports existing line-ending normalization warnings (`LF will be replaced by CRLF`) only.

## Commit

`8da29e5 feat: persist loot filter settings`

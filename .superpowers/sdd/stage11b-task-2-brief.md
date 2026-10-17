### Task 2: Fixed V1 Settings Codec

**Files:**
- Modify: `src/core/CMakeLists.txt`
- Create: `src/core/crc32.hpp`
- Create: `src/core/crc32.cpp`
- Modify: `src/persistence/CMakeLists.txt`
- Modify: `src/persistence/crc32.hpp`
- Delete: `src/persistence/crc32.cpp`
- Modify: `src/platform/settings/CMakeLists.txt`
- Create: `src/platform/settings/settings_codec.hpp`
- Create: `src/platform/settings/settings_codec.cpp`
- Modify: `tests/settings/CMakeLists.txt`
- Create: `tests/settings/settings_codec_tests.cpp`
- Modify: `tests/settings/settings_test_main.cpp`

**Interfaces:**
- Consumes: `SettingsData`, `validate_settings`.
- Produces:

```cpp
inline constexpr std::size_t kSettingsEncodedSize = 44U;
enum class SettingsCodecError : std::uint8_t {
    none, wrong_size, wrong_magic, wrong_format, wrong_payload_size,
    bad_crc, reserved_nonzero, invalid_settings
};
struct SettingsDecodeResult final {
    SettingsCodecError error{SettingsCodecError::none};
    SettingsData settings{};
};
[[nodiscard]] std::array<std::uint8_t, kSettingsEncodedSize>
    encode_settings(const SettingsData&) noexcept;
[[nodiscard]] SettingsDecodeResult decode_settings(
    const std::uint8_t*, std::size_t) noexcept;
```

**Layout:** magic `ARPGSET1` at 0–7; format `1` at 8–9; payload size `20` at 10–11; revision/generation at 12–19; volume/mode/vsync/reserved at 20–23; ten keys at 24–33; six reserved zero bytes at 34–39; CRC32 of bytes 8–39 at 40–43.

- [ ] **Step 1: Write layout and corruption RED tests**

Assert every offset, little-endian revision `0x0102030405060708`, golden CRC, round-trip, all 44 single-byte covered mutations, bad magic/format/length/reserved/keys/duplicates, null input, truncation and trailing bytes.

- [ ] **Step 2: Verify RED**

Run `cmake --build --preset windows-msvc-debug --target arpg_settings_tests`.

Expected: compile fails because codec symbols are missing.

- [ ] **Step 3: Implement explicit byte codec**

Use byte writes/reads only; no struct serialization or `reinterpret_cast`. Move the existing implementation unchanged to `arpg::core::crc32`/`crc32_update`; keep `persistence/crc32.hpp` as inline compatibility forwarding functions so all V1–V6 callers and tests retain their source API. Link `arpg_settings PRIVATE arpg_core`; it must not link `arpg_persistence` or include checkpoint/save-store headers.

- [ ] **Step 4: Run focused tests**

Run `ctest --test-dir out/build/windows-msvc-debug -R "^(settings.units|persistence.units)$" --output-on-failure`.

Expected: 2/2 pass and existing V6 codec cases remain unchanged.

- [ ] **Step 5: Commit**

```powershell
git add src/core src/persistence src/platform/settings tests/settings
git commit -m "feat: encode versioned settings records"
```

---


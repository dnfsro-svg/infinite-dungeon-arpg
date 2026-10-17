### Task 3: Dual-Slot Atomic Settings Store

**Files:**
- Modify: `src/platform/settings/CMakeLists.txt`
- Create: `src/platform/settings/settings_store.hpp`
- Create: `src/platform/settings/settings_store.cpp`
- Create: `tests/settings/settings_store_tests.cpp`
- Modify: `tests/settings/CMakeLists.txt`
- Modify: `tests/settings/settings_test_main.cpp`

**Interfaces:**

```cpp
enum class SettingsLoadStatus : std::uint8_t {
    loaded, defaults_missing, recovered_single_slot, defaults_corrupt
};
enum class SettingsSaveStatus : std::uint8_t {
    committed, invalid_settings, stale_revision, revision_overflow,
    write_failed, readback_failed
};
struct SettingsLoadResult final { SettingsLoadStatus status; SettingsData settings; };
struct SettingsSaveResult final { SettingsSaveStatus status; SettingsData settings; };
struct SettingsFileOps final {
    void* context{};
    bool (*read)(void*, const std::filesystem::path&, std::vector<std::uint8_t>&){};
    bool (*replace)(void*, const std::filesystem::path&,
        const std::uint8_t*, std::size_t){};
};
class SettingsStore final {
public:
    explicit SettingsStore(std::filesystem::path, SettingsFileOps = native_settings_file_ops());
    [[nodiscard]] SettingsLoadResult load() const;
    [[nodiscard]] SettingsSaveResult save(
        const SettingsData& committed, SettingsData draft) const;
};
```

`save` requires `draft.revision == committed.revision`; it increments revision internally after overflow check, validates, writes the invalid/older slot through `.tmp`, atomically replaces, reads back and returns the published value.

- [ ] **Step 1: Write fake-I/O RED tests**

Cover missing slots, A/B newest selection, wrap-safe comparison without allowing revision wrap, one-slot recovery, equal-generation identical data, equal-generation conflict, invalid CRC, write failure, replace failure, readback failure, and preservation of the last valid slot.

- [ ] **Step 2: Verify RED**

Run the settings target; expected compile failure for missing store.

- [ ] **Step 3: Implement native and injected I/O**

Native replacement writes `settings-a.bin.tmp` or `settings-b.bin.tmp`, flushes/closes, then on Windows calls `MoveFileExW(temp, slot, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)` to replace only that slot. Load never deletes files and never touches `slot-a.sav`/`slot-b.sav`.

- [ ] **Step 4: Run tests and filesystem boundary scan**

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^(settings.units|persistence.units)$" --output-on-failure
rg -n "slot-a\.sav|slot-b\.sav|checkpoint_codec|SaveStore" src/platform/settings
```

Expected: tests 2/2; `rg` has no matches.

- [ ] **Step 5: Commit**

```powershell
git add src/platform/settings tests/settings
git commit -m "feat: persist settings in atomic dual slots"
```

---


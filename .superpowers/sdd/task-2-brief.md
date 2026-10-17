### Task 2: Eight-Tick Bounded Input Buffer

**Files:** `src/combat/input_buffer.hpp/.cpp`, `tests/combat/input_buffer_tests.cpp`, related CMake/test-main entries.

**Interface:** 32 fixed entries, each valid for exactly 8 active input ticks; oldest matching action can be consumed without head-of-line blocking.

- [ ] **Step 1: Add four RED cases**

Prove oldest matching consumption, exact T..T+7 validity, `age(true)` pause, 33rd push overflow, stable removal, clear and diagnostics. Change exact combat count 3→7.

```cpp
InputBuffer buffer;
ARPG_REQUIRE(buffer.push(Action::light));
for (int tick = 0; tick < 7; ++tick) { buffer.age(false); }
ARPG_REQUIRE(buffer.consume(Action::light));
ARPG_REQUIRE(buffer.push(Action::heavy));
for (int tick = 0; tick < 8; ++tick) { buffer.age(false); }
ARPG_REQUIRE(!buffer.consume(Action::heavy));
ARPG_REQUIRE(buffer.expired_count() == 1);
```

- [ ] **Step 2: Observe missing-header RED**

Run core-debug; expected missing `combat/input_buffer.hpp`.

- [ ] **Step 3: Implement fixed storage**

```cpp
enum class Action : std::uint8_t { light, jump, heavy, launcher };
class InputBuffer final {
public:
    static constexpr std::size_t kCapacity = 32;
    static constexpr std::uint8_t kLifetimeTicks = 8;
    [[nodiscard]] bool push(Action) noexcept;
    [[nodiscard]] bool consume(Action) noexcept;
    void age(bool paused) noexcept;
    void clear() noexcept;
    void reset_diagnostics() noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] std::uint32_t expired_count() const noexcept;
    [[nodiscard]] std::uint32_t overflow_count() const noexcept;
private:
    struct Entry final { Action action{}; std::uint8_t remaining_ticks{}; };
    std::array<Entry,kCapacity> entries_{};
    std::size_t size_{};
    std::uint32_t expired_count_{};
    std::uint32_t overflow_count_{};
};
```

Push appends `{action,8}`; consume removes the oldest matching entry; age(false) decrements once and stably compacts nonzero entries; age(true) does nothing. No allocation.

- [ ] **Step 4: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
rg -n "malloc|calloc|realloc|new " src/combat/input_buffer.hpp src/combat/input_buffer.cpp
git add src/combat tests/combat
git commit -m "feat: add bounded combat input buffer"
```

Expected combat `7 cases, 0 failures`.

---


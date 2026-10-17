### Task 5: XYZ Collision, Position Correction, Hit Registry, and Feedback Aggregation

**Files:** `combat_collision.hpp/.cpp`, `hit_resolution.cpp`, `hit_resolution_tests.cpp`, type/world/CMake/test-main updates.

**Interfaces:** inclusive AABB, facing mirror, one assist correction, per-serial hit flags, `CombatEvent`, event queue capacity 64 and `try_pop_event`.

- [ ] **Step 1: Add five RED hit cases**

Prove inclusive X/Y/Z boundaries and epsilon miss, facing mirror, depth tolerance, one target once across active ticks, three-target independent hits plus one impact summary/max Hit Stop, and J1 real-hit cancel at elapsed 8 versus whiff 13. Change count 15→20.

- [ ] **Step 2: Observe RED**

Run core-debug; expected HP/events unchanged because collision is absent.

- [ ] **Step 3: Add events and collision functions**

```cpp
enum class CombatEventKind : std::uint8_t {
 swing, hit, impact_summary, landing, break_started,
 defeated, respawned, reset
};
struct CombatEvent final {
 CombatEventKind kind{};
 std::uint64_t tick{};
 AttackId attack{AttackId::none};
 std::uint8_t target_index{0xFF};
 std::uint8_t hit_count{};
 FeedbackLevel feedback{};
 Vec3 position{};
 int value{};
};
```

CombatWorld owns `core::BoundedQueue<CombatEvent,64>`; every failed push increments diagnostics. Hurtbox half extents are light `{0.45,0.35,1.40}`, normal `{0.55,0.40,1.60}`, heavy `{0.70,0.50,1.90}`. Mirror only local X, translate all axes, and use inclusive overlap.

Assist selects by smallest hurtbox surface gap then index. Require horizontal gap `<=0.35`, absolute Y `<=0.45`, correction only X, magnitude `<=0.18`, clamped to room.

- [ ] **Step 4: Collect before applying attacker freeze**

Build one world attack box, collect indices 0→2 without mutating iteration order, then apply HP/raw-break changes and record the pending ImpactKind. Mark each hit, emit per-target hit, set connected, accumulate max feedback/hit count, and set target Hit Stop 3/5/7. Task 6 turns ImpactKind into motion/reaction state; Task 7 adds armored suppression and breaking-blow priority. After the loop set player Hit Stop once and emit one first-hit `impact_summary`; emit swing at attack start. A frozen attack does not perform collision again.

- [ ] **Step 5: Run GREEN and commit**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
git add src/combat tests/combat
git commit -m "feat: resolve deterministic combat hits"
```

Expected combat `20 cases, 0 failures`.

---


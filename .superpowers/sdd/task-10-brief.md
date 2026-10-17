### Task 10: Deterministic stress, allocation and capacity gates

**Files:**
- Modify: `tests/combat/break_stress_tests.cpp`
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `tests/combat/combat_test_main.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Interfaces:**
- Consumes complete combat/director/session implementation.
- Produces no product API; adds release-grade invariants and count guards.

- [ ] **Step 1: Add stress tests and observe failures before final hardening**

Add:

1. 10,000 director plans across all four ecologies with zero illegal plans, deterministic replay equality, and no capacity overflow.
2. 1,000 complete rooms using real monster AI/player attacks and explicit save confirmation after each transition.
3. Snapshot allocation counter before/after the steady-state portion; assert zero net live allocation growth and zero allocations inside instrumented combat ticks/director generation.
4. Force each pool to capacity and verify one extra creation is rejected, existing slots remain unchanged, and diagnostic counters saturate instead of wrapping.

- [ ] **Step 2: Run Debug and Release stress executables**

Expected before hardening: any hidden capacity, event queue, timeout, or allocation assumptions fail with a named invariant rather than hanging.

- [ ] **Step 3: Make only the minimal bounded fixes exposed by RED**

Allowed fixes are capacity constants already approved, bounded event draining, saturating diagnostics, or deterministic tick limits. Do not weaken assertions or add dynamic containers.

- [ ] **Step 4: Run full build matrix**

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
```

Expected: Core-only, Debug, and Release CTest all pass; direct combat/dungeon executables report zero failures; stress logs report 1000 rooms and zero sustained allocations/overflows.

- [ ] **Step 5: Commit**

```powershell
git add tests/combat tests/dungeon src
git commit -m "test: verify stage 4 encounter endurance"
```

---


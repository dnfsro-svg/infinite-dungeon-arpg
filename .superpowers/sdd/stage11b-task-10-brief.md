# Stage 11-B Task 10 Brief — Stress and Architecture Guards

Implement only Task 10 in `docs/superpowers/plans/2026-07-17-stage11b-settings-input.md`.

Add CTest names `stage11b.settings_stress.atomic_reload_zero_alloc` and `stage11b.architecture.settings_boundaries` exactly as planned. The stress test performs 1,000 swap/apply/load cycles across both slots, restart intervals 17/31/43, verifies direct versus restarted settings, revisions 1..1000, unique valid bindings, final bytes and sentinel hashes. Allocation probes must prove zero allocations for validation, assign/swap, frame mapping and unchanged hint refresh; Store I/O is explicitly excluded.

Architecture guard must reject raylib/Combat/Dungeon/Items/Progression/Passives includes in settings, settings headers in core, gameplay save filenames in settings store, and direct gameplay key constants in host/HUD. It needs its own mutation self-check on copied sources for #include <raylib.h>, `slot-a.sav`, `KEY_J`, and duplicate stable key, with named reasons. Dedicated environment filter and 300 second timeout are required.

Scope is tests/settings and tests/platform registrations/guards only. Do not change product behavior or implement Task11+. TDD first: add tests/guards, prove RED missing registration or guard failure, then implement. Build and run the two focused CTests plus settings/platform/persistence regression (or equivalent direct binaries when CTest log is sandbox-blocked), diff check, commit `test: stress stage 11b settings boundaries`, and report evidence.

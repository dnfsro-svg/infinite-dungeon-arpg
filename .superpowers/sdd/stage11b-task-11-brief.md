# Stage 11-B Task 11 Brief — Real Raylib Validation Evidence

Implement only Task 11 from `docs/superpowers/plans/2026-07-17-stage11b-settings-input.md`.

Create the exact enum `Stage11BValidationScenario` with the specified eight values. Add formal validation that runs the actual raylib host and only injects physical key edges before the normal production mapper. It must never expose setters for `PauseMenuState`, `SettingsData`, or logical actions.

The evidence summary must contain fresh 1280x720 screenshots and: paused tick before/after 120 *presented* frames; player/monster hash; committed revision; old/new attack counts; swap pair; restart binding; one-slot recovery; corrupt-default status; unchanged character-save hash. Rebound attack proves old physical key produces zero actions and new key at least one after Apply. Restart must destroy and reconstruct host runtime from the settings directory.

Add production evidence guards requiring `sample_physical_keys -> map_host_frame_input -> pause gate -> submit_frame_actions`, one post-Present capture helper, actual SettingsStore save/load and host restart. Reject direct queue_action, direct committed state assignment, TestAccess, pre-Present capture and validation-only input setters. Include mutation self-test and bad host input fixture.

TDD: first register/formal guard so missing target or guard failure is demonstrated RED; implement minimum production-backed harness; run every `^stage11b.settings_(formal|evidence)` test, execute validator, inspect screenshots for size and visible pause/settings/rebound content, run diff check, and commit `test: validate stage 11b settings in raylib`. No Task12 documentation or global final suite yet.

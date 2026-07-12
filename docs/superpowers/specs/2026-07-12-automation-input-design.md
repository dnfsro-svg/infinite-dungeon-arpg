# Stage 3 Automation Input Compatibility Design

## Goal

Allow the Windows desktop-control layer to drive the raylib game for acceptance testing without changing normal physical-keyboard behavior.

## Scope

- Add an opt-in `--automation-input` host argument.
- When disabled, input remains exactly as today: `IsKeyDown` drives WASD movement and `IsKeyPressed` drives J/K/L/E/F1/F12/R/Escape behavior.
- When enabled, queued raylib character input provides a fallback for `WASD`, `J`, `K`, `L`, `E`, `R`, and `F`. Uppercase and lowercase are equivalent.
- Synthetic movement characters create a short bounded movement pulse so repeated text input can move the player while preserving physical held-key input.
- Synthetic action characters are edge-triggered once per received character.
- `F` toggles the debug overlay in automation mode; F12 screenshots and Escape remain physical/window controls to avoid ambiguous text commands.

## Architecture

Introduce a small platform-only, raylib-free `AutomationInputState` helper. It consumes character codes, stores bounded movement-pulse counters and one-frame action flags, and exposes movement/action queries. The raylib host drains `GetCharPressed()` only when the launch option is enabled, merges the fallback movement with physical WASD, and merges fallback action edges with existing key edges.

The helper lives under `src/platform/raylib` but does not include raylib, so platform unit tests can validate it deterministically.

## Safety and Compatibility

- The feature is disabled unless `--automation-input` is present.
- No dungeon, combat, persistence, RNG, or save-format code changes.
- Bounded counters prevent unbounded queues or allocations.
- Physical keyboard input remains authoritative and can be used simultaneously.
- Unknown characters are ignored.

## Verification

- RED/GREEN unit tests cover argument parsing, disabled behavior, case-insensitive actions, bounded movement pulses, edge consumption, and unknown characters.
- Run platform tests and full Debug/Release CTest suites.
- Launch with fixed seed `8`, isolated save directory, and `--automation-input`; drive the intended route with desktop text input and verify the room-3 `Hole SEALED/READY` state and `E` descent.

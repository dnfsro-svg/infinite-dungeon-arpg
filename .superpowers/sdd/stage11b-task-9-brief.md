# Stage 11-B Task 9 Brief — Dynamic Control Hints

Implement only Task 9 from `docs/superpowers/plans/2026-07-17-stage11b-settings-input.md`.

Goal: replace stale hard-coded gameplay control text with fixed-buffer hints generated from the committed binding revision. The host refreshes after startup load and after a successful Apply only; render paths consume `const ControlHints&` and never rebuild the strings per frame.

Required API:

```cpp
struct ControlHints final {
    std::array<char, 160> primary{};
    std::array<char, 160> secondary{};
    std::uint64_t revision{};
};
void refresh_control_hints(ControlHints&, const settings::SettingsData&) noexcept;
```

Scope files are exactly the Task 9 files listed in the plan, plus narrowly necessary integration tests/source guards. Preserve fixed F1/F12/Esc labels. Use stable-key labels and `snprintf`; no heap allocations during 10,000 unchanged refresh calls. Tests must cover defaults, arbitrary swaps, revision refresh/no-refresh, NUL termination, all labels, no stale hard-coded gameplay line, and allocation behavior.

Do not implement Task 10+ or alter pause/settings semantics. First write RED tests, then implement, build, run platform tests and the prescribed `rg` stale-line guard, `git diff --check`, and commit `feat: show current bindings in hud`. Report changed files, tests, guard result and commit.

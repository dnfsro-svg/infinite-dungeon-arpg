### Stage 11-D Task 8: Architecture Guards and Regression Stress

Worktree: `E:/game/.worktrees/stage11d-loot-filter`
Start commit: `1004940`

Implement only Task 8 from `docs/superpowers/plans/2026-07-18-stage11d-ground-loot-filter.md`. The design spec is authoritative. Do not start formal raylib evidence (Task 9) or documentation/final gates (Task 10).

Required TDD sequence:

1. Register `stage11d.architecture.loot_boundaries` before the guard script exists and run that exact CTest to capture genuine RED.
2. Implement positive checks that reject every boundary listed in Task 8 without brittle variable-name/format assumptions.
3. Add a mutation self-test that proves all seven required bad variants fail for the intended reason: `std::string`, `IsKeyDown`, `session.tick`, `SettingsStore`, dungeon-to-settings include, rarity check in explicit pickup, and filter condition missing abyss bypass. Include at least two semantics-preserving good variants so the guard does not merely grep one spelling.
4. Extend deterministic dungeon equipment and settings stress traces exactly as Task 8 requires. Tests must first fail for missing coverage/behavior before the minimum test/guard implementation is completed.
5. Run the named guard/self-test, direct dungeon/settings stress executables, relevant architecture CTests, 100,000-operation allocation probes, and `git diff --check`.

Scope and evidence rules:

- Do not weaken any existing architecture/evidence guard.
- Do not add production behavior unless a genuine regression is exposed; report any such need before editing production.
- Guard scripts must fail closed if required source files, mutation tokens, or substitutions are missing.
- Avoid unbounded seed searches or network access.
- Preserve all tracked Stage 10/11 historical `.superpowers/sdd/task-8*` reports. Write the new report to `.superpowers/sdd/stage11d-task-8-report.md` and force-add only that new long-term report; keep `task-8-brief.md` and `progress.md` local/ignored.
- Commit as `test: guard stage 11d loot boundaries`, leave the worktree clean, and report commit hash plus exact RED/GREEN commands/counts.

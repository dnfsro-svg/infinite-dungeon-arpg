# Stage 11-B Task 12 Brief — Documentation, Full Gates, Reviews and Stop

Implement only Task 12 from the Stage11-B plan. Do not begin HUD completion, filters, macOS or Stage11-C; do not merge main.

Update README with default keys, Esc overlay priority, Apply/Cancel, settings location, swap conflicts and reset defaults. Create `docs/validation/stage11b-settings-input.md`: map every design requirement to CTest names; document V1 44-byte layout, dual-slot recovery, 1,000 stress, fresh formal screenshot paths and explicitly excluded scope.

Run the exact fresh Debug and Release commands from Task12 in MSVC 19.44/SDK26100 environment: configure fresh, clean-first build, complete CTest. Record totals and failures (if any) accurately. Ensure Release formal evidence is fresh. Run complete boundary/artifact audit from the plan; no generated artifacts tracked, no scope expansion.

Commit docs only as `docs: complete stage 11b settings milestone` after builds/audit. Then report all exact evidence needed for two independent full-diff reviewers: base `918f2e9..HEAD`, design invariants, fixed bytes, freeze, input latency, V6 hash, screenshots and mutation coverage. The controller will arrange separate reviews; do not self-approve the milestone.

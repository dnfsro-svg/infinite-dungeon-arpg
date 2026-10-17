# Stage 16 Task 5 report - material bag, labels, and pickup feedback

- Separate material labels and colors read `ground_materials` directly; they never consult the equipment filter.
- Exalt, Directed Core, +12, and +15 use an emphasized beam and label treatment.
- A 14-slot material bag was added to the paused inventory detail region without changing the equipment grid capacity.
- Empty material slots cannot replace `selected_material()`.
- Pickup receipts aggregate by material id for two seconds and publish name-plus-count feedback through HUD notices.

TDD evidence: the initial new view/bag tests failed because both headers were absent; the pickup aggregation test later failed because `MaterialPickupFeedbackState` was absent. Both passed after minimal implementations.

Final verification in the fixed MSVC/SDK environment: `arpg_platform_tests` and `arpg_game` built; `ctest --test-dir out\\build\\windows-msvc-debug -R '^platform.units$' --output-on-failure` passed 1/1 in 5.50s; `git diff --check` passed.

Review fix: added a RED 800x450 layout test for the overlap between item detail
text and the material bag.  `material_bag_detail_bounds()` now reserves the
material-bag region and the inventory detail scissor is clipped above it.
Final verification after the fix: platform.units 1/1 passed in 5.43s and
`arpg_game` linked.

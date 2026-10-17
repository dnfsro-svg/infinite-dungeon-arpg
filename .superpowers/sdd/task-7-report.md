# Stage 7 Task 7 Report

## Delivered

- Added a pure, window-free passive tree view-math API: fixed-canvas projection,
  hit testing, visual-state selection, and gameplay-input gate decisions.
- Added a raylib-only full-screen passive star-chart overlay with route colours,
  undirected links, node states, points, mouse tooltip, keystone benefit/cost
  text, and autosave feedback.
- Wired the host so `P` only toggles the chart in a clean `awaiting_exit` room.
  While open, it captures movement, J/K/L, reset, door actions, descent, and
  left mouse clicks. A click asks `DungeonSession` to allocate or refund; the
  host does not calculate graph legality or points.
- Updated HUD instructions and displays the player barrier when present.

## Test-driven evidence

1. Added `passive_tree_view_tests.cpp` before the production files.
2. RED was observed after a Visual Studio environment configure: the build
   failed because `passive_tree_view_math.hpp` did not exist.
3. GREEN: `arpg_platform_tests.exe` reports `50 cases, 0 failures`.
4. Focused CTest passed all 9 selected checks:
   `platform.view_math`, `platform.input_latency_source`, and the eight
   `architecture.*raylib` checks.
5. `git diff --check` passed.

## Boundary confirmation

`passive_tree_view_math` depends on dungeon/passives data but contains no
raylib include. `passive_tree_renderer` is the sole new renderer that includes
raylib. No raylib dependency was added to passives, combat, dungeon, or
persistence.

## Deferred

Window-driven visual acceptance and end-to-end stress/replay validation remain
for Task 8.

## Review fix

- Root cause: the catalog intentionally reused the same local coordinates for
  fire/lightning and water/chaos route shapes, while the original view math
  projected them without a route layout transform. `hit_test_passive_node()`
  then returned the first lower-ID circle at each overlap.
- Fix: the pure view layer now gives route bases 8/22/36/50 stable top-left,
  top-right, bottom-left, and bottom-right offsets. It does not alter catalog
  coordinates, passive rules, allocation, refunds, or persistence.
- Added a 56-node platform test that verifies every route node hits itself and
  every cross-route pair has a distinct projection. The platform executable now
  reports 51 passing cases.
- Renderer labels and tooltips now use concrete per-node modifier text. Each
  Keystone permanently displays its real `收益` and `代价`; Notables show their
  actual values and non-Keystone nodes explicitly use `代价: 无` where relevant.

## Final spacing fix

- The first review fix separated route centers but left the opposing Keystone
  hit circles too close. Route horizontal offsets are now `-200` and `+200`;
  vertical offsets and each route's local structure remain unchanged.
- The projection regression now checks every cross-route pair among nodes
  8–63 at both 1280×720 and 800×450. It requires Euclidean center distance to
  exceed the two projected radii plus a 4px margin, while still proving that
  every node center resolves to its own ID.

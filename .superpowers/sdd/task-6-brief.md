### Task 6: Renderer and Draft Preview Integration

**Files:**
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/hud_renderer.hpp`
- Modify: `src/platform/raylib/hud_renderer.cpp`
- Modify: `src/platform/raylib/hud_font.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/hud_font_tests.cpp`
- Modify: `tests/platform/hud_host_integration_tests.cpp`

**Interfaces:**
- Produces: `CombatRenderer::set_loot_filter_mode(settings::LootFilterMode) noexcept`.
- Changes: `draw_room` consumes a prebuilt `GroundLootView` for icon visibility.
- Produces: `HudRenderer::draw_ground_loot(const GroundLootView&) const noexcept`.

- [ ] **Step 1: Write failing integration/font tests**

Require one view build per presented frame, the exact same visible ordinals in icon and label plans, and CJK font coverage for `普通魔法稀有已拾取`. Require settings screen to send draft mode to the renderer and every other screen to send committed mode.

- [ ] **Step 2: Verify RED**

Build platform tests; expected missing renderer APIs and missing glyph coverage.

- [ ] **Step 3: Integrate one view into the render pipeline**

Build once in `CombatRenderer::draw`, draw eligible shapes in the room pass, then draw label panels after actors and before the normal HUD. Use the HUD-owned font and palette; no `TextFormat` or `std::string` in the label path.

- [ ] **Step 4: Integrate preview/commit selection in host**

When pause screen is `settings`, pass `pause_menu.draft.loot_filter_mode` only to renderer. Always map `live_settings.loot_filter_mode` to `DungeonRuntime::fixed_tick`. On cancel/apply failure, existing rollback automatically restores the rendered mode.

- [ ] **Step 5: Run platform and settings integration tests GREEN**

Expected: preview changes visibility, cancel restores it, committed policy never uses draft, and existing HUD/overlay ordering remains intact.

- [ ] **Step 6: Commit**

```powershell
git add src/platform/raylib tests/platform
git commit -m "feat: render filtered ground loot labels"
```


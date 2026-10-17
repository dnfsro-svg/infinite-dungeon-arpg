### Task 10: 正式 raylib 验收、性能与部署闭环

**Files:**
- Modify: `tests/platform/stage12_material_formal_game_validation.cpp`
- Modify: `tests/platform/stage12_material_validator.ps1`
- Modify: `tests/platform/stage17_skill_stones_game_validation.cpp`
- Modify: `tests/platform/stage17_skill_stones_validator.ps1`
- Modify: `tests/platform/package_material_pack_v2_tests.py`
- Modify: `tools/package_material_pack_v2.py`
- Create: `docs/validation/material-runtime-integration-repair.md`
- Create: `docs/validation/evidence/material-runtime-integration/.gitkeep`

**Approved validation-driven scope extensions (2026-07-26):**
- Modify: `src/platform/raylib/material_pack.hpp/.cpp` only for monotonic,
  saturating texture API diagnostic counters and read-only accessors.
- Modify: `tests/platform/CMakeLists.txt` only to make the Stage 10 input/hole
  executable a real build dependency of the Stage 12 formal fixture.
- Modify: `src/platform/raylib/raylib_host.cpp` only to capture the current
  back buffer before the one-and-only `EndDrawing`; artifact swapping is forbidden.
- Modify: `src/platform/raylib/hud_layout.cpp` only to support the formal
  800x450 viewport with a 0.625 minimum HUD scale.
- Modify: `src/platform/raylib/material_pack.cpp` only so
  `ui_warning_modal` samples its dark local `(64,64)` background and alone
  omits the center ornament; every other nine-slice remains unchanged.
- Modify existing cases in `tests/platform/hud_layout_tests.cpp`,
  `tests/platform/material_asset_validation_tests.cpp`, and
  `tests/platform/death_overlay_view_tests.cpp`; do not register a new case or
  change the 473-case platform total.
- Stage 12 must prove all formal HUD panel/text-safe rectangles are inside
  800x450, 1280x720, and 1920x1080.  Its death screenshots must reject a
  large bright-gray panel interior and its pure layout tests must reject line
  overlap.
- The lightning showcase remains the existing active scenario.  Formal C++
  evidence derives shooter/dasher ROIs from production projection and actual
  material frame/scale data; PowerShell may not use obsolete fixed coordinates
  or weaken connected difference-contour proof.

**Interfaces:**
- Consumes every production path from Tasks 1–9.
- Produces machine-readable screenshots/metrics and a deployable package manifest; no gameplay code is introduced here.

- [ ] **Step 1: Extend formal fixtures before changing validators.**

Add deterministic fixture modes for:

```text
cross-ecology: fire room + chaos chaser
doors-closed / doors-open: all four directions visible
draw-slash: ticks 0, 45, 46, 66, 89
storm-swords: ticks 0, 71, 72, 180, 323, 324, 342, 359
environment: water/lightning/chaos at 800x450, 1280x720, 1920x1080
death: warning modal + title plate
stress: 30 monsters + storm swords for 1800 presented frames
```

Run every non-stress mode at `{800,450}`, `{1280,720}` and `{1920,1080}`. Run the stress mode at 1920×1080 after a 300-frame warmup.

Record per frame: requested/resident mask, resident bytes, load/unload totals, material draw status for each monster/door/skill, base-player suppression, procedural-main-visual count, average FPS and sorted frame-time p99 (used as 1% low: `1000 / p99_ms`).

- [ ] **Step 2: Run validators RED against the existing runtime.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage12_material_formal arpg_stage17_skill_stones_game_validation --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^(stage12\.material_formal|stage17\.skill_stones\.real_raylib)$' --output-on-failure -j 1
```

Expected: new evidence assertions fail on old output fields or missing captures; a graphics-context/environment failure is reported separately from a rendering assertion failure.

- [ ] **Step 3: Implement evidence checks with numeric and pixel gates.**

Require:

- cross-ecology screenshot reports both fire background and chaos-chaser atlas resident and drawn;
- four door sprite IDs are distinct, no default-font `?` glyph is emitted, and no opaque red rectangle covers more than 10% of any door bbox;
- captured draw-slash atlas frames include 0 and 35 and the union is 0..35; storm union is 0..23;
- healthy skill frames report base-player suppression and zero procedural main visuals; an injected missing-map fixture reports the fallback inverse;
- each environment prop alpha bbox is inside the viewport and above the HUD reserve in all nine captures;
- death capture records nonzero `ui_warning_modal` nine-slice and `ui_label_plate` draws;
- stress `resident_bytes <= 268435456`, no load/unload occurs after the warmup request stabilizes, average FPS ≥60 and `1000/p99_ms >=45`.

The validator must reject showcase-only captures as proof for formal room integration.

- [ ] **Step 4: Update packaging and deployed-file checks.**

Add the public door pair, both skill material maps, environment build JSON, this plan, `docs/superpowers/specs/2026-07-25-material-runtime-integration-repair-design.md`, and the new source/validator files to `package_material_pack_v2.py`. Replace the historic constants with the reviewed manifest totals `FULL_PACK_BYTES = 329430304`, `RESIDENT_PEAK_BYTES = 226800928`, and `TRANSITION_PEAK_BYTES = 261010720`; the package test requires the formal evidence values and packaged metadata to match all three numbers and verifies SHA-256 hashes against source files.

Add these exact package references:

```python
PLAN_FILES += (
    "docs/superpowers/specs/2026-07-25-material-runtime-integration-repair-design.md",
    "docs/superpowers/plans/2026-07-25-material-runtime-integration-repair.md",
    "docs/validation/material-runtime-integration-repair.md",
)
INTEGRATION_FILES += (
    "src/platform/raylib/material_residency.hpp",
    "src/platform/raylib/material_residency.cpp",
    "src/platform/raylib/material_pack.hpp",
    "src/platform/raylib/material_pack.cpp",
    "src/platform/raylib/material_asset_types.hpp",
    "src/platform/raylib/material_animation.hpp",
    "src/platform/raylib/material_animation.cpp",
    "src/platform/raylib/monster_material_presenter.hpp",
    "src/platform/raylib/monster_material_presenter.cpp",
    "src/platform/raylib/environment_prop_layout.hpp",
    "src/platform/raylib/environment_prop_layout.cpp",
    "src/platform/raylib/active_skill_assets.hpp",
    "src/platform/raylib/active_skill_assets.cpp",
    "src/platform/raylib/host_input.cpp",
    "src/platform/raylib/raylib_input.hpp",
    "src/platform/raylib/raylib_input.cpp",
    "tools/build_environment_props.py",
    "tools/build_active_skill_material_maps.py",
    "tests/platform/environment_prop_asset_pipeline_tests.py",
    "tests/platform/active_skill_material_asset_pipeline_tests.py",
)
```

- [ ] **Step 5: Run the bounded final verification sequence.**

Run each command serially and do not keep the game open between build steps:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_stage12_material_formal arpg_stage17_skill_stones_game_validation arpg_game --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|combat\.units|skills\.units|dungeon\.units)$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-debug -R '^(stage12\.(environment_prop_asset_pipeline|material_formal|material_evidence_validator)|stage17\.(active_skill_material_asset_pipeline|skill_stones\.(real_raylib|evidence_validator))|platform\.(input_latency_source|host_input_source))$' --output-on-failure -j 1

.\scripts\Configure.ps1 -Preset windows-msvc-release
cmake --build --preset windows-msvc-release --target arpg_game arpg_stage12_material_formal arpg_stage17_skill_stones_game_validation --parallel 1
ctest --test-dir out/build/windows-msvc-release -R '^(stage12\.material_formal|stage12\.material_evidence_validator|stage17\.skill_stones\.real_raylib|stage17\.skill_stones\.evidence_validator)$' --output-on-failure -j 1
```

Expected: targeted unit/asset/input suites, Debug/Release targets, all required viewport captures, residency budget and performance gates pass. Do not run unrelated full 109-test CTest unless a targeted failure indicates cross-module risk.

- [ ] **Step 6: Inspect screenshots and system resource evidence.**

Open the 800×450, 1280×720 and 1920×1080 captures at original resolution and record a pass/fail row for cross-ecology monster, four doors, both skills’ phases, three ecology prop sets and death panel. Record compiler/game process lists before and after validation and confirm no `arpg_game`, compiler, CTest or helper remains running.

- [ ] **Step 7: Write validation handoff and commit.**

`docs/validation/material-runtime-integration-repair.md` records commit, exact commands/exit codes, screenshot paths, atlas bytes, load/unload counts, average FPS, 1% low, remaining art gaps (fire full action boards and storm 96 independent character frames), and any graphics-environment limitation without relabeling it as a gameplay failure.

```powershell
git add tests/platform/stage12_material_formal_game_validation.cpp tests/platform/stage12_material_validator.ps1 tests/platform/stage17_skill_stones_game_validation.cpp tests/platform/stage17_skill_stones_validator.ps1 tests/platform/package_material_pack_v2_tests.py tools/package_material_pack_v2.py docs/validation/material-runtime-integration-repair.md docs/validation/evidence/material-runtime-integration
git commit -m "test: validate material runtime integration"
```

---

## Completion gate

The branch is complete only when all ten task commits exist, the worktree is clean, targeted Debug/Release verification is green, the three-resolution screenshots have been inspected at original size, residency stays under 256 MB, the stress capture meets 60 FPS average and 45 FPS 1% low, and no background build/game process remains. If real raylib cannot create a graphics context, preserve logs and stop before claiming visual completion; headless unit success alone is insufficient.

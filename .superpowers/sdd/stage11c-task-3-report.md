# Stage 11-C Task 3 Report — Resolution-Safe HUD Layout

## Status

GREEN. Scope limited to Task 3; no renderer, host integration, Session/Store access, input, RNG, fixed tick, or Task 4+ work was added.

## RED

Tests were registered before production layout files existed. Under the repository MSVC x64 / SDK 10.0.26100.0 environment, the build failed as expected:

```
hud_layout_tests.cpp(3): fatal error C1083: cannot open include file: 'hud_layout.hpp'
```

The first sandboxed build attempt was blocked before compilation because the sandbox denied writes to `out/build`; rerunning the same repository build outside that sandbox reached the expected missing-production-interface RED.

## Changed behavior

- Added a pure, `noexcept` `HudLayout` calculator with `HudRect` overlap/containment helpers.
- Anchors use the 1280x720 logical canvas and exact `clamp(min(width/1280, height/720), 0.8, 1.5)` scale.
- Defines a safe area, three non-overlapping primary panels, two notices above the bottom abyss confirmation zone, a free central combat exclusion rectangle, and an opt-in debug rectangle.
- Invalid zero or negative viewports return an empty layout with zero scale.
- Tests cover 1024x576, 1280x720, 1920x1080, safe containment, combat exclusion, notice placement, debug toggle, helper semantics, and invalid viewports.

## Exact GREEN

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
# ninja: no work to do. (exit 0)

ctest --test-dir out/build/windows-msvc-debug -R "^platform.units$" --output-on-failure
# 1/1 Test #27: platform.units Passed; 100% tests passed

out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
# 206 cases, 0 failures

git diff --check
# exit 0
```

## Commit

`f7f5949 feat: define responsive hud safe layout`

## Self-review

- Source audit found no Session/Store, RNG, input sampling, fixed tick, drawing, dynamic-container, or allocation API in `hud_layout.hpp/.cpp`.
- The new layout source is linked through the raylib platform library; tests are registered in the existing platform suite, whose expected case count is 206.
- `render_layout.hpp` includes the new pure layout interface but leaves existing legacy render-layout behavior unchanged.

## Concerns

None. The layout intentionally defines rectangles only; rendering and host composition are deferred to later tasks.

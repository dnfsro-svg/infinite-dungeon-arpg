### Task 2: Deterministic layout and launcher icon

**Files:**
- Create: `src/launcher/launcher_layout.hpp`
- Create: `src/launcher/launcher_layout.cpp`
- Create: `tools/build_launcher_icon.py`
- Create: `assets/launcher/infinite_dungeon.ico`
- Create: `tests/launcher/launcher_layout_tests.cpp`
- Create: `tests/launcher/launcher_icon_tests.py`
- Modify: `src/launcher/CMakeLists.txt`
- Modify: `tests/launcher/CMakeLists.txt`
- Modify: `tests/launcher/launcher_test_main.cpp`

**Interfaces:**
- Consumes: the fixed 920×560 logical-pixel design and `assets/stage12/ui_material.png`.
- Produces:
  - `struct RectF { float left; float top; float right; float bottom; };`
  - `struct LauncherLayout { RectF title; RectF subtitle; RectF status; RectF start; RectF verify; RectF save; RectF exit; RectF path; };`
  - `LauncherLayout make_launcher_layout(float width, float height) noexcept;`
  - `bool overlaps(RectF, RectF) noexcept;`
  - `struct PixelSize { int width; int height; };`
  - `PixelSize launcher_pixel_size(unsigned int dpi) noexcept;`
  - a multi-resolution `infinite_dungeon.ico` containing 16, 32, 48, 64, 128, and 256 pixel frames.

- [ ] **Step 1: Write failing layout tests**

Add five C++ cases and raise the suite count from 7 to 12:

```cpp
design_size_keeps_every_rect_inside_client_area();
buttons_never_overlap_each_other();
status_and_path_do_not_overlap_actions();
undersized_client_area_returns_empty_layout();
dpi_scaling_preserves_the_fixed_logical_size();
```

Use 920×560 for the first three cases. The undersized case passes 799×449 and requires all rectangles to be zero. The DPI case requires exact pixel sizes `920×560`, `1150×700`, `1380×840`, and `1840×1120` for 96, 120, 144, and 192 DPI. Add only declarations to `launcher_layout.hpp` and an empty translation unit so RED is an unresolved-symbol failure.

- [ ] **Step 2: Run the focused C++ test and verify RED**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_launcher_tests
```

Expected: unresolved `make_launcher_layout`/`overlaps` symbols.

- [ ] **Step 3: Implement the fixed-DIP layout**

Return these exact rectangles for a 920×560 client area:

```cpp
title    = {56, 42, 864, 98};
subtitle = {58, 102, 862, 132};
status   = {58, 174, 862, 226};
start    = {58, 260, 862, 342};
verify   = {58, 370, 308, 426};
save     = {335, 370, 585, 426};
exit     = {612, 370, 862, 426};
path     = {58, 474, 862, 522};
```

For larger client areas, center the 920×560 design surface and offset all rectangles; for either dimension below 800×450, return zero rectangles. `overlaps()` treats touching edges as non-overlap.

`launcher_pixel_size()` computes each dimension as `(logical * dpi + 95) / 96`, rejects DPI 0 with `{0,0}`, and is the only size conversion used by the Win32 window.

- [ ] **Step 4: Run layout tests and verify GREEN**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_launcher_tests
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.units$' --output-on-failure
```

Expected: 12 cases, 0 failures.

- [ ] **Step 5: Write the failing icon asset test**

Create `launcher_icon_tests.py` using Pillow. It opens `assets/launcher/infinite_dungeon.ico`, requires the exact frame set `{(16,16),(32,32),(48,48),(64,64),(128,128),(256,256)}`, requires RGBA alpha extrema `(0,255)`, and rejects a fully black or fully transparent 256×256 frame. Register it as `launcher.icon_asset` with labels `headless;launcher;assets` and working directory `${PROJECT_SOURCE_DIR}`.

Run:

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.icon_asset$' --output-on-failure
```

Expected: FAIL because the icon does not exist.

- [ ] **Step 6: Generate the deterministic icon and verify GREEN**

`build_launcher_icon.py` must crop cell 9 (`x=128, y=128, width=128, height=128`) from `assets/stage12/ui_material.png`, preserve alpha, center it on a transparent square, resize with Lanczos, and write the exact six ICO sizes. Run the generator once and commit both the generator and generated ICO.

Run:

```powershell
python tools/build_launcher_icon.py
ctest --test-dir out/build/windows-msvc-debug -R '^launcher\.icon_asset$' --output-on-failure
```

Expected: icon test passes.

- [ ] **Step 7: Commit**

```powershell
git add src/launcher tests/launcher tools/build_launcher_icon.py assets/launcher/infinite_dungeon.ico
git commit -m "feat: add launcher layout and icon"
```

---


### Task 2: 生成公共四元素门与独立环境物件

**Files:**
- Create: `tools/build_environment_props.py`
- Create: `tests/platform/environment_prop_asset_pipeline_tests.py`
- Create: `assets/stage12/element_doors.png`
- Create: `assets/stage12/element_doors_material.png`
- Create: `assets/stage12/environment-props-build.json`
- Modify: `assets/stage12/water_environment.png`
- Modify: `assets/stage12/water_environment_material.png`
- Modify: `assets/stage12/lightning_environment.png`
- Modify: `assets/stage12/lightning_environment_material.png`
- Modify: `assets/stage12/chaos_environment.png`
- Modify: `assets/stage12/chaos_environment_material.png`
- Modify: `tools/build_water_material_slice.py`
- Modify: `tools/build_lightning_material_slice.py`
- Modify: `tools/build_chaos_material_slice.py`
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/material_residency.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/ecology_material_coverage_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `assets/stage12/README.md`
- Modify: `assets/stage12/environment-source.md`
- Modify: `assets/stage12/water-source.md`
- Modify: `assets/stage12/lightning-source.md`
- Modify: `assets/stage12/chaos-source.md`

**Interfaces:**
- Consumes `MaterialResidencyRequest::require` from Task 1.
- Produces `MaterialAtlasId::element_doors`; existing `environment_door_fire/water/lightning/chaos` sprites move to it without changing sprite IDs.
- Produces deterministic `build_element_doors(root)`, `build_ecology_environment(ecology, root)` and `validate_outputs(root)` functions.

- [ ] **Step 1: Write the RED Python asset tests.**

Register `stage12.environment_prop_asset_pipeline` and assert:

```python
self.assertEqual(Image.open(color).size, Image.open(material).size)
self.assertEqual(Image.open(element_doors).size, (1024, 256))
self.assertEqual(color.getchannel("A").tobytes(),
                 material.getchannel("A").tobytes())
for cell in range(4):
    tile = element_doors.crop((cell * 256, 0, (cell + 1) * 256, 256))
    bbox = tile.getchannel("A").getbbox()
    self.assertIsNotNone(bbox)
    self.assertGreaterEqual(min(bbox[0], bbox[1], 256 - bbox[2], 256 - bbox[3]), 12)
self.assertEqual(len({cell.tobytes() for cell in door_cells}), 4)
```

For each water/lightning/chaos object cell, require a nonempty alpha bbox, at least 8 transparent pixels on every edge, matching material alpha, one dominant connected component, and the manifest source rectangle/anchor from `environment-props-build.json`.

- [ ] **Step 2: Run the RED asset test.**

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
```

Expected: failure because the shared builder, public door atlas and build manifest do not exist.

- [ ] **Step 3: Implement the deterministic exporter.**

`build_element_doors()` takes the authored fire door from `(768, 0, 1024, 256)` of `fire_environment.png` and the water/lightning/chaos doors from `(512, 0, 768, 256)` of their current atlases. For every source, retain the largest connected foreground component, crop its alpha bbox, add 12 transparent pixels, aspect-fit it into a 256×256 cell, align its foot to y=244, and preserve alpha.

`build_ecology_environment()` keeps the 512×512 room field at `(0,0)` and repacks exactly five isolated cells:

```python
LAYOUT = {
    "wall": (512, 0),
    "surface_prop": (512, 256),
    "hole": (0, 512),
    "light": (256, 512),
    "solid_prop": (512, 512),
}
```

Use the authored `art_source/stage12/backgrounds/<ecology>/<ecology>-wall-tile-v1.png` for `wall`; use the existing per-ecology concept crop coordinates already present in the three old builders for the remaining four objects. Convert border-connected background to transparency with a fixed RGB distance threshold of 28, keep the largest subject component plus components at least 2% of its area, feather alpha by 2 pixels, crop, pad by 8 pixels and aspect-fit without stretching. If the output subject still touches the safe margin or the second unrelated component exceeds 2%, raise `RuntimeError` and do not overwrite output files.

Generate material maps with channels `R=roughness`, `G=emissive`, `B=metalness`, `A=color alpha`; write all outputs to temporary siblings, validate them, then atomically replace the committed PNGs and JSON. Make the three existing ecology scripts call this shared builder for their environment stage so they cannot overwrite it with the old collage path.

- [ ] **Step 4: Register manifest and residency metadata.**

Append `MaterialAtlasId::element_doors` immediately before `count`, register it as common `1024×256` with `1'048'576U` bytes per RGBA image, move all four door frames to x offsets `0,256,512,768`, source size `256×256`, foot anchor `{128,244}`, and add the atlas to `base_material_residency_request()`.

Update water/lightning/chaos frame records to the five-cell layout emitted in JSON. Use each object’s emitted foot anchor and alpha bbox; do not reuse `{128,246}` for every object. Extend the budget test to include `element_doors`.

Add exactly two C++ cases to `ecology_material_coverage_tests.cpp`: one verifies four unique common-door frames, the other cross-checks all emitted prop records against manifest bounds. Change the platform expected case count from 447 to 449.

- [ ] **Step 5: Generate, validate and run platform metadata tests.**

```powershell
python tools/build_environment_props.py --root .
python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-debug -R '^stage12\.environment_prop_asset_pipeline$' --output-on-failure -j 1
```

Expected: byte-for-byte repeat generation, four unique door cells, isolated props, manifest bounds and the updated residency budget pass.

- [ ] **Step 6: Commit.**

```powershell
git add tools/build_environment_props.py tools/build_water_material_slice.py tools/build_lightning_material_slice.py tools/build_chaos_material_slice.py tests/platform/environment_prop_asset_pipeline_tests.py tests/platform/CMakeLists.txt tests/platform/ecology_material_coverage_tests.cpp tests/platform/platform_test_main.cpp src/platform/raylib/material_asset_types.hpp src/platform/raylib/material_manifest.hpp src/platform/raylib/material_residency.cpp assets/stage12/element_doors.png assets/stage12/element_doors_material.png assets/stage12/environment-props-build.json assets/stage12/water_environment.png assets/stage12/water_environment_material.png assets/stage12/lightning_environment.png assets/stage12/lightning_environment_material.png assets/stage12/chaos_environment.png assets/stage12/chaos_environment_material.png assets/stage12/README.md assets/stage12/environment-source.md assets/stage12/water-source.md assets/stage12/lightning-source.md assets/stage12/chaos-source.md
git commit -m "feat: export isolated environment materials"
```


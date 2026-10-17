### Task 1: 键色主体提取与元素调色原语

**Files:**
- Modify: `tools/build_environment_props.py:13-177`
- Test: `tests/platform/environment_prop_asset_pipeline_tests.py:32-70`
- Test: `tests/platform/environment_prop_asset_pipeline_tests.py:62-110`

**Interfaces:**
- Consumes: `PIL.Image.Image`，键色源图左上角 RGB，元素名 `fire|water|lightning|chaos`。
- Produces: `_keyed_subject(path: Path) -> Image.Image` 和 `_element_variant(subject: Image.Image, element: str) -> Image.Image`。

- [ ] **Step 1: Write the failing primitive tests**

  在测试文件动态加载 builder，对两张真实键色源断言角点 alpha 为 0、主体非空、bbox 占比非近乎实心矩形，并断言四个变体 alpha 完全一致且 RGB 互不相同：

  ```python
  door = module._keyed_subject(ROOT / "art_source/stage12/door-concept-v1.png")
  hole = module._keyed_subject(ROOT / "art_source/stage12/abyss-hole-concept-v1.png")
  for subject in (door, hole):
      alpha = subject.getchannel("A")
      self.assertEqual(alpha.getpixel((0, 0)), 0)
      self.assertIsNotNone(alpha.getbbox())
  variants = [module._element_variant(door, name)
              for name in ("fire", "water", "lightning", "chaos")]
  self.assertEqual(len({image.getchannel("A").tobytes() for image in variants}), 1)
  self.assertEqual(len({image.convert("RGB").tobytes() for image in variants}), 4)
  ```

- [ ] **Step 2: Run the primitive test to verify RED**

  Run: `python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_keyed_sources_produce_shared_isolated_element_variants -v`

  Expected: `ERROR` because `_keyed_subject` / `_element_variant` do not exist.

- [ ] **Step 3: Implement deterministic keyed extraction and tinting**

  在 builder 中导入 `ImageOps`，加入固定色板：

  ```python
  ELEMENT_PALETTES = {
      "fire": ((52, 14, 8), (255, 120, 36)),
      "water": ((7, 28, 58), (58, 196, 255)),
      "lightning": ((38, 32, 5), (255, 226, 72)),
      "chaos": ((35, 8, 52), (208, 72, 255)),
  }
  ```

  `_keyed_subject` 必须使用左上角键色，RGB 距离平方 `<=12**2` 设 alpha 0，`>=96**2` 保留原 alpha，中间做整数线性过渡；只保留阈值 8 以上的最大 8 连通主体，用 2px GaussianBlur 恢复抗锯齿，最后按 alpha bbox 加 8px 裁剪。`_element_variant` 用 `ImageOps.colorize(subject.convert("L"), dark, light)` 生成 RGB，再原样写回 subject alpha。

- [ ] **Step 4: Run primitive GREEN**

  Run: `python -m unittest tests.platform.environment_prop_asset_pipeline_tests.EnvironmentPropAssetPipelineTests.test_keyed_sources_produce_shared_isolated_element_variants -v`

  Expected: `OK` with `Ran 1 test`.

- [ ] **Step 5: Commit the primitive**

  ```powershell
  git add -- tools/build_environment_props.py tests/platform/environment_prop_asset_pipeline_tests.py
  git commit -m "feat: add keyed environment subject pipeline"
  ```


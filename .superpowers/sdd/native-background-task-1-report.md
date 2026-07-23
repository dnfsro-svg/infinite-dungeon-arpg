# Native fire room background build report

## RED

- Command: `E:\codex1\python-dev\Python313\python.exe -m unittest tests.platform.native_room_background_asset_pipeline_tests -v`
- Result: expected FAIL (1 failure, 1 pass).
- Failure reason: `native room background builder is missing`; the builder, build
  report and three required outputs did not exist before the implementation.

## GREEN

- Command: `E:\codex1\python-dev\Python313\python.exe -m unittest tests.platform.native_room_background_asset_pipeline_tests -v`
- Result: PASS (2 tests, 0 failures, no warnings).
- The test invokes the actual builder, verifies the exact source manifest,
  source/output SHA-256 entries, all recorded placement scales, exact dimensions
  and modes, and byte-for-byte single-pass LANCZOS runtime derivation.

## Outputs and zero-upscale proof

| Output | Pixels | SHA-256 |
| --- | ---: | --- |
| `art_source/stage12/backgrounds/fire/fire-room-background-master.png` | 3840 x 2160 RGBA | `5bab11b0b5ad2745bebded1b810a085ac72a7641143ebb21cc1c54d5a048e57d` |
| `assets/stage12/fire_room_background.png` | 2560 x 1440 RGBA | `e24e7f866386aa00fea5f1df6105e968c9fd8c32ed654c383f6c61d0b29dd30a` |
| `assets/stage12/fire_room_background_material.png` | 2560 x 1440 RGBA | `69ab6e3bcb4ff9a345e8f1f9bf18d511a33d253a996acba81cba94b5343c78c0` |

- Inputs are only the two declared files under
  `art_source/stage12/backgrounds/fire/`; the manifest does not reference old
  `assets/stage12/fire_environment.png`.
- Build report: `assets/stage12/room-background-build.json`.
- Placements: 19 native crop placements; maximum `scale_x = 1.0`, maximum
  `scale_y = 1.0`. No source `resize()` is used.
- Runtime is the master resized exactly once with `Image.Resampling.LANCZOS`.
- ROI check: central luminance standard deviation `13.5502`, top-edge
  luminance standard deviation `16.0006`; central ROI is not a solid color.

## Modified files

- `tools/build_native_room_backgrounds.py`
- `tests/platform/native_room_background_asset_pipeline_tests.py`
- `art_source/stage12/backgrounds/background-sources.json`
- `art_source/stage12/backgrounds/fire/fire-room-background-master.png`
- `assets/stage12/fire_room_background.png`
- `assets/stage12/fire_room_background_material.png`
- `assets/stage12/room-background-build.json`
- `.superpowers/sdd/native-background-task-1-report.md`

## Commit

- Build delivery commit: `3f0752b2b4c3fee72eeff934b9bc041364862bc8`
  (`feat: author native fire room background`).

## Visual inspection and concerns

- Opened the 3840 x 2160 master and 2560 x 1440 runtime output. Both retain
  the dark-steel/ancient-gold/ember structural frame, have a clear unoccupied
  central combat area, and contain no character, monster, UI, text or bright
  central interaction object.
- No obvious crop seam was observed at either inspected size. The intentionally
  restrained ember treatment keeps the room dark; runtime renderer integration
  and four-direction gameplay layers remain outside this offline Task 1 scope.

## 审查修复：RED

- 新增回归命令：`E:\codex1\python-dev\Python313\python.exe -m unittest tests.platform.native_room_background_asset_pipeline_tests -v`
- 第一次 RED：4 项中 3 项失败。精确失败为缺少三张 v2 清单源、
  `test_fire_manifest_has_multiple_independent_native_floor_samples` 仅发现 1
  张地面源、以及
  `test_indexed_clean_tree_contains_all_manifest_inputs_and_rebuilds` 报告
  `fire-floor-tile-v1.png` / `fire-wall-tile-v1.png` 未被 Git 跟踪。
- 视觉返工后的第二次 RED：`test_fire_build_is_traceable_downsampled_and_never_upscaled`
  在已不适用的旧顶边 ROI 上失败（中心亮度标准差 `11.5984`，顶边 `4.0277`）。
  测试改为检验真实房间左缘结构 ROI，而非刻意低对比的远顶部暗墙。

## 审查修复：GREEN 与可复现性

- GREEN 命令：`E:\codex1\python-dev\Python313\python.exe -m unittest tests.platform.native_room_background_asset_pipeline_tests -v`
- 结果：PASS，4/4，6.998 秒，无测试警告。
- 归档回归测试对当前 Git **索引树**执行 `git write-tree`、`git archive`，
  在临时干净树中检查所有 manifest 输入存在并实际运行构建器；因此不能再由
  脏工作树掩盖缺失输入。
- 交叉断言每条 placement 的 source/target 宽高、报告比例和实际矩形比值；
  任意 source crop 到 target 的任一方向放大会失败，不能只伪报
  `scale_x/scale_y`。

## 审查修复：视觉与最终证明

- 已将原有两个 v1 输入和三张原创 ImageGen v2 地面/墙体片纳入版本库；另加入
  原创连续 2.5D 房间布局源 `fire-room-layout-v2.png`。旧图只作为风格参考，未复制
  其构图或石块布局。
- 最终 master/runtime 均已用 `view_image` 人工复查：连续核心画面有远端小而暗的
  石砌后墙、清楚的中景过渡和近端更大的石板；无明显重复图章、无拼贴硬边、无角色、
  UI、文字或中央交互物。外围为低对比暗钢/炭黑过渡，不影响中央战斗区可读性。
- 最终输出 SHA-256：master
  `faed0cfe9699ea065e81e3a75ceb8393e86a5803f1f8d411c3752cbf1912fbee`；runtime
  `2b52350acc3a486d9f5e15a6a2f06e1889353ac2dd9475d5aea7963bce72a512`；material
  `e16457a328d0820543c9fea9c31cbb48a5257115a2e9661807f6f0d23b8a3978`。
- 47 条 placement，最大 `scale_x = 1.0`、最大 `scale_y = 1.0`；核心中央
  luminance stddev 为 `11.5984`，真实房间左缘结构 ROI 为 `16.3743`。
- 修复提交：`3a8ad132759f54857957fcedf2fe5e070deda3b0`
  （`fix: harden native fire background proof`）。

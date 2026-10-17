# Lightning ecology source and build notes

The original lightning-room concept and complete animation boards are stored
under `art_source/stage12`. They were generated specifically for this project
with the built-in OpenAI image generator; no fire or water room art is reused.

Accepted animation boards are:

- shooter: `idle-12-v4`, `move-16-v1`, `special-20-v1`, `hurt-8-v2`,
  `death-16-v5`
- dasher: `idle-12-v1`, `move-16-v3`, `special-20-v3`, `hurt-8-v1`,
  `death-16-v2`

Each accepted board has a matching `-alpha-` derivative. Before normalization,
the deterministic builder detects every complete main connected subject across
the whole board and maps it one-to-one to its intended row/column slot. Frames
are direct source-pixel crops around those subjects: crop rectangles may not
overlap, no target slot may be missing or duplicated, and no character pixels
are moved, copied, composited, scaled or rewritten at this source-validation
stage. This avoids treating an unsafe fixed grid line as a valid crop boundary.

The builder then normalizes each validated direct crop to a 96 x 96 atlas cell.
After scaling and tiny-island cleanup it re-detects the surviving main component
and aligns every frame to foot baseline 92. Gates reject duplicate frames,
adjacent changes below 3 percent, registered pose similarity at or above 97
percent, exact three-frame linear interpolation, a main connected silhouette
below 94 percent, secondary components above 2 percent, or per-state final foot
baseline drift above one pixel. The tiny-island pass only removes detached
chroma-key antialias specks of 47 pixels or fewer; it does not redraw or
interpolate poses.

Run `python tools/build_lightning_material_slice.py` from the repository root
to rebuild and validate the three color atlases and their paired RGBA material
maps. Material channels encode roughness, electric emission, metal response and
coverage. The environment builder retains the dark storm palette while lifting
authored brass and warning-lamp accents for gameplay readability.
# Lightning ecology environment export

Lightning room field、wall 和四个独立道具由 `tools/build_environment_props.py` 生成；其固定
五格布局及每格 alpha 边界/足部锚点以 `environment-props-build.json` 为准。

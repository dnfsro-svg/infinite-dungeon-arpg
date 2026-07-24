# Chaos ecology source and build notes

The original chaos-room concept and complete animation boards live under
`art_source/stage12`. They were generated for this project with the built-in
OpenAI image generator on flat magenta chroma-key backgrounds. No fire, water
or lightning environment or monster pixels are reused.

Accepted boards are:

- chaser: `idle-12-v1`, `move-16-v1`, `special-20-v2`, `hurt-8-v1`,
  `death-16-v1`
- hazard: `idle-12-v2`, `move-16-v2`, `special-20-v1`, `hurt-8-v1`,
  `death-16-v1`

Every accepted board has a matching `-alpha-` derivative. The builder detects
complete connected subjects across the entire board, maps exactly one subject
to each row/column slot, requires non-overlapping direct crop rectangles and
proves that all detected subject pixels were mapped. Source validation does not
move, copy, composite, scale or rewrite character pixels.

After direct crops pass, normalization places each authored pose in a 96 x 96
atlas cell and aligns the surviving main component to foot baseline 92. Gates
reject duplicate full frames, adjacent material change below 3 percent,
registered whole-frame translation, correlated three-frame interpolation,
main connectivity below 94 percent, secondary connectivity above 2 percent and
per-state foot drift above one pixel. Tiny chroma-key antialias islands of at
most 47 pixels may be removed; poses are never synthesized.

Run `python tools/build_chaos_material_slice.py` from the repository root to
rebuild and validate the three color atlases and paired RGBA material maps.
Material channels encode roughness, magenta/acid emissive response, metal
response and coverage.

Rejected source candidates are intentionally retained as untracked evidence:
`chaos-chaser-special-20-v1`, `chaos-hazard-idle-12-v1` and
`chaos-hazard-move-16-v1` (and their alpha derivatives). They are not referenced
by the builder and must not be staged.

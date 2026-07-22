# Lightning ecology source and build notes

The original lightning-room concept and complete animation boards are stored
under `art_source/stage12`. They were generated specifically for this project
with the built-in OpenAI image generator; no fire or water room art is reused.

Accepted animation boards are:

- shooter: `idle-12-v1`, `move-16-v1`, `special-20-v1`, `hurt-8-v2`,
  `death-16-v5`
- dasher: `idle-12-v1`, `move-16-v3`, `special-20-v3`, `hurt-8-v1`,
  `death-16-v2`

Each accepted board has a matching `-alpha-` derivative. The deterministic
builder crops every authored pose, normalizes it to a 96 x 96 cell and a common
foot anchor, and rejects duplicate frames, adjacent changes below 3 percent,
or a main connected silhouette below 94 percent. A final tiny-island pass only
removes detached chroma-key antialias specks of 47 pixels or fewer; it does not
redraw or interpolate poses.

Run `python tools/build_lightning_material_slice.py` from the repository root
to rebuild and validate the three color atlases and their paired RGBA material
maps. Material channels encode roughness, electric emission, metal response and
coverage. The environment builder retains the dark storm palette while lifting
authored brass and warning-lamp accents for gameplay readability.

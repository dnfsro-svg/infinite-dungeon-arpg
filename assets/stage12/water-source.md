# Water ecology source and build notes

The original source concepts are stored in `art_source/stage12`:

- `water-environment-concept-v1.png`
- `water-bulwark-concept-v1.png` and `water-bulwark-alpha-v1.png`
- `water-support-concept-v1.png` and `water-support-alpha-v1.png`

They were generated with the built-in OpenAI image generator for this project,
then the two character sources were chroma-keyed to alpha with the bundled
`remove_chroma_key.py` helper. No fire-room art is used by the water pack.

Run `python tools/build_water_material_slice.py` from the repository root to
rebuild the three color atlases and their deterministic material maps.

## Built-in image generation prompts

- Environment: original square orthographic flooded underground hall; dark
  steel, charcoal wet stone and restrained antique gold; separate wet floor,
  damp wall, sealed door, flooded opening, cyan lantern, mineral coral and
  bronze grate motifs; uncluttered combat center; no characters or text.
- Water bulwark: one full-body ancient dark-steel shield guardian with a large
  transparent layered water-blue tower shield, cyan rim and antique-gold trim,
  facing right on a flat `#ff00ff` chroma-key background; no text or shadow.
- Water support: one full-body agile tide-channeler in charcoal cloth and dark
  steel, crescent staff with water orb and compact cyan support rings, facing
  right on a flat `#ff00ff` chroma-key background; no text or shadow.

# UI material atlas source

`ui_material.png` and `ui_material_material.png` were built from the original
source board `art_source/stage12/ui/dark-steel-ancient-gold-ui-board-v1.png`.

The source board was generated with OpenAI ImageGen on 2026-07-22 for this
project. The prompt requested forty original front-facing dark-steel,
ancient-gold and blue-white-energy ARPG interface ornaments on a removable
flat magenta background. It explicitly prohibited text, logos, watermarks,
existing-game references and copied UI. `tools/build_ui_material_atlas.py`
performs deterministic 8 x 5 slicing, key removal, safe-cell fitting and
roughness/emissive/metalness map generation.

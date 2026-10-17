# Item and material atlas source

`items_ui.png` and `items_ui_material.png` are built by
`tools/build_item_material_atlas.py` from the original source board
`art_source/stage12/items/dark-steel-ancient-gold-item-board-v1.png`.

The source board was created for this project with OpenAI's built-in image
generation tool on 2026-07-22. The prompt requested an original 6 x 5 set of
dark-steel, ancient-gold and blue-white-energy ARPG icons on a flat removable
magenta background, with unique silhouettes and no text, logos, watermarks, or
references to existing game assets. The deterministic builder removes the key,
normalizes each authored icon into a safe 128 x 128 cell, validates its dark
outline, and derives the paired roughness/emissive/metalness material map.

# Stage 12 effects and loot atlas source

- Original source: `art_source/stage12/effects-ui-concept-v1.png` (copied, never moved, 2026-07-19).
- Prompt: original comic action-RPG effects and item-icon concept, four-column chroma-key source; requested fire, water, lightning, chaos, hit sparks, launcher arc, landing dust, affix aura, and rarity markers without text or watermark.
- Cleanup: `tools/build_stage12_effects_atlas.py` removes near-green chroma pixels, crops the four-by-four source cells, contrast-normalizes and centers the first eight effects, then draws clean white/blue/gold/abyss rarity markers for deterministic legibility.
- Output: `effects_ui.png`, 1024x1024 RGBA (4 MiB). Frames are 128x128: row 0 elemental effects; row 1 hit/launcher/dust/aura; row 2 normal/magic/rare/abyss loot.
- Attribution: generated original source supplied in the local Codex image generation output; no third-party game assets are used.

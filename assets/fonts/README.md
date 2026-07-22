# Bundled UI font

- Family: Noto Sans SC
- File: `NotoSansSC[wght].ttf`
- Upstream: https://github.com/google/fonts/tree/main/ofl/notosanssc
- Raw source: https://github.com/google/fonts/raw/main/ofl/notosanssc/NotoSansSC%5Bwght%5D.ttf
- Retrieved: 2026-07-22
- SHA-256: `A3041811A78C361B1DE50F953C805E0244951C21C5BD412F7232EF0D899AF0DA`
- License: SIL Open Font License 1.1; the verbatim license is bundled as `OFL.txt`.

The runtime builds a deterministic 346-glyph atlas from this font for the
current Simplified Chinese UI corpus plus required ASCII. Glyphs are rasterized
at a 64-pixel source size, filtered bilinearly, and drawn at UI sizes up to 26
pixels. One atlas is 4 MiB in the current Raylib capture (8 MiB budget); the
three owned overlay atlases total 12 MiB (24 MiB budget). The source font stays
bundled so the executable has no dependency on Windows system fonts.

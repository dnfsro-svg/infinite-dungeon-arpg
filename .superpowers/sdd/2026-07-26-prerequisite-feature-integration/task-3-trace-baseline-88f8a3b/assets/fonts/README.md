# Bundled UI font

- Family: Noto Sans CJK SC Medium
- Runtime file: `NotoSansCJKsc-Medium.otf`
- Upstream: https://github.com/notofonts/noto-cjk/tree/main/Sans/OTF/SimplifiedChinese
- Raw source: https://raw.githubusercontent.com/notofonts/noto-cjk/main/Sans/OTF/SimplifiedChinese/NotoSansCJKsc-Medium.otf
- Retrieved: 2026-07-22
- SHA-256: `CA094F6B0001FB048CA39DDD797A0CDB0179E1E55C6561E111C49C3E6A61D7B7`
- License: SIL Open Font License 1.1; the verbatim license is bundled as `OFL.txt`.

The static Medium (500) face is intentional. raylib 6.0 does not select the
weight axis of a variable font through `LoadFontEx`; the previous variable
font therefore rendered at its Thin (100) default weight. The runtime builds a
deterministic 346-glyph atlas for the current Simplified Chinese UI corpus plus
required ASCII. Glyphs are rasterized from a 96-pixel source size into a
2048-by-2048 grayscale atlas, then drawn at integer-aligned UI coordinates.
The UI scales its display size from the 1280-by-720 reference layout up to
1.5x at 1920-by-1080, so full-HD text is physically larger instead of being a
bilinear enlargement of a 720p label. The source font stays bundled so the
executable has no dependency on Windows system fonts.

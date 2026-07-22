# Lightning material slice report

## Scope

Milestone 3 adds an original lightning ecology material slice without changing
gameplay, AI, damage, movement or monster statistics:

- independent 768 x 768 lightning environment color/material atlases;
- independent 864 x 864 color/material atlases for `lightning_shooter` and
  `lightning_dasher`;
- room background, wall, door, abyss hole, arc lamp, capacitor bank and
  grounding rod presentation;
- five complete authored animation states per monster: idle 12, move 16,
  special 20, hurt 8 and death 16;
- ecology-scoped loading and release, with only the active environment and two
  monster texture pairs resident.

## RED to GREEN

The initial x64 build failed because the lightning room slice and lightning
atlas identifiers did not exist. New tests then specified independent atlas
ownership, room prop coverage, high-contrast dark/yellow/cyan palette, all 72
frames per monster, presenter state mapping and the 256 MiB texture budget.

The complete source-board gate rejects duplicate frames, adjacent changes under
3 percent, a main connected silhouette below 94 percent and secondary
components above 2 percent. The builder normalizes every frame to one scale and
foot anchor; atlas tests additionally require stable shape and a fully collapsed
final death pose. All ten accepted boards pass. A bounded cleanup removes only
detached chroma-key antialias
islands of at most 47 pixels; no pose is synthesized or interpolated.

## Runtime and formal evidence

The formal host forces a lightning room, captures
`lightning-monsters-1280x720.png`, and reports hard runtime assertions for the
shader pipeline, lightning ecology readiness and all three lightning atlas
pairs. The validator also decodes the screenshot, checks all six PNG dimensions
and requires the new paired byte floor.

Visual inspection confirmed a readable dark storm-forge room, cyan electric
accents, brass warning structures, visible lightning shooter/dasher art and an
unobstructed central combat lane.

## Verification

- `python tools/build_lightning_material_slice.py` -- PASS
- MSVC x64 Ninja build of `arpg_platform_tests` and
  `arpg_stage12_material_formal` -- PASS
- `platform.units` -- PASS, 407 cases and 0 failures
- `stage12.material_formal` -- PASS
- `stage12.material_evidence_validator` -- PASS
- `stage12.material_root_safety` -- PASS
- formal paired texture total: 150,765,568 bytes, below 268,435,456 bytes

# Task 2 Review Round 1

Verdict: Changes requested

## Critical

- `assets/stage12/chaos_environment.png` wall cell `(512,0)` has alpha bbox `[111,150,145,236]` (34x86), a thin fragment rather than the complete authored `backgrounds/chaos/chaos-wall-tile-v1.png`. Root cause: the wall path uses largest-component-only filtering.

## Important

- Water/lightning/chaos doors must be sourced from the pre-generation existing atlas rectangle `(512,0,768,256)`, not re-cropped from concept art.
- All temporary outputs must be validated before any destination replacement; the current per-file replace followed by validation permits partial publication.
- The C++ emitted-prop-record test must genuinely cross-check `environment-props-build.json` against the manifest instead of duplicating hard-coded expectations.

## Verified good

- Manifest/residency integration, the four budget expectation deltas, enum count update, old-builder delegation, authorized scope, dimensions, and color/material alpha pairing.
- Existing platform binary: 449 cases, 0 failures.

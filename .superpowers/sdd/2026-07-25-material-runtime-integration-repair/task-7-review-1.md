# Task 7 Review Round 1

Verdict: Changes requested

## Important

- The fifth center decorative placement intersects the real gameplay-hole sprite bounds in all nine ecology/resolution combinations. Extend the pure layout clamp with an exclusion zone derived from the actual `kHoleCenter` projection, ecology-specific hole definition/anchor, and runtime hole scale. Move only the colliding decor by the minimum safe translation while preserving viewport/HUD and decor-overlap constraints. Strengthen each existing ecology case to assert zero decor/gameplay-hole intersection; do not add a fourth case or modify gameplay collision/hole position.

## Minor ledger

- Tests only assert positive recommended scales rather than exact reviewed per-prop scale values. Defer as non-blocking metadata redundancy.

## Verified good

- Fixed capacity/count, hole ID exclusion, generated metadata, runtime no-JSON rule, flip math, shared draw loop, independent fallbacks, outline behavior, decor overlap, exact case count, and no gameplay changes.

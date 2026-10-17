# Task 6 Review Round 1

Verdict: Changes requested

## Important

- Healthy material mode suppresses the existing 24 storm-sword secondary placements together with the procedural main visual. The brief requires `effect_center` to continue driving those 24 placements; only the fan, central triangle sword, and finisher ellipse are fallback-only. Always build the 24-sword plan for an active storm skill and render it in both material and fallback modes, while keeping `procedural_main_visual_count == 0` and `finisher_visible == false` in material mode.

## Verified good

- Exact 36/24 frame mappings, single shared plan, current-skill readiness, player suppression/failure behavior, allocation probe, icons, case count, and unchanged combat timelines.

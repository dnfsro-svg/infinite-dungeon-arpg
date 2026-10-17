# Task 4 Review Round 1

Verdict: Changes requested

## Important

- Fire explicit 256x256 frames are drawn with the 96x96 ecology scale `0.92`, enlarging fire monsters by about 2.08x. Preserve `0.92` for the six 96x96 atlases and use the existing fire geometry envelope `78/176` for the two fire atlases; add a regression assertion.
- The cross-ecology healthy-path case does not execute/prove the renderer fallback decision. `make_material_residency_request()` scans only `[0, monster_count)`, but snapshots preserve sparse pool slots while `monster_count` is only active count. Scan every fixed monster slot and filter by `active`; strengthen the existing third Task 4 case with a sparse fire-room / high-slot chaos chaser and prove material draw succeeds without silhouette fallback.

## Minor ledger

- Non-centered explicit fire foot anchors are not mirrored on `flip_x`; the visible horizontal drift is deferred per the SDD Minor policy.

## Verified good

- 8x8 phase clips, exact fire cells/durations/anchors, charger cell 7 exclusion, duration loop/clamp, non-fire special ranges, presenter capacity/reset, generic clip removal, exact case count, and task scope.

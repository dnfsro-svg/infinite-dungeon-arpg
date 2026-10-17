# Stage 10 Dense Driver Event-Lease Recovery Plan

## Scope

Repair only the standalone Stage 10 validation fixture driver. Preserve the
round-5 equipment, five-lane sweep, combat geometry, assertions, 12,000-tick
budget, registered timeout, production behavior, and one `session.tick` per
driver-loop iteration.

## Established RED

The preserved round-5 direct run exited natively with code 8 at driver tick
12,000: 55 of 600 monsters were defeated, 545 remained, the player stayed at
768 HP with 92 barrier at `(-10.152, -47.4393)`, and only 5 Storm, 11 Draw,
and 11 light actions were accepted or queued. The initial fixture diff hash
was `8bcc00efda5fc9fd4c6dd030800c3875e8853947`.

## Implementation

- [x] Add fixture-only lease state and failure diagnostics.
- [x] Select active living residents by stable squared-distance then ordinal,
  with a one-attempt stalled-ordinal exclusion.
- [x] Freeze the existing interior stance/facing on acquisition and release
  on absence/death, stale real attack geometry after arrival, or one
  controllable stance movement that makes less than `1e-4F` squared-distance
  progress.
- [x] Require a controllable sweep tick that changes player XY after an
  unreachable stance before reacquisition.
- [x] Compute Storm, Draw, and light geometry independently from readiness;
  preserve Storm then Draw then light request order.
- [ ] Make ranged X arrival one-sided: keep the 0.25 tolerance but require
  `facing_sign * (player.x - stance.x) >= 0`, so the locked target remains
  inside the real Draw range.
- [ ] When the ranged stance is reached, the player is action-ready, no skill
  is accepted, and light geometry is false, transition the same lease to
  `close_for_light`. In that mode use the existing
  `movement_toward(player, locked_target.position)` and keep the same
  Storm/Draw/light priority. Do not invent a close-range constant.
- [ ] Treat a controllable, nonzero close-pursuit movement with no subsequent
  XY change as a movement stall; release, one-attempt exclude, and sweep.
  Cooldown itself still never releases the lease.

## Verification

- [ ] Audit forbidden added lines, unique driver-loop tick, whitespace, and
  exact file scope.
- [ ] Configure in the VS2022 x64 developer environment with
  `CMAKE_BUILD_PARALLEL_LEVEL=1`.
- [ ] Build only `arpg_stage10_validation_fixture` with `-j1`.
- [ ] Run the native executable once via `Start-Process` and require exit 0.
- [ ] Only after direct GREEN, run exactly
  `^stage10\.validation_fixture\.real_abyss_transactions$` with `-j1` and
  require 1/1 passed.
- [ ] Stage only this plan and the Stage 10 fixture, then commit with the
  authorized message.

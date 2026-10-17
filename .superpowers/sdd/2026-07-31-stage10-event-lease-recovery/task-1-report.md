# Stage 10 Event-Lease Recovery — Task 1 Report

## Status

RED after the maximum five direct fix/diagnostic rounds. The event-driven
lease, approved one-sided stance convergence, and committed close-pursuit
contract compile, but the unchanged 12,000-tick budget still expires at
119/600 defeated. CTest was not run and no implementation files were staged
or committed because direct GREEN is mandatory.

## Protected baseline

- Worktree: `E:\game\.worktrees\task9-gate-recovery`
- Branch: `codex/task9-gate-recovery`
- Committed HEAD: `2eec9c26a88f40cf0227fd01b89a3c773d996438`
- Before round 5, the control agent committed the evidence-driven contract
  clarification as HEAD `0d8aa52`; its spec change was not edited or staged by
  this task agent.
- Preserved round-5 fixture diff hash before editing:
  `8bcc00efda5fc9fd4c6dd030800c3875e8853947`
- Initial staging area: empty
- Initial tracked dirty scope: only
  `tests/dungeon/stage10_validation_fixture.cpp`
- Established RED was consumed without rerunning it: native exit 8 at tick
  12,000, 55/600 defeated, 545 remaining, player fixed at
  `(-10.152, -47.4393)`, Storm/Draw/light `5/11/11`.

## Implementation

- Added the brief-specified lease state and failure-only counters.
- Added stable nearest-resident selection by squared XY distance, then lower
  monster ordinal, with one acquisition attempt excluding a stalled ordinal.
- Added immediate absence/death release, stale real-geometry release after
  stance/facing arrival, and controllable movement-progress release at
  `1e-4F` squared-distance progress.
- Added persistent sweep escape. A successful controllable sweep XY change is
  required before reacquisition; a blocked sweep tick advances to the next
  waypoint in the preserved five-lane sweep.
- Factored resident-wide Storm/Draw/light geometry from action readiness and
  retained the required Storm, Draw, then light request order.
- Refined stance X convergence after round-3 evidence: X can stop only on the
  interior-facing side with absolute error at most `0.25F`; Y remains the
  unchanged symmetric `0.25F` rule.
- Kept exactly one `session.tick(movement)` call in `drive_clear()`.

## Configure and build

VS2022 BuildTools Developer PowerShell was loaded with target/host x64 and
`CMAKE_BUILD_PARALLEL_LEVEL=1`.

1. Configure: `cmake --preset windows-msvc-debug`
   - exit 0
   - 7,163 ms
   - `VSCMD_ARG_TGT_ARCH=x64`
2. Initial serial target-only build:
   `cmake --build --preset windows-msvc-debug --target arpg_stage10_validation_fixture -- -j1`
   - exit 0
   - 2,850 ms

## Direct rounds

Every direct run used `Start-Process -Wait -PassThru -NoNewWindow` and
preserved the native exit code. No CTest followed a nonzero direct result.

### Round 1 — initial event lease

- Build exit 0, 2,850 ms.
- Direct native exit 8, 2,191 ms.
- Terminal evidence: 55/600 defeated, 545 remaining, full HP, lock none,
  acquisitions 17, absent releases 16, stale releases 0, movement-stall
  releases 1, sweep escape active for 6,238 ticks at waypoint 0,
  Storm/Draw/light `5/11/11`.
- Diagnosis: the unreachable stance released correctly, but the first sweep
  direction was itself blocked and never changed waypoint.

### Round 2 — blocked sweep waypoint advance

- Change: after a controllable requested sweep tick produces no XY change,
  deterministically advance the existing sweep waypoint while remaining in
  escape.
- Build exit 0, 2,754 ms.
- Direct native exit 8, 2,387 ms.
- Terminal evidence: 65/600 defeated, 535 remaining, lock 119, stance reached,
  acquisitions 66, absent releases 20, movement-stall releases 45, sweep 46
  ticks, Storm/Draw/light `7/11/11`.
- Diagnosis: sweep escape recovered, but a reached lease remained useful under
  one geometry while producing very few actions.

### Round 3 — failure-only geometry/readiness probe

- Diagnostic build 1 exit 0, 2,866 ms; after adding the requested locked HP
  and player-facing fields, final diagnostic build exit 0, 2,813 ms.
- Direct native exit 8, 2,200 ms.
- Terminal evidence:

  ```text
  defeated/remaining: 65/535
  geometry storm/draw/light: 1/0/0
  cooldown draw/storm: 0/601
  active skill/basic attack: none/none
  hurt/hit stop/input size: 0/0/0
  lock 119: (-40.6202,-47.9291), HP 100
  stance: (-35.6202,-47.9016), facing left, reached
  player: (-35.402,-47.9294), facing left
  stance distance squared: 0.0483582
  ```

- Diagnosis: the symmetric X dead zone stopped 0.2182 units on the outward
  side. The locked target's forward distance became 5.2182, outside the real
  Draw range 5.0, while it remained inside Storm geometry and correctly held
  the lease through Storm cooldown.

### Round 4 — approved one-sided stance convergence

- Change: retain absolute `0.25F` arrival tolerance but require
  `facing_sign * (player.x - locked_stance.x) >= 0`; move one legal horizontal
  step across the stance when stopped on the geometry-invalid side.
- Build exit 0, 2,865 ms.
- Direct native exit 8, 2,715 ms.
- Terminal evidence:

  ```text
  defeated/remaining: 124/476
  geometry storm/draw/light: 1/1/0
  cooldown draw/storm: 124/601
  active skill/basic attack: none/none
  hurt/hit stop/input size: 0/0/0
  lock 168: (25.1245,-44.8455), HP 135
  stance: (21.1117,-44.227), facing right, reached
  player: (21.1448,-44.3901), facing right
  acquisitions/releases absent/stale/stall: 87/45/0/41
  sweep ticks/waypoint: 937/1
  Storm/Draw/light: 7/38/18
  ```

- Diagnosis: the frozen stance now has real Draw geometry, but the lease is
  required to remain while Storm or Draw geometry is useful. With light
  geometry false and both skills cooling down, there is no authorized
  navigation/action transition until a cooldown ends. Storm can be accepted
  at most about seven times and Draw at most about fifty times in 12,000 ticks
  under the unchanged 1,800/240 cooldowns. The observed 45 accepted skills
  defeated only 124 monsters, so the remaining 476 cannot be cleared by this
  lease contract inside the fixed budget.

## Audits and scope

- `git diff --check`: exit 0 before execution (only normal LF-to-CRLF notice).
- Forbidden injection search: no `relay_defeated(` or `events_.try_push(`.
- Added-line audit: no HP, remaining-target, defeated-state, room-phase, or
  cooldown writes. The only added `12000` occurrence is the preserved
  failure-diagnostic call; the loop budget is unchanged.
- Unique driver tick audit: exactly one `session.tick(` in `drive_clear()`.
- No production, Stage 9, other worktree, old plan, density, bounds, golden,
  timeout, assertion, or Stage B-G changes.
- Current staging area remains empty. The superseded
  `docs/superpowers/plans/2026-07-31-stage10-dense-fixture-recovery.md` remains
  untracked and unstaged.

## Concern / blocker

The current binding rules have a throughput contradiction on the measured
fixture: a geometrically useful reached lease cannot release due to cooldown,
accepted actions, or active-skill state; its fixed Draw-range stance also has
no light geometry. The fourth run demonstrates that correct Draw geometry
improves progress but cannot approach 600 defeats inside 12,000 ticks.

### Round 5 — committed close-for-light pursuit contract

- Change: after a reached ranged stance completes an action-ready decision
  with no accepted skill and false light geometry, retain the same ordinal and
  enter `close_for_light`; use the existing `movement_toward(player,
  locked_target.position)` while preserving Storm/Draw/light priority. A
  controllable nonzero close-pursuit movement with no subsequent XY change
  uses the existing movement-stall release, one-attempt exclusion, and sweep.
- Build exit 0, 3,055 ms.
- Direct native exit 8, 3,047 ms.
- Terminal evidence:

  ```text
  defeated/remaining: 119/481
  lock: none
  releases absent/stale/stall: 54/0/3
  sweep escape/ticks/waypoint: 1/3599/0
  player: (-76.7703,-60.7451)
  geometry storm/draw/light: 0/0/0
  cooldown draw/storm: 0/0
  active skill/basic attack/hurt/hit stop/input: none/none/0/0/0
  Storm/Draw/light: 5/29/40
  ```

- Diagnosis: close pursuit increased real light attacks, but the terminal
  sweep exposed a separate deterministic arrival deadband. Waypoint 0 is near
  `x=-77.178`; the final player X differs by about 0.408. The sweep changes
  waypoint only when absolute X error is at most `0.25F`, while its required
  existing `movement_toward()` emits zero X input at error at most `0.45F`.
  With Y already reached, movement is zero, no controllable nonzero progress
  check is armed, and waypoint 0 can never advance.

The card has reached its five-failed-round limit. No further fix or direct run
was attempted. The next fresh root-cause/design card should align the fixture
sweep's waypoint-arrival decision with the existing movement helper's `0.45F`
X dead zone, or deterministically advance a waypoint when the helper returns
zero. This must remain fixture-only and preserve the five lanes, production
geometry, budget, and unique tick. No implementation commit exists.

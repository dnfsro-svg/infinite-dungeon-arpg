# Stage 11-A Task 12 Report

## Result

- Commit: `a22d190` (`test: validate stage 11a death flow in raylib`)
- Guard review fix: `d6e1c84` (`test: harden stage 11 death evidence guard`)
- Final guard fix: `ae13913` (`test: reject stage 11 death field mutation`)
- Headless production-API fixture: PASS.
- Real raylib five-path validation: PASS.
- Positive evidence guard, three WILL_FAIL negative fixtures, and mutation
  reason self-test: PASS.
- Existing Stage 10 evidence guards plus platform input-latency/module-boundary guards: PASS.

## RED evidence

The initial registered fixture returned `RED: fixture not implemented` and
`stage11.death_fixture.transactions` failed as expected before production
implementation. The first direct build outside the repository MSVC environment
also hit the project's intentional Windows SDK gate; the valid RED was then
rerun under the fixed VS 2022 x64 / SDK 10.0.26100 environment.

## GREEN evidence

Exact Task 12 command:

```text
cmake --build --preset windows-msvc-debug --target arpg_stage11_death_validation_fixture arpg_stage11_death_formal_game_validation
ctest --preset windows-msvc-debug -R '^stage11\.(death_fixture|death_formal|death_evidence)' --output-on-failure
```

Result after review fixes: 7/7 passed, 0 failed. Related guards: 6/6
passed, 0 failed.

## Independent-review fixes

- Added a mutation self-test that invokes the real guard and asserts the exact
  capture-order rejection marker. The malformed host retains every required
  production-host token, so no earlier missing-token failure can mask capture
  ordering.
- Added an independent TestAccess rejection assertion.
- Added public death-injection guards and generated mutations covering
  `.death =`, `->death =`, death snapshot assignment/emplace, death checkpoint
  assignment/emplace, and `make_death_checkpoint(...)`. Read-only access such
  as `.death.lifecycle` remains allowed.
- Added a separate `WILL_FAIL` public-death-injection CTest fixture.

Review-fix RED: the mutation self-test initially failed because public death
assignment reached the later `Fixture lacks production API` check instead of a
public-injection rejection. GREEN: the mutation self-test passed all generated
public mutations plus the independent TestAccess and pre-present capture
mutations.

Final-review RED and GREEN:

- RED: `.death.lifecycle =` reached `Fixture lacks production API` instead of
  the public-death rejection; direct read-only `.death == expected` was then
  shown to be a false positive under the old assignment pattern.
- GREEN: the guard strips `==`, `!=`, `<=`, and `>=` before matching direct or
  nested death-field mutation. The self-test now rejects `.death.lifecycle =`,
  `.death.target_room.seed =`, `->death.final_damage =`, indexed `+=`, `-=`,
  and nested `|=`, while explicitly accepting direct and nested read-only
  equality/inequality comparisons.

## Formal artifacts

Directory: `out/build/windows-msvc-debug/bin/stage11-death-formal-validation`

- `01-normal-death.png`: `7B177CFA3848300E71D215BEC8EE6889A14D5D2D1DB7C6A992A0414D81DECB6B`
- `02-restarted-death.png`: `BD74011BB985D927F4E8F7F6F6CF2FBCCD2A38F787C7CAEF23DDB463AFE0F000`
- `03-deep-continued.png`: `00B9DBB9E174B6180509DC70BE194B110229D2919B82B997242B6053B6BA2177`
- `04-floor-one-continued.png`: `4895F9327E86A15D5D6C9A3D39BFFE207E7680A042F8D8A0C066AA9BC7D68D53`
- `05-abyss-death.png`: `6D0AB885A819ED350B102A805C5DCAA57B7AADB8ADB9CAC1B9C169342BCC42AB`

The validator inspected all five fresh 1280x720 images, sampled colors,
luminance and non-background content, required death-panel content for the
three death captures, and compared a normalized panel-region SHA-256 for the
normal/restarted recap. Panel hash:
`c9e0b18d154e24f88c59db48622f7aa1945491294f247cb4ea9d3ec0af189302`.

Summary invariants:

```text
normal_death_recap=PASS target=5643726640842626946 hash=13841719523499322986
restart_same_recap=PASS target=5643726640842626946 hash=13841719523499322986
deep_continue=PASS death_depth=2 target_depth=1 final_depth=1
floor_one_continue=PASS target_depth=1 final_depth=1 clamp=1
abyss_death_recap=PASS failed_resolution=1 abandoned=1
```

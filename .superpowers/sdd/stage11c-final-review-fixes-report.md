# Stage 11-C Final Review Fixes Report

Date: 2026-07-18
Worktree: `E:/game/.worktrees/stage11c-complete-hud`
Base before fixes: `82cb8f5`

## A — static projection cache

RED:

- Added host invariant for first-build counters, unchanged normal/recovery/death
  frames, independent objective/navigation/hint invalidation, dynamic player and
  notice refresh, and invalid owner inertness.
- Added persistent projector coverage to the 100k stress path.
- MSVC RED failed because `HudStaticFormattingDiagnostics`,
  `HudViewModelProjector`, and the renderer observer did not exist.

GREEN:

- Commit: `94708ae feat: cache static HUD projection text`.
- Exact objective key: abyss flag, phase, wave index/count, remaining targets,
  abyss danger/rule/pending/unpicked rewards.
- Exact navigation key: depth, floor-room, ecology and all four biases.
- Control hint key: committed `ControlHints.revision`; bounded hint suffix is
  cached while pending XP remains dynamically formatted.
- Counters are saturating `uint64_t` and observable read-only. Cached
  truncation diagnostics remain present on every projected frame.
- `platform.units`: 241/241. Stress: cold-first and steady allocations both 0.

## B — touch-driven door prompts

RED:

- Unit RED showed the abyss door still contained the rebound `F`/`Q` key and
  used the old text.
- The updated formal validator rejected the old evidence with
  `abyss-warning Chinese semantic mismatch`.

GREEN:

- Commit: `26438a2 fix: align HUD door prompts with touch input`.
- Ordinary/abyss door prompts contain no Interact prefix: `进入出口` and
  `离开后再次触碰同一出口以放弃全部剩余奖励`.
- Hole/descent prompts continue using the committed Interact key.
- Production and shared font corpora include the complete new Chinese text.
- Formal and validator passed; `abyss-warning.png` visibly shows both exact
  unprefixed door lines with no missing-glyph boxes.

## C — PNG byte/hash binding

RED:

- Old validator accepted named `tampered_png_pixel` and
  `tampered_aggregate_image_hash` attacks.

GREEN:

- Commit: `d568f66 test: bind Stage11C PNG evidence hashes`.
- C++ runner and C# validator both use FNV-1a offset
  `1469598103934665603`, prime `1099511628211`, byte-order traversal and
  unchecked `uint64` wrap.
- Fixture bytes `00 01 02 7F 80 FF` equal `12476124638988131554` in the C++
  static assertion, aggregate report and validator.
- All seven named validator mutations are rejected for their intended reason.

## Focused verification

- Stage11C: 8/8 in 31.73s — stress, formal, evidence validator/self-test,
  architecture guard/self-test, evidence guard/self-test.
- Relevant regressions: 5/5 in 71.02s — death five paths, platform units,
  settings formal, input latency source and host input source.
- `git diff --check`: clean after the final documentation commit.
- Full Debug 70 and Release were deliberately not run in this fix task; the
  parent runs them after final re-review.

## Fresh formal evidence

Directory:
`out/build/windows-msvc-debug/tests/platform/stage11c hud evidence`

- combat: image `3187940371865125120`, snapshot `13190197248370712797`
- low-health: image `10456196618109886151`, snapshot `3176236133384143545`
- cleared: image `4444140300855791158`, snapshot `11591981634548195452`
- abyss-warning: image `10878588640541785448`, snapshot `12392544927936643533`
- level-up: image `13021216917453511782`, snapshot `11591981634548195452`
- debug: image `7343483459043957270`, snapshot `13190197248370712797`

All six are fresh 1280x720 production Raylib captures, `font_ready=1`, each
`*_valid=1`, aggregate `result=pass`. Manual inspection: HUD panels remain in
safe bounds, Chinese objectives/notices render, CJK notice regions report zero
missing-glyph boxes, and F1 is visible only in `debug.png`.

## Concerns

- No open implementation concern. Objective cache conservatively invalidates
  if hidden abyss fields change on a non-abyss snapshot; this cannot make text
  stale and does not affect the same-snapshot/revision invariant.
- Final parent spec/quality re-review: Spec PASS / Quality APPROVED; no findings.
- Final HEAD includes `c1098a1`, which makes Stage 9 child-process launch and
  empty-diagnostic failures explicit without retrying or weakening mutations.
- Fresh Debug: clean-first 279/279; CTest 70/70 in 434.70 seconds.
- Fresh Release: clean-first 279/279; CTest 70/70 in 293.80 seconds.

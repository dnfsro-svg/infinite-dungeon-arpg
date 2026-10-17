# Task 1 report: POE2 inventory shell contracts

## Scope

- Added the three-page `InventoryPage` contract with the temporary
  `equipment_materials = equipment` compatibility alias.
- Added fixed-capacity shell, status, tooltip, pointer-priority, item-visual,
  visible-source, and transient-status contracts and pure implementations.
- Added the two inventory-local typography minima and the approved 26 catalog
  IDs without changing ordinal 0-44, glyph capacity, atlas count, or budget.
- Extended existing test cases only; no `kCases` element was added.

## TDD evidence

The initial MSVC RED followed the test expansion. Command:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
if ($LASTEXITCODE -eq 0) { throw 'Task1 compile RED missing' }
```

First business diagnostics (MSVC):

```text
ui_text_catalog_tests.cpp(195): error C2838: “inventory_page_skill_stones”: 成员声明中的限定名称非法
ui_text_catalog_tests.cpp(195): error C2065: “inventory_page_skill_stones”: 未声明的标识符
```

This is the expected RED because the expanded real catalog contract referred
to a new business ID before its enum/lookup implementation existed; it was not
an include, parser, test-framework, or environment failure.

## Verification

Executed from `E:\game\.worktrees\loot-suction-animation`:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
```

Result: build exit 0; selector reported `120 cases, 0 failures`; catalog visitor
reported 329; glyph plan reported `639` codepoints with capacity `640`, one
atlas, and a 32 MiB budget. The new 10,000-iteration shell-builder assertion
also observed zero steady-state allocations.

## Scope audit

`git diff --check` passed. The staged commit contains only the eight files
listed in Task 1; this report is intentionally not staged.

## Self-review

- Scope: checked staged names against the eight-file brief list; no renderer,
  dispatcher, Task 2, full-suite, Release, or window work was included.
- Edge cases: invalid viewport/source, unknown item ID, stale non-equipment
  page, scroll visibility, generation rollback, tooltip measurement failure,
  and UTF-8-safe long bindings fail closed or remain bounded.
- YAGNI: implementation is limited to pure Task 1 shell/presentation contracts
  and preserves the temporary compatibility alias; renderer wiring and later
  task behavior remain out of scope.

## Review fix round 1

### RED

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
```

First assertion: `inventory_view_math.inventory filtering` at
`inventory_view_math_tests.cpp:331`: stale `equipped_ids[0] = 999U` produced a
valid visible source although `state.items` was empty. This reproduced the
unknown/expired equipped-ID review finding. The same RED set also added a
224px wrapped-segment layout assertion.

### GREEN

After adding item-presence validation and a fixed-capacity visual-line preview
before tooltip backing allocation, the identical command completed with build
exit 0 and `120 cases, 0 failures`; catalog=329 and glyph=639/640, one 32 MiB
atlas.

### Files and review

Round 1 changed only `inventory_view_math.cpp`,
`inventory_view_math_tests.cpp`, and `shared_ui_font_tests.cpp` of the allowed
eight. The glyph test now exercises all 26 catalog IDs through `ui_text(id)`.
Self-review: the preview uses no heap allocation and rejects over-wide/invalid
measurements before placement; visible equipped sources are now fail-closed
when no current item owns the ID. Remaining broader trace-coverage findings
from the external review are recorded for the next fix round rather than
misrepresented as closed here.

## Review fix round 2

### Coverage and RED

Extended the existing filtering/comparison cases (no new `TestCase`) with the
remaining source/filter/page/scroll/resize, transition corridor/suppression,
transient generation/frame, four receipt/tone, and scroll-gating traces.

The first new production RED was:

```text
[FAIL] inventory_view_math.inventory build comparison:
saved_after_old_receipt.presentation.kind == InventoryTransientStatusKind::saved
(inventory_view_math_tests.cpp:698)
```

It reproduced a valid receipt generation of 11 followed by a normal saved
commit generation of 12 with `receipt_valid=false` and default receipt
generation 0. The former logic treated zero as a generation rollback and
discarded the saved presentation.

### Fix and verification

`InventoryTransientStatusInput::receipt_commit_generation` is now read or
updated only when `receipt_valid` is true. `InventoryTooltipInteractionInput`
also has an explicit `source_invalidated` frame signal, distinguishing a
resolver-invalidated frozen source from an ordinary no-candidate corridor
frame; the former clears hover/frozen/visible without changing caller-owned
selection.

Executed:

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
powershell.exe -NoProfile -File tests/platform/full_chinese_inventory_selector.ps1 `
  -ExecutablePath out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
```

GREEN output: build exit 0; `120 cases, 0 failures`; catalog visitor 329;
glyph plan `codepoint_count=639 capacity=640 atlas_instances=1
atlas_budget_bytes=33554432`.

### Self-review

Only the allowed `inventory_view_math.hpp`, `inventory_view_math.cpp`, and
`inventory_view_math_tests.cpp` changed in this round. The appended boolean is
last in its aggregate input and defaults false, so existing callers retain
normal corridor retention. All new checks use stack/fixed arrays; the existing
10,000-frame allocation probe remains green. No renderer wiring, Task 2 work,
full/formal suite, Release build, or window execution was run.

## Review fix round 3

### RED

Added two minimal traces to the existing transient comparison case: (1) an old
session at commit/receipt 50 followed by a new-session invalid receipt frame at
commit 1, then valid receipt 2; and (2) initial invalid receipt generation 20
followed by the same generation becoming valid. The specified build and
selector command failed first at:

```text
[FAIL] inventory_view_math.inventory build comparison:
new_session_baseline.next.last_receipt_generation == 0U
(inventory_view_math_tests.cpp:712)
```

This proved the rollback path retained old-session receipt generation 50 when
the new frame had no valid receipt.

### GREEN

Initialization and commit-generation rollback now adopt a receipt generation
only when `receipt_valid`; otherwise they explicitly reset the receipt baseline
to zero. The same specified command then completed with build exit 0 and
`120 cases, 0 failures`; catalog visitor 329; glyph plan `639/640`, one atlas,
`33554432` bytes.

### Self-review

Only `inventory_view_math.cpp` and `inventory_view_math_tests.cpp` changed.
The adjustment is restricted to new-session receipt baselines and leaves the
existing strict generation, 180-frame, recovered, and normal-save paths in
place. No heap state or renderer behavior was introduced.

## Controller final verification

After the final scoped re-review reported every finding addressed and no new
Critical/Important issue, the controller independently ran the specified
Debug configure/build and inventory selector. The build exited 0 (`ninja: no
work to do`); the selector reported `120 cases, 0 failures`, catalog visitor
329, and glyph plan `639/640`, one atlas, 33554432 bytes. Commit-range,
working-tree, and cached diff checks passed; the range contains exactly the
eight Task 1 files and the working tree is clean.

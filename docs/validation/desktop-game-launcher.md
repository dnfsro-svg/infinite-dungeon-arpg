# Desktop game launcher validation

Date: 2026-07-26

Status: **PASS**. Build, automated launcher gates, deployment, shortcut
readback, save preservation, dependency inspection, and native visual checks
at 100%, 125%, 150%, and 200% passed without relaxing any gate.

## Recovery semantics

The existing platform and dungeon tests collectively cover pending death with
automatic continuation disabled, an exact committed continuation when enabled,
pre-publish `not_committed`, post-publish `indeterminate`, loading an already
clear checkpoint, restart/idempotence behavior, and malformed committed death
receipts. The focused binary reported `477 cases, 0 failures`; therefore no
production defect was found and no recovery-fix commit was created.

## Build and launcher gates

The Debug platform target built serially and `platform.units` passed. The
Release launcher and launcher tests built serially after loading the MSVC
developer environment in the same PowerShell process. All five launcher gates
passed: units, icon asset, manifest, window smoke, and deploy self-test. The
final narrow rerun passed 5/5 launcher tests, 1/1 platform test, and the direct
platform executable reported `477 cases, 0 failures`. No build, launcher, or
game process remained afterward.

## Deployment

The owned deployment root was
`E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859`. The real deployment root
is `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app`, and the desktop
shortcut is `C:\Users\dnfsr\Desktop\无限地下城.lnk`. The shortcut target,
working directory, and icon all point to the real deployment root and contain
no `.worktrees` component.

The production save directory was measured before and after the owned and real
deployments. Both snapshots contained two files totaling 1248 bytes. The file
lengths and UTC write-time ticks were identical; see
`evidence/desktop-launcher/launcher-validation.txt`.

## Visual evidence

Computer Use selected the exact launcher window for the 100%, 125%, and 150%
visual inspections. After the approved restart, Computer Use was unavailable,
so the 200% checks used an exact owned launcher PID and HWND plus a native
Win32 client capture. Every image shows pure-color green/red status text,
non-overlapping Chinese labels, a visible Start button, and a non-white client:

- `evidence/desktop-launcher/ready-100.png`
- `evidence/desktop-launcher/missing-assets-100.png`
- `evidence/desktop-launcher/ready-125.png`
- `evidence/desktop-launcher/missing-assets-125.png`
- `evidence/desktop-launcher/ready-150.png`
- `evidence/desktop-launcher/missing-assets-150.png`
- `evidence/desktop-launcher/ready-200.png`
- `evidence/desktop-launcher/missing-assets-200.png`

The system started at 125%, was changed through the Windows Display Settings UI
for the 100% and 150% checks, and was read back at the original 125% afterward.

The physical 1920x1080 panel could not fit the exact 200% client, so the approved
signed VDD 25.7.23 path was used. Following the restart, the single VDD was set
to 3840x2160 at 60 Hz and Windows Display Settings identified its scale as 200%
(recommended). The physical target was unavailable to the active topology, so
the 200% validation used the VDD as the sole validation display; no duplicate
VDD or 800x600 state was accepted.

For both 200% states, the launcher was started from
`E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859\app`, its exact HWND was
verified to belong to the started PID, and capture points were verified to be
owned by that PID. `GetDpiForWindow` returned 192 and `GetClientRect` returned
1840x1120 for both runs. `ready-200.png` contains 274 sampled pure-green pixels
and SHA-256 `C71843AC75271C0208DD9FDF569748921DFB281CA1D70308DE907515A0157141`.
`missing-assets-200.png` contains 428 sampled pure-red pixels and SHA-256
`741CB05CEC8E9618F54914C375FC4822D90D54639FDC4FCD10463E6009FCD030`.
The owned `assets\skills` directory was restored immediately after the missing
state capture. The old constrained 200% blocker image is intentionally excluded
from committed evidence.

## Dependencies

`dumpbin /dependents` succeeded for both deployed executables. The launcher uses
Direct2D/DirectWrite and Win32 system DLLs. The game uses Win32 system DLLs plus
the MSVC/UCRT runtime DLLs. Exact output is recorded in the text evidence.

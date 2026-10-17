# Task 2 report: launcher deployment and pending-death recovery

## Status

**DONE.** Launcher deployment, save preservation, 100%/125%/150%/200% native
visual evidence, signed-VDD rollback, physical-display restoration, and all
required launcher/platform gates passed without relaxing any gate, timeout,
pixel threshold, or expected sequence. Independent review found one test-only
coverage gap; commit `39bc3b1` now directly proves a fresh second production
initialization with `continue=true` against the already-clear save is
idempotent. No production source changed.

Validation evidence is committed at `c7a281a`; the review fix is committed at
`39bc3b1`. No push was performed.

## Implementation and deployment

- Production sources were not changed. One existing platform test case was
  extended by the independent-review follow-up.
- Existing pending-death recovery behavior passed all 477 focused platform
  cases, so no empty recovery-fix commit was manufactured.
- Release launcher/game binaries were deployed first to the owned external
  fixture `E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859`, then to
  `%LOCALAPPDATA%\InfiniteDungeon\app`.
- The real desktop shortcut was created at
  `C:\Users\dnfsr\Desktop\无限地下城.lnk`.
- The owned missing-assets state was created reversibly by renaming only the
  owned fixture's `assets\skills` directory to `assets\skills.missing-task2`.
- The first owned-deployment attempt used the worktree `out` directory and was
  intentionally rejected by the `.worktrees` path gate before real deployment;
  it was rerun from the external owned fixture without relaxing the gate.

## Pending-death TDD audit

Tests cover disabled continuation, enabled exact success, `not_committed`,
`indeterminate`, already-clear/restart behavior, and corrupt receipt faults
across the platform runtime and dungeon transaction suites. The existing
`production_startup_auto_continues_pending_death` case now uses a fresh second
`DungeonRuntime` against the same cleared save and directly proves successful
initialization, absent death state, disk lifecycle `none`, and unchanged commit
generation. Focused execution is GREEN (`477 cases, 0 failures`). No production
recovery defect was found.

## Save directory evidence

Production path:
`C:\Users\dnfsr\AppData\Local\InfiniteDungeon\save`.

Before and after owned plus real deployments:

- count: 2 -> 2
- total bytes: 1248 -> 1248
- `run_a.sav`: 460 bytes,
  `LastWriteTimeUtcTicks=639201529634735548` unchanged
- `run_b.sav`: 788 bytes,
  `LastWriteTimeUtcTicks=639202504336470684` unchanged

The serialized before/after metadata objects were exactly equal.

## Shortcut readback

Owned fixture:

- TargetPath:
  `E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859\app\无限地下城启动器.exe`
- WorkingDirectory:
  `E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859\app`
- IconLocation: target path plus `,0`

Real desktop:

- TargetPath:
  `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app\无限地下城启动器.exe`
- WorkingDirectory:
  `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app`
- IconLocation: target path plus `,0`

All real fields matched exactly and contained no `.worktrees` component.

## Commands and results

1. `cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1`
   - exit 0; no work required.
2. `ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure`
   - 1/1 passed, 0 failed.
3. `out/build/windows-msvc-debug/bin/arpg_platform_tests.exe`
   - 477 cases, 0 failures.
4. `. ./scripts/Configure.ps1 -Preset windows-msvc-release -Fresh`
   - exit 0.
5. Same-process MSVC developer environment plus
   `cmake --build --preset windows-msvc-release --target arpg_launcher arpg_launcher_tests -- -j1`
   - 180/180 steps, exit 0.
6. `ctest --test-dir out/build/windows-msvc-release -R '^launcher\.(units|manifest|window_smoke|icon_asset|deploy_self_test)$' --output-on-failure`
   - 5/5 passed, 0 failed.
7. `./scripts/DeployLauncher.ps1 -GameBuildDirectory ./out/build/windows-msvc-release/bin`
   - real deployment exit 0 after owned deployment succeeded.
8. `dumpbin /dependents` on both deployed executables
   - exit 0 for both; exact DLL lists are in `launcher-validation.txt`.

The first standalone Release build invocation lacked the MSVC `INCLUDE/LIB`
environment because each shell call is a new process and failed on `cstdint`.
The root cause was verified; loading `Configure.ps1` and building in the same
PowerShell process succeeded. A separate one-second execution timeout also
terminated an early retry; process inspection proved no build remained before
the successful retry.

During the final verification rerun, the first window-smoke attempt correctly
rejected an Explorer `Windows 工具` window that occluded its pixel-capture
points. Computer Use uniquely selected and closed that window; the unchanged
five-gate command was then rerun and passed 5/5. No timeout or pixel gate was
changed.

## Visual evidence and 200% blocker

Passing evidence:

- `docs/validation/evidence/desktop-launcher/ready-100.png`
- `docs/validation/evidence/desktop-launcher/missing-assets-100.png`
- `docs/validation/evidence/desktop-launcher/ready-125.png`
- `docs/validation/evidence/desktop-launcher/missing-assets-125.png`
- `docs/validation/evidence/desktop-launcher/ready-150.png`
- `docs/validation/evidence/desktop-launcher/missing-assets-150.png`

At each passing DPI, Computer Use uniquely selected the intended process-backed
window and confirmed pure-color status text, non-overlapping Chinese labels,
the visible Start button, and the dark non-white client. Windows Display
Settings was used for system 100%/125%/150%. The original 125% recommended
setting was restored and read back before cleanup.

The only attached display has physical resolution 1920x1080. Its standard
scaling list contains 100%, 125%, 150%, and 175%, not 200%; custom 200% would
require sign-out and was not performed.

After system scaling was restored, a controlled 192-DPI `WM_DPICHANGED` was
sent to the exact ready launcher to exercise its production handler. On strict
retry, the exact production style `0x14CA0000` and ex-style `0x00040100` were
passed to `AdjustWindowRectExForDpi`, yielding an untrimmed 1866x1191 outer
size. Suggested RECT `20,20,1886,1211` deliberately extended beyond the
physical display. Windows nevertheless limited the actual outer rectangle to
`20,20,1886,1122`; Per-Monitor-V2 `GetClientRect` returned 1848x1055 rather
than the required 1840x1120.

Computer Use captured the full constrained surface at
`docs/validation/evidence/desktop-launcher/blocked-ready-200-controlled.png`.
It is blocker evidence, not a passing 200% screenshot. Per the prerequisite
sequence, missing-assets 200% was not attempted after ready 200% failed.

## Dumpbin summary

- Launcher: `d2d1.dll`, `DWrite.dll`, `SHELL32.dll`, `ole32.dll`,
  `USER32.dll`, `GDI32.dll`, `KERNEL32.dll`.
- Game: `WINMM.dll`, `USER32.dll`, `KERNEL32.dll`, `GDI32.dll`,
  `SHELL32.dll`, MSVC runtime DLLs, and UCRT API-set DLLs.

## Changed files

- Created `docs/validation/desktop-game-launcher.md`.
- Created `docs/validation/evidence/desktop-launcher/launcher-validation.txt`.
- Created seven PNG evidence files in that evidence directory (six passing
  100/125/150 images and one 200 blocker image).
- Created this report.
- No production source or test source changed.

## Self-review

- Correct worktree, branch, and base were maintained.
- All builds were serialized with `-j1` / `CMAKE_BUILD_PARALLEL_LEVEL=1`.
- No second build was started while one was alive.
- Production saves were measured before and after deployment and were unchanged.
- Real deployed paths contain no `.worktrees` component.
- System scaling was restored to the original 125% recommended setting.
- All launcher/game/build processes were absent on final process readback.
- No files were deleted from the repository, no git reset was used, and no
  push or commit was performed.

## Problem requiring resolution

The 200% visual gate needs a display/virtual display whose usable pixel height
allows the exact 1840x1120 client, or another approved environment that can
provide an exact 192-DPI top-level surface. On this single 1920x1080 display,
the OS enforces an outer-height ceiling even when the suggested rectangle is
not trimmed to the work area. Until exact 200% ready and missing-assets images
are captured, Task 2 remains BLOCKED.

## Approved virtual-display recovery attempt (2026-07-26)

The approved VDD Control 25.7.23 recovery was started from the audited package
at `E:\codex1\Temp\VDD-25.7.23-audit-20260726\extracted`. Before installation:

- `Confirm-SecureBootUEFI` returned `True`.
- `bcdedit /enum {current}` contained no enabled `testsigning` or
  `nointegritychecks` setting (`flightsigning` was `No`).
- the Display DriverStore contained only Intel `oem126.inf` and NVIDIA
  `oem94.inf`; no MttVDD device or virtual monitor was present.
- `VDD Control.exe`, `SignedDrivers\x86\VDD\mttvdd.cat`, and
  `SignedDrivers\x86\VDD\MttVDD.dll` had valid SignPath Foundation
  signatures. No Virtual Audio Driver action was taken.
- the physical display remained the sole screen at 1920x1080 and 125% scale.

VDD Control was used only through its **Install Driver** action. Its console
selected `SignedDrivers\x86\VDD`, copied the display configuration to
`C:\VirtualDisplayDriver\vdd_settings.xml`, and then Windows presented the
device-software publisher confirmation for `MikeTheTech` / SignPath
Foundation. Windows Security UI could not be operated by Computer Use, so the
user clicked **Install** personally. There was no request to disable Secure
Boot, enable test signing, install a test certificate, install Virtual Audio
Driver, or reboot.

Installation produced two independent `Root\MttVDD` root devices
(`ROOT\DISPLAY\0000` and `ROOT\DISPLAY\0001`), each with its own MTT1337
monitor child, despite the XML monitor count being one. Their first-install
times were approximately 17 seconds apart, proving that the install path had
been triggered twice rather than representing a normal parent/child pair.
VDD Control's own **Uninstall Driver** action removed both root devices. Its
uninstaller then raised an unhandled `Value cannot be null (image)` dialog;
DriverStore package `oem148.inf` remained. At that point the Computer Use
capture backend reported `WinDisc` and the physical BOE0B02 display path was no
longer active.

After the approved deterministic fallback, the bundled Microsoft-signed
`devcon.exe` plus the already verified SignPath `MttVDD.inf` was invoked once:

`devcon.exe install MttVDD.inf Root\MttVDD`

It returned exit 0 (`Drivers installed successfully`) and produced exactly one
root device, `ROOT\DISPLAY\0000`, with exactly one MTT1337 monitor. No virtual
audio device or driver was installed. The only active display path was then
`DISPLAY15 -> VDD by MTT`, 800x600 at 30 Hz, primary.

An ignored one-time native helper used `QueryDisplayConfig(QDC_ALL_PATHS)` to
record the topology. The internal BOE target (UID 8388688) was not available
and had no source/target mode index. One direct
`SetDisplayConfig(SDC_TOPOLOGY_EXTEND | SDC_APPLY | SDC_ALLOW_CHANGES |
SDC_SAVE_TO_DATABASE)` attempt returned Win32 error 87
(`ERROR_INVALID_PARAMETER`) and did not change the paths.

Per controller approval, exactly one recovery action was then issued against
the exact Intel UHD instance:

`pnputil /restart-device PCI\VEN_8086&DEV_A7A8&SUBSYS_171E1025&REV_04\3&11583659&0&10`

PnPUtil returned exit 0 and `Device restarted successfully`, but fifteen
two-second polls never observed BOE0B02 or an active/available `DISPLAY7`
path. Final PnP still showed Intel UHD, NVIDIA, the single VDD root, and the
single VDD monitor as OK, but no BOE monitor. VDD remains installed because it
is the only active display path; uninstalling it would leave no active desktop.

No ready/missing 200% evidence, post-uninstall cleanup, final gate rerun, or
Task 2 commit has been claimed from this blocked state.

### Reboot handoff

The user approved a system restart as the next recovery boundary. Before
handoff, VDD Control was closed without uninstalling the driver. Readback found
no build, game, launcher, test, CMake, Ninja, MSBuild, compiler, or linker
process. Exactly one `ROOT\DISPLAY\0000` VDD root and one MTT1337 monitor remain
OK, backed by signed DriverStore package `oem148.inf`. No shutdown or reboot was
issued by this task. State: `READY_FOR_REBOOT`.

## Post-reboot recovery result (2026-07-26)

The approved restart occurred at `2026-07-26T18:11:05.5000000Z`. The resume
shell and Explorer were both in active console session 1. PnP readback showed
the Integrated Monitor (`DISPLAY\BOE0B02\4&26F2EF5B&0&UID8388688`) and the
single VDD/MTT pair present with `CM_PROB_NONE`, and DriverStore still contained
the SignPath-signed `oem148.inf` package.

However, live `QueryDisplayConfig(QDC_ALL_PATHS)` and WinForms readback did not
match the expected handoff topology. They showed exactly one active path:
`DISPLAY6 -> VDD by MTT`, 800x600 at position 0,0, primary. The BOE target was
present in PnP but remained `targetAvailable=0`, inactive, and without source or
target mode indices. The state remained unchanged during ten two-second polls.

Controller approval allowed one non-destructive recovery request only:

`DisplaySwitch.exe /internal`

It exited 0 at `2026-07-26T18:16:45Z`. Fifteen subsequent two-second polls,
through `2026-07-26T18:17:20Z`, continued to show active console session 1,
exactly one WinForms screen (`DISPLAY6`, primary, 800x600), exactly one active
DisplayConfig path (`DISPLAY6 -> VDD by MTT`, 800x600), and the BOE PnP node as
present/OK while its display target remained unavailable.

Per the binding stop condition, no second topology action was attempted. No DPI
change, 200% capture, driver uninstall, test rerun, evidence staging, commit,
push, or Task 3 work was performed after this failed recovery. Task 2 remains
`BLOCKED`; `HEAD` remains `373f85961d5eb0c612d4b12157c16baffe3b8f45`.

## VDD-only 200% validation and commit (2026-07-26)

The controller clarified that a simultaneously active physical-plus-VDD
topology was not an original Task 2 gate. The current single VDD validation
display was therefore accepted as the approved environment, provided the
original 3840x2160@60, 192-DPI, 1840x1120-client, native-image, and visual
requirements remained unchanged.

`EnumDisplaySettings` enumerated a native 3840x2160@60 mode for `DISPLAY6`.
`ChangeDisplaySettingsEx` applied that exact mode and returned 0. API readback
then reported `DISPLAY6 -> VDD by MTT` active/available at 3840x2160, position
0,0, and `EnumDisplaySettings(ENUM_CURRENT_SETTINGS)` reported 60 Hz. Windows
Display Settings identified the VDD scale as `200% (recommended)`.

Computer Use was unavailable after the restart. The approved fallback used an
ignored PowerShell helper with exact owned launcher PID/HWND checks,
Per-Monitor-V2 Win32 geometry APIs, client ownership sampling, and native
`CopyFromScreen` client capture. It did not resize, scale, or crop the saved
PNG. The launcher was moved onto the VDD before geometry polling so its normal
`WM_DPICHANGED` path ran.

- ready: PID 20904, HWND 0xA0396, `GetDpiForWindow=192`,
  `GetClientRect=1840x1120`, client origin 53,98, 274 sampled pure-green
  pixels; SHA-256
  `C71843AC75271C0208DD9FDF569748921DFB281CA1D70308DE907515A0157141`.
- missing assets: PID 11156, HWND 0x705C4, `GetDpiForWindow=192`,
  `GetClientRect=1840x1120`, client origin 53,98, 428 sampled pure-red pixels;
  SHA-256
  `741CB05CEC8E9618F54914C375FC4822D90D54639FDC4FCD10463E6009FCD030`.

Original-resolution inspection passed both images: pure green/red status text,
non-overlapping Chinese labels, visible Start button (correctly disabled for
missing assets), and dark non-white client. The owned `assets\skills` directory
was restored immediately after the missing-assets capture. Both launcher PIDs
were closed and no launcher process remained.

The old `blocked-ready-200-controlled.png` and repository-root
`task2_display_tool.obj` were moved, after absolute-path containment checks,
into this plan's ignored `blocked-artifacts` directory. They were not deleted
or staged.

Final proportional gates passed unchanged:

- Release launcher gate: 5/5 passed, 0 failed, 9.78 seconds.
- Debug platform CTest: 1/1 passed, 0 failed, 11.15 seconds.
- Direct platform binary: 477 cases, 0 failures.
- Production save metadata remained exactly equal: two files, 1248 bytes;
  `run_a.sav` 460 bytes/tick 639201529634735548 and `run_b.sav` 788 bytes/tick
  639202504336470684.
- Relevant build, launcher, game, and VDD Control process count: 0.

Exactly two validation documents and eight passing PNGs were staged.
`git diff --check` and `git diff --cached --check` passed. Commit
`c7a281af8bf56c36b9f52cd92c6255fae3802383` was created with message
`test: validate desktop launcher integration`; no push was performed. The
tracked worktree was clean after commit.

### Pre-teardown checkpoint

The remaining rollback target is exactly one device `ROOT\DISPLAY\0000`, its
single `DISPLAY\MTT1337` monitor, and signed DriverStore package `oem148.inf`.
The intended deterministic commands are:

`pnputil /remove-device ROOT\DISPLAY\0000`

`pnputil /delete-driver oem148.inf /uninstall /force`

The BOE PnP node is present/OK but its target is currently unavailable to this
agent's active display topology. Removing the VDD may therefore leave no active
desktop until the approved second restart. No teardown success or physical
recovery is claimed until command output and post-restart API readback exist.

### VDD teardown result and second-restart handoff

The deterministic teardown was executed after the validation commit:

1. `pnputil /remove-device ROOT\DISPLAY\0000` returned exit 0 and
   `Device removed successfully`.
2. `pnputil /delete-driver oem148.inf /uninstall /force` returned exit 0 and
   reported both `Driver package uninstalled` and
   `Driver package deleted successfully`.

Immediate readback at `2026-07-26T18:29:31Z` showed:

- `ROOT\DISPLAY\0000` was no longer enumerated.
- present MTT1337 monitor count was 0; three historical MTT monitor entries
  remained non-present `CM_PROB_PHANTOM` records only.
- DriverStore contained neither `oem148.inf` nor `mttvdd.inf`.
- BOE0B02 remained present/OK with `CM_PROB_NONE`.
- the current session had no active DisplayConfig path;
  `QueryDisplayConfig(QDC_ALL_PATHS)` returned error 87 with zero paths/modes,
  and WinForms exposed the disconnected fallback `WinDisc` surface.
- relevant build, launcher, game, and VDD Control process count was 0.
- tracked worktree status was clean at
  `c7a281af8bf56c36b9f52cd92c6255fae3802383`.

This is the expected approved second-restart boundary. Physical recovery is not
claimed yet. After the controller restarts the system, final Task 2 readback
must show BOE0B02 active primary at 1920x1080/125%, zero present VDD/MTT,
`oem148.inf` absent, relevant processes zero, and the same clean commit.

## Final post-teardown restart verification

The approved second restart completed at
`2026-07-26T18:31:38.5000000Z`. Final API and PnP readback established:

- exactly one active/available DisplayConfig path: `DISPLAY1`, source ID 0,
  target UID 8388688, source mode 1920x1080 at position 0,0;
- `EnumDisplaySettings(ENUM_CURRENT_SETTINGS)` returned 1920x1080 at 144 Hz;
- the unique present monitor with UID 8388688 was Integrated Monitor BOE0B02,
  status OK, `CM_PROB_NONE`;
- present VDD/MTT device count was 0, and DriverStore contained neither
  `oem148.inf` nor `mttvdd.inf`;
- Windows Display Settings exposed the unique physical display's scaling list
  as 100%, 125% (recommended), 150%, and 175%. UI Automation explicitly
  selected `125% (recommended)` and read back `IsSelected=True`;
- a fresh owned launcher window (PID 16664, HWND 0x2E0254) returned
  `GetDpiForWindow=120` and client 1150x700, exactly 920x560 DIPs at 125%;
- an explicitly DPI-unaware thread returned logical desktop metrics
  1536x864, matching 1920x1080 at 125%;
- the launcher exited, relevant process count remained 0, and production save
  metadata remained two files / 1248 bytes with both original ticks;
- `HEAD` remained `c7a281af8bf56c36b9f52cd92c6255fae3802383`
  and tracked status was clean.

Task 2 is complete. No push was performed and Task 3 was not started.

## Independent review fix evidence

Independent review required direct same-case coverage for a fresh second
production initialization after successful death recovery. The existing
`production_startup_auto_continues_pending_death` case now creates a fresh
`DungeonRuntime` with the unchanged `continue_pending_death_on_initialize =
true` configuration against the same already-clear save. It proves the second
initialize succeeds, both runtime and disk death state remain clear, disk death
lifecycle is `none`, and commit generation remains unchanged.

Verification for commit `39bc3b1`:

- serial Debug target build: exit 0;
- `platform.units`: 1/1 passed, 0 failed;
- direct `arpg_platform_tests.exe`: 477 cases, 0 failures;
- `git diff --check` and staged diff check: exit 0;
- committed path: only `tests/platform/dungeon_runtime_tests.cpp`;
- production files, case registry, launcher evidence, and Task 3 files:
  unchanged;
- push: not performed.

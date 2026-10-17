# Review package: 373f85961d5eb0c612d4b12157c16baffe3b8f45..c7a281af8bf56c36b9f52cd92c6255fae3802383

## Commits
c7a281a test: validate desktop launcher integration

## Files changed
 docs/validation/desktop-game-launcher.md           |  85 +++++++++++++++++
 .../desktop-launcher/launcher-validation.txt       | 102 +++++++++++++++++++++
 .../desktop-launcher/missing-assets-100.png        | Bin 0 -> 51323 bytes
 .../desktop-launcher/missing-assets-125.png        | Bin 0 -> 49620 bytes
 .../desktop-launcher/missing-assets-150.png        | Bin 0 -> 48243 bytes
 .../desktop-launcher/missing-assets-200.png        | Bin 0 -> 284735 bytes
 .../evidence/desktop-launcher/ready-100.png        | Bin 0 -> 51872 bytes
 .../evidence/desktop-launcher/ready-125.png        | Bin 0 -> 48553 bytes
 .../evidence/desktop-launcher/ready-150.png        | Bin 0 -> 47761 bytes
 .../evidence/desktop-launcher/ready-200.png        | Bin 0 -> 279409 bytes
 10 files changed, 187 insertions(+)

## Diff
diff --git a/docs/validation/desktop-game-launcher.md b/docs/validation/desktop-game-launcher.md
new file mode 100644
index 0000000..0709f71
--- /dev/null
+++ b/docs/validation/desktop-game-launcher.md
@@ -0,0 +1,85 @@
+# Desktop game launcher validation
+
+Date: 2026-07-26
+
+Status: **PASS**. Build, automated launcher gates, deployment, shortcut
+readback, save preservation, dependency inspection, and native visual checks
+at 100%, 125%, 150%, and 200% passed without relaxing any gate.
+
+## Recovery semantics
+
+The existing platform and dungeon tests collectively cover pending death with
+automatic continuation disabled, an exact committed continuation when enabled,
+pre-publish `not_committed`, post-publish `indeterminate`, loading an already
+clear checkpoint, restart/idempotence behavior, and malformed committed death
+receipts. The focused binary reported `477 cases, 0 failures`; therefore no
+production defect was found and no recovery-fix commit was created.
+
+## Build and launcher gates
+
+The Debug platform target built serially and `platform.units` passed. The
+Release launcher and launcher tests built serially after loading the MSVC
+developer environment in the same PowerShell process. All five launcher gates
+passed: units, icon asset, manifest, window smoke, and deploy self-test. The
+final narrow rerun passed 5/5 launcher tests, 1/1 platform test, and the direct
+platform executable reported `477 cases, 0 failures`. No build, launcher, or
+game process remained afterward.
+
+## Deployment
+
+The owned deployment root was
+`E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859`. The real deployment root
+is `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app`, and the desktop
+shortcut is `C:\Users\dnfsr\Desktop\无限地下城.lnk`. The shortcut target,
+working directory, and icon all point to the real deployment root and contain
+no `.worktrees` component.
+
+The production save directory was measured before and after the owned and real
+deployments. Both snapshots contained two files totaling 1248 bytes. The file
+lengths and UTC write-time ticks were identical; see
+`evidence/desktop-launcher/launcher-validation.txt`.
+
+## Visual evidence
+
+Computer Use selected the exact launcher window for the 100%, 125%, and 150%
+visual inspections. After the approved restart, Computer Use was unavailable,
+so the 200% checks used an exact owned launcher PID and HWND plus a native
+Win32 client capture. Every image shows pure-color green/red status text,
+non-overlapping Chinese labels, a visible Start button, and a non-white client:
+
+- `evidence/desktop-launcher/ready-100.png`
+- `evidence/desktop-launcher/missing-assets-100.png`
+- `evidence/desktop-launcher/ready-125.png`
+- `evidence/desktop-launcher/missing-assets-125.png`
+- `evidence/desktop-launcher/ready-150.png`
+- `evidence/desktop-launcher/missing-assets-150.png`
+- `evidence/desktop-launcher/ready-200.png`
+- `evidence/desktop-launcher/missing-assets-200.png`
+
+The system started at 125%, was changed through the Windows Display Settings UI
+for the 100% and 150% checks, and was read back at the original 125% afterward.
+
+The physical 1920x1080 panel could not fit the exact 200% client, so the approved
+signed VDD 25.7.23 path was used. Following the restart, the single VDD was set
+to 3840x2160 at 60 Hz and Windows Display Settings identified its scale as 200%
+(recommended). The physical target was unavailable to the active topology, so
+the 200% validation used the VDD as the sole validation display; no duplicate
+VDD or 800x600 state was accepted.
+
+For both 200% states, the launcher was started from
+`E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859\app`, its exact HWND was
+verified to belong to the started PID, and capture points were verified to be
+owned by that PID. `GetDpiForWindow` returned 192 and `GetClientRect` returned
+1840x1120 for both runs. `ready-200.png` contains 274 sampled pure-green pixels
+and SHA-256 `C71843AC75271C0208DD9FDF569748921DFB281CA1D70308DE907515A0157141`.
+`missing-assets-200.png` contains 428 sampled pure-red pixels and SHA-256
+`741CB05CEC8E9618F54914C375FC4822D90D54639FDC4FCD10463E6009FCD030`.
+The owned `assets\skills` directory was restored immediately after the missing
+state capture. The old constrained 200% blocker image is intentionally excluded
+from committed evidence.
+
+## Dependencies
+
+`dumpbin /dependents` succeeded for both deployed executables. The launcher uses
+Direct2D/DirectWrite and Win32 system DLLs. The game uses Win32 system DLLs plus
+the MSVC/UCRT runtime DLLs. Exact output is recorded in the text evidence.
diff --git a/docs/validation/evidence/desktop-launcher/launcher-validation.txt b/docs/validation/evidence/desktop-launcher/launcher-validation.txt
new file mode 100644
index 0000000..eb0139f
--- /dev/null
+++ b/docs/validation/evidence/desktop-launcher/launcher-validation.txt
@@ -0,0 +1,102 @@
+Desktop launcher validation evidence
+Date: 2026-07-26
+Status: PASS (100/125/150/200 visual gates and final narrow tests passed; no threshold relaxed)
+Base/HEAD: 373f85961d5eb0c612d4b12157c16baffe3b8f45
+Branch: codex/integration-large-square-rooms
+
+FOCUSED DEATH RECOVERY
+Command: cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
+Result: exit 0; ninja: no work to do
+Command: ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
+Result: 1/1 passed, 0 failed, 10.02 seconds
+Command: out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
+Result: 477 cases, 0 failures
+TDD result: existing coverage passed; no production defect, test edit, or recovery-fix commit.
+
+RELEASE BUILD
+Command: . ./scripts/Configure.ps1 -Preset windows-msvc-release -Fresh
+Result: exit 0
+Command (same MSVC developer shell): cmake --build --preset windows-msvc-release --target arpg_launcher arpg_launcher_tests -- -j1
+Result: exit 0; 180/180 build steps completed
+Command: ctest --test-dir out/build/windows-msvc-release -R '^launcher\.(units|manifest|window_smoke|icon_asset|deploy_self_test)$' --output-on-failure
+Result: 5/5 passed, 0 failed, 6.91 seconds
+Post-run process check: no CMake, CTest, Ninja, compiler, linker, launcher, or game process remained after cleanup.
+
+FINAL NARROW RERUN
+Command: ctest --test-dir out/build/windows-msvc-release -R '^launcher\.(units|manifest|window_smoke|icon_asset|deploy_self_test)$' --output-on-failure
+Result: 5/5 passed, 0 failed, 9.78 seconds
+Command: ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
+Result: 1/1 passed, 0 failed, 11.15 seconds
+Command: out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
+Result: 477 cases, 0 failures
+Post-run process check: 0 relevant build, launcher, game, and VDD Control processes.
+
+DEPLOYMENT
+Owned root: E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859
+Owned shortcut: E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859\desktop\无限地下城.lnk
+Owned TargetPath: E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859\app\无限地下城启动器.exe
+Owned WorkingDirectory: E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859\app
+Owned IconLocation: E:\codex1\Temp\InfiniteDungeonTask2Owned-373f859\app\无限地下城启动器.exe,0
+Real shortcut: C:\Users\dnfsr\Desktop\无限地下城.lnk
+Real TargetPath: C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app\无限地下城启动器.exe
+Real WorkingDirectory: C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app
+Real IconLocation: C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app\无限地下城启动器.exe,0
+Path gate: all real deployed/shortcut paths contain no .worktrees component.
+
+PRODUCTION SAVE PRESERVATION
+Directory: C:\Users\dnfsr\AppData\Local\InfiniteDungeon\save
+Before: exists=true, count=2, totalBytes=1248
+  run_a.sav length=460 LastWriteTimeUtcTicks=639201529634735548
+  run_b.sav length=788 LastWriteTimeUtcTicks=639202504336470684
+After: exists=true, count=2, totalBytes=1248
+  run_a.sav length=460 LastWriteTimeUtcTicks=639201529634735548
+  run_b.sav length=788 LastWriteTimeUtcTicks=639202504336470684
+Comparison: exact equality; deployment did not change production saves.
+Final narrow-rerun comparison: exact equality; still 2 files / 1248 bytes with identical UTC ticks.
+
+VISUAL EVIDENCE (Computer Use exact-window capture)
+PASS ready-100.png: 922x592, sha256=5B208562BA1A8C2C66B27D543164F4EC994EC73218553038DA47CD213D43BC6C
+PASS missing-assets-100.png: 922x592, sha256=8979BEF002FFB9C00C92F9B347E56E24696030FF034A091E9B9ECD57FE5FF691
+PASS ready-125.png: 922x591, sha256=087B371644CC7665C80CE6AC4A9FE21436D903802B95239C268F1D220767DF3E
+PASS missing-assets-125.png: 922x591, sha256=951DB7391067970E5A0A9A8712B10DBD8139C4234BBC554BB4E615221C8DF41D
+PASS ready-150.png: 923x591, sha256=B25CE1F86072082CFB7887D4507FE90AFB682705514632C5E4FA08E7F3CE89F2
+PASS missing-assets-150.png: 923x591, sha256=0285C78B09BF959AAF7F232F03EA1AE6417893F8992A990FEC55382F356FEC49
+PASS ready-200.png: native client 1840x1120, sha256=C71843AC75271C0208DD9FDF569748921DFB281CA1D70308DE907515A0157141
+PASS missing-assets-200.png: native client 1840x1120, sha256=741CB05CEC8E9618F54914C375FC4822D90D54639FDC4FCD10463E6009FCD030
+Visual checks at passing DPIs: pure green/red status text, non-overlapping Chinese labels, visible Start button, non-white client.
+Physical-display scaling: original 125% recommended; changed through Windows Settings for 100% and 150%; restored to 125% before VDD validation.
+200 topology: one ROOT\DISPLAY\0000 VDD with one MTT1337 monitor; active VDD mode 3840x2160@60; Windows Display Settings showed 200% (recommended).
+200 capture method: Computer Use unavailable after restart; exact owned launcher PID + HWND, Per-Monitor-V2 Win32 client capture, no scaling/cropping.
+200 ready readback: PID=20904 HWND=0xA0396 display=DISPLAY6 GetDpiForWindow=192 GetClientRect=1840x1120 pureGreenSamples=274.
+200 missing readback: PID=11156 HWND=0x705C4 display=DISPLAY6 GetDpiForWindow=192 GetClientRect=1840x1120 pureRedSamples=428.
+200 visual outcome: PASS for both states. Owned assets/skills was restored and both launcher PIDs exited.
+
+DUMPBIN DEPENDENTS
+Launcher: C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app\无限地下城启动器.exe
+  d2d1.dll
+  DWrite.dll
+  SHELL32.dll
+  ole32.dll
+  USER32.dll
+  GDI32.dll
+  KERNEL32.dll
+Game: C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app\arpg_game.exe
+  WINMM.dll
+  USER32.dll
+  KERNEL32.dll
+  GDI32.dll
+  SHELL32.dll
+  MSVCP140.dll
+  VCRUNTIME140.dll
+  VCRUNTIME140_1.dll
+  api-ms-win-crt-runtime-l1-1-0.dll
+  api-ms-win-crt-heap-l1-1-0.dll
+  api-ms-win-crt-stdio-l1-1-0.dll
+  api-ms-win-crt-string-l1-1-0.dll
+  api-ms-win-crt-math-l1-1-0.dll
+  api-ms-win-crt-filesystem-l1-1-0.dll
+  api-ms-win-crt-environment-l1-1-0.dll
+  api-ms-win-crt-time-l1-1-0.dll
+  api-ms-win-crt-utility-l1-1-0.dll
+  api-ms-win-crt-convert-l1-1-0.dll
+  api-ms-win-crt-locale-l1-1-0.dll
diff --git a/docs/validation/evidence/desktop-launcher/missing-assets-100.png b/docs/validation/evidence/desktop-launcher/missing-assets-100.png
new file mode 100644
index 0000000..1c1bf61
Binary files /dev/null and b/docs/validation/evidence/desktop-launcher/missing-assets-100.png differ
diff --git a/docs/validation/evidence/desktop-launcher/missing-assets-125.png b/docs/validation/evidence/desktop-launcher/missing-assets-125.png
new file mode 100644
index 0000000..04fa46c
Binary files /dev/null and b/docs/validation/evidence/desktop-launcher/missing-assets-125.png differ
diff --git a/docs/validation/evidence/desktop-launcher/missing-assets-150.png b/docs/validation/evidence/desktop-launcher/missing-assets-150.png
new file mode 100644
index 0000000..4661d2d
Binary files /dev/null and b/docs/validation/evidence/desktop-launcher/missing-assets-150.png differ
diff --git a/docs/validation/evidence/desktop-launcher/missing-assets-200.png b/docs/validation/evidence/desktop-launcher/missing-assets-200.png
new file mode 100644
index 0000000..2026b8a
Binary files /dev/null and b/docs/validation/evidence/desktop-launcher/missing-assets-200.png differ
diff --git a/docs/validation/evidence/desktop-launcher/ready-100.png b/docs/validation/evidence/desktop-launcher/ready-100.png
new file mode 100644
index 0000000..c46ace9
Binary files /dev/null and b/docs/validation/evidence/desktop-launcher/ready-100.png differ
diff --git a/docs/validation/evidence/desktop-launcher/ready-125.png b/docs/validation/evidence/desktop-launcher/ready-125.png
new file mode 100644
index 0000000..a7da919
Binary files /dev/null and b/docs/validation/evidence/desktop-launcher/ready-125.png differ
diff --git a/docs/validation/evidence/desktop-launcher/ready-150.png b/docs/validation/evidence/desktop-launcher/ready-150.png
new file mode 100644
index 0000000..57ecd1d
Binary files /dev/null and b/docs/validation/evidence/desktop-launcher/ready-150.png differ
diff --git a/docs/validation/evidence/desktop-launcher/ready-200.png b/docs/validation/evidence/desktop-launcher/ready-200.png
new file mode 100644
index 0000000..5a43d05
Binary files /dev/null and b/docs/validation/evidence/desktop-launcher/ready-200.png differ

### Task 7: Merge, Full Verification, and Completion Gate

**Worktree:** E:\game\.worktrees\m00-foundation

**Files:**
- Merge: task/m00-core-runtime
- Merge: task/m00-raylib-host
- Do not modify main.

**Interfaces:**
- Consumes the independently passing core and host branches.
- Produces the verified milestone/m00-foundation branch and factual completion report.
- Any integration fix requires its own failing reproduction, focused commit, and rerun of every command in Steps 3–6.

- [ ] **Step 1: Merge the core branch first**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation -c user.name=Codex -c user.email=codex@local merge --no-ff task/m00-core-runtime -m "merge: integrate stage 0 core runtime"
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
~~~

Expected: merge has no ownership conflict; twenty-two core cases and architecture.core_no_raylib pass.

- [ ] **Step 2: Merge the host branch second**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation -c user.name=Codex -c user.email=codex@local merge --no-ff task/m00-raylib-host -m "merge: integrate stage 0 raylib host"
~~~

Expected: merge has no conflict because the host branch only changed src/platform/raylib/raylib_host.cpp.

- [ ] **Step 3: Run clean Debug and Release builds and tests**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
ctest --test-dir .\out\build\windows-msvc-debug -N
ctest --test-dir .\out\build\windows-msvc-release -N
~~~

Expected:

- Both builds succeed with no project warnings.
- core.units runs twenty-two cases with zero failures.
- architecture.core_no_raylib passes.
- Each ctest -N command lists exactly two tests.

- [ ] **Step 4: Verify the toolchain, dependency mode, executable, and ignored outputs**

Run:

~~~powershell
Select-String -Path .\out\build\windows-msvc-debug\CMakeCache.txt -Pattern 'CMAKE_CXX_COMPILER:FILEPATH|CMAKE_BUILD_TYPE:STRING|BUILD_SHARED_LIBS:BOOL'
Select-String -Path .\out\build\windows-msvc-release\CMakeCache.txt -Pattern 'CMAKE_CXX_COMPILER:FILEPATH|CMAKE_BUILD_TYPE:STRING|BUILD_SHARED_LIBS:BOOL'
Get-Item .\out\build\windows-msvc-release\bin\arpg_game.exe
Get-ChildItem .\out\build\windows-msvc-release\bin -Filter 'raylib*.dll'
git -c safe.directory=E:/game/.worktrees/m00-foundation check-ignore out/build/windows-msvc-debug/CMakeCache.txt
~~~

Expected:

- Compiler path ends in cl.exe.
- Debug cache reports CMAKE_BUILD_TYPE=Debug.
- Release cache reports CMAKE_BUILD_TYPE=Release.
- BUILD_SHARED_LIBS=OFF.
- arpg_game.exe exists.
- No raylib DLL is present because raylib is static.
- The CMake cache path is ignored by Git.

- [ ] **Step 5: Verify both user exit paths and process cleanup**

Run the Release executable once and exit with Esc:

~~~powershell
$escapeRun = Start-Process -FilePath .\out\build\windows-msvc-release\bin\arpg_game.exe -WindowStyle Normal -PassThru
$escapeRun.Id
~~~

Run it a second time and use the window close button:

~~~powershell
$closeRun = Start-Process -FilePath .\out\build\windows-msvc-release\bin\arpg_game.exe -WindowStyle Normal -PassThru
$closeRun.Id
~~~

After each visible process closes, run:

~~~powershell
Get-Process arpg_game -ErrorAction SilentlyContinue
~~~

Expected: both exits close normally and the final process query returns no arpg_game process.

- [ ] **Step 6: Inspect repository cleanliness and milestone history**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation diff --check
git -c safe.directory=E:/game/.worktrees/m00-foundation status --short --branch
git -c safe.directory=E:/game/.worktrees/m00-foundation log --oneline --decorate -8
~~~

Expected: milestone/m00-foundation is clean; history shows the baseline, fixed-step, three core component commits, host commit, and two merge commits.

- [ ] **Step 7: Report and stop**

Report these factual results to the user:

1. Exact Debug and Release build commands and exit status.
2. CTest test names, core case count, and failure count.
3. The confirmed raylib commit, version macros, and static-library state.
4. Manual graybox/HUD, Esc, close-button, and process-cleanup results.
5. milestone/m00-foundation HEAD commit.
6. Any observed limitation that does not violate Stage 0.

Stop with milestone/m00-foundation unmerged into main. Keep the integration worktree available for user inspection. Do not create a Stage 1 branch.

---

## Specification Coverage Matrix

| Design requirement | Implemented and verified by |
|---|---|
| C++17, CMake, Ninja, MSVC, SDK | Task 1 Steps 2–3; Task 7 Steps 3–4 |
| Exact static raylib 6.0 | Task 1 Step 2; Task 6 compile-time assertions; Task 7 Step 4 |
| Core/raylib separation | Task 1 Step 5; every CTest run |
| 60 Hz fixed update and 8-step cap | Task 2 |
| Deterministic independent streams | Task 3 |
| Generational fixed pool | Task 4 |
| Bounded FIFO queue | Task 5 |
| No C++ heap allocation in pool/queue pressure loops | Tasks 4–5 |
| 1280×720 low-cost 2.5D graybox and diagnostics | Task 6 |
| Headless Debug and Release tests | Task 7 Step 3 |
| Esc, close button, no residual process | Task 7 Step 5 |
| Worktree ownership and merge order | Tasks 1–2 and Task 7 |
| Stop before Stage 1 | Task 7 Step 7 |

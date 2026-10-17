# Task 1 implementation report

Status: **BLOCKED — direct GREEN gate failed**

## Preserved RED and scope

- Recorded RED was preserved without rerun: native exit 8 at 12,000 ticks,
  184/600 defeated, wall 3.874s.
- Committed HEAD: `db642ae079ffec66c65c496a79a3b4ba2cd61d8c`.
- Branch: `codex/task9-gate-recovery`.
- Staging area was empty.
- Sole tracked dirty file before this card:
  `tests/dungeon/stage10_validation_fixture.cpp`.
- `git diff --binary -- tests/dungeon/stage10_validation_fixture.cpp |
  git hash-object --stdin` produced the required inherited hash
  `0f6fd993948be99d1be604e066ec2bb3cebcdcdd`.

## Implemented diff

- `tests/dungeon/stage10_validation_fixture.cpp`
  - includes `core/gameplay_limits.hpp` and `<limits>`;
  - adds checked `std::uint64_t` population scaling using
    `ceil(N * 60000 / 736)` and minimum 12,000;
  - rejects zero population, population above
    `arpg::limits::kRoomMonsterCapacity`, and a budget outside `uint32_t`;
  - snapshots the initial population after servicing pending saves and computes
    one finite budget per `drive_clear()` call;
  - prints the actual driver budget and initial population in failure output;
  - loads and validates the durable cleared checkpoint before reward
    materialization;
  - seeds the abandon branch from that captured cleared checkpoint and removes
    the redundant second combat clear.
- `tests/dungeon/CMakeLists.txt`
  - changes only
    `stage10.validation_fixture.real_abyss_transactions` from timeout 30 to 60.
- Final tracked diff scope contains exactly those two files. Relative to
  committed HEAD (therefore including the intentional inherited driver diff),
  `git diff --stat` reports 707 insertions and 32 deletions.

## Static audit

- `git diff --check` — exit 0.
- `rg -n "relay_defeated\\s*\\(|events_\\.try_push\\s*\\(|defeat_monster\\s*\\(" tests/dungeon/stage10_validation_fixture.cpp`
  — no matches.
- Scoped `drive_clear()` source scan — exactly one
  `session.tick(movement)` in the loop.
- `git diff --cached --quiet` — exit 0; index empty.
- `git diff --name-only` — only the two allowed tracked test files.

## Configure and build

The first plain-shell configure attempt did not enter compilation:

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
cmake --preset windows-msvc-debug
```

- exit 1, 0.083s;
- diagnostic: `Stage 0 requires Windows SDK 10.0.26100.0; detected ''`.

The required VS2022 x64 environment was then loaded without changing source:

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && set "WindowsSDKVersion=10.0.26100.0\" && set "CMAKE_BUILD_PARALLEL_LEVEL=1" && cmake --preset windows-msvc-debug'
```

- exit 0, 9.324s; configure and generation completed.

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && set "WindowsSDKVersion=10.0.26100.0\" && set "CMAKE_BUILD_PARALLEL_LEVEL=1" && cmake --build --preset windows-msvc-debug --target arpg_stage10_validation_fixture --parallel 1'
```

- exit 0, 4.455s;
- compiled the fixture object and linked
  `bin/arpg_stage10_validation_fixture.exe`.

## Direct gate failure

```powershell
$process = Start-Process `
  'out/build/windows-msvc-debug/bin/arpg_stage10_validation_fixture.exe' `
  -Wait -PassThru -NoNewWindow
$process.ExitCode
```

- exit 8, 23.911s;
- 48,914-tick budget for initial population 600;
- 486 defeated, 114 remaining;
- player HP 596/768, barrier 0;
- Storm/Draw/light accepted or queued: 18/89/242;
- lock acquired 300, absent releases 293, movement-stall releases 6;
- sweep ticks 6,127;
- one melee-chain entry, 280 melee handoffs;
- light-lane recovery entries/ticks/reacquired: 6/4,745/6.

Exact terminal summary:

```text
clear failed phase=1 abyss=1 rule=8 room=1 hp=596 targets=114 budget=48914 initial_population=600
dense driver failed reason=tick_budget driver_tick=48914 driver_budget=48914 initial_population=600 session_tick=48914 initial=600 defeated=486 remaining=114
direct_exit=8 elapsed_seconds=23.911
```

Per the task card, both exact CTest selectors were **not run**. No staging or
commit occurred, and neither the formula nor the 60-second timeout was enlarged
after the failed direct gate.

# raylib 6.0 Stage 0 Foundation Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Build and verify the Windows Stage 0 C++17/raylib 6.0 foundation: deterministic fixed-step timing and RNG, fixed-capacity containers, a visible 2.5D graybox host, and headless CTest coverage.

**Architecture:** arpg_core is a raylib-free static library. arpg_raylib privately consumes arpg_core and the pinned static raylib target, while arpg_game is only the process entry point. A milestone integration worktree freezes the public timing/diagnostic contract before separate core-runtime and raylib-host worktrees proceed in parallel.

**Tech Stack:** C++17, raylib 6.0 commit dbc56a87da87d973a9c5baa4e7438a9d20121d28, CMake 3.25+, Ninja, MSVC 19.44 x64, Windows SDK 10.0.26100.0, CTest, PowerShell.

## Global Constraints

- Implement only Stage 0. Do not add players, monsters, combat, doors, dungeon generation, abyss rolls, saves, progression, items, affixes, audio, networking, ECS, or physics.
- Use the official raylib 6.0 commit archive and SHA256 81b06ce7c19cf3b634b0271c23c361ba6ad8bf45fb8b036abbfeb4260ec1e126. Never fall back to a system raylib.
- arpg_core must not include or link raylib, raymath, rlgl, or raylib-cpp.
- Simulation uses a fixed 1/60 second step and executes at most 8 fixed updates per rendered frame.
- Random values use xoshiro256** with SplitMix64 initialization and explicit 64-bit seeds.
- FixedPool and BoundedQueue must not allocate, resize, block, overwrite live data, or throw on normal capacity failure.
- Project code uses /W4 /permissive-. These flags must not propagate into raylib.
- Only the active room graybox is rendered. No textures, fonts, audio files, or other runtime assets are loaded.
- Tests remain headless. Window behavior is verified manually after all CTest checks pass.
- No task worktree may modify root CMakeLists.txt, CMakePresets.json, cmake/, or scripts/ after the integration baseline is committed.
- Do not merge milestone/m00-foundation into main and do not begin Stage 1 until the user accepts the Stage 0 completion report.

---

## Final File Map

| Path | Responsibility | Owner |
|---|---|---|
| CMakeLists.txt | Root targets, exact MSVC/SDK guard, warnings, build graph | integration |
| CMakePresets.json | Debug, Release, and core-only Debug presets | integration |
| cmake/Dependencies.cmake | Pinned static raylib 6.0 FetchContent dependency | integration |
| scripts/Configure.ps1 | MSVC discovery and CMake configure | integration |
| scripts/Build.ps1 | Configure then build selected preset | integration |
| scripts/Test.ps1 | Configure, build, then run CTest | integration |
| src/core/fixed_step.hpp and .cpp | Fixed 60 Hz accumulator and diagnostics | integration contract, then core |
| src/core/deterministic_rng.hpp and .cpp | SplitMix64/xoshiro256** streams | core |
| src/core/fixed_pool.hpp | Header-only generational fixed pool | core |
| src/core/bounded_queue.hpp | Header-only fixed ring queue | core |
| src/platform/raylib/raylib_host.hpp and .cpp | Window loop, graybox renderer, HUD | host |
| src/app/main.cpp | Process entry point only | integration contract |
| tests/core/test_framework.hpp | Static, allocation-free unit-test descriptors | integration contract |
| tests/core/allocation_probe.hpp and .cpp | Global C++ allocation counter for pressure tests | core |
| tests/core/fixed_step_tests.cpp | Fixed-step behavior tests | integration contract, then core |
| tests/core/deterministic_rng_tests.cpp | RNG golden and stream-isolation tests | core |
| tests/core/fixed_pool_tests.cpp | Pool behavior, lifetime, and allocation tests | core |
| tests/core/bounded_queue_tests.cpp | Queue behavior, wrap, lifetime, and allocation tests | core |
| tests/platform/core_boundary_test.cmake | Source and target dependency guard | integration contract |

## Worktree Topology and Order

1. main remains the accepted design/plan baseline.
2. Task 1 creates E:\game\.worktrees\m00-foundation on milestone/m00-foundation.
3. Task 2 completes and freezes FixedStepRunner and RaylibHostConfig on the integration branch.
4. After Task 2 passes, create E:\game\.worktrees\m00-core-runtime on task/m00-core-runtime and E:\game\.worktrees\m00-raylib-host on task/m00-raylib-host.
5. Tasks 3–5 execute sequentially in the core worktree. Task 6 may run in parallel in the host worktree.
6. Task 7 merges core first and host second into the integration worktree, then performs full verification.

---

### Task 1: Buildable Integration Baseline

**Worktree:** E:\game\.worktrees\m00-foundation on milestone/m00-foundation

**Files:**
- Create: CMakeLists.txt
- Create: CMakePresets.json
- Create: cmake/Dependencies.cmake
- Create: scripts/Configure.ps1
- Create: scripts/Build.ps1
- Create: scripts/Test.ps1
- Create: src/core/CMakeLists.txt
- Create: src/core/fixed_step.hpp
- Create: src/core/fixed_step.cpp
- Create: src/platform/raylib/CMakeLists.txt
- Create: src/platform/raylib/raylib_host.hpp
- Create: src/platform/raylib/raylib_host.cpp
- Create: src/app/CMakeLists.txt
- Create: src/app/main.cpp
- Create: tests/core/CMakeLists.txt
- Create: tests/core/test_framework.hpp
- Create: tests/core/test_main.cpp
- Create: tests/core/fixed_step_tests.cpp
- Create: tests/platform/CMakeLists.txt
- Create: tests/platform/core_boundary_test.cmake

**Interfaces:**
- Produces: arpg::core::FixedStepFrame and arpg::core::FixedStepRunner::advance(double) noexcept.
- Produces: arpg::platform::RaylibHostConfig and arpg::platform::run_raylib_host(const RaylibHostConfig&) noexcept.
- Produces: CMake targets arpg_core, arpg_raylib, arpg_game, and arpg_core_tests.
- Consumes: approved design at docs/superpowers/specs/2026-07-10-raylib-stage0-foundation-design.md.

- [ ] **Step 1: Create and verify the integration worktree**

Run from E:\game:

~~~powershell
git -c safe.directory=E:/game check-ignore .worktrees/probe
git -c safe.directory=E:/game worktree add .worktrees/m00-foundation -b milestone/m00-foundation main
git -c safe.directory=E:/game/.worktrees/m00-foundation -C .worktrees/m00-foundation status --short --branch
~~~

Expected: the ignore command prints .worktrees/probe; the new worktree reports branch milestone/m00-foundation with no changes.

- [ ] **Step 2: Add the exact root build and dependency configuration**

Create CMakeLists.txt:

~~~cmake
cmake_minimum_required(VERSION 3.25)

project(infinite_dungeon_arpg VERSION 0.1.0 LANGUAGES C CXX)

option(ARPG_BUILD_GRAPHICS "Build the raylib host and game executable" ON)

if(NOT MSVC)
    message(FATAL_ERROR "Stage 0 requires MSVC")
endif()

if(NOT MSVC_VERSION EQUAL 1944)
    message(FATAL_ERROR
        "Stage 0 requires MSVC 19.44; detected "
        "${CMAKE_CXX_COMPILER_ID} ${CMAKE_CXX_COMPILER_VERSION}")
endif()

if(NOT CMAKE_SIZEOF_VOID_P EQUAL 8)
    message(FATAL_ERROR "Stage 0 requires an x64 target")
endif()

string(REGEX REPLACE "[/\\\\]+$" "" ARPG_WINDOWS_SDK "$ENV{WindowsSDKVersion}")
if(NOT ARPG_WINDOWS_SDK STREQUAL "10.0.26100.0")
    message(FATAL_ERROR
        "Stage 0 requires Windows SDK 10.0.26100.0; detected "
        "'${ARPG_WINDOWS_SDK}'")
endif()

set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
set(CMAKE_CXX_EXTENSIONS OFF)
set(CMAKE_RUNTIME_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/bin")
set(CMAKE_LIBRARY_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")
set(CMAKE_ARCHIVE_OUTPUT_DIRECTORY "${CMAKE_BINARY_DIR}/lib")

function(arpg_enable_project_warnings target_name)
    if(MSVC)
        target_compile_options("${target_name}" PRIVATE /W4 /permissive-)
    endif()
endfunction()

include(CTest)
add_subdirectory(src/core)

if(ARPG_BUILD_GRAPHICS)
    include(cmake/Dependencies.cmake)
    add_subdirectory(src/platform/raylib)
    add_subdirectory(src/app)
endif()

if(BUILD_TESTING)
    add_subdirectory(tests/core)
    add_subdirectory(tests/platform)
endif()
~~~

Create cmake/Dependencies.cmake:

~~~cmake
include_guard(GLOBAL)
include(FetchContent)

set(BUILD_EXAMPLES OFF CACHE BOOL "Do not build raylib examples" FORCE)
set(BUILD_SHARED_LIBS OFF CACHE BOOL "Build raylib statically" FORCE)
set(PLATFORM "Desktop" CACHE STRING "raylib platform" FORCE)

FetchContent_Declare(
    raylib
    URL "https://github.com/raysan5/raylib/archive/dbc56a87da87d973a9c5baa4e7438a9d20121d28.tar.gz"
    URL_HASH "SHA256=81b06ce7c19cf3b634b0271c23c361ba6ad8bf45fb8b036abbfeb4260ec1e126"
    DOWNLOAD_DIR "${PROJECT_SOURCE_DIR}/out/downloads"
    DOWNLOAD_NAME "raylib-dbc56a87da87d973a9c5baa4e7438a9d20121d28.tar.gz"
    DOWNLOAD_EXTRACT_TIMESTAMP TRUE
)

FetchContent_MakeAvailable(raylib)

if(NOT TARGET raylib)
    message(FATAL_ERROR "Pinned raylib dependency did not create target raylib")
endif()

get_target_property(ARPG_RAYLIB_TYPE raylib TYPE)
if(NOT ARPG_RAYLIB_TYPE STREQUAL "STATIC_LIBRARY")
    message(FATAL_ERROR "raylib must be static; detected ${ARPG_RAYLIB_TYPE}")
endif()

file(READ "${raylib_SOURCE_DIR}/src/raylib.h" ARPG_RAYLIB_HEADER)
foreach(EXPECTED_REGEX IN ITEMS
        "#define[ \t]+RAYLIB_VERSION_MAJOR[ \t]+6"
        "#define[ \t]+RAYLIB_VERSION_MINOR[ \t]+0"
        "#define[ \t]+RAYLIB_VERSION_PATCH[ \t]+0")
    string(REGEX MATCH "${EXPECTED_REGEX}" DEFINE_MATCH "${ARPG_RAYLIB_HEADER}")
    if(NOT DEFINE_MATCH)
        message(FATAL_ERROR "Pinned raylib archive has unexpected version macros")
    endif()
endforeach()
~~~

Create CMakePresets.json:

~~~json
{
  "version": 6,
  "cmakeMinimumRequired": {
    "major": 3,
    "minor": 25,
    "patch": 0
  },
  "configurePresets": [
    {
      "name": "windows-msvc-base",
      "hidden": true,
      "generator": "Ninja",
      "binaryDir": "${sourceDir}/out/build/${presetName}",
      "cacheVariables": {
        "BUILD_TESTING": "ON",
        "CMAKE_EXPORT_COMPILE_COMMANDS": "ON"
      }
    },
    {
      "name": "windows-msvc-debug",
      "inherits": "windows-msvc-base",
      "displayName": "Windows MSVC Debug",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Debug"
      }
    },
    {
      "name": "windows-msvc-release",
      "inherits": "windows-msvc-base",
      "displayName": "Windows MSVC Release",
      "cacheVariables": {
        "CMAKE_BUILD_TYPE": "Release"
      }
    },
    {
      "name": "windows-msvc-core-debug",
      "inherits": "windows-msvc-debug",
      "displayName": "Windows MSVC Core-only Debug",
      "cacheVariables": {
        "ARPG_BUILD_GRAPHICS": "OFF"
      }
    }
  ],
  "buildPresets": [
    {
      "name": "windows-msvc-debug",
      "configurePreset": "windows-msvc-debug"
    },
    {
      "name": "windows-msvc-release",
      "configurePreset": "windows-msvc-release"
    },
    {
      "name": "windows-msvc-core-debug",
      "configurePreset": "windows-msvc-core-debug"
    }
  ],
  "testPresets": [
    {
      "name": "windows-msvc-debug",
      "configurePreset": "windows-msvc-debug",
      "output": { "outputOnFailure": true },
      "execution": { "stopOnFailure": true }
    },
    {
      "name": "windows-msvc-release",
      "configurePreset": "windows-msvc-release",
      "output": { "outputOnFailure": true },
      "execution": { "stopOnFailure": true }
    },
    {
      "name": "windows-msvc-core-debug",
      "configurePreset": "windows-msvc-core-debug",
      "output": { "outputOnFailure": true },
      "execution": { "stopOnFailure": true }
    }
  ]
}
~~~

- [ ] **Step 3: Add the three PowerShell entry scripts**

Create scripts/Configure.ps1:

~~~powershell
[CmdletBinding()]
param(
    [ValidateSet(
        'windows-msvc-debug',
        'windows-msvc-release',
        'windows-msvc-core-debug')]
    [string]$Preset = 'windows-msvc-debug',
    [switch]$Fresh
)

$ErrorActionPreference = 'Stop'

function Invoke-ArpgNative {
    param(
        [Parameter(Mandatory)]
        [string]$FilePath,
        [string[]]$ArgumentList = @()
    )

    & $FilePath @ArgumentList
    if ($LASTEXITCODE -ne 0) {
        throw "$FilePath failed with exit code $LASTEXITCODE"
    }
}

function Enter-ArpgMsvcEnvironment {
    $vswhere = Join-Path ${env:ProgramFiles(x86)} 'Microsoft Visual Studio\Installer\vswhere.exe'
    if (-not (Test-Path -LiteralPath $vswhere)) {
        throw "vswhere.exe not found: $vswhere"
    }

    $vswhereArgs = @(
        '-latest',
        '-version', '[17.0,18.0)',
        '-products', 'Microsoft.VisualStudio.Product.BuildTools',
        '-requires', 'Microsoft.VisualStudio.Component.VC.Tools.x86.x64',
        'Microsoft.VisualStudio.Component.Windows11SDK.26100',
        '-property', 'installationPath'
    )
    $installPath = & $vswhere @vswhereArgs
    if ($LASTEXITCODE -ne 0 -or [string]::IsNullOrWhiteSpace($installPath)) {
        throw 'VS 2022 Build Tools with MSVC x64 and SDK 26100 not found'
    }

    $installPath = ($installPath | Select-Object -First 1).Trim()
    $devShell = Join-Path $installPath 'Common7\Tools\Microsoft.VisualStudio.DevShell.dll'
    if (-not (Test-Path -LiteralPath $devShell)) {
        throw "Developer PowerShell module not found: $devShell"
    }

    Import-Module $devShell -Force
    Enter-VsDevShell -VsInstallPath $installPath -SkipAutomaticLocation -DevCmdArguments '-arch=x64 -host_arch=x64 -winsdk=10.0.26100.0' | Out-Null

    if ($env:VSCMD_ARG_HOST_ARCH -ne 'x64' -or $env:VSCMD_ARG_TGT_ARCH -ne 'x64') {
        throw 'MSVC environment is not x64 host/x64 target'
    }
    if ($env:WindowsSDKVersion.TrimEnd('\') -ne '10.0.26100.0') {
        throw "Unexpected Windows SDK: $env:WindowsSDKVersion"
    }

    $compiler = Get-Command cl.exe -ErrorAction Stop
    $version = [Version]$compiler.FileVersionInfo.FileVersion
    if ($version.Major -ne 19 -or $version.Minor -ne 44) {
        throw "MSVC 19.44 required; detected $version"
    }

    $cmake = Get-Command cmake.exe -ErrorAction Stop
    Get-Command ninja.exe -ErrorAction Stop | Out-Null
    $capabilities = & $cmake.Source -E capabilities | ConvertFrom-Json
    if ([Version]$capabilities.version.string -lt [Version]'3.25.0') {
        throw "CMake 3.25+ required; detected $($capabilities.version.string)"
    }
}

$root = (Resolve-Path (Join-Path $PSScriptRoot '..')).Path
Push-Location $root
try {
    Enter-ArpgMsvcEnvironment
    $cmakeArgs = @('--preset', $Preset)
    if ($Fresh) {
        $cmakeArgs += '--fresh'
    }
    Invoke-ArpgNative -FilePath 'cmake.exe' -ArgumentList $cmakeArgs
}
finally {
    Pop-Location
}
~~~

Create scripts/Build.ps1:

~~~powershell
[CmdletBinding()]
param(
    [ValidateSet(
        'windows-msvc-debug',
        'windows-msvc-release',
        'windows-msvc-core-debug')]
    [string]$Preset = 'windows-msvc-debug',
    [switch]$Fresh
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Configure.ps1') -Preset $Preset -Fresh:$Fresh

Push-Location $root
try {
    Invoke-ArpgNative -FilePath 'cmake.exe' -ArgumentList @('--build', '--preset', $Preset)
}
finally {
    Pop-Location
}
~~~

Create scripts/Test.ps1:

~~~powershell
[CmdletBinding()]
param(
    [ValidateSet(
        'windows-msvc-debug',
        'windows-msvc-release',
        'windows-msvc-core-debug')]
    [string]$Preset = 'windows-msvc-debug',
    [switch]$Fresh
)

$ErrorActionPreference = 'Stop'
. (Join-Path $PSScriptRoot 'Build.ps1') -Preset $Preset -Fresh:$Fresh

Push-Location $root
try {
    Invoke-ArpgNative -FilePath 'ctest.exe' -ArgumentList @('--preset', $Preset, '--no-tests=error')
}
finally {
    Pop-Location
}
~~~

- [ ] **Step 4: Freeze the public core and host interfaces and local targets**

Create src/core/fixed_step.hpp:

~~~cpp
#pragma once

#include <cstdint>

namespace arpg::core {

struct FixedStepFrame final {
    std::uint32_t steps{};
    std::uint64_t total_ticks{};
    double interpolation_alpha{};
    double dropped_seconds{};
    std::uint64_t invalid_input_count{};
};

class FixedStepRunner final {
public:
    static constexpr double kStepSeconds = 1.0 / 60.0;
    static constexpr std::uint32_t kMaxStepsPerFrame = 8;

    [[nodiscard]] FixedStepFrame advance(double frame_seconds) noexcept;

private:
    double accumulator_seconds_{};
    double dropped_seconds_{};
    std::uint64_t total_ticks_{};
    std::uint64_t invalid_input_count_{};
};

}  // namespace arpg::core
~~~

Create src/core/fixed_step.cpp in the red state:

~~~cpp
#include "core/fixed_step.hpp"
~~~

Create src/core/CMakeLists.txt:

~~~cmake
add_library(arpg_core STATIC fixed_step.cpp)

target_include_directories(arpg_core
    PUBLIC "${PROJECT_SOURCE_DIR}/src")
target_compile_features(arpg_core PUBLIC cxx_std_17)

arpg_enable_project_warnings(arpg_core)
~~~

Create src/platform/raylib/raylib_host.hpp:

~~~cpp
#pragma once

#include <cstdint>

namespace arpg::platform {

struct RaylibHostConfig final {
    int window_width{1280};
    int window_height{720};
    const char* window_title{"Infinite Dungeon - Stage 0"};
    std::uint64_t root_seed{0x6D30305F5241594CULL};
};

enum class HostExitCode : int {
    success = 0,
    window_initialization_failed = 1
};

[[nodiscard]] HostExitCode run_raylib_host(
    const RaylibHostConfig& config) noexcept;

}  // namespace arpg::platform
~~~

Create the initial src/platform/raylib/raylib_host.cpp:

~~~cpp
#include "raylib_host.hpp"

#include "core/fixed_step.hpp"

#include <raylib.h>

#include <cstdint>

#if !defined(RAYLIB_VERSION_MAJOR) || \
    !defined(RAYLIB_VERSION_MINOR) || \
    !defined(RAYLIB_VERSION_PATCH)
#error "raylib version macros are unavailable"
#endif

static_assert(RAYLIB_VERSION_MAJOR == 6, "raylib 6.0.0 is required");
static_assert(RAYLIB_VERSION_MINOR == 0, "raylib 6.0.0 is required");
static_assert(RAYLIB_VERSION_PATCH == 0, "raylib 6.0.0 is required");

namespace arpg::platform {

HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
    InitWindow(config.window_width, config.window_height, config.window_title);
    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "raylib window initialization failed");
        return HostExitCode::window_initialization_failed;
    }

    SetExitKey(KEY_ESCAPE);
    SetTargetFPS(60);

    core::FixedStepRunner fixed_step;
    while (!WindowShouldClose()) {
        const core::FixedStepFrame frame =
            fixed_step.advance(static_cast<double>(GetFrameTime()));

        BeginDrawing();
        ClearBackground(Color{18, 21, 29, 255});
        DrawText("Stage 0 integration baseline", 32, 32, 24, RAYWHITE);
        DrawText(
            TextFormat(
                "tick: %llu",
                static_cast<unsigned long long>(frame.total_ticks)),
            32,
            72,
            20,
            LIGHTGRAY);
        EndDrawing();
    }

    CloseWindow();
    return HostExitCode::success;
}

}  // namespace arpg::platform
~~~

Create src/platform/raylib/CMakeLists.txt:

~~~cmake
add_library(arpg_raylib STATIC raylib_host.cpp)

target_include_directories(arpg_raylib
    PUBLIC "${CMAKE_CURRENT_SOURCE_DIR}")
target_compile_features(arpg_raylib PUBLIC cxx_std_17)
target_link_libraries(arpg_raylib PRIVATE arpg_core raylib)

arpg_enable_project_warnings(arpg_raylib)
~~~

Create src/app/main.cpp:

~~~cpp
#include "raylib_host.hpp"

int main() {
    const arpg::platform::RaylibHostConfig config{};
    return static_cast<int>(arpg::platform::run_raylib_host(config));
}
~~~

Create src/app/CMakeLists.txt:

~~~cmake
add_executable(arpg_game main.cpp)

target_compile_features(arpg_game PRIVATE cxx_std_17)
target_link_libraries(arpg_game PRIVATE arpg_raylib)

arpg_enable_project_warnings(arpg_game)
~~~

- [ ] **Step 5: Add the lightweight test harness, first failing test, and architecture guard**

Create tests/core/test_framework.hpp:

~~~cpp
#pragma once

#include <cmath>
#include <cstddef>

namespace arpg::test {

struct Failure final {
    const char* expression{};
    const char* file{};
    int line{};
};

using TestFunction = Failure (*)() noexcept;

struct TestCase final {
    const char* name;
    TestFunction function;
};

struct TestSuite final {
    const char* name;
    const TestCase* cases;
    std::size_t count;
};

[[nodiscard]] inline bool near(
    double lhs,
    double rhs,
    double tolerance = 1.0e-12) noexcept {
    return std::fabs(lhs - rhs) <= tolerance;
}

}  // namespace arpg::test

#define ARPG_REQUIRE(expression)                                      \
    do {                                                              \
        if (!(expression)) {                                          \
            return ::arpg::test::Failure{#expression, __FILE__, __LINE__}; \
        }                                                             \
    } while (false)
~~~

Create tests/core/fixed_step_tests.cpp:

~~~cpp
#include "test_framework.hpp"

#include "core/fixed_step.hpp"

namespace {

arpg::test::Failure one_full_step() noexcept {
    arpg::core::FixedStepRunner runner;
    const auto frame =
        runner.advance(arpg::core::FixedStepRunner::kStepSeconds);

    ARPG_REQUIRE(frame.steps == 1);
    ARPG_REQUIRE(frame.total_ticks == 1);
    ARPG_REQUIRE(arpg::test::near(frame.interpolation_alpha, 0.0));
    return {};
}

arpg::test::Failure two_half_steps() noexcept {
    arpg::core::FixedStepRunner runner;
    const double half =
        arpg::core::FixedStepRunner::kStepSeconds * 0.5;

    const auto first = runner.advance(half);
    ARPG_REQUIRE(first.steps == 0);
    ARPG_REQUIRE(arpg::test::near(first.interpolation_alpha, 0.5));

    const auto second = runner.advance(half);
    ARPG_REQUIRE(second.steps == 1);
    ARPG_REQUIRE(second.total_ticks == 1);
    ARPG_REQUIRE(arpg::test::near(second.interpolation_alpha, 0.0));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"one full step", &one_full_step},
    {"two half steps", &two_half_steps},
};

}  // namespace

arpg::test::TestSuite fixed_step_suite() noexcept {
    return {
        "fixed_step",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
~~~

Create tests/core/test_main.cpp:

~~~cpp
#include "test_framework.hpp"

#include <cstdio>

arpg::test::TestSuite fixed_step_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        fixed_step_suite(),
    };

    int failures = 0;
    int checks = 0;
    for (const auto& suite : suites) {
        for (std::size_t index = 0; index < suite.count; ++index) {
            ++checks;
            const auto failure = suite.cases[index].function();
            if (failure.expression != nullptr) {
                ++failures;
                std::fprintf(
                    stderr,
                    "[FAIL] %s.%s: %s (%s:%d)\n",
                    suite.name,
                    suite.cases[index].name,
                    failure.expression,
                    failure.file,
                    failure.line);
            }
        }
    }

    std::printf("%d cases, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
~~~

Create tests/core/CMakeLists.txt:

~~~cmake
add_executable(arpg_core_tests
    test_main.cpp
    fixed_step_tests.cpp)

target_compile_features(arpg_core_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_core_tests PRIVATE arpg_core)
arpg_enable_project_warnings(arpg_core_tests)

add_test(NAME core.units COMMAND arpg_core_tests)
set_tests_properties(core.units PROPERTIES LABELS "headless;core")
~~~

Create tests/platform/core_boundary_test.cmake:

~~~cmake
if(NOT DEFINED CORE_DIR)
    message(FATAL_ERROR "CORE_DIR is required")
endif()

file(GLOB_RECURSE CORE_FILES
    LIST_DIRECTORIES FALSE
    "${CORE_DIR}/*.h"
    "${CORE_DIR}/*.hpp"
    "${CORE_DIR}/*.inl"
    "${CORE_DIR}/*.c"
    "${CORE_DIR}/*.cc"
    "${CORE_DIR}/*.cpp")

foreach(CORE_FILE IN LISTS CORE_FILES)
    file(READ "${CORE_FILE}" CORE_CONTENT)
    string(TOLOWER "${CORE_CONTENT}" CORE_CONTENT_LOWER)
    if(CORE_CONTENT_LOWER MATCHES
            "raylib\\.h|raymath\\.h|rlgl\\.h|raylib-cpp")
        message(FATAL_ERROR
            "Core file depends on raylib: ${CORE_FILE}")
    endif()
endforeach()
~~~

Create tests/platform/CMakeLists.txt:

~~~cmake
foreach(PROPERTY_NAME IN ITEMS
        LINK_LIBRARIES
        INTERFACE_LINK_LIBRARIES
        INCLUDE_DIRECTORIES
        INTERFACE_INCLUDE_DIRECTORIES)
    get_target_property(PROPERTY_VALUE arpg_core "${PROPERTY_NAME}")
    if(PROPERTY_VALUE)
        string(TOLOWER "${PROPERTY_VALUE}" PROPERTY_VALUE_LOWER)
        if(PROPERTY_VALUE_LOWER MATCHES "raylib|_deps[/\\\\]raylib")
            message(FATAL_ERROR
                "arpg_core ${PROPERTY_NAME} leaks raylib: ${PROPERTY_VALUE}")
        endif()
    endif()
endforeach()

add_test(
    NAME architecture.core_no_raylib
    COMMAND "${CMAKE_COMMAND}"
        "-DCORE_DIR=${PROJECT_SOURCE_DIR}/src/core"
        -P "${CMAKE_CURRENT_SOURCE_DIR}/core_boundary_test.cmake")
set_tests_properties(
    architecture.core_no_raylib
    PROPERTIES LABELS "headless;architecture")
~~~

- [ ] **Step 6: Run the first test and verify the red state**

Run from E:\game\.worktrees\m00-foundation:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
~~~

Expected: configuration downloads and validates the pinned raylib archive, compilation succeeds, and linking fails because FixedStepRunner::advance(double) has no definition.

- [ ] **Step 7: Implement the minimum fixed-step behavior**

Replace src/core/fixed_step.cpp with:

~~~cpp
#include "core/fixed_step.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::core {

FixedStepFrame FixedStepRunner::advance(double frame_seconds) noexcept {
    if (std::isfinite(frame_seconds) && frame_seconds >= 0.0) {
        accumulator_seconds_ += frame_seconds;
    }

    std::uint32_t steps = 0;
    if (accumulator_seconds_ >= kStepSeconds) {
        accumulator_seconds_ -= kStepSeconds;
        ++steps;
        ++total_ticks_;
    }

    const double alpha = std::clamp(
        accumulator_seconds_ / kStepSeconds,
        0.0,
        1.0);
    return {
        steps,
        total_ticks_,
        alpha,
        dropped_seconds_,
        invalid_input_count_,
    };
}

}  // namespace arpg::core
~~~

- [ ] **Step 8: Verify the green baseline in graphics and core-only modes**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
~~~

Expected for both commands: core.units and architecture.core_no_raylib pass; CTest reports 100% tests passed, 0 failed.

- [ ] **Step 9: Check and commit the integration baseline**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation diff --check
git -c safe.directory=E:/game/.worktrees/m00-foundation status --short
git -c safe.directory=E:/game/.worktrees/m00-foundation add CMakeLists.txt CMakePresets.json cmake scripts src tests
git -c safe.directory=E:/game/.worktrees/m00-foundation -c user.name=Codex -c user.email=codex@local commit -m "feat: establish stage 0 build baseline"
~~~

Expected: one commit containing only the listed Stage 0 baseline files; out/ remains ignored.

---

### Task 2: Complete and Freeze Fixed-Step Semantics

**Worktree:** E:\game\.worktrees\m00-foundation

**Files:**
- Modify: src/core/fixed_step.cpp
- Modify: tests/core/fixed_step_tests.cpp

**Interfaces:**
- Consumes and freezes: arpg::core::FixedStepRunner::advance(double) noexcept.
- Produces: exact 60 Hz partition invariance, 8-step cap, accumulated dropped seconds, invalid-input count, and alpha in [0, 1).
- Host worktree may consume FixedStepFrame after this task; later core tasks must not alter it.

- [ ] **Step 1: Replace the basic tests with complete fixed-step acceptance tests**

Replace tests/core/fixed_step_tests.cpp with:

~~~cpp
#include "test_framework.hpp"

#include "core/fixed_step.hpp"

#include <cmath>
#include <limits>

namespace {

using arpg::core::FixedStepRunner;

arpg::test::Failure one_full_step() noexcept {
    FixedStepRunner runner;
    const auto frame = runner.advance(FixedStepRunner::kStepSeconds);
    ARPG_REQUIRE(frame.steps == 1);
    ARPG_REQUIRE(frame.total_ticks == 1);
    ARPG_REQUIRE(arpg::test::near(frame.interpolation_alpha, 0.0));
    return {};
}

arpg::test::Failure two_half_steps() noexcept {
    FixedStepRunner runner;
    const double half = FixedStepRunner::kStepSeconds * 0.5;
    const auto first = runner.advance(half);
    ARPG_REQUIRE(first.steps == 0);
    ARPG_REQUIRE(arpg::test::near(first.interpolation_alpha, 0.5));

    const auto second = runner.advance(half);
    ARPG_REQUIRE(second.steps == 1);
    ARPG_REQUIRE(second.total_ticks == 1);
    ARPG_REQUIRE(arpg::test::near(second.interpolation_alpha, 0.0));
    return {};
}

arpg::test::Failure frame_partitioning_is_stable() noexcept {
    FixedStepRunner sixty_fps;
    FixedStepRunner thirty_fps;
    FixedStepRunner mixed;

    for (int index = 0; index < 60; ++index) {
        static_cast<void>(sixty_fps.advance(FixedStepRunner::kStepSeconds));
    }
    for (int index = 0; index < 30; ++index) {
        static_cast<void>(
            thirty_fps.advance(FixedStepRunner::kStepSeconds * 2.0));
    }
    for (int index = 0; index < 6; ++index) {
        static_cast<void>(mixed.advance(FixedStepRunner::kStepSeconds));
        static_cast<void>(
            mixed.advance(FixedStepRunner::kStepSeconds * 2.0));
        static_cast<void>(
            mixed.advance(FixedStepRunner::kStepSeconds * 3.0));
        static_cast<void>(
            mixed.advance(FixedStepRunner::kStepSeconds * 4.0));
    }

    const auto sixty = sixty_fps.advance(0.0);
    const auto thirty = thirty_fps.advance(0.0);
    const auto varied = mixed.advance(0.0);
    ARPG_REQUIRE(sixty.total_ticks == 60);
    ARPG_REQUIRE(thirty.total_ticks == 60);
    ARPG_REQUIRE(varied.total_ticks == 60);
    ARPG_REQUIRE(arpg::test::near(sixty.dropped_seconds, 0.0));
    ARPG_REQUIRE(arpg::test::near(thirty.dropped_seconds, 0.0));
    ARPG_REQUIRE(arpg::test::near(varied.dropped_seconds, 0.0));
    return {};
}

arpg::test::Failure cap_and_fraction_are_exact() noexcept {
    FixedStepRunner exact_cap;
    const auto eight = exact_cap.advance(
        FixedStepRunner::kStepSeconds * 8.0);
    ARPG_REQUIRE(eight.steps == 8);
    ARPG_REQUIRE(arpg::test::near(eight.dropped_seconds, 0.0));
    ARPG_REQUIRE(arpg::test::near(eight.interpolation_alpha, 0.0));

    FixedStepRunner over_cap;
    const auto ten_and_half = over_cap.advance(
        FixedStepRunner::kStepSeconds * 10.5);
    ARPG_REQUIRE(ten_and_half.steps == 8);
    ARPG_REQUIRE(
        arpg::test::near(
            ten_and_half.dropped_seconds,
            FixedStepRunner::kStepSeconds * 2.0));
    ARPG_REQUIRE(
        arpg::test::near(ten_and_half.interpolation_alpha, 0.5));

    FixedStepRunner nine_steps;
    const auto nine = nine_steps.advance(
        FixedStepRunner::kStepSeconds * 9.0);
    ARPG_REQUIRE(nine.steps == 8);
    ARPG_REQUIRE(
        arpg::test::near(
            nine.dropped_seconds,
            FixedStepRunner::kStepSeconds));
    ARPG_REQUIRE(arpg::test::near(nine.interpolation_alpha, 0.0));
    return {};
}

arpg::test::Failure one_second_drops_fifty_two_steps() noexcept {
    FixedStepRunner runner;
    const auto frame = runner.advance(1.0);
    ARPG_REQUIRE(frame.steps == 8);
    ARPG_REQUIRE(frame.total_ticks == 8);
    ARPG_REQUIRE(
        arpg::test::near(
            frame.dropped_seconds,
            FixedStepRunner::kStepSeconds * 52.0));
    ARPG_REQUIRE(arpg::test::near(frame.interpolation_alpha, 0.0));
    return {};
}

arpg::test::Failure invalid_input_preserves_accumulator() noexcept {
    FixedStepRunner runner;
    const auto half =
        runner.advance(FixedStepRunner::kStepSeconds * 0.5);
    ARPG_REQUIRE(arpg::test::near(half.interpolation_alpha, 0.5));

    static_cast<void>(runner.advance(-1.0));
    static_cast<void>(
        runner.advance(std::numeric_limits<double>::infinity()));
    static_cast<void>(
        runner.advance(-std::numeric_limits<double>::infinity()));
    const auto invalid =
        runner.advance(std::numeric_limits<double>::quiet_NaN());

    ARPG_REQUIRE(invalid.steps == 0);
    ARPG_REQUIRE(invalid.total_ticks == 0);
    ARPG_REQUIRE(invalid.invalid_input_count == 4);
    ARPG_REQUIRE(
        arpg::test::near(invalid.interpolation_alpha, 0.5));

    const auto negative_zero = runner.advance(-0.0);
    ARPG_REQUIRE(negative_zero.invalid_input_count == 4);
    return {};
}

arpg::test::Failure huge_finite_input_is_bounded() noexcept {
    FixedStepRunner runner;
    const auto frame =
        runner.advance(std::numeric_limits<double>::max());
    ARPG_REQUIRE(frame.steps == FixedStepRunner::kMaxStepsPerFrame);
    ARPG_REQUIRE(frame.invalid_input_count == 0);
    ARPG_REQUIRE(std::isfinite(frame.dropped_seconds));
    ARPG_REQUIRE(frame.interpolation_alpha >= 0.0);
    ARPG_REQUIRE(frame.interpolation_alpha < 1.0);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"one full step", &one_full_step},
    {"two half steps", &two_half_steps},
    {"frame partitioning", &frame_partitioning_is_stable},
    {"cap and fraction", &cap_and_fraction_are_exact},
    {"one second regression", &one_second_drops_fifty_two_steps},
    {"invalid input", &invalid_input_preserves_accumulator},
    {"huge finite input", &huge_finite_input_is_bounded},
};

}  // namespace

arpg::test::TestSuite fixed_step_suite() noexcept {
    return {
        "fixed_step",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
~~~

- [ ] **Step 2: Run the expanded suite and verify the red state**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
~~~

Expected: core.units fails. The minimum implementation executes only one step per call, does not count invalid inputs, and does not record dropped time.

- [ ] **Step 3: Implement bounded fixed-step accumulation**

Replace src/core/fixed_step.cpp with:

~~~cpp
#include "core/fixed_step.hpp"

#include <algorithm>
#include <cmath>
#include <limits>

namespace arpg::core {
namespace {

constexpr double kToleranceFactor =
    8.0 * std::numeric_limits<double>::epsilon();

[[nodiscard]] double saturating_add(
    double current,
    double addition) noexcept {
    const double maximum = std::numeric_limits<double>::max();
    if (!std::isfinite(addition) || current > maximum - addition) {
        return maximum;
    }
    return current + addition;
}

[[nodiscard]] double snap_near_integer(double value) noexcept {
    const double nearest = std::nearbyint(value);
    const double tolerance =
        kToleranceFactor * std::max(1.0, std::fabs(value));
    return std::fabs(value - nearest) <= tolerance
        ? nearest
        : value;
}

}  // namespace

FixedStepFrame FixedStepRunner::advance(double frame_seconds) noexcept {
    if (!std::isfinite(frame_seconds) || frame_seconds < 0.0) {
        ++invalid_input_count_;
        return {
            0,
            total_ticks_,
            std::clamp(
                accumulator_seconds_ / kStepSeconds,
                0.0,
                std::nextafter(1.0, 0.0)),
            dropped_seconds_,
            invalid_input_count_,
        };
    }

    accumulator_seconds_ += frame_seconds;
    std::uint32_t steps = 0;

    while (steps < kMaxStepsPerFrame) {
        const double tolerance =
            kToleranceFactor *
            std::max(kStepSeconds, std::fabs(accumulator_seconds_));
        if (accumulator_seconds_ + tolerance < kStepSeconds) {
            break;
        }

        accumulator_seconds_ -= kStepSeconds;
        if (accumulator_seconds_ < 0.0 &&
            std::fabs(accumulator_seconds_) <= tolerance) {
            accumulator_seconds_ = 0.0;
        }
        ++steps;
        ++total_ticks_;
    }

    const double drop_tolerance =
        kToleranceFactor *
        std::max(kStepSeconds, std::fabs(accumulator_seconds_));
    if (steps == kMaxStepsPerFrame &&
        accumulator_seconds_ + drop_tolerance >= kStepSeconds) {
        const double remaining_steps =
            accumulator_seconds_ / kStepSeconds;
        double retained_seconds = 0.0;
        double dropped_now = 0.0;

        if (std::isfinite(remaining_steps)) {
            const double normalized =
                snap_near_integer(remaining_steps);
            const double whole_steps = std::floor(normalized);
            retained_seconds =
                (normalized - whole_steps) * kStepSeconds;
            dropped_now = whole_steps * kStepSeconds;
        } else if (std::isfinite(accumulator_seconds_)) {
            retained_seconds =
                std::fmod(accumulator_seconds_, kStepSeconds);
            dropped_now = accumulator_seconds_ - retained_seconds;
        } else {
            retained_seconds = 0.0;
            dropped_now = std::numeric_limits<double>::max();
        }

        const double remainder_tolerance =
            kToleranceFactor * kStepSeconds;
        if (retained_seconds >= kStepSeconds - remainder_tolerance) {
            retained_seconds = 0.0;
            dropped_now =
                saturating_add(dropped_now, kStepSeconds);
        }

        accumulator_seconds_ =
            std::clamp(
                retained_seconds,
                0.0,
                std::nextafter(kStepSeconds, 0.0));
        dropped_seconds_ =
            saturating_add(dropped_seconds_, dropped_now);
    }

    return {
        steps,
        total_ticks_,
        std::clamp(
            accumulator_seconds_ / kStepSeconds,
            0.0,
            std::nextafter(1.0, 0.0)),
        dropped_seconds_,
        invalid_input_count_,
    };
}

}  // namespace arpg::core
~~~

- [ ] **Step 4: Run core-only and full Debug tests**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\scripts\Test.ps1 -Preset windows-msvc-debug
~~~

Expected: seven fixed-step cases pass; both CTest presets report 100% tests passed.

- [ ] **Step 5: Commit the frozen timing contract**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation diff --check
git -c safe.directory=E:/game/.worktrees/m00-foundation add src/core/fixed_step.cpp tests/core/fixed_step_tests.cpp
git -c safe.directory=E:/game/.worktrees/m00-foundation -c user.name=Codex -c user.email=codex@local commit -m "feat: harden fixed timestep runtime"
~~~

Expected: the integration branch is clean and FixedStepRunner/FixedStepFrame are now frozen for parallel work.

- [ ] **Step 6: Create the two task worktrees from the frozen integration commit**

Run from E:\game:

~~~powershell
git -c safe.directory=E:/game worktree add .worktrees/m00-core-runtime -b task/m00-core-runtime milestone/m00-foundation
git -c safe.directory=E:/game worktree add .worktrees/m00-raylib-host -b task/m00-raylib-host milestone/m00-foundation
git -c safe.directory=E:/game/.worktrees/m00-core-runtime -C .worktrees/m00-core-runtime status --short --branch
git -c safe.directory=E:/game/.worktrees/m00-raylib-host -C .worktrees/m00-raylib-host status --short --branch
~~~

Expected: both task worktrees are clean and point to the same commit as milestone/m00-foundation.

---

### Task 3: Deterministic Random Streams

**Worktree:** E:\game\.worktrees\m00-core-runtime

**Files:**
- Create: src/core/deterministic_rng.hpp
- Create: src/core/deterministic_rng.cpp
- Create: tests/core/deterministic_rng_tests.cpp
- Modify: src/core/CMakeLists.txt
- Modify: tests/core/CMakeLists.txt
- Modify: tests/core/test_main.cpp

**Interfaces:**
- Produces: arpg::core::DeterministicRng::DeterministicRng(std::uint64_t) noexcept.
- Produces: std::uint64_t DeterministicRng::next_u64() noexcept.
- Produces: static DeterministicRng DeterministicRng::derive_stream(std::uint64_t root_seed, std::uint64_t stream_id) noexcept.
- Uses no standard-library random engine or distribution.

- [ ] **Step 1: Add the RNG interface and golden tests in a red state**

Create src/core/deterministic_rng.hpp:

~~~cpp
#pragma once

#include <array>
#include <cstdint>

namespace arpg::core {

class DeterministicRng final {
public:
    explicit DeterministicRng(std::uint64_t seed) noexcept;

    [[nodiscard]] std::uint64_t next_u64() noexcept;

    [[nodiscard]] static DeterministicRng derive_stream(
        std::uint64_t root_seed,
        std::uint64_t stream_id) noexcept;

private:
    std::array<std::uint64_t, 4> state_{};
};

}  // namespace arpg::core
~~~

Create src/core/deterministic_rng.cpp in the red state:

~~~cpp
#include "core/deterministic_rng.hpp"
~~~

Create tests/core/deterministic_rng_tests.cpp:

~~~cpp
#include "test_framework.hpp"

#include "core/deterministic_rng.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <limits>

namespace {

using arpg::core::DeterministicRng;

template <std::size_t Count>
arpg::test::Failure require_sequence(
    DeterministicRng& rng,
    const std::array<std::uint64_t, Count>& expected) noexcept {
    for (const auto value : expected) {
        ARPG_REQUIRE(rng.next_u64() == value);
    }
    return {};
}

arpg::test::Failure seed_zero_golden_sequence() noexcept {
    DeterministicRng rng{0};
    constexpr std::array<std::uint64_t, 6> expected = {
        0x99EC5F36CB75F2B4ULL,
        0xBF6E1F784956452AULL,
        0x1A5F849D4933E6E0ULL,
        0x6AA594F1262D2D2CULL,
        0xBBA5AD4A1F842E59ULL,
        0xFFEF8375D9EBCACAULL,
    };
    return require_sequence(rng, expected);
}

arpg::test::Failure seed_one_golden_sequence() noexcept {
    DeterministicRng rng{1};
    constexpr std::array<std::uint64_t, 4> expected = {
        0xB3F2AF6D0FC710C5ULL,
        0x853B559647364CEAULL,
        0x92F89756082A4514ULL,
        0x642E1C7BC266A3A7ULL,
    };
    return require_sequence(rng, expected);
}

arpg::test::Failure identical_seeds_remain_identical() noexcept {
    DeterministicRng lhs{0xA55AA55A12345678ULL};
    DeterministicRng rhs{0xA55AA55A12345678ULL};
    for (int index = 0; index < 1000; ++index) {
        ARPG_REQUIRE(lhs.next_u64() == rhs.next_u64());
    }
    return {};
}

arpg::test::Failure derived_stream_golden_sequences() noexcept {
    constexpr std::uint64_t root = 0x0123456789ABCDEFULL;

    auto stream_zero = DeterministicRng::derive_stream(root, 0);
    constexpr std::array<std::uint64_t, 4> expected_zero = {
        0xDBA706AE738CF8D8ULL,
        0x11379A5E6A1305B6ULL,
        0xEF5DD13D35E639B8ULL,
        0x47A7F4426A216B33ULL,
    };
    auto failure = require_sequence(stream_zero, expected_zero);
    ARPG_REQUIRE(failure.expression == nullptr);

    auto stream_one = DeterministicRng::derive_stream(root, 1);
    constexpr std::array<std::uint64_t, 4> expected_one = {
        0x4D5D904D32A74EEEULL,
        0xD6AD6D7AF502CF07ULL,
        0x4565597487C3FB08ULL,
        0x97756A448E7CD5D9ULL,
    };
    failure = require_sequence(stream_one, expected_one);
    ARPG_REQUIRE(failure.expression == nullptr);

    auto stream_max = DeterministicRng::derive_stream(
        root,
        std::numeric_limits<std::uint64_t>::max());
    constexpr std::array<std::uint64_t, 4> expected_max = {
        0x95E14F3C01B793E8ULL,
        0x13CA973572CEBA9FULL,
        0x59AA56671F0B3124ULL,
        0xC7A20ADE941E50BFULL,
    };
    failure = require_sequence(stream_max, expected_max);
    ARPG_REQUIRE(failure.expression == nullptr);
    return {};
}

arpg::test::Failure streams_are_isolated() noexcept {
    constexpr std::uint64_t root = 0x0123456789ABCDEFULL;
    auto advanced_zero = DeterministicRng::derive_stream(root, 0);
    for (int index = 0; index < 100; ++index) {
        static_cast<void>(advanced_zero.next_u64());
    }

    auto stream_one_after = DeterministicRng::derive_stream(root, 1);
    auto stream_one_fresh = DeterministicRng::derive_stream(root, 1);
    for (int index = 0; index < 64; ++index) {
        ARPG_REQUIRE(
            stream_one_after.next_u64() ==
            stream_one_fresh.next_u64());
    }
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"seed zero golden", &seed_zero_golden_sequence},
    {"seed one golden", &seed_one_golden_sequence},
    {"identical seeds", &identical_seeds_remain_identical},
    {"derived streams golden", &derived_stream_golden_sequences},
    {"stream isolation", &streams_are_isolated},
};

}  // namespace

arpg::test::TestSuite deterministic_rng_suite() noexcept {
    return {
        "deterministic_rng",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
~~~

Add deterministic_rng.cpp to src/core/CMakeLists.txt:

~~~cmake
add_library(arpg_core STATIC
    fixed_step.cpp
    deterministic_rng.cpp)
~~~

Replace tests/core/CMakeLists.txt with:

~~~cmake
add_executable(arpg_core_tests
    test_main.cpp
    fixed_step_tests.cpp
    deterministic_rng_tests.cpp)

target_compile_features(arpg_core_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_core_tests PRIVATE arpg_core)
arpg_enable_project_warnings(arpg_core_tests)

add_test(NAME core.units COMMAND arpg_core_tests)
set_tests_properties(core.units PROPERTIES LABELS "headless;core")
~~~

Replace tests/core/test_main.cpp with:

~~~cpp
#include "test_framework.hpp"

#include <cstdio>

arpg::test::TestSuite fixed_step_suite() noexcept;
arpg::test::TestSuite deterministic_rng_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        fixed_step_suite(),
        deterministic_rng_suite(),
    };

    int failures = 0;
    int checks = 0;
    for (const auto& suite : suites) {
        for (std::size_t index = 0; index < suite.count; ++index) {
            ++checks;
            const auto failure = suite.cases[index].function();
            if (failure.expression != nullptr) {
                ++failures;
                std::fprintf(
                    stderr,
                    "[FAIL] %s.%s: %s (%s:%d)\n",
                    suite.name,
                    suite.cases[index].name,
                    failure.expression,
                    failure.file,
                    failure.line);
            }
        }
    }

    std::printf("%d cases, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
~~~

- [ ] **Step 2: Run the RNG tests and verify the red state**

Run from E:\game\.worktrees\m00-core-runtime:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
~~~

Expected: linking fails because the DeterministicRng constructor, next_u64, and derive_stream are declared but not defined.

- [ ] **Step 3: Implement SplitMix64 initialization and xoshiro256\*\***

Replace src/core/deterministic_rng.cpp with:

~~~cpp
#include "core/deterministic_rng.hpp"

#include <cstdint>

namespace arpg::core {
namespace {

[[nodiscard]] std::uint64_t rotate_left(
    std::uint64_t value,
    unsigned int shift) noexcept {
    return (value << shift) | (value >> (64U - shift));
}

[[nodiscard]] std::uint64_t splitmix64_next(
    std::uint64_t& state) noexcept {
    state += 0x9E3779B97F4A7C15ULL;
    std::uint64_t value = state;
    value =
        (value ^ (value >> 30U)) * 0xBF58476D1CE4E5B9ULL;
    value =
        (value ^ (value >> 27U)) * 0x94D049BB133111EBULL;
    return value ^ (value >> 31U);
}

}  // namespace

DeterministicRng::DeterministicRng(std::uint64_t seed) noexcept {
    std::uint64_t splitmix_state = seed;
    for (auto& value : state_) {
        value = splitmix64_next(splitmix_state);
    }

    if ((state_[0] | state_[1] | state_[2] | state_[3]) == 0) {
        state_[0] = 0x9E3779B97F4A7C15ULL;
    }
}

std::uint64_t DeterministicRng::next_u64() noexcept {
    const std::uint64_t result =
        rotate_left(state_[1] * 5ULL, 7U) * 9ULL;
    const std::uint64_t shifted = state_[1] << 17U;

    state_[2] ^= state_[0];
    state_[3] ^= state_[1];
    state_[1] ^= state_[2];
    state_[0] ^= state_[3];
    state_[2] ^= shifted;
    state_[3] = rotate_left(state_[3], 45U);
    return result;
}

DeterministicRng DeterministicRng::derive_stream(
    std::uint64_t root_seed,
    std::uint64_t stream_id) noexcept {
    std::uint64_t id_state = stream_id;
    const std::uint64_t mixed_id = splitmix64_next(id_state);
    std::uint64_t outer_state = root_seed ^ mixed_id;
    const std::uint64_t child_seed =
        splitmix64_next(outer_state);
    return DeterministicRng{child_seed};
}

}  // namespace arpg::core
~~~

- [ ] **Step 4: Run all core tests**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
~~~

Expected: twelve cases pass across fixed_step and deterministic_rng; architecture.core_no_raylib also passes.

- [ ] **Step 5: Commit deterministic streams**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-core-runtime diff --check
git -c safe.directory=E:/game/.worktrees/m00-core-runtime add src/core tests/core
git -c safe.directory=E:/game/.worktrees/m00-core-runtime -c user.name=Codex -c user.email=codex@local commit -m "feat: add deterministic random streams"
~~~

Expected: task/m00-core-runtime is clean after one RNG commit.

---

### Task 4: Generational Fixed-Capacity Object Pool

**Worktree:** E:\game\.worktrees\m00-core-runtime

**Files:**
- Create: src/core/fixed_pool.hpp
- Create: tests/core/allocation_probe.hpp
- Create: tests/core/allocation_probe.cpp
- Create: tests/core/fixed_pool_tests.cpp
- Modify: tests/core/CMakeLists.txt
- Modify: tests/core/test_main.cpp

**Interfaces:**
- Produces: arpg::core::FixedPool<T, N>, Handle, try_emplace, release, get, contains, size, empty, full, and capacity.
- Capacity and all slots are compile-time fixed; Handle is a 32-bit index plus a 32-bit generation.
- Normal full, invalid, stale, and duplicate-release cases return failure without throwing or asserting.

- [ ] **Step 1: Add the allocation probe and failing pool tests**

Create tests/core/allocation_probe.hpp:

~~~cpp
#pragma once

#include <cstdint>

namespace arpg::test {

[[nodiscard]] std::uint64_t allocation_count() noexcept;

}  // namespace arpg::test
~~~

Create tests/core/allocation_probe.cpp:

~~~cpp
#include "allocation_probe.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {

std::atomic<std::uint64_t> g_allocations{0};

[[nodiscard]] void* allocate_unaligned(std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc{};
}

[[nodiscard]] void* allocate_aligned(
    std::size_t size,
    std::size_t alignment) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
#if defined(_MSC_VER)
    if (void* memory =
            _aligned_malloc(size == 0 ? 1 : size, alignment)) {
        return memory;
    }
#else
    void* memory = nullptr;
    if (posix_memalign(
            &memory,
            alignment,
            size == 0 ? 1 : size) == 0) {
        return memory;
    }
#endif
    throw std::bad_alloc{};
}

void free_aligned(void* memory) noexcept {
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

}  // namespace

namespace arpg::test {

std::uint64_t allocation_count() noexcept {
    return g_allocations.load(std::memory_order_relaxed);
}

}  // namespace arpg::test

void* operator new(std::size_t size) {
    return allocate_unaligned(size);
}

void* operator new[](std::size_t size) {
    return allocate_unaligned(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

void* operator new(
    std::size_t size,
    const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](
    std::size_t size,
    const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](size);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(
    void* memory,
    const std::nothrow_t&) noexcept {
    ::operator delete(memory);
}

void operator delete[](
    void* memory,
    const std::nothrow_t&) noexcept {
    ::operator delete[](memory);
}

void* operator new(
    std::size_t size,
    std::align_val_t alignment) {
    return allocate_aligned(
        size,
        static_cast<std::size_t>(alignment));
}

void* operator new[](
    std::size_t size,
    std::align_val_t alignment) {
    return allocate_aligned(
        size,
        static_cast<std::size_t>(alignment));
}

void operator delete(
    void* memory,
    std::align_val_t) noexcept {
    free_aligned(memory);
}

void operator delete[](
    void* memory,
    std::align_val_t) noexcept {
    free_aligned(memory);
}

void operator delete(
    void* memory,
    std::size_t,
    std::align_val_t) noexcept {
    free_aligned(memory);
}

void operator delete[](
    void* memory,
    std::size_t,
    std::align_val_t) noexcept {
    free_aligned(memory);
}

void* operator new(
    std::size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size, alignment);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](
    std::size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](size, alignment);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(
    void* memory,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    ::operator delete(memory, alignment);
}

void operator delete[](
    void* memory,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    ::operator delete[](memory, alignment);
}
~~~

Create tests/core/fixed_pool_tests.cpp:

~~~cpp
#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "core/fixed_pool.hpp"

#include <cstdint>
#include <utility>

namespace {

struct Item final {
    explicit Item(int initial) noexcept : value(initial) {}
    int value;
};

struct Tracked final {
    explicit Tracked(int* destroyed_count) noexcept
        : destroyed(destroyed_count) {}

    ~Tracked() noexcept {
        ++(*destroyed);
    }

    int* destroyed;
};

arpg::test::Failure initial_state_and_capacity() noexcept {
    arpg::core::FixedPool<Item, 2> pool;
    ARPG_REQUIRE(pool.empty());
    ARPG_REQUIRE(!pool.full());
    ARPG_REQUIRE(pool.size() == 0);
    ARPG_REQUIRE(pool.capacity() == 2);
    return {};
}

arpg::test::Failure full_pool_preserves_objects() noexcept {
    arpg::core::FixedPool<Item, 2> pool;
    const auto first = pool.try_emplace(10);
    const auto second = pool.try_emplace(20);
    const auto rejected = pool.try_emplace(30);

    ARPG_REQUIRE(first.has_value());
    ARPG_REQUIRE(second.has_value());
    ARPG_REQUIRE(!rejected.has_value());
    ARPG_REQUIRE(pool.full());
    ARPG_REQUIRE(pool.get(*first)->value == 10);
    ARPG_REQUIRE(pool.get(*second)->value == 20);

    const auto& const_pool = pool;
    ARPG_REQUIRE(const_pool.get(*first)->value == 10);
    return {};
}

arpg::test::Failure stale_and_invalid_handles_fail() noexcept {
    using Pool = arpg::core::FixedPool<Item, 1>;
    Pool pool;
    const auto old_handle = pool.try_emplace(7);
    ARPG_REQUIRE(old_handle.has_value());
    ARPG_REQUIRE(pool.release(*old_handle));
    ARPG_REQUIRE(!pool.release(*old_handle));

    const auto fresh_handle = pool.try_emplace(9);
    ARPG_REQUIRE(fresh_handle.has_value());
    ARPG_REQUIRE(fresh_handle->index == old_handle->index);
    ARPG_REQUIRE(
        fresh_handle->generation != old_handle->generation);
    ARPG_REQUIRE(pool.get(*old_handle) == nullptr);
    ARPG_REQUIRE(!pool.contains(*old_handle));

    const Pool::Handle out_of_range{99, 1};
    const Pool::Handle zero_generation{0, 0};
    ARPG_REQUIRE(pool.get(out_of_range) == nullptr);
    ARPG_REQUIRE(!pool.release(out_of_range));
    ARPG_REQUIRE(pool.get(zero_generation) == nullptr);
    return {};
}

arpg::test::Failure values_are_destroyed_once() noexcept {
    int destroyed = 0;
    {
        arpg::core::FixedPool<Tracked, 2> pool;
        const auto first = pool.try_emplace(&destroyed);
        const auto second = pool.try_emplace(&destroyed);
        ARPG_REQUIRE(first.has_value());
        ARPG_REQUIRE(second.has_value());
        ARPG_REQUIRE(pool.release(*first));
        ARPG_REQUIRE(destroyed == 1);
    }
    ARPG_REQUIRE(destroyed == 2);
    return {};
}

arpg::test::Failure pressure_loop_has_no_allocations() noexcept {
    std::uint64_t checksum = 0;
    const auto before = arpg::test::allocation_count();
    {
        arpg::core::FixedPool<Item, 4> pool;
        for (int value = 0; value < 10000; ++value) {
            const auto handle = pool.try_emplace(value);
            ARPG_REQUIRE(handle.has_value());
            checksum +=
                static_cast<std::uint64_t>(pool.get(*handle)->value);
            ARPG_REQUIRE(pool.release(*handle));
        }
    }
    const auto after = arpg::test::allocation_count();

    ARPG_REQUIRE(checksum == 49995000ULL);
    ARPG_REQUIRE(after == before);
    return {};
}

static_assert(
    noexcept(
        std::declval<arpg::core::FixedPool<Item, 1>&>()
            .try_emplace(1)));

constexpr arpg::test::TestCase kCases[] = {
    {"initial state", &initial_state_and_capacity},
    {"full pool", &full_pool_preserves_objects},
    {"stale handles", &stale_and_invalid_handles_fail},
    {"destruction", &values_are_destroyed_once},
    {"no allocations", &pressure_loop_has_no_allocations},
};

}  // namespace

arpg::test::TestSuite fixed_pool_suite() noexcept {
    return {
        "fixed_pool",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
~~~

Replace tests/core/CMakeLists.txt with:

~~~cmake
add_executable(arpg_core_tests
    test_main.cpp
    allocation_probe.cpp
    fixed_step_tests.cpp
    deterministic_rng_tests.cpp
    fixed_pool_tests.cpp)

target_compile_features(arpg_core_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_core_tests PRIVATE arpg_core)
arpg_enable_project_warnings(arpg_core_tests)

add_test(NAME core.units COMMAND arpg_core_tests)
set_tests_properties(core.units PROPERTIES LABELS "headless;core")
~~~

Replace tests/core/test_main.cpp with:

~~~cpp
#include "test_framework.hpp"

#include <cstdio>

arpg::test::TestSuite fixed_step_suite() noexcept;
arpg::test::TestSuite deterministic_rng_suite() noexcept;
arpg::test::TestSuite fixed_pool_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        fixed_step_suite(),
        deterministic_rng_suite(),
        fixed_pool_suite(),
    };

    int failures = 0;
    int checks = 0;
    for (const auto& suite : suites) {
        for (std::size_t index = 0; index < suite.count; ++index) {
            ++checks;
            const auto failure = suite.cases[index].function();
            if (failure.expression != nullptr) {
                ++failures;
                std::fprintf(
                    stderr,
                    "[FAIL] %s.%s: %s (%s:%d)\n",
                    suite.name,
                    suite.cases[index].name,
                    failure.expression,
                    failure.file,
                    failure.line);
            }
        }
    }

    std::printf("%d cases, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
~~~

- [ ] **Step 2: Build and verify the missing pool red state**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
~~~

Expected: compilation fails because core/fixed_pool.hpp does not exist.

- [ ] **Step 3: Implement the fixed pool**

Create src/core/fixed_pool.hpp:

~~~cpp
#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace arpg::core {

template <typename T, std::size_t N>
class FixedPool final {
public:
    using index_type = std::uint32_t;
    using generation_type = std::uint32_t;

    static constexpr index_type invalid_index =
        std::numeric_limits<index_type>::max();

    struct Handle final {
        index_type index{invalid_index};
        generation_type generation{0};

        friend constexpr bool operator==(
            Handle lhs,
            Handle rhs) noexcept {
            return lhs.index == rhs.index &&
                lhs.generation == rhs.generation;
        }

        friend constexpr bool operator!=(
            Handle lhs,
            Handle rhs) noexcept {
            return !(lhs == rhs);
        }
    };

    FixedPool() noexcept {
        static_assert(N > 0, "FixedPool capacity must be positive");
        static_assert(
            N <= std::numeric_limits<index_type>::max(),
            "FixedPool capacity exceeds index range");
        static_assert(
            std::is_nothrow_destructible_v<T>,
            "FixedPool values must be nothrow destructible");

        for (std::size_t index = 0; index < N; ++index) {
            slots_[index].next_free =
                index + 1 < N
                ? static_cast<index_type>(index + 1)
                : invalid_index;
        }
    }

    ~FixedPool() noexcept = default;

    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;
    FixedPool(FixedPool&&) = delete;
    FixedPool& operator=(FixedPool&&) = delete;

    template <
        typename... Args,
        std::enable_if_t<
            std::is_nothrow_constructible_v<T, Args...>,
            int> = 0>
    [[nodiscard]] std::optional<Handle> try_emplace(
        Args&&... args) noexcept {
        if (free_head_ == invalid_index) {
            return std::nullopt;
        }

        assert(free_head_ < N);
        const index_type index = free_head_;
        Slot& slot = slots_[index];
        free_head_ = slot.next_free;
        slot.next_free = invalid_index;
        slot.value.emplace(std::forward<Args>(args)...);
        ++size_;
        assert(size_ <= N);
        return Handle{index, slot.generation};
    }

    [[nodiscard]] bool release(Handle handle) noexcept {
        if (!contains(handle)) {
            return false;
        }

        Slot& slot = slots_[handle.index];
        slot.value.reset();
        ++slot.generation;
        if (slot.generation == 0) {
            ++slot.generation;
        }
        slot.next_free = free_head_;
        free_head_ = handle.index;
        --size_;
        assert(size_ <= N);
        return true;
    }

    [[nodiscard]] T* get(Handle handle) noexcept {
        return contains(handle)
            ? &(*slots_[handle.index].value)
            : nullptr;
    }

    [[nodiscard]] const T* get(Handle handle) const noexcept {
        return contains(handle)
            ? &(*slots_[handle.index].value)
            : nullptr;
    }

    [[nodiscard]] bool contains(Handle handle) const noexcept {
        return handle.index < N &&
            handle.generation != 0 &&
            slots_[handle.index].generation == handle.generation &&
            slots_[handle.index].value.has_value();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return size_ == 0;
    }

    [[nodiscard]] bool full() const noexcept {
        return size_ == N;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return N;
    }

private:
    struct Slot final {
        std::optional<T> value;
        generation_type generation{1};
        index_type next_free{invalid_index};
    };

    std::array<Slot, N> slots_{};
    index_type free_head_{0};
    std::size_t size_{0};
};

}  // namespace arpg::core
~~~

- [ ] **Step 4: Run pool behavior and allocation tests**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
~~~

Expected: seventeen cases pass across fixed_step, deterministic_rng, and fixed_pool; the allocation-pressure case reports no failure.

- [ ] **Step 5: Commit the pool**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-core-runtime diff --check
git -c safe.directory=E:/game/.worktrees/m00-core-runtime add src/core/fixed_pool.hpp tests/core
git -c safe.directory=E:/game/.worktrees/m00-core-runtime -c user.name=Codex -c user.email=codex@local commit -m "feat: add generational fixed pool"
~~~

Expected: task/m00-core-runtime is clean after the pool commit.

---

### Task 5: Fixed-Capacity Bounded Queue

**Worktree:** E:\game\.worktrees\m00-core-runtime

**Files:**
- Create: src/core/bounded_queue.hpp
- Create: tests/core/bounded_queue_tests.cpp
- Modify: tests/core/CMakeLists.txt
- Modify: tests/core/test_main.cpp

**Interfaces:**
- Produces: arpg::core::BoundedQueue<T, N>, try_emplace, try_push, try_pop, front, size, empty, full, and capacity.
- Queue storage is std::array<std::optional<T>, N>; normal empty/full results use bool, nullptr, or std::nullopt.
- No overwrite, blocking, dynamic growth, or mutable front pointer is exposed.

- [ ] **Step 1: Add failing queue behavior and pressure tests**

Create tests/core/bounded_queue_tests.cpp:

~~~cpp
#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "core/bounded_queue.hpp"

#include <cstdint>
#include <utility>

namespace {

struct ConstructedProbe final {
    ConstructedProbe(
        int initial,
        int* construction_count) noexcept
        : value(initial),
          constructions(construction_count) {
        ++(*constructions);
    }

    ConstructedProbe(const ConstructedProbe& other) noexcept
        : value(other.value),
          constructions(other.constructions) {
        ++(*constructions);
    }

    ConstructedProbe(ConstructedProbe&& other) noexcept
        : value(other.value),
          constructions(other.constructions) {
        ++(*constructions);
    }

    int value;
    int* constructions;
};

struct DestructionProbe final {
    explicit DestructionProbe(int* count) noexcept
        : destroyed(count) {}

    DestructionProbe(DestructionProbe&& other) noexcept
        : destroyed(other.destroyed) {
        other.destroyed = nullptr;
    }

    ~DestructionProbe() noexcept {
        if (destroyed != nullptr) {
            ++(*destroyed);
        }
    }

    int* destroyed;
};

arpg::test::Failure empty_queue_fails_cleanly() noexcept {
    arpg::core::BoundedQueue<int, 3> queue;
    ARPG_REQUIRE(queue.empty());
    ARPG_REQUIRE(!queue.full());
    ARPG_REQUIRE(queue.front() == nullptr);
    ARPG_REQUIRE(!queue.try_pop().has_value());
    ARPG_REQUIRE(queue.capacity() == 3);
    return {};
}

arpg::test::Failure full_queue_does_not_construct() noexcept {
    int constructions = 0;
    arpg::core::BoundedQueue<ConstructedProbe, 2> queue;
    ARPG_REQUIRE(queue.try_emplace(10, &constructions));
    ARPG_REQUIRE(queue.try_emplace(20, &constructions));
    ARPG_REQUIRE(constructions == 2);
    ARPG_REQUIRE(!queue.try_emplace(30, &constructions));
    ARPG_REQUIRE(constructions == 2);
    ARPG_REQUIRE(queue.full());
    ARPG_REQUIRE(queue.front()->value == 10);
    return {};
}

arpg::test::Failure queue_is_fifo_and_wraps() noexcept {
    arpg::core::BoundedQueue<int, 3> queue;
    ARPG_REQUIRE(queue.try_push(1));
    ARPG_REQUIRE(queue.try_push(2));
    ARPG_REQUIRE(queue.try_push(3));

    ARPG_REQUIRE(queue.try_pop().value() == 1);
    ARPG_REQUIRE(queue.try_pop().value() == 2);
    ARPG_REQUIRE(queue.try_push(4));
    ARPG_REQUIRE(queue.try_push(5));

    ARPG_REQUIRE(queue.try_pop().value() == 3);
    ARPG_REQUIRE(queue.try_pop().value() == 4);
    ARPG_REQUIRE(queue.try_pop().value() == 5);
    ARPG_REQUIRE(queue.empty());
    return {};
}

arpg::test::Failure queued_values_are_destroyed() noexcept {
    int destroyed = 0;
    {
        arpg::core::BoundedQueue<DestructionProbe, 2> queue;
        ARPG_REQUIRE(queue.try_emplace(&destroyed));
        ARPG_REQUIRE(queue.try_emplace(&destroyed));
        ARPG_REQUIRE(destroyed == 0);
    }
    ARPG_REQUIRE(destroyed == 2);
    return {};
}

arpg::test::Failure pressure_loop_has_no_allocations() noexcept {
    std::uint64_t checksum = 0;
    const auto before = arpg::test::allocation_count();
    {
        arpg::core::BoundedQueue<int, 4> queue;
        for (int value = 0; value < 10000; ++value) {
            ARPG_REQUIRE(queue.try_push(value));
            const auto popped = queue.try_pop();
            ARPG_REQUIRE(popped.has_value());
            checksum += static_cast<std::uint64_t>(*popped);
        }
    }
    const auto after = arpg::test::allocation_count();

    ARPG_REQUIRE(checksum == 49995000ULL);
    ARPG_REQUIRE(after == before);
    return {};
}

static_assert(
    noexcept(
        std::declval<arpg::core::BoundedQueue<int, 2>&>()
            .try_push(1)));

constexpr arpg::test::TestCase kCases[] = {
    {"empty queue", &empty_queue_fails_cleanly},
    {"full queue", &full_queue_does_not_construct},
    {"fifo wrap", &queue_is_fifo_and_wraps},
    {"destruction", &queued_values_are_destroyed},
    {"no allocations", &pressure_loop_has_no_allocations},
};

}  // namespace

arpg::test::TestSuite bounded_queue_suite() noexcept {
    return {
        "bounded_queue",
        kCases,
        sizeof(kCases) / sizeof(kCases[0]),
    };
}
~~~

Replace tests/core/CMakeLists.txt with:

~~~cmake
add_executable(arpg_core_tests
    test_main.cpp
    allocation_probe.cpp
    fixed_step_tests.cpp
    deterministic_rng_tests.cpp
    fixed_pool_tests.cpp
    bounded_queue_tests.cpp)

target_compile_features(arpg_core_tests PRIVATE cxx_std_17)
target_link_libraries(arpg_core_tests PRIVATE arpg_core)
arpg_enable_project_warnings(arpg_core_tests)

add_test(NAME core.units COMMAND arpg_core_tests)
set_tests_properties(core.units PROPERTIES LABELS "headless;core")
~~~

Replace tests/core/test_main.cpp with:

~~~cpp
#include "test_framework.hpp"

#include <cstdio>

arpg::test::TestSuite fixed_step_suite() noexcept;
arpg::test::TestSuite deterministic_rng_suite() noexcept;
arpg::test::TestSuite fixed_pool_suite() noexcept;
arpg::test::TestSuite bounded_queue_suite() noexcept;

int main() {
    const arpg::test::TestSuite suites[] = {
        fixed_step_suite(),
        deterministic_rng_suite(),
        fixed_pool_suite(),
        bounded_queue_suite(),
    };

    int failures = 0;
    int checks = 0;
    for (const auto& suite : suites) {
        for (std::size_t index = 0; index < suite.count; ++index) {
            ++checks;
            const auto failure = suite.cases[index].function();
            if (failure.expression != nullptr) {
                ++failures;
                std::fprintf(
                    stderr,
                    "[FAIL] %s.%s: %s (%s:%d)\n",
                    suite.name,
                    suite.cases[index].name,
                    failure.expression,
                    failure.file,
                    failure.line);
            }
        }
    }

    std::printf("%d cases, %d failures\n", checks, failures);
    return failures == 0 ? 0 : 1;
}
~~~

- [ ] **Step 2: Build and verify the missing queue red state**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
~~~

Expected: compilation fails because core/bounded_queue.hpp does not exist.

- [ ] **Step 3: Implement the bounded ring queue**

Create src/core/bounded_queue.hpp:

~~~cpp
#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace arpg::core {

template <typename T, std::size_t N>
class BoundedQueue final {
public:
    static_assert(N > 0, "BoundedQueue capacity must be positive");
    static_assert(
        std::is_nothrow_move_constructible_v<T>,
        "BoundedQueue values must be nothrow move constructible");
    static_assert(
        std::is_nothrow_destructible_v<T>,
        "BoundedQueue values must be nothrow destructible");

    BoundedQueue() noexcept = default;
    ~BoundedQueue() noexcept = default;

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;
    BoundedQueue(BoundedQueue&&) = delete;
    BoundedQueue& operator=(BoundedQueue&&) = delete;

    template <
        typename... Args,
        std::enable_if_t<
            std::is_nothrow_constructible_v<T, Args...>,
            int> = 0>
    [[nodiscard]] bool try_emplace(Args&&... args) noexcept {
        if (full()) {
            return false;
        }

        slots_[tail_].emplace(std::forward<Args>(args)...);
        tail_ = (tail_ + 1) % N;
        ++size_;
        return true;
    }

    template <
        typename Value = T,
        std::enable_if_t<
            std::is_nothrow_copy_constructible_v<Value>,
            int> = 0>
    [[nodiscard]] bool try_push(const T& value) noexcept {
        return try_emplace(value);
    }

    [[nodiscard]] bool try_push(T&& value) noexcept {
        return try_emplace(std::move(value));
    }

    [[nodiscard]] std::optional<T> try_pop() noexcept {
        if (empty()) {
            return std::nullopt;
        }

        std::optional<T> result{
            std::move(*slots_[head_])};
        slots_[head_].reset();
        head_ = (head_ + 1) % N;
        --size_;
        return result;
    }

    [[nodiscard]] const T* front() const noexcept {
        return empty() ? nullptr : &(*slots_[head_]);
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return size_ == 0;
    }

    [[nodiscard]] bool full() const noexcept {
        return size_ == N;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return N;
    }

private:
    std::array<std::optional<T>, N> slots_{};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t size_{0};
};

}  // namespace arpg::core
~~~

- [ ] **Step 4: Run the complete core suite**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
~~~

Expected: twenty-two cases pass across all four core suites; architecture.core_no_raylib passes.

- [ ] **Step 5: Inspect allocation guarantees and commit**

Run:

~~~powershell
rg -n "malloc|calloc|realloc|new " src/core/fixed_pool.hpp src/core/bounded_queue.hpp
git -c safe.directory=E:/game/.worktrees/m00-core-runtime diff --check
git -c safe.directory=E:/game/.worktrees/m00-core-runtime add src/core/bounded_queue.hpp tests/core
git -c safe.directory=E:/game/.worktrees/m00-core-runtime -c user.name=Codex -c user.email=codex@local commit -m "feat: add bounded event queue"
~~~

Expected: rg finds no direct allocation calls; all changes are committed and task/m00-core-runtime is clean.

---

### Task 6: raylib 6.0 Graybox Host and Diagnostics HUD

**Worktree:** E:\game\.worktrees\m00-raylib-host

**Files:**
- Modify: src/platform/raylib/raylib_host.cpp

**Interfaces:**
- Consumes frozen RaylibHostConfig and FixedStepRunner from Task 2.
- Keeps raylib types private to raylib_host.cpp.
- Produces a 1280×720 resizable graybox window, 60 FPS render limit, 60 Hz fixed update, seven HUD diagnostics, Esc exit, and close-button exit.

- [ ] **Step 1: Run the baseline window and verify the visual red state**

Run:

~~~powershell
.\scripts\Build.ps1 -Preset windows-msvc-release -Fresh
Start-Process -FilePath .\out\build\windows-msvc-release\bin\arpg_game.exe -WindowStyle Normal -PassThru
~~~

Expected: the window opens and exits normally, but only shows a flat dark background, the baseline title, and tick count. It does not yet satisfy the graybox-room or complete-HUD acceptance criteria.

- [ ] **Step 2: Implement the low-cost 2.5D room and complete HUD**

Replace src/platform/raylib/raylib_host.cpp with:

~~~cpp
#include "raylib_host.hpp"

#include "core/fixed_step.hpp"

#include <raylib.h>

#include <cstdint>

#if !defined(RAYLIB_VERSION_MAJOR) || \
    !defined(RAYLIB_VERSION_MINOR) || \
    !defined(RAYLIB_VERSION_PATCH)
#error "raylib version macros are unavailable"
#endif

static_assert(RAYLIB_VERSION_MAJOR == 6, "raylib 6.0.0 is required");
static_assert(RAYLIB_VERSION_MINOR == 0, "raylib 6.0.0 is required");
static_assert(RAYLIB_VERSION_PATCH == 0, "raylib 6.0.0 is required");

namespace arpg::platform {
namespace {

[[nodiscard]] Vector2 lerp(
    Vector2 from,
    Vector2 to,
    float amount) noexcept {
    return {
        from.x + (to.x - from.x) * amount,
        from.y + (to.y - from.y) * amount,
    };
}

void draw_graybox_room() noexcept {
    const float width = static_cast<float>(GetScreenWidth());
    const float height = static_cast<float>(GetScreenHeight());

    const Vector2 back_left{width * 0.20F, height * 0.22F};
    const Vector2 back_right{width * 0.80F, height * 0.22F};
    const Vector2 floor_left{width * 0.04F, height * 0.92F};
    const Vector2 floor_right{width * 0.96F, height * 0.92F};

    DrawRectangleGradientV(
        0,
        0,
        GetScreenWidth(),
        GetScreenHeight(),
        Color{13, 17, 27, 255},
        Color{28, 32, 43, 255});

    DrawRectangle(
        static_cast<int>(back_left.x),
        0,
        static_cast<int>(back_right.x - back_left.x),
        static_cast<int>(back_left.y),
        Color{31, 37, 51, 255});

    DrawTriangle(
        Vector2{0.0F, 0.0F},
        back_left,
        floor_left,
        Color{22, 27, 39, 255});
    DrawTriangle(
        Vector2{0.0F, 0.0F},
        floor_left,
        Vector2{0.0F, height},
        Color{22, 27, 39, 255});
    DrawTriangle(
        Vector2{width, 0.0F},
        floor_right,
        back_right,
        Color{22, 27, 39, 255});
    DrawTriangle(
        Vector2{width, 0.0F},
        Vector2{width, height},
        floor_right,
        Color{22, 27, 39, 255});

    const Color floor{45, 51, 63, 255};
    DrawTriangle(back_left, floor_left, floor_right, floor);
    DrawTriangle(back_left, floor_right, back_right, floor);

    const Color grid = Color{87, 99, 119, 110};
    for (int column = 0; column <= 10; ++column) {
        const float amount =
            static_cast<float>(column) / 10.0F;
        DrawLineEx(
            lerp(back_left, back_right, amount),
            lerp(floor_left, floor_right, amount),
            1.0F,
            grid);
    }
    for (int row = 0; row <= 8; ++row) {
        const float linear =
            static_cast<float>(row) / 8.0F;
        const float perspective = linear * linear;
        DrawLineEx(
            lerp(back_left, floor_left, perspective),
            lerp(back_right, floor_right, perspective),
            1.0F,
            grid);
    }

    DrawLineEx(back_left, back_right, 3.0F, Color{118, 130, 151, 255});
    DrawLineEx(back_left, floor_left, 3.0F, Color{91, 103, 124, 255});
    DrawLineEx(back_right, floor_right, 3.0F, Color{91, 103, 124, 255});
}

void draw_diagnostics(
    const core::FixedStepFrame& frame,
    std::uint64_t root_seed) noexcept {
    DrawRectangleRounded(
        Rectangle{20.0F, 20.0F, 390.0F, 228.0F},
        0.08F,
        6,
        Color{7, 10, 17, 218});
    DrawRectangleRoundedLines(
        Rectangle{20.0F, 20.0F, 390.0F, 228.0F},
        0.08F,
        6,
        Color{91, 114, 151, 255});

    constexpr int x = 40;
    constexpr int font_size = 20;
    constexpr int line_height = 28;
    int y = 38;
    const Color text{218, 226, 239, 255};
    const Color accent{110, 207, 255, 255};

    DrawText(TextFormat("raylib %s", RAYLIB_VERSION), x, y, font_size, accent);
    y += line_height;
    DrawText(
        TextFormat(
            "seed 0x%016llX",
            static_cast<unsigned long long>(root_seed)),
        x,
        y,
        font_size,
        text);
    y += line_height;
    DrawText(
        TextFormat(
            "tick %llu",
            static_cast<unsigned long long>(frame.total_ticks)),
        x,
        y,
        font_size,
        text);
    y += line_height;
    DrawText(
        TextFormat(
            "fixed steps %u / %u",
            frame.steps,
            core::FixedStepRunner::kMaxStepsPerFrame),
        x,
        y,
        font_size,
        text);
    y += line_height;
    DrawText(
        TextFormat("alpha %.3f", frame.interpolation_alpha),
        x,
        y,
        font_size,
        text);
    y += line_height;
    DrawText(
        TextFormat("dropped %.6f s", frame.dropped_seconds),
        x,
        y,
        font_size,
        text);
    y += line_height;
    DrawText(
        TextFormat(
            "invalid dt %llu",
            static_cast<unsigned long long>(
                frame.invalid_input_count)),
        x,
        y,
        font_size,
        text);
}

}  // namespace

HostExitCode run_raylib_host(const RaylibHostConfig& config) noexcept {
    SetConfigFlags(FLAG_VSYNC_HINT | FLAG_WINDOW_RESIZABLE);
    InitWindow(config.window_width, config.window_height, config.window_title);
    if (!IsWindowReady()) {
        TraceLog(LOG_ERROR, "raylib window initialization failed");
        return HostExitCode::window_initialization_failed;
    }

    SetWindowMinSize(800, 450);
    SetExitKey(KEY_ESCAPE);
    SetTargetFPS(60);

    core::FixedStepRunner fixed_step;
    while (!WindowShouldClose()) {
        const core::FixedStepFrame frame =
            fixed_step.advance(static_cast<double>(GetFrameTime()));

        BeginDrawing();
        draw_graybox_room();
        draw_diagnostics(frame, config.root_seed);
        EndDrawing();
    }

    CloseWindow();
    return HostExitCode::success;
}

}  // namespace arpg::platform
~~~

- [ ] **Step 3: Build and run all headless checks**

Run:

~~~powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug
.\scripts\Test.ps1 -Preset windows-msvc-release
~~~

Expected: core.units and architecture.core_no_raylib pass in both configurations. No test initializes a window.

- [ ] **Step 4: Perform the host visual green check**

Run:

~~~powershell
$process = Start-Process -FilePath .\out\build\windows-msvc-release\bin\arpg_game.exe -WindowStyle Normal -PassThru
$process.Id
~~~

Inspect the visible window, then exit with Esc.

Expected:

- A resizable 1280×720 low-cost perspective room is visible.
- The HUD shows raylib 6.0, the root seed, total tick, fixed steps, alpha, dropped seconds, and invalid-dt count.
- The tick advances while alpha remains in [0, 1).
- Esc closes the process normally.

- [ ] **Step 5: Commit the host**

Run:

~~~powershell
git -c safe.directory=E:/game/.worktrees/m00-raylib-host diff --check
git -c safe.directory=E:/game/.worktrees/m00-raylib-host add src/platform/raylib/raylib_host.cpp
git -c safe.directory=E:/game/.worktrees/m00-raylib-host -c user.name=Codex -c user.email=codex@local commit -m "feat: render stage 0 raylib graybox"
~~~

Expected: task/m00-raylib-host is clean after one host commit.

---

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

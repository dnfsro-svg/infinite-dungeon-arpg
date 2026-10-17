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


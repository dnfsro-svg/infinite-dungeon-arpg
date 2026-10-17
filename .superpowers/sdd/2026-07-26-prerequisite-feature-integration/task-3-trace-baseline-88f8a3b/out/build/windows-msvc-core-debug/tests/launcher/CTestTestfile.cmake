# CMake generated Testfile for 
# Source directory: Q:/tests/launcher
# Build directory: Q:/out/build/windows-msvc-core-debug/tests/launcher
# 
# This file includes the relevant testing commands required for 
# testing this directory and lists subdirectories to be tested as well.
add_test([=[launcher.units]=] "Q:/out/build/windows-msvc-core-debug/bin/arpg_launcher_tests.exe")
set_tests_properties([=[launcher.units]=] PROPERTIES  LABELS "headless;launcher" _BACKTRACE_TRIPLES "Q:/tests/launcher/CMakeLists.txt;19;add_test;Q:/tests/launcher/CMakeLists.txt;0;")
add_test([=[launcher.icon_asset]=] "E:/codex1/python-dev/Python313/python.exe" "Q:/tests/launcher/launcher_icon_tests.py")
set_tests_properties([=[launcher.icon_asset]=] PROPERTIES  LABELS "headless;launcher;assets" WORKING_DIRECTORY "Q:/" _BACKTRACE_TRIPLES "Q:/tests/launcher/CMakeLists.txt;23;add_test;Q:/tests/launcher/CMakeLists.txt;0;")
add_test([=[launcher.manifest]=] "C:/Program Files/CMake/bin/cmake.exe" "-P" "Q:/tests/launcher/launcher_manifest_test.cmake")
set_tests_properties([=[launcher.manifest]=] PROPERTIES  _BACKTRACE_TRIPLES "Q:/tests/launcher/CMakeLists.txt;31;add_test;Q:/tests/launcher/CMakeLists.txt;0;")
add_test([=[launcher.deploy_self_test]=] "powershell.exe" "-NoProfile" "-ExecutionPolicy" "Bypass" "-File" "Q:/tests/launcher/deploy_launcher_self_test.ps1" "-DeployScript" "Q://scripts/DeployLauncher.ps1")
set_tests_properties([=[launcher.deploy_self_test]=] PROPERTIES  LABELS "headless;launcher;deployment" _BACKTRACE_TRIPLES "Q:/tests/launcher/CMakeLists.txt;45;add_test;Q:/tests/launcher/CMakeLists.txt;0;")

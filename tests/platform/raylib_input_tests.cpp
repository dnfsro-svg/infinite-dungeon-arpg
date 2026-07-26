#include "test_framework.hpp"

#include "raylib_input.hpp"

#include <raylib.h>

namespace {

using namespace arpg;

test::Failure win32_mapper_maps_explicit_numpad_virtual_keys() noexcept {
    ARPG_REQUIRE(platform::win32_virtual_key_for_raylib(KEY_KP_1) == 0x61);
    ARPG_REQUIRE(platform::win32_virtual_key_for_raylib(KEY_KP_2) == 0x62);
    ARPG_REQUIRE(platform::win32_virtual_key_for_raylib(KEY_KP_3) == 0x63);
    ARPG_REQUIRE(platform::win32_virtual_key_for_raylib(KEY_KP_4) == 0x64);
    ARPG_REQUIRE(platform::win32_virtual_key_for_raylib(KEY_KP_5) == 0x65);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"Win32 mapper maps explicit numpad virtual keys",
        &win32_mapper_maps_explicit_numpad_virtual_keys},
};

}  // namespace

arpg::test::TestSuite raylib_input_suite() noexcept {
    return arpg::test::make_suite("raylib_input", kCases);
}

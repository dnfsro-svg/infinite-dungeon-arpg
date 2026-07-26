#include "test_framework.hpp"

#include "raylib_input.hpp"

#include <raylib.h>

#include <cctype>
#include <fstream>
#include <iterator>
#include <string>

namespace {

using namespace arpg;

[[nodiscard]] std::string without_ascii_whitespace(
    const std::string& source) {
    std::string compact{};
    compact.reserve(source.size());
    for (char character : source) {
        if (!std::isspace(static_cast<unsigned char>(character))) {
            compact.push_back(character);
        }
    }
    return compact;
}

test::Failure win32_mapper_and_pressed_sampling_are_explicit() noexcept {
    ARPG_REQUIRE(platform::win32_virtual_key_for_raylib(KEY_KP_1) == 0x61);
    ARPG_REQUIRE(platform::win32_virtual_key_for_raylib(KEY_KP_2) == 0x62);
    ARPG_REQUIRE(platform::win32_virtual_key_for_raylib(KEY_KP_3) == 0x63);
    ARPG_REQUIRE(platform::win32_virtual_key_for_raylib(KEY_KP_4) == 0x64);
    ARPG_REQUIRE(platform::win32_virtual_key_for_raylib(KEY_KP_5) == 0x65);

    std::ifstream input{ARPG_PROJECT_SOURCE_DIR
        "/src/platform/raylib/raylib_input.cpp", std::ios::binary};
    ARPG_REQUIRE(input.is_open());
    const std::string source{std::istreambuf_iterator<char>{input}, {}};
    const std::string compact = without_ascii_whitespace(source);
    const std::size_t pressed_start = compact.find(
        "boolplatform_key_pressed(intkey)noexcept{");
    const std::size_t down_start = compact.find(
        "boolplatform_key_down(intkey)noexcept{");
    ARPG_REQUIRE(pressed_start != std::string::npos);
    ARPG_REQUIRE(down_start > pressed_start);
    const std::string pressed_body = compact.substr(
        pressed_start, down_start - pressed_start);
    ARPG_REQUIRE(pressed_body.find(
        "constboolraylib_pressed=IsKeyPressed(key);")
        != std::string::npos);
    ARPG_REQUIRE(pressed_body.find(
        "constshortasynchronous_state=asynchronous_key_state("
        "win32_virtual_key_for_raylib(key));") != std::string::npos);
    ARPG_REQUIRE(pressed_body.find(
        "returnraylib_pressed||(asynchronous_state&0x0001)!=0;")
        != std::string::npos);
    ARPG_REQUIRE(pressed_body.find("returnIsKeyPressed(key)||")
        == std::string::npos);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"Win32 mapper and pressed sampling are explicit",
        &win32_mapper_and_pressed_sampling_are_explicit},
};

}  // namespace

arpg::test::TestSuite raylib_input_suite() noexcept {
    return arpg::test::make_suite("raylib_input", kCases);
}

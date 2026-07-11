#include "raylib_host.hpp"

int main() {
    const arpg::platform::RaylibHostConfig config{};
    return static_cast<int>(arpg::platform::run_raylib_host(config));
}

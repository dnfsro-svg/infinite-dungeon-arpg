#include "raylib_host.hpp"
#include "host_launch_options.hpp"

#include <cstdio>
#include <utility>

int main(int argc, char** argv) {
    const auto arguments = arpg::platform::parse_host_arguments(
        argc, const_cast<const char* const*>(argv));
    if (arguments.error != arpg::platform::HostArgumentError::none) {
        std::fprintf(stderr, "Invalid command line arguments (error %u)\n",
            static_cast<unsigned>(arguments.error));
        return static_cast<int>(arpg::platform::HostExitCode::invalid_arguments);
    }
    arpg::platform::RaylibHostConfig config =
        arpg::platform::make_production_host_config(
            std::move(arguments.options));
    return static_cast<int>(arpg::platform::run_raylib_host(config));
}

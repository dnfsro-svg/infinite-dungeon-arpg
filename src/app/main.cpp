#include "raylib_host.hpp"
#include "host_launch_options.hpp"

#include <cstdio>

int main(int argc, char** argv) {
    const auto arguments = arpg::platform::parse_host_arguments(
        argc, const_cast<const char* const*>(argv));
    if (arguments.error != arpg::platform::HostArgumentError::none) {
        std::fprintf(stderr, "Invalid command line arguments (error %u)\n",
            static_cast<unsigned>(arguments.error));
        return static_cast<int>(arpg::platform::HostExitCode::invalid_arguments);
    }
    arpg::platform::RaylibHostConfig config{};
    config.save_directory = arguments.options.save_directory;
    config.settings_directory = arguments.options.settings_directory;
    config.new_run_seed = arguments.options.new_run_seed;
    return static_cast<int>(arpg::platform::run_raylib_host(config));
}

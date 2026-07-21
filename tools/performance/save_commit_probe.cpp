#include "dungeon/dungeon_progression.hpp"
#include "persistence/save_store.hpp"

#include <algorithm>
#include <charconv>
#include <chrono>
#include <cstdint>
#include <cstdio>
#include <filesystem>
#include <numeric>
#include <string>
#include <string_view>
#include <vector>

namespace {

constexpr std::size_t kDefaultSampleCount = 101U;
constexpr std::size_t kMaximumSampleCount = 10000U;
constexpr std::uint64_t kProbeSeed = 0x53544147453138ULL;

bool parse_sample_count(
    int argument_count,
    char** arguments,
    std::size_t& sample_count) noexcept {
    sample_count = kDefaultSampleCount;
    if (argument_count == 1) return true;
    if (argument_count != 2 || arguments[1] == nullptr) return false;

    const std::string_view text{arguments[1]};
    std::size_t parsed = 0U;
    const auto result = std::from_chars(
        text.data(), text.data() + text.size(), parsed);
    if (result.ec != std::errc{} || result.ptr != text.data() + text.size()
            || parsed == 0U || parsed > kMaximumSampleCount) {
        return false;
    }
    sample_count = parsed;
    return true;
}

std::filesystem::path probe_directory(const char* executable) {
    const std::filesystem::path executable_path =
        std::filesystem::absolute(executable == nullptr ? "" : executable);
    const auto unique = std::chrono::steady_clock::now()
        .time_since_epoch().count();
    return executable_path.parent_path()
        / ("save-commit-probe-data-" + std::to_string(unique));
}

double median_of(const std::vector<double>& sorted) noexcept {
    const std::size_t middle = sorted.size() / 2U;
    if ((sorted.size() & 1U) != 0U) return sorted[middle];
    return (sorted[middle - 1U] + sorted[middle]) / 2.0;
}

double percentile_95_of(const std::vector<double>& sorted) noexcept {
    const std::size_t rank = (sorted.size() * 95U + 99U) / 100U;
    return sorted[rank == 0U ? 0U : rank - 1U];
}

}  // namespace

int main(int argument_count, char** arguments) {
    std::size_t sample_count = 0U;
    if (!parse_sample_count(argument_count, arguments, sample_count)) {
        std::fprintf(stderr,
            "usage: arpg_save_commit_probe [sample-count 1..10000]\n");
        return 2;
    }

    const arpg::dungeon::RunStateBuildResult built =
        arpg::dungeon::make_initial_run_state(
            kProbeSeed, arpg::dungeon::DungeonRules{});
    if (built.fault != arpg::dungeon::DungeonFault::none) {
        std::fprintf(stderr, "initial-state-failed fault=%u\n",
            static_cast<unsigned>(built.fault));
        return 3;
    }

    const std::filesystem::path directory = probe_directory(arguments[0]);
    arpg::persistence::SaveStore store{{directory}};
    arpg::dungeon::checkpoint::DungeonRunState state = built.state;
    const arpg::persistence::SaveCommitResult warmup = store.commit(state);
    if (warmup.state != arpg::persistence::SaveCommitState::committed
            || warmup.verified_state.commit_generation
                != state.commit_generation) {
        std::fprintf(stderr, "warmup-commit-failed state=%u error=%u\n",
            static_cast<unsigned>(warmup.state),
            static_cast<unsigned>(warmup.error));
        return 4;
    }

    std::vector<double> samples;
    samples.reserve(sample_count);
    for (std::size_t index = 0U; index < sample_count; ++index) {
        ++state.commit_generation;
        const auto started = std::chrono::steady_clock::now();
        const arpg::persistence::SaveCommitResult committed =
            store.commit(state);
        const auto finished = std::chrono::steady_clock::now();
        if (committed.state
                    != arpg::persistence::SaveCommitState::committed
                || committed.verified_state.commit_generation
                    != state.commit_generation) {
            std::fprintf(stderr,
                "measured-commit-failed sample=%zu state=%u error=%u\n",
                index, static_cast<unsigned>(committed.state),
                static_cast<unsigned>(committed.error));
            return 5;
        }
        samples.push_back(std::chrono::duration<double, std::milli>(
            finished - started).count());
    }

    std::sort(samples.begin(), samples.end());
    const double average = std::accumulate(
        samples.begin(), samples.end(), 0.0) / samples.size();
    std::printf("save_commit_probe samples=%zu\n", samples.size());
    std::printf("directory=%s\n", directory.string().c_str());
    std::printf("min_ms=%.6f\n", samples.front());
    std::printf("median_ms=%.6f\n", median_of(samples));
    std::printf("p95_ms=%.6f\n", percentile_95_of(samples));
    std::printf("max_ms=%.6f\n", samples.back());
    std::printf("average_ms=%.6f\n", average);
    return 0;
}

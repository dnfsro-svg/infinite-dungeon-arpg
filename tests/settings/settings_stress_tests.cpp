#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "control_hints.hpp"
#include "host_input.hpp"
#include "dungeon_runtime.hpp"
#include "persistence/save_store.hpp"
#include "platform/settings/settings_codec.hpp"
#include "platform/settings/settings_store.hpp"
#include "platform/settings/settings_types.hpp"

#include <array>
#include <chrono>
#include <cstddef>
#include <cstdint>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <filesystem>
#include <fstream>
#include <string>
#include <vector>

namespace {

namespace platform = arpg::platform;
namespace persistence = arpg::persistence;
namespace settings = arpg::settings;

// Independently derived from the documented 1,000-step mutation sequence and
// the settings file layout (including its CRC32 field), not from store output.
constexpr std::uint64_t kExpectedSlotAHash = 0xB18681AE11B562E5ULL;
constexpr std::uint64_t kExpectedSlotBHash = 0x2FCCF872A6729E6FULL;

struct MemorySlots final {
    std::array<std::uint8_t, settings::kSettingsEncodedSize> a{};
    std::array<std::uint8_t, settings::kSettingsEncodedSize> b{};
    bool has_a{};
    bool has_b{};
};

[[nodiscard]] bool slot_is_a(const std::filesystem::path& path) noexcept {
    return path.filename() == "settings-a.bin";
}

bool memory_read(void* context, const std::filesystem::path& path,
    std::vector<std::uint8_t>& bytes) {
    auto& slots = *static_cast<MemorySlots*>(context);
    const bool a = slot_is_a(path);
    if (a ? !slots.has_a : !slots.has_b) {
        return false;
    }
    const auto& source = a ? slots.a : slots.b;
    bytes.assign(source.begin(), source.end());
    return true;
}

bool memory_replace(void* context, const std::filesystem::path& path,
    const std::uint8_t* bytes, std::size_t size) {
    if (bytes == nullptr || size != settings::kSettingsEncodedSize) {
        return false;
    }
    auto& slots = *static_cast<MemorySlots*>(context);
    auto& destination = slot_is_a(path) ? slots.a : slots.b;
    std::memcpy(destination.data(), bytes, destination.size());
    if (slot_is_a(path)) {
        slots.has_a = true;
    } else {
        slots.has_b = true;
    }
    return true;
}

[[nodiscard]] settings::SettingsFileOps memory_ops(MemorySlots& slots) noexcept {
    return {&slots, &memory_read, &memory_replace};
}

[[nodiscard]] bool same_settings(const settings::SettingsData& lhs,
    const settings::SettingsData& rhs) noexcept {
    return lhs.master_sfx_percent == rhs.master_sfx_percent
        && lhs.window_mode == rhs.window_mode
        && lhs.vsync_enabled == rhs.vsync_enabled
        && lhs.bindings == rhs.bindings
        && lhs.revision == rhs.revision;
}

[[nodiscard]] std::uint64_t sentinel_hash(
    const std::array<std::uint8_t, settings::kSettingsEncodedSize>& bytes) noexcept {
    std::uint64_t value = 1469598103934665603ULL;
    for (const std::uint8_t byte : bytes) {
        value ^= byte;
        value *= 1099511628211ULL;
    }
    return value;
}

struct V6Fingerprint final {
    std::uint64_t hash{};
    std::uintmax_t size{};
    bool valid{};
};

[[nodiscard]] V6Fingerprint v6_fingerprint(const std::filesystem::path& path) {
    std::error_code error{};
    const std::uintmax_t size = std::filesystem::file_size(path, error);
    if (error || size == 0U) return {};
    std::ifstream stream(path, std::ios::binary);
    std::uint64_t hash = 1469598103934665603ULL;
    char byte{};
    while (stream.get(byte)) {
        hash ^= static_cast<unsigned char>(byte);
        hash *= 1099511628211ULL;
    }
    return {hash, size, true};
}

[[nodiscard]] bool establish_v6_slots(const std::filesystem::path& directory) {
    platform::DungeonRuntimeConfig config{};
    config.save.directory = directory;
    config.new_run_seed = 0x11B11BULL;
    platform::DungeonRuntime runtime(config);
    if (!runtime.initialize()) return false;
    persistence::SaveStore store({directory});
    const persistence::SaveLoadResult loaded = store.load();
    if (loaded.state != persistence::SaveLoadState::ready
            || loaded.checkpoint.commit_generation == UINT64_MAX) {
        return false;
    }
    auto second = loaded.checkpoint;
    ++second.commit_generation;
    return store.commit(second).state == persistence::SaveCommitState::committed;
}

struct StressOutcome final {
    settings::SettingsData direct{};
    std::array<std::uint8_t, settings::kSettingsEncodedSize> slot_a{};
    std::array<std::uint8_t, settings::kSettingsEncodedSize> slot_b{};
    std::uint64_t slot_a_hash{};
    std::uint64_t slot_b_hash{};
    bool success{};
};

[[nodiscard]] StressOutcome run_stress_cycles() noexcept {
    MemorySlots slots{};
    settings::SettingsStore store{"stage11b-stress", memory_ops(slots)};
    settings::SettingsData direct = settings::default_settings();
    constexpr std::array<std::uint32_t, 3> restart_intervals{{17U, 31U, 43U}};

    for (std::uint64_t revision = 1U; revision <= 1000U; ++revision) {
        settings::SettingsData draft = direct;
        const auto target = static_cast<settings::SettingAction>(
            revision % static_cast<std::uint64_t>(settings::SettingAction::count));
        const std::size_t owner = static_cast<std::size_t>(
            (revision * 7U) % draft.bindings.size());
        if (!settings::assign_or_swap(draft, target, draft.bindings[owner])) {
            return {};
        }
        draft.master_sfx_percent = static_cast<std::uint8_t>((revision % 21U) * 5U);
        draft.window_mode = (revision & 1U) != 0U
            ? settings::WindowMode::fullscreen : settings::WindowMode::windowed;
        draft.vsync_enabled = (revision % 3U) != 0U;
        const auto saved = store.save(direct, draft);
        if (saved.status != settings::SettingsSaveStatus::committed
                || saved.settings.revision != revision
                || settings::validate_settings(saved.settings)
                    != settings::SettingsValidationError::none) {
            return {};
        }
        direct = saved.settings;

        for (const std::uint32_t interval : restart_intervals) {
            if (revision % interval != 0U) {
                continue;
            }
            const auto restarted = settings::SettingsStore{
                "stage11b-stress", memory_ops(slots)}.load();
            if (!same_settings(restarted.settings, direct)) {
                return {};
            }
        }
    }

    const auto restarted = settings::SettingsStore{
        "stage11b-stress", memory_ops(slots)}.load();
    if (!slots.has_a || !slots.has_b || !same_settings(restarted.settings, direct)) {
        return {};
    }
    return {direct, slots.a, slots.b, sentinel_hash(slots.a), sentinel_hash(slots.b), true};
}

arpg::test::Failure thousand_atomic_reload_cycles_are_restart_stable() noexcept {
    const StressOutcome first = run_stress_cycles();
    const StressOutcome second = run_stress_cycles();
    ARPG_REQUIRE(first.success);
    ARPG_REQUIRE(second.success);
    ARPG_REQUIRE(first.direct.revision == 1000U);
    ARPG_REQUIRE(second.direct.revision == 1000U);
    ARPG_REQUIRE(same_settings(first.direct, second.direct));
    ARPG_REQUIRE(first.slot_a == second.slot_a);
    ARPG_REQUIRE(first.slot_b == second.slot_b);
    ARPG_REQUIRE(first.slot_a_hash != 0U);
    ARPG_REQUIRE(first.slot_b_hash != 0U);
    ARPG_REQUIRE(first.slot_a_hash == kExpectedSlotAHash);
    ARPG_REQUIRE(first.slot_b_hash == kExpectedSlotBHash);
    ARPG_REQUIRE(first.slot_a[12U] == 0xE7U);
    ARPG_REQUIRE(first.slot_a[13U] == 0x03U);
    ARPG_REQUIRE(first.slot_b[12U] == 0xE8U);
    ARPG_REQUIRE(first.slot_b[13U] == 0x03U);
    ARPG_REQUIRE(first.slot_a_hash == second.slot_a_hash);
    ARPG_REQUIRE(first.slot_b_hash == second.slot_b_hash);
    ARPG_REQUIRE(first.slot_a_hash != first.slot_b_hash);
    return {};
}

arpg::test::Failure real_v6_slots_are_unchanged_by_thousand_settings_cycles() noexcept {
    const auto stamp = std::chrono::steady_clock::now().time_since_epoch().count();
    const std::filesystem::path directory = std::filesystem::temp_directory_path()
        / ("arpg-stage11b-v6-settings-" + std::to_string(stamp));
    std::error_code error{};
    std::filesystem::create_directories(directory, error);
    ARPG_REQUIRE(!error);
    ARPG_REQUIRE(establish_v6_slots(directory));
    const V6Fingerprint before_a = v6_fingerprint(directory / "run_a.sav");
    const V6Fingerprint before_b = v6_fingerprint(directory / "run_b.sav");
    const StressOutcome stress = run_stress_cycles();
    const V6Fingerprint after_a = v6_fingerprint(directory / "run_a.sav");
    const V6Fingerprint after_b = v6_fingerprint(directory / "run_b.sav");
    ARPG_REQUIRE(stress.success);
    ARPG_REQUIRE(before_a.valid && before_b.valid);
    ARPG_REQUIRE(after_a.valid && after_b.valid);
    ARPG_REQUIRE(before_a.hash == after_a.hash && before_a.size == after_a.size);
    ARPG_REQUIRE(before_b.hash == after_b.hash && before_b.size == after_b.size);
    return {};
}

arpg::test::Failure settings_hot_paths_and_cached_frame_paths_allocate_nothing() noexcept {
    settings::SettingsData values = settings::default_settings();
    platform::PhysicalKeySnapshot snapshot{};
    snapshot.down[static_cast<std::size_t>(settings::StableKey::w)] = true;
    snapshot.pressed[static_cast<std::size_t>(settings::StableKey::j)] = true;
    platform::ControlHints hints{};
    platform::refresh_control_hints(hints, values);

    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 10000U; ++iteration) {
        ARPG_REQUIRE(settings::validate_settings(values)
            == settings::SettingsValidationError::none);
        const auto action = static_cast<settings::SettingAction>(
            iteration % static_cast<std::size_t>(settings::SettingAction::count));
        const auto owner = (iteration * 3U) % values.bindings.size();
        ARPG_REQUIRE(settings::assign_or_swap(values, action, values.bindings[owner]));
        snapshot.pressed.fill(false);
        snapshot.pressed[static_cast<std::size_t>(settings::binding_for(
            values, settings::SettingAction::light_attack))] = true;
        const auto mapped = platform::map_host_frame_input(values, snapshot);
        ARPG_REQUIRE(mapped.keys.movement);
        ARPG_REQUIRE(mapped.combat_actions[0]);
        platform::refresh_control_hints(hints, values);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"1000 atomic reload cycles", &thousand_atomic_reload_cycles_are_restart_stable},
    {"real V6 slots survive 1000 settings cycles", &real_v6_slots_are_unchanged_by_thousand_settings_cycles},
    {"settings hot paths allocate nothing", &settings_hot_paths_and_cached_frame_paths_allocate_nothing},
};

}  // namespace

int main() {
    char* filter = nullptr;
    std::size_t filter_length = 0U;
#if defined(_WIN32)
    const errno_t filter_error = _dupenv_s(
        &filter, &filter_length, "ARPG_TEST_FILTER");
#else
    const errno_t filter_error = 0;
    filter = std::getenv("ARPG_TEST_FILTER");
#endif
    const bool accepted = filter_error == 0 && filter != nullptr
        && std::strcmp(filter, "stage11b.settings_stress") == 0;
    std::free(filter);
    static_cast<void>(filter_length);
    if (!accepted) {
        std::fprintf(stderr,
            "settings stress requires ARPG_TEST_FILTER=stage11b.settings_stress\n");
        return 2;
    }
    const arpg::test::TestSuite suites[] = {
        arpg::test::make_suite("settings_stress", kCases),
    };
    return arpg::test::run_suites(suites, 3, "stage 11b settings stress");
}

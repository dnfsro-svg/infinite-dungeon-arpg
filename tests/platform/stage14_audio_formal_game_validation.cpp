#include "audio_manifest.hpp"
#include "audio_pack.hpp"
#include "audio_routing.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <limits>
#include <sstream>
#include <string>
#include <vector>

namespace {

namespace combat = arpg::combat;
namespace platform = arpg::platform;

constexpr std::size_t kAssetCount =
    static_cast<std::size_t>(platform::AudioAssetId::count);
constexpr std::uint32_t kSampleRate = 44100U;
constexpr std::uint16_t kSampleSize = 16U;
constexpr std::uint16_t kChannels = 1U;
constexpr std::uint32_t kSilenceFrames = 11025U;
constexpr std::uint64_t kPcmBudget = 8U * 1024U * 1024U;

struct DecodedAsset final {
    platform::AudioAssetId id{platform::AudioAssetId::count};
    std::string id_name{};
    std::string path{};
    std::string sha256{};
    std::vector<std::int16_t> samples{};
    std::uint32_t peak{};
};

struct TraceSpec final {
    combat::CombatEventKind kind{};
    combat::AttackId attack{combat::AttackId::none};
    combat::FeedbackLevel feedback{combat::FeedbackLevel::light};
    platform::AudioCue cue{platform::AudioCue::swing_light};
    const char* cue_name{};
    const char* event_name{};
};

constexpr std::array<const char*, kAssetCount> kAssetNames{{
    "swing_light_1", "swing_light_2", "swing_finisher", "swing_launcher",
    "impact_1", "impact_2", "impact_3", "impact_low", "player_hurt",
    "landing", "enemy_defeat", "warning_blink", "warning_chain",
    "warning_death",
}};

constexpr std::array<TraceSpec, 14> kTraceSpecs{{
    {combat::CombatEventKind::swing, combat::AttackId::j1,
        combat::FeedbackLevel::light, platform::AudioCue::swing_light,
        "swing_light", "swing:j1"},
    {combat::CombatEventKind::swing, combat::AttackId::j2,
        combat::FeedbackLevel::light, platform::AudioCue::swing_light,
        "swing_light", "swing:j2"},
    {combat::CombatEventKind::swing, combat::AttackId::j3,
        combat::FeedbackLevel::heavy, platform::AudioCue::swing_finisher,
        "swing_finisher", "swing:j3"},
    {combat::CombatEventKind::swing, combat::AttackId::launcher,
        combat::FeedbackLevel::heavy, platform::AudioCue::swing_launcher,
        "swing_launcher", "swing:launcher"},
    {combat::CombatEventKind::hit, combat::AttackId::j1,
        combat::FeedbackLevel::light, platform::AudioCue::impact,
        "impact", "hit:light"},
    {combat::CombatEventKind::hit, combat::AttackId::j2,
        combat::FeedbackLevel::medium, platform::AudioCue::impact,
        "impact", "hit:medium"},
    {combat::CombatEventKind::hit, combat::AttackId::j3,
        combat::FeedbackLevel::heavy, platform::AudioCue::impact,
        "impact", "hit:heavy"},
    {combat::CombatEventKind::impact_summary, combat::AttackId::none,
        combat::FeedbackLevel::heavy, platform::AudioCue::impact_low,
        "impact_low", "impact_summary:heavy"},
    {combat::CombatEventKind::player_hit, combat::AttackId::none,
        combat::FeedbackLevel::heavy, platform::AudioCue::player_hurt,
        "player_hurt", "player_hit:heavy"},
    {combat::CombatEventKind::landing, combat::AttackId::none,
        combat::FeedbackLevel::light, platform::AudioCue::landing,
        "landing", "landing:light"},
    {combat::CombatEventKind::defeated, combat::AttackId::none,
        combat::FeedbackLevel::heavy, platform::AudioCue::enemy_defeat,
        "enemy_defeat", "defeated:heavy"},
    {combat::CombatEventKind::affix_blink_warning, combat::AttackId::none,
        combat::FeedbackLevel::light, platform::AudioCue::warning_blink,
        "warning_blink", "affix_blink_warning:light"},
    {combat::CombatEventKind::affix_chain_warning, combat::AttackId::none,
        combat::FeedbackLevel::light, platform::AudioCue::warning_chain,
        "warning_chain", "affix_chain_warning:light"},
    {combat::CombatEventKind::affix_death_warning, combat::AttackId::none,
        combat::FeedbackLevel::heavy, platform::AudioCue::warning_death,
        "warning_death", "affix_death_warning:heavy"},
}};

[[nodiscard]] bool path_is_within(const std::filesystem::path& child,
    const std::filesystem::path& parent) {
    const auto relative = child.lexically_relative(parent);
    return !relative.empty() && relative != "." && !relative.is_absolute()
        && *relative.begin() != "..";
}

[[nodiscard]] bool canonical_path(const std::filesystem::path& path,
    std::filesystem::path& result) {
    std::error_code error{};
    result = std::filesystem::weakly_canonical(
        std::filesystem::absolute(path, error), error).lexically_normal();
    return !error && !result.empty();
}

[[nodiscard]] bool allowed_scratch_parent(
    const std::filesystem::path& candidate) {
    std::filesystem::path absolute{};
    std::filesystem::path allowed{};
    if (!canonical_path(candidate, absolute)
        || !canonical_path(std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}
            / "out" / "build", allowed)) return false;
    const auto lexical = std::filesystem::absolute(candidate).lexically_normal();
    return lexical.filename() == "stage14 audio evidence"
        && path_is_within(absolute, allowed);
}

[[nodiscard]] bool contains_symlink(const std::filesystem::path& root) {
    std::error_code error{};
    const auto status = std::filesystem::symlink_status(root, error);
    if (!error && std::filesystem::is_symlink(status)) return true;
    if (error && error != std::errc::no_such_file_or_directory) return true;
    error.clear();
    if (!std::filesystem::exists(root, error) || error) return error.value() != 0;
    std::filesystem::recursive_directory_iterator iterator{
        root, std::filesystem::directory_options::skip_permission_denied, error};
    const std::filesystem::recursive_directory_iterator end{};
    while (!error && iterator != end) {
        if (std::filesystem::is_symlink(iterator->symlink_status(error))) {
            return true;
        }
        iterator.increment(error);
    }
    return error.value() != 0;
}

[[nodiscard]] bool prepare_run(const std::filesystem::path& candidate,
    std::filesystem::path& run_root) {
    if (!allowed_scratch_parent(candidate)) return false;
    std::filesystem::path parent{};
    if (!canonical_path(candidate, parent)) return false;
    run_root = parent / "stage14-run";
    if (!path_is_within(run_root, parent) || contains_symlink(run_root)) {
        return false;
    }
    std::error_code error{};
    std::filesystem::remove_all(run_root, error);
    if (error) return false;
    std::filesystem::create_directories(run_root, error);
    return !error;
}

[[nodiscard]] bool root_safety_self_test(const std::filesystem::path& root) {
    std::error_code error{};
    std::filesystem::create_directories(root, error);
    if (error) return false;
    const auto sentinel = root / "sentinel.txt";
    std::ofstream output(sentinel, std::ios::out | std::ios::trunc);
    output << "must survive";
    output.close();
    std::filesystem::path ignored{};
    const bool rejected_candidate = !prepare_run(root, ignored);
    const bool rejected_project = !prepare_run(
        std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}, ignored);
    const bool rejected_lookalike = !prepare_run(
        std::filesystem::temp_directory_path()
            / "stage14-root-safety-outside" / "out" / "build"
            / "stage14 audio evidence",
        ignored);
    return rejected_candidate && rejected_project && rejected_lookalike
        && std::filesystem::is_regular_file(sentinel, error) && !error;
}

[[nodiscard]] bool sha256_file(const std::filesystem::path& path,
    std::string& result) {
    std::ifstream input(path, std::ios::binary);
    if (!input) return false;
    std::vector<unsigned char> bytes{
        std::istreambuf_iterator<char>{input}, std::istreambuf_iterator<char>{}};
    if (input.bad()) return false;

    constexpr std::array<std::uint32_t, 64> kRoundConstants{{
        0x428A2F98U, 0x71374491U, 0xB5C0FBCFU, 0xE9B5DBA5U,
        0x3956C25BU, 0x59F111F1U, 0x923F82A4U, 0xAB1C5ED5U,
        0xD807AA98U, 0x12835B01U, 0x243185BEU, 0x550C7DC3U,
        0x72BE5D74U, 0x80DEB1FEU, 0x9BDC06A7U, 0xC19BF174U,
        0xE49B69C1U, 0xEFBE4786U, 0x0FC19DC6U, 0x240CA1CCU,
        0x2DE92C6FU, 0x4A7484AAU, 0x5CB0A9DCU, 0x76F988DAU,
        0x983E5152U, 0xA831C66DU, 0xB00327C8U, 0xBF597FC7U,
        0xC6E00BF3U, 0xD5A79147U, 0x06CA6351U, 0x14292967U,
        0x27B70A85U, 0x2E1B2138U, 0x4D2C6DFCU, 0x53380D13U,
        0x650A7354U, 0x766A0ABBU, 0x81C2C92EU, 0x92722C85U,
        0xA2BFE8A1U, 0xA81A664BU, 0xC24B8B70U, 0xC76C51A3U,
        0xD192E819U, 0xD6990624U, 0xF40E3585U, 0x106AA070U,
        0x19A4C116U, 0x1E376C08U, 0x2748774CU, 0x34B0BCB5U,
        0x391C0CB3U, 0x4ED8AA4AU, 0x5B9CCA4FU, 0x682E6FF3U,
        0x748F82EEU, 0x78A5636FU, 0x84C87814U, 0x8CC70208U,
        0x90BEFFFAU, 0xA4506CEBU, 0xBEF9A3F7U, 0xC67178F2U,
    }};
    auto rotate_right = [](std::uint32_t value, unsigned int shift) noexcept {
        return (value >> shift) | (value << (32U - shift));
    };
    const std::uint64_t bit_length =
        static_cast<std::uint64_t>(bytes.size()) * 8U;
    bytes.push_back(0x80U);
    while (bytes.size() % 64U != 56U) bytes.push_back(0U);
    for (int shift = 56; shift >= 0; shift -= 8) {
        bytes.push_back(static_cast<unsigned char>(bit_length >> shift));
    }

    std::array<std::uint32_t, 8> state{{
        0x6A09E667U, 0xBB67AE85U, 0x3C6EF372U, 0xA54FF53AU,
        0x510E527FU, 0x9B05688CU, 0x1F83D9ABU, 0x5BE0CD19U,
    }};
    for (std::size_t offset{}; offset < bytes.size(); offset += 64U) {
        std::array<std::uint32_t, 64> words{};
        for (std::size_t index{}; index < 16U; ++index) {
            const std::size_t at = offset + index * 4U;
            words[index] = (static_cast<std::uint32_t>(bytes[at]) << 24U)
                | (static_cast<std::uint32_t>(bytes[at + 1U]) << 16U)
                | (static_cast<std::uint32_t>(bytes[at + 2U]) << 8U)
                | static_cast<std::uint32_t>(bytes[at + 3U]);
        }
        for (std::size_t index = 16U; index < words.size(); ++index) {
            const std::uint32_t a = words[index - 15U];
            const std::uint32_t b = words[index - 2U];
            const std::uint32_t small0 = rotate_right(a, 7U)
                ^ rotate_right(a, 18U) ^ (a >> 3U);
            const std::uint32_t small1 = rotate_right(b, 17U)
                ^ rotate_right(b, 19U) ^ (b >> 10U);
            words[index] = words[index - 16U] + small0
                + words[index - 7U] + small1;
        }
        std::uint32_t a = state[0];
        std::uint32_t b = state[1];
        std::uint32_t c = state[2];
        std::uint32_t d = state[3];
        std::uint32_t e = state[4];
        std::uint32_t f = state[5];
        std::uint32_t g = state[6];
        std::uint32_t h = state[7];
        for (std::size_t index{}; index < words.size(); ++index) {
            const std::uint32_t big1 = rotate_right(e, 6U)
                ^ rotate_right(e, 11U) ^ rotate_right(e, 25U);
            const std::uint32_t choice = (e & f) ^ ((~e) & g);
            const std::uint32_t temporary1 = h + big1 + choice
                + kRoundConstants[index] + words[index];
            const std::uint32_t big0 = rotate_right(a, 2U)
                ^ rotate_right(a, 13U) ^ rotate_right(a, 22U);
            const std::uint32_t majority = (a & b) ^ (a & c) ^ (b & c);
            const std::uint32_t temporary2 = big0 + majority;
            h = g;
            g = f;
            f = e;
            e = d + temporary1;
            d = c;
            c = b;
            b = a;
            a = temporary1 + temporary2;
        }
        state[0] += a;
        state[1] += b;
        state[2] += c;
        state[3] += d;
        state[4] += e;
        state[5] += f;
        state[6] += g;
        state[7] += h;
    }

    std::ostringstream encoded{};
    encoded << std::hex << std::setfill('0');
    for (const std::uint32_t word : state) {
        encoded << std::setw(8) << word;
    }
    result = encoded.str();
    return result.size() == 64U;
}

[[nodiscard]] std::uint32_t pcm_peak(const std::int16_t* samples,
    std::size_t count) noexcept {
    std::uint32_t peak{};
    for (std::size_t index{}; index < count; ++index) {
        const std::int32_t sample = samples[index];
        const std::uint32_t magnitude = sample == -32768
            ? 32768U
            : static_cast<std::uint32_t>(sample < 0 ? -sample : sample);
        if (magnitude > peak) peak = magnitude;
    }
    return peak;
}

[[nodiscard]] bool decode_assets(std::vector<DecodedAsset>& decoded,
    std::uint64_t& total_pcm_bytes) {
    const auto manifest = platform::default_audio_manifest();
    if (manifest.entry_count != kAssetCount) return false;
    const auto project = std::filesystem::path{ARPG_PROJECT_SOURCE_DIR};
    decoded.clear();
    decoded.reserve(kAssetCount);
    total_pcm_bytes = 0U;
    for (std::size_t index{}; index < manifest.entry_count; ++index) {
        const auto& entry = manifest.entries[index];
        if (static_cast<std::size_t>(entry.id) != index || entry.path == nullptr) {
            return false;
        }
        const auto file = project / std::filesystem::path{entry.path};
        Wave wave = LoadWave(file.string().c_str());
        const bool valid = IsWaveValid(wave) && wave.data != nullptr
            && wave.frameCount != 0U && wave.frameCount <= 66150U
            && wave.sampleRate == kSampleRate && wave.sampleSize == kSampleSize
            && wave.channels == kChannels;
        if (!valid) {
            if (IsWaveValid(wave)) UnloadWave(wave);
            return false;
        }
        const auto* samples = static_cast<const std::int16_t*>(wave.data);
        const std::size_t frame_count = wave.frameCount;
        const std::uint32_t peak = pcm_peak(samples, frame_count);
        DecodedAsset asset{};
        asset.id = entry.id;
        asset.id_name = kAssetNames[index];
        asset.path = entry.path;
        asset.samples.assign(samples, samples + frame_count);
        asset.peak = peak;
        const bool hashed = sha256_file(file, asset.sha256);
        UnloadWave(wave);
        if (!hashed || peak == 0U || peak >= 32768U) return false;
        total_pcm_bytes += static_cast<std::uint64_t>(frame_count) * 2U;
        decoded.push_back(std::move(asset));
    }
    return decoded.size() == kAssetCount && total_pcm_bytes <= kPcmBudget;
}

void write_u16(std::ostream& output, std::uint16_t value) {
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
}

void write_u32(std::ostream& output, std::uint32_t value) {
    output.put(static_cast<char>(value & 0xFFU));
    output.put(static_cast<char>((value >> 8U) & 0xFFU));
    output.put(static_cast<char>((value >> 16U) & 0xFFU));
    output.put(static_cast<char>((value >> 24U) & 0xFFU));
}

void write_i16(std::ostream& output, std::int16_t value) {
    const auto bits = static_cast<std::uint16_t>(value);
    write_u16(output, bits);
}

[[nodiscard]] bool write_showcase(const std::filesystem::path& path,
    const std::vector<DecodedAsset>& decoded, std::uint64_t& total_frames) {
    total_frames = 0U;
    for (std::size_t index{}; index < decoded.size(); ++index) {
        total_frames += decoded[index].samples.size();
        if (index + 1U < decoded.size()) total_frames += kSilenceFrames;
    }
    const std::uint64_t data_bytes = total_frames * 2U;
    if (data_bytes > (std::numeric_limits<std::uint32_t>::max)() - 36U) {
        return false;
    }
    std::ofstream output(path, std::ios::binary | std::ios::trunc);
    output.write("RIFF", 4);
    write_u32(output, static_cast<std::uint32_t>(36U + data_bytes));
    output.write("WAVEfmt ", 8);
    write_u32(output, 16U);
    write_u16(output, 1U);
    write_u16(output, kChannels);
    write_u32(output, kSampleRate);
    write_u32(output, kSampleRate * kChannels * (kSampleSize / 8U));
    write_u16(output, kChannels * (kSampleSize / 8U));
    write_u16(output, kSampleSize);
    output.write("data", 4);
    write_u32(output, static_cast<std::uint32_t>(data_bytes));
    for (std::size_t index{}; index < decoded.size(); ++index) {
        for (const std::int16_t sample : decoded[index].samples) {
            write_i16(output, sample);
        }
        if (index + 1U < decoded.size()) {
            for (std::uint32_t silence{}; silence < kSilenceFrames; ++silence) {
                write_i16(output, 0);
            }
        }
    }
    output.close();
    std::error_code error{};
    return output && std::filesystem::file_size(path, error) == 44U + data_bytes
        && !error;
}

struct FakeSoundBoundary final {
    unsigned int next_handle{1U};
    std::array<unsigned int, 32> played{};
    std::size_t played_count{};
};

FakeSoundBoundary* g_sound_boundary{};

Wave real_load_wave(const char* path) { return LoadWave(path); }
bool real_wave_valid(Wave wave) { return IsWaveValid(wave); }
Sound fake_load_sound(Wave) {
    Sound sound{};
    if (g_sound_boundary != nullptr) {
        sound.frameCount = g_sound_boundary->next_handle++;
    }
    return sound;
}
bool fake_sound_valid(Sound sound) { return sound.frameCount != 0U; }
void real_unload_wave(Wave wave) { UnloadWave(wave); }
void fake_unload_sound(Sound) {}
void fake_play_sound(Sound sound) {
    if (g_sound_boundary != nullptr
        && g_sound_boundary->played_count < g_sound_boundary->played.size()) {
        g_sound_boundary->played[g_sound_boundary->played_count++] =
            sound.frameCount;
    }
}
void fake_stop_sound(Sound) {}

[[nodiscard]] platform::AudioSoundApi formal_audio_api() noexcept {
    return {&real_load_wave, &real_wave_valid, &fake_load_sound,
        &fake_sound_valid, &real_unload_wave, &fake_unload_sound,
        &fake_play_sound, &fake_stop_sound};
}

class CurrentPathScope final {
public:
    explicit CurrentPathScope(const std::filesystem::path& next)
        : original_(std::filesystem::current_path()) {
        std::filesystem::current_path(next);
    }
    ~CurrentPathScope() {
        std::error_code ignored{};
        std::filesystem::current_path(original_, ignored);
    }
    CurrentPathScope(const CurrentPathScope&) = delete;
    CurrentPathScope& operator=(const CurrentPathScope&) = delete;
private:
    std::filesystem::path original_{};
};

struct FallbackTraceResult final {
    bool passed{};
    std::size_t fallback_count{};
    std::vector<std::string> traces{};
    std::size_t budget_allowed{};
};

[[nodiscard]] bool copy_audio_tree(const std::filesystem::path& root) {
    const auto source = std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}
        / "assets" / "stage14" / "audio";
    const auto destination = root / "assets" / "stage14" / "audio";
    std::error_code error{};
    std::filesystem::create_directories(destination.parent_path(), error);
    if (error) return false;
    std::filesystem::copy(source, destination,
        std::filesystem::copy_options::recursive, error);
    return !error;
}

[[nodiscard]] FallbackTraceResult run_fallback_and_trace(
    const std::filesystem::path& run_root) {
    FallbackTraceResult result{};
    const auto scratch = run_root / "fallback-scratch";
    if (!copy_audio_tree(scratch)) return result;
    const auto corrupt = scratch / "assets" / "stage14" / "audio"
        / "impact-1.wav";
    {
        std::fstream file(corrupt, std::ios::binary | std::ios::in | std::ios::out);
        if (!file) return result;
        file.write("BAD!", 4);
        if (!file) return result;
    }

    FakeSoundBoundary boundary{};
    g_sound_boundary = &boundary;
    bool pack_loaded{};
    bool fallback_exact{};
    bool trace_ok = true;
    {
        CurrentPathScope cwd{scratch};
        platform::AudioPack pack{formal_audio_api()};
        pack_loaded = pack.load();
        for (std::size_t index{}; index < kAssetCount; ++index) {
            const auto id = static_cast<platform::AudioAssetId>(index);
            if (pack.using_fallback(id)) ++result.fallback_count;
        }
        fallback_exact = result.fallback_count == 1U
            && pack.using_fallback(platform::AudioAssetId::impact_1);

        platform::AudioSelectionState selection{};
        platform::AudioPlaybackBudget budget{};
        for (std::size_t index{}; index < kTraceSpecs.size(); ++index) {
            const TraceSpec& spec = kTraceSpecs[index];
            combat::CombatEvent event{};
            event.kind = spec.kind;
            event.attack = spec.attack;
            event.feedback = spec.feedback;
            event.tick = 100U + static_cast<std::uint64_t>(index) * 20U;
            const platform::AudioPlan plan = platform::route_audio_plan(event);
            const bool routed = plan.cues == platform::audio_cue_mask(spec.cue);
            const bool allowed = routed && budget.allow(spec.cue, event.tick);
            const auto selected = allowed
                ? selection.select(spec.cue)
                : platform::AudioAssetId::count;
            const std::size_t plays_before = boundary.played_count;
            if (selected != platform::AudioAssetId::count) pack.play(selected);
            const bool played = boundary.played_count == plays_before + 1U
                && boundary.played[plays_before]
                    == static_cast<unsigned int>(selected) + 1U;
            if (allowed) ++result.budget_allowed;
            std::ostringstream line{};
            line << "trace|" << index << '|' << spec.cue_name << '|'
                 << spec.event_name << '|'
                 << (selected == platform::AudioAssetId::count
                    ? "invalid"
                    : kAssetNames[static_cast<std::size_t>(selected)])
                 << '|' << (allowed ? "ALLOW" : "REJECT") << '|'
                 << event.tick;
            result.traces.push_back(line.str());
            trace_ok = trace_ok && routed && allowed && played;
        }
        pack.unload();
    }
    g_sound_boundary = nullptr;
    result.passed = pack_loaded && fallback_exact && trace_ok
        && result.budget_allowed == kTraceSpecs.size()
        && boundary.next_handle == kAssetCount + 1U;
    return result;
}

[[nodiscard]] bool allowed_committed_root(const std::filesystem::path& root) {
    std::filesystem::path actual{};
    std::filesystem::path expected{};
    return canonical_path(root, actual)
        && canonical_path(std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}
            / "docs" / "validation" / "evidence"
            / "stage14-audio-material-pack", expected)
        && actual == expected;
}

[[nodiscard]] bool copy_committed_evidence(
    const std::filesystem::path& run_root,
    const std::filesystem::path& committed_root) {
    if (!allowed_committed_root(committed_root)) return false;
    std::error_code error{};
    std::filesystem::create_directories(committed_root, error);
    if (error) return false;
    for (const char* name : {"stage14-audio-evidence.txt",
            "stage14-audio-showcase.wav"}) {
        std::filesystem::copy_file(run_root / name, committed_root / name,
            std::filesystem::copy_options::overwrite_existing, error);
        if (error) return false;
    }
    return true;
}

[[nodiscard]] bool write_report(const std::filesystem::path& path,
    const std::vector<DecodedAsset>& decoded, std::uint64_t total_pcm_bytes,
    const FallbackTraceResult& trace, std::uint64_t showcase_frames,
    const std::string& showcase_hash) {
    std::ofstream report(path, std::ios::out | std::ios::trunc);
    report << "schema=stage14-audio-evidence-v1\n"
           << "asset_count=" << decoded.size() << '\n'
           << "pcm_total_bytes=" << total_pcm_bytes << '\n'
           << "pcm_budget_limit=" << kPcmBudget << '\n'
           << "single_resource_fallback="
           << (trace.passed ? "PASS" : "FAIL") << '\n'
           << "fallback_corrupt_id=impact_1\n"
           << "fallback_count=" << trace.fallback_count << '\n'
           << "cue_trace_count=" << trace.traces.size() << '\n'
           << "playback_budget_allowed=" << trace.budget_allowed << '\n'
           << "showcase_frames=" << showcase_frames << '\n'
           << "showcase_sha256=" << showcase_hash << '\n';
    for (const DecodedAsset& asset : decoded) {
        const std::uint64_t frames = asset.samples.size();
        const std::uint64_t duration_us =
            (frames * 1000000U + (kSampleRate / 2U)) / kSampleRate;
        report << "asset|" << asset.id_name << '|' << asset.path << '|'
               << asset.sha256 << '|' << kSampleRate << '|' << kSampleSize
               << '|' << kChannels << '|' << frames << '|' << duration_us
               << '|' << asset.peak << '|' << frames * 2U << '\n';
    }
    for (const std::string& line : trace.traces) report << line << '\n';
    report << "result=" << (trace.passed ? "PASS" : "FAIL") << '\n';
    return static_cast<bool>(report);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 3 && argv[1] != nullptr && argv[2] != nullptr
        && std::string{argv[1]} == "--root-safety-self-test") {
        return root_safety_self_test(std::filesystem::absolute(argv[2])) ? 0 : 1;
    }
    if (argc != 3 || argv[1] == nullptr || argv[2] == nullptr) return 2;

    const auto scratch_parent = std::filesystem::absolute(argv[1]);
    const auto committed_root = std::filesystem::absolute(argv[2]);
    std::filesystem::path run_root{};
    if (!prepare_run(scratch_parent, run_root)
        || !allowed_committed_root(committed_root)) return 3;

    std::vector<DecodedAsset> decoded{};
    std::uint64_t total_pcm_bytes{};
    if (!decode_assets(decoded, total_pcm_bytes)) return 4;
    const FallbackTraceResult trace = run_fallback_and_trace(run_root);
    if (!trace.passed) return 5;

    const auto showcase = run_root / "stage14-audio-showcase.wav";
    std::uint64_t showcase_frames{};
    std::string showcase_hash{};
    if (!write_showcase(showcase, decoded, showcase_frames)
        || !sha256_file(showcase, showcase_hash)
        || !write_report(run_root / "stage14-audio-evidence.txt", decoded,
            total_pcm_bytes, trace, showcase_frames, showcase_hash)
        || !copy_committed_evidence(run_root, committed_root)) return 6;

    std::cout << "stage14 audio formal PASS: 14 real LoadWave decodes, "
              << "one fallback, 14 cue traces, " << showcase_frames
              << " showcase frames\n";
    return 0;
}

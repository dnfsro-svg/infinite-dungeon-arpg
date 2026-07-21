#include "audio_scene.hpp"

#include <raylib.h>

#include <array>
#include <cmath>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iomanip>
#include <iostream>
#include <sstream>
#include <string>

namespace {

namespace platform = arpg::platform;

struct AssetSpec final {
    const char* name;
    const char* relative_path;
    bool stream;
};

constexpr std::array<AssetSpec, 10> kAssets{{
    {"music_explore", "assets/stage15/audio/music-explore.ogg", true},
    {"music_combat", "assets/stage15/audio/music-combat.ogg", true},
    {"ambience_room", "assets/stage15/audio/ambience-room.ogg", true},
    {"ambience_abyss", "assets/stage15/audio/ambience-abyss.ogg", true},
    {"ui_navigate", "assets/stage15/audio/ui-navigate.wav", false},
    {"ui_confirm", "assets/stage15/audio/ui-confirm.wav", false},
    {"ui_cancel", "assets/stage15/audio/ui-cancel.wav", false},
    {"ui_open", "assets/stage15/audio/ui-open.wav", false},
    {"ui_close", "assets/stage15/audio/ui-close.wav", false},
    {"ui_reward", "assets/stage15/audio/ui-reward.wav", false},
}};

[[nodiscard]] bool within_or_equal(const std::filesystem::path& child,
    const std::filesystem::path& parent) {
    const auto relative = child.lexically_relative(parent);
    return relative == "." || (!relative.empty() && !relative.is_absolute()
        && *relative.begin() != "..");
}

[[nodiscard]] bool canonical(const std::filesystem::path& value,
    std::filesystem::path& result) {
    std::error_code error{};
    result = std::filesystem::weakly_canonical(
        std::filesystem::absolute(value, error), error).lexically_normal();
    return !error && !result.empty();
}

[[nodiscard]] bool allowed_output(const std::filesystem::path& value) {
    std::filesystem::path candidate{};
    std::filesystem::path source{};
    if (!canonical(value, candidate)
        || !canonical(ARPG_PROJECT_SOURCE_DIR, source)) return false;
    return within_or_equal(candidate, source / "out")
        || within_or_equal(candidate,
            source / "docs/validation/evidence/stage15-audio-mix");
}

[[nodiscard]] std::string scene_trace() {
    platform::AudioSceneState scene{};
    const platform::AudioBusLevels levels{80U, 75U, 50U, 40U, 90U};
    const auto explore = scene.update({}, levels, 0.016F);
    const auto combat = scene.update({true, true, false, false}, levels, 0.40F);
    const auto death = scene.update({true, true, false, true}, levels, 0.40F);
    const bool pass = explore.music == platform::MusicTrack::explore
        && explore.ambience == platform::AmbienceTrack::room
        && combat.music == platform::MusicTrack::combat
        && combat.ambience == platform::AmbienceTrack::abyss
        && std::abs(combat.sfx_gain - 0.60F) < 0.0001F
        && std::abs(combat.ui_gain - 0.72F) < 0.0001F
        && death.music_gain[1] < combat.music_gain[1];
    return pass ? "PASS" : "FAIL";
}

[[nodiscard]] bool collect(std::string& evidence) {
    if (!IsAudioDeviceReady()) InitAudioDevice();
    if (!IsAudioDeviceReady()) return false;

    std::ostringstream out{};
    out << std::fixed << std::setprecision(6);
    out << "schema=stage15-audio-evidence-v1\n";
    out << "asset_count=10\n";
    out << "stream_count=4\n";
    out << "ui_count=6\n";
    out << "audio_device=ready\n";
    std::uintmax_t total_bytes{};
    std::size_t playing_streams{};
    std::size_t valid_ui{};

    for (const auto& asset : kAssets) {
        const std::filesystem::path path =
            std::filesystem::path{ARPG_PROJECT_SOURCE_DIR} / asset.relative_path;
        std::error_code error{};
        const std::uintmax_t size = std::filesystem::file_size(path, error);
        if (error || size == 0U) return false;
        total_bytes += size;
        if (asset.stream) {
            Music music = LoadMusicStream(path.string().c_str());
            if (!IsMusicValid(music)) return false;
            music.looping = true;
            SetMusicVolume(music, 0.01F);
            PlayMusicStream(music);
            UpdateMusicStream(music);
            const bool playing = IsMusicStreamPlaying(music);
            if (playing) ++playing_streams;
            out << "asset|" << asset.name << '|' << asset.relative_path
                << "|stream|" << size << '|' << music.stream.sampleRate << '|'
                << music.stream.sampleSize << '|' << music.stream.channels << '|'
                << GetMusicTimeLength(music) << '|' << (playing ? "PLAY" : "STOP")
                << "\n";
            StopMusicStream(music);
            UnloadMusicStream(music);
        } else {
            Wave wave = LoadWave(path.string().c_str());
            if (!IsWaveValid(wave)) return false;
            Sound sound = LoadSoundFromWave(wave);
            const bool valid = IsSoundValid(sound);
            if (valid) {
                ++valid_ui;
                SetSoundVolume(sound, 0.01F);
                PlaySound(sound);
                StopSound(sound);
            }
            out << "asset|" << asset.name << '|' << asset.relative_path
                << "|ui|" << size << '|' << wave.sampleRate << '|'
                << wave.sampleSize << '|' << wave.channels << '|'
                << static_cast<double>(wave.frameCount) / wave.sampleRate << '|'
                << (valid ? "VALID" : "INVALID") << "\n";
            if (valid) UnloadSound(sound);
            UnloadWave(wave);
            if (!valid) return false;
        }
    }

    out << "file_total_bytes=" << total_bytes << "\n";
    out << "file_budget_limit=16777216\n";
    out << "playing_streams=" << playing_streams << "\n";
    out << "valid_ui=" << valid_ui << "\n";
    out << "fallback_count=0\n";
    out << "scene_route=" << scene_trace() << "\n";
    const bool pass = total_bytes <= 16777216U && playing_streams == 4U
        && valid_ui == 6U && scene_trace() == "PASS";
    out << "result=" << (pass ? "PASS" : "FAIL") << "\n";
    evidence = out.str();
    CloseAudioDevice();
    return pass;
}

[[nodiscard]] bool write_evidence(const std::filesystem::path& directory,
    const std::string& evidence) {
    if (!allowed_output(directory)) return false;
    std::error_code error{};
    std::filesystem::create_directories(directory, error);
    if (error) return false;
    std::ofstream output{directory / "stage15-audio-evidence.txt",
        std::ios::binary | std::ios::trunc};
    output << evidence;
    return static_cast<bool>(output);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc == 3 && std::string{argv[1]} == "--root-safety-self-test") {
        const std::filesystem::path outside =
            std::filesystem::path{ARPG_PROJECT_SOURCE_DIR}.parent_path()
                / "stage15-forbidden-output";
        const bool rejected = !write_evidence(outside, "forbidden");
        std::cout << (rejected ? "root-safety=PASS\n" : "root-safety=FAIL\n");
        return rejected ? 0 : 1;
    }
    if (argc != 3) {
        std::cerr << "usage: formal <scratch-parent> <committed-directory>\n";
        return 2;
    }
    std::string evidence{};
    const bool collected = collect(evidence);
    const bool scratch = collected && write_evidence(
        std::filesystem::path{argv[1]} / "stage15-run", evidence);
    const bool committed = scratch && write_evidence(argv[2], evidence);
    std::cout << evidence;
    return committed ? 0 : 1;
}

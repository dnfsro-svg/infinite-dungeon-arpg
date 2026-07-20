#include "stream_pack.hpp"

#include <algorithm>
#include <array>
#include <cstddef>

namespace arpg::platform {
namespace {

constexpr std::array<const char*, static_cast<std::size_t>(StreamAssetId::count)> kPaths{{
    "assets/stage15/audio/music-explore.ogg",
    "assets/stage15/audio/music-combat.ogg",
    "assets/stage15/audio/ambience-room.ogg",
    "assets/stage15/audio/ambience-abyss.ogg",
}};

[[nodiscard]] constexpr bool known(StreamAssetId id) noexcept {
    return static_cast<std::size_t>(id) < kPaths.size();
}

[[nodiscard]] constexpr bool valid_api(MusicStreamApi api) noexcept {
    return api.load && api.valid && api.unload && api.play && api.playing
        && api.update && api.stop && api.set_volume;
}

[[nodiscard]] MusicStreamApi native_api() noexcept {
    return {&LoadMusicStream, &IsMusicValid, &UnloadMusicStream,
        &PlayMusicStream, &IsMusicStreamPlaying, &UpdateMusicStream,
        &StopMusicStream, &SetMusicVolume};
}

}  // namespace

StreamPack::StreamPack() noexcept : StreamPack(native_api()) {}
StreamPack::StreamPack(MusicStreamApi api) noexcept : api_(api) {}
StreamPack::~StreamPack() noexcept { unload(); }

bool StreamPack::load() noexcept {
    unload();
    if (!valid_api(api_)) return false;
    for (std::size_t index{}; index < kCount; ++index) {
        Music stream = api_.load(kPaths[index]);
        if (!api_.valid(stream)) continue;
        stream.looping = true;
        music_[index] = stream;
        available_[index] = true;
    }
    return true;
}

void StreamPack::play(StreamAssetId id) noexcept {
    if (!known(id) || !valid_api(api_)) return;
    const auto index = static_cast<std::size_t>(id);
    if (available_[index] && api_.valid(music_[index]) && !api_.playing(music_[index])) {
        api_.play(music_[index]);
    }
}

void StreamPack::update() noexcept {
    if (!valid_api(api_)) return;
    for (std::size_t index{}; index < kCount; ++index) {
        if (available_[index] && api_.valid(music_[index]) && api_.playing(music_[index])) {
            api_.update(music_[index]);
        }
    }
}

void StreamPack::set_volume(StreamAssetId id, float volume) noexcept {
    if (!known(id) || !valid_api(api_)) return;
    const auto index = static_cast<std::size_t>(id);
    if (available_[index] && api_.valid(music_[index])) {
        api_.set_volume(music_[index], std::clamp(volume, 0.0F, 1.0F));
    }
}

void StreamPack::stop_all() noexcept {
    if (!valid_api(api_)) return;
    for (std::size_t index{}; index < kCount; ++index) {
        if (available_[index] && api_.valid(music_[index]) && api_.playing(music_[index])) {
            api_.stop(music_[index]);
        }
    }
}

void StreamPack::unload() noexcept {
    for (std::size_t index{}; index < kCount; ++index) {
        if (available_[index] && api_.valid && api_.unload && api_.valid(music_[index])) {
            api_.unload(music_[index]);
        }
        music_[index] = {};
        available_[index] = false;
    }
}

bool StreamPack::available(StreamAssetId id) const noexcept {
    return known(id) && available_[static_cast<std::size_t>(id)];
}

}  // namespace arpg::platform

#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::platform {

enum class StreamAssetId : std::uint8_t {
    music_explore,
    music_combat,
    ambience_room,
    ambience_abyss,
    count
};

struct MusicStreamApi final {
    Music (*load)(const char* path){};
    bool (*valid)(Music music){};
    void (*unload)(Music music){};
    void (*play)(Music music){};
    bool (*playing)(Music music){};
    void (*update)(Music music){};
    void (*stop)(Music music){};
    void (*set_volume)(Music music, float volume){};
};

class StreamPack final {
public:
    StreamPack() noexcept;
    explicit StreamPack(MusicStreamApi api) noexcept;
    ~StreamPack() noexcept;

    StreamPack(const StreamPack&) = delete;
    StreamPack& operator=(const StreamPack&) = delete;

    [[nodiscard]] bool load() noexcept;
    void play(StreamAssetId id) noexcept;
    void update() noexcept;
    void set_volume(StreamAssetId id, float volume) noexcept;
    void stop_all() noexcept;
    void unload() noexcept;
    [[nodiscard]] bool available(StreamAssetId id) const noexcept;

private:
    static constexpr std::size_t kCount = static_cast<std::size_t>(StreamAssetId::count);
    MusicStreamApi api_{};
    std::array<Music, kCount> music_{};
    std::array<bool, kCount> available_{};
};

}  // namespace arpg::platform

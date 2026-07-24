#pragma once

#include <raylib.h>

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::platform {

enum class ActiveSkillAtlasId : std::uint8_t {
    draw_slash,
    storm_swords,
    count,
};

struct ActiveSkillAtlasDefinition final {
    ActiveSkillAtlasId id{ActiveSkillAtlasId::draw_slash};
    const char* path{};
    int width{};
    int height{};
    std::uint8_t columns{};
    std::uint8_t rows{};
    std::size_t frame_count{};
};

struct ActiveSkillAtlasFrame final {
    Rectangle source{};
    Vector2 foot_anchor{};
    Vector2 weapon_anchor{};
};

[[nodiscard]] const ActiveSkillAtlasDefinition* active_skill_atlas_definition(
    ActiveSkillAtlasId id) noexcept;
[[nodiscard]] std::optional<ActiveSkillAtlasFrame> active_skill_atlas_frame(
    ActiveSkillAtlasId id, std::size_t frame_index) noexcept;

class ActiveSkillAssets final {
public:
    [[nodiscard]] bool load() noexcept;
    void unload() noexcept;
    [[nodiscard]] bool ready(ActiveSkillAtlasId id) const noexcept;
    [[nodiscard]] bool draw(ActiveSkillAtlasId id, std::size_t frame_index,
        Vector2 foot_position, bool flip_x, float scale, Color tint) const noexcept;

private:
    std::array<Texture2D, static_cast<std::size_t>(ActiveSkillAtlasId::count)>
        textures_{};
};

}  // namespace arpg::platform

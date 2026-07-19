#pragma once

#include "hud_view_model.hpp"

#include <cstdint>

namespace arpg::platform {

struct LootPickupFeedback final {
    bool ready{};
    bool abyss{};
    HudText96 text{};
    std::uint64_t item_id{};
};

class LootPickupFeedbackState final {
public:
    [[nodiscard]] LootPickupFeedback observe(
        const DungeonRenderStatus&) noexcept;

private:
    enum class Attachment : std::uint8_t {
        unattached,
        live,
    };
    std::uint64_t generation_{};
    std::uint64_t item_id_{};
    bool has_high_water_{};
    bool preserve_attachment_after_block_{};
    Attachment attachment_{Attachment::unattached};
};

}  // namespace arpg::platform

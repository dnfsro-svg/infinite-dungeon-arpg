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
        empty_observed,
        receipt_baseline,
    };
    std::uint64_t generation_{};
    std::uint64_t item_id_{};
    Attachment attachment_{Attachment::unattached};
};

}  // namespace arpg::platform

#pragma once

#include "combat/combat_types.hpp"
#include "combat/input_buffer.hpp"

#include <array>
#include <cstdint>

namespace arpg::combat {

class CombatWorld final {
public:
    explicit CombatWorld(CombatLabConfig config = {}) noexcept;

    [[nodiscard]] bool queue_action(Action action) noexcept;
    void tick(MovementInput movement) noexcept;
    void reset() noexcept;
    [[nodiscard]] CombatSnapshot snapshot() const noexcept;

private:
    struct PlayerRuntime final {
        Vec3 position{};
        Vec3 velocity{};
        Facing facing{Facing::right};
        PlayerState state{PlayerState::idle};
        std::uint8_t combo_stage{};
        std::uint16_t hit_stop_ticks{};
        bool air_attack_available{true};
    };

    struct DummyRuntime final {
        DummyKind kind{DummyKind::light};
        Vec3 spawn{};
        Vec3 position{};
        Vec3 velocity{};
        ReactionState reaction{ReactionState::idle};
        ArmorState armor{ArmorState::none};
        std::uint16_t reaction_ticks{};
        std::uint16_t break_window_ticks{};
        std::uint16_t hit_stop_ticks{};
        int hp{};
        int max_hp{};
        int break_value{};
        int max_break{};
    };

    struct AttackRuntime final {
        AttackId id{AttackId::none};
        std::uint16_t elapsed_ticks{};
        std::uint64_t serial{};
        bool connected{};
        bool impact_event_emitted{};
        std::array<bool, kDummyCount> hit_targets{};
    };

    void simulate_player(MovementInput movement) noexcept;

    CombatLabConfig config_{};
    PlayerRuntime player_{};
    std::array<DummyRuntime, kDummyCount> dummies_{};
    AttackRuntime attack_{};
    InputBuffer input_buffer_{};
    std::uint64_t tick_{};
    std::uint32_t event_overflow_count_{};
};

}  // namespace arpg::combat

#pragma once

#include "combat/combat_types.hpp"
#include "combat/input_buffer.hpp"
#include "core/bounded_queue.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::combat {

class CombatWorld final {
public:
    explicit CombatWorld(CombatLabConfig config = {}) noexcept;

    [[nodiscard]] bool queue_action(Action action) noexcept;
    void tick(MovementInput movement) noexcept;
    void reset() noexcept;
    [[nodiscard]] CombatSnapshot snapshot() const noexcept;
    [[nodiscard]] std::optional<CombatEvent> try_pop_event() noexcept;

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
        ImpactKind pending_impact{ImpactKind::light_hitstun};
        bool has_pending_impact{};
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
    void simulate_target(std::size_t index) noexcept;
    void apply_dummy_impact(
        std::size_t index,
        const AttackDefinition& definition) noexcept;
    void respawn_dummy(std::size_t index) noexcept;
    void apply_attack_assist(const AttackDefinition& definition) noexcept;
    void resolve_attack_hits() noexcept;
    void emit_event(const CombatEvent& event) noexcept;

    CombatLabConfig config_{};
    PlayerRuntime player_{};
    std::array<DummyRuntime, kDummyCount> dummies_{};
    AttackRuntime attack_{};
    InputBuffer input_buffer_{};
    core::BoundedQueue<CombatEvent, 64> events_{};
    std::uint64_t tick_{};
    std::uint32_t event_overflow_count_{};
};

}  // namespace arpg::combat

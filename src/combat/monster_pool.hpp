#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::combat {

class CombatWorld;

struct MonsterRuntime final {
    bool active{};
    std::uint16_t generation{};
    MonsterId id{MonsterId::chaos_chaser};
    DummyKind kind{DummyKind::normal};
    Vec3 spawn{};
    Vec3 position{};
    Vec3 velocity{};
    Facing facing{Facing::right};
    ReactionState reaction{ReactionState::idle};
    ArmorState armor{ArmorState::none};
    std::uint16_t reaction_ticks{};
    MonsterAiPhase ai_phase{MonsterAiPhase::idle};
    std::uint16_t ai_ticks{};
    std::uint32_t attack_serial{};
    bool contact_attack_resolved{};
    int hp{};
    int max_hp{};
    int break_value{};
    int max_break{};
    int shield{};
    std::uint16_t shield_ticks{};
    std::uint16_t break_window_ticks{};
    std::uint16_t hit_stop_ticks{};
    std::uint16_t owner_transient_counter{};
};

struct MonsterHandle final {
    std::uint16_t index{0xFFFF};
    std::uint16_t generation{};
};

class MonsterPool final {
public:
    void clear() noexcept;
    [[nodiscard]] std::optional<MonsterHandle> spawn(
        MonsterId id, Vec3 position) noexcept;
    [[nodiscard]] bool destroy(MonsterHandle handle) noexcept;
    [[nodiscard]] std::size_t active_count() const noexcept;
    [[nodiscard]] MonsterRuntime* get(MonsterHandle handle) noexcept;
    [[nodiscard]] const MonsterRuntime* get(MonsterHandle handle) const noexcept;
    [[nodiscard]] const std::array<MonsterRuntime, kMonsterCapacity>&
    slots() const noexcept;

private:
    friend class CombatWorld;
    static void advance_generation(MonsterRuntime& runtime) noexcept;

    std::array<MonsterRuntime, kMonsterCapacity> slots_{};
    std::size_t active_count_{};
};

}  // namespace arpg::combat

#pragma once

#include "modifiers/modifier_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::modifiers {

inline constexpr std::size_t kEffectCapacity = 8U;
inline constexpr std::size_t kEffectCommandCapacity = 16U;

using EffectId = std::uint32_t;

enum class RefreshRule : std::uint8_t {
    reject,
    refresh_duration,
    add_stack,
    replace_weaker,
};

enum class EffectCommandKind : std::uint8_t {
    none,
    set_shield,
    clear_shield,
};

struct EffectCommandTemplate final {
    EffectCommandKind kind{EffectCommandKind::none};
    FixedValue value{};
};

struct EffectCommand final {
    EffectCommandKind kind{EffectCommandKind::none};
    FixedValue value{};
    EffectId effect_id{};
};

struct EffectDefinition final {
    EffectId id{};
    int duration_ticks{};
    RefreshRule refresh_rule{RefreshRule::reject};
    std::uint8_t max_stacks{1};
    FixedValue strength{};
    Modifier modifier{};
    bool has_modifier{};
    EffectCommandTemplate on_apply{};
    EffectCommandTemplate on_refresh{};
    EffectCommandTemplate on_expire{};
};

enum class ApplyResult : std::uint8_t {
    applied,
    refreshed,
    stacked,
    rejected,
    capacity_rejected,
};

struct EffectDiagnostics final {
    std::uint32_t effect_overflows{};
    std::uint32_t command_overflows{};
};

struct ActiveEffectCheckpoint final {
    EffectId id{};
    std::int32_t remaining_ticks{};
    std::uint8_t stacks{};
    std::uint8_t max_stacks{};
    RefreshRule refresh_rule{RefreshRule::reject};
    FixedValue strength{};
    Modifier modifier{};
    bool has_modifier{};
    EffectCommandTemplate on_expire{};
    bool occupied{};
};

struct EffectSetCheckpoint final {
    std::array<ActiveEffectCheckpoint, kEffectCapacity> effects{};
    std::array<EffectCommand, kEffectCommandCapacity> commands{};
    std::uint8_t command_head{};
    std::uint8_t command_count{};
    EffectDiagnostics diagnostics{};
};

class EffectSet final {
public:
    static constexpr std::size_t kCapacity = kEffectCapacity;
    static constexpr std::size_t kCommandCapacity = kEffectCommandCapacity;
    static constexpr std::size_t capacity() noexcept { return kCapacity; }
    static constexpr std::size_t command_capacity() noexcept {
        return kCommandCapacity;
    }

    ApplyResult apply(const EffectDefinition& definition) noexcept;
    void tick() noexcept;
    bool remove(EffectId id) noexcept;
    void clear() noexcept;

    [[nodiscard]] std::size_t active_count() const noexcept;
    [[nodiscard]] int remaining_ticks(EffectId id) const noexcept;
    [[nodiscard]] std::uint8_t stack_count(EffectId id) const noexcept;
    [[nodiscard]] std::size_t queued_command_count() const noexcept;
    [[nodiscard]] const EffectDiagnostics& diagnostics() const noexcept;
    [[nodiscard]] bool same_state(const EffectSet& other) const noexcept;
    void capture_checkpoint(EffectSetCheckpoint& out) const noexcept;
    [[nodiscard]] bool restore_checkpoint(
        const EffectSetCheckpoint& checkpoint) noexcept;
    std::size_t copy_modifiers(
        std::array<Modifier, kCapacity>& output) const noexcept;
    bool pop_command(EffectCommand& command) noexcept;

private:
    struct ActiveEffect final {
        EffectId id{};
        int remaining_ticks{};
        std::uint8_t stacks{};
        std::uint8_t max_stacks{};
        RefreshRule refresh_rule{RefreshRule::reject};
        FixedValue strength{};
        Modifier modifier{};
        bool has_modifier{};
        EffectCommandTemplate on_expire{};
        bool occupied{};
    };

    ActiveEffect* find(EffectId id) noexcept;
    const ActiveEffect* find(EffectId id) const noexcept;
    void emit(EffectCommandTemplate command, EffectId id) noexcept;

    std::array<ActiveEffect, kCapacity> effects_{};
    std::array<EffectCommand, kCommandCapacity> commands_{};
    std::size_t command_head_{};
    std::size_t command_count_{};
    EffectDiagnostics diagnostics_{};
};

}  // namespace arpg::modifiers

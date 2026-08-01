#pragma once

#include "combat/combat_types.hpp"

#include <array>
#include <cstddef>
#include <cstdint>
#include <optional>

namespace arpg::combat {

struct CombatDefeatRecord final {
    MonsterOrdinal monster_ordinal{kInvalidMonsterOrdinal};
    MonsterId monster_id{MonsterId::count};
    std::uint16_t affix_score{};
    bool reward_eligible{};
};

class CombatDefeatLedger final {
public:
    void clear() noexcept;
    [[nodiscard]] bool append(CombatDefeatRecord record) noexcept;
    [[nodiscard]] std::optional<CombatDefeatRecord> try_pop() noexcept;
    [[nodiscard]] std::size_t size() const noexcept;
    [[nodiscard]] bool overflowed() const noexcept;

private:
    std::array<CombatDefeatRecord, kDefeatLedgerCapacity> records_{};
    std::size_t head_{};
    std::size_t count_{};
    bool overflowed_{};
};

}  // namespace arpg::combat

#include "combat/combat_defeat_ledger.hpp"

namespace arpg::combat {

void CombatDefeatLedger::clear() noexcept {
    records_ = {};
    head_ = 0U;
    count_ = 0U;
    overflowed_ = false;
}

bool CombatDefeatLedger::append(CombatDefeatRecord record) noexcept {
    if (record.monster_ordinal == kInvalidMonsterOrdinal
        || count_ == records_.size()) {
        overflowed_ = count_ == records_.size();
        return false;
    }
    records_[(head_ + count_) % records_.size()] = record;
    ++count_;
    return true;
}

std::optional<CombatDefeatRecord> CombatDefeatLedger::try_pop() noexcept {
    if (count_ == 0U) return std::nullopt;
    const CombatDefeatRecord record = records_[head_];
    records_[head_] = {};
    head_ = (head_ + 1U) % records_.size();
    --count_;
    return record;
}

std::size_t CombatDefeatLedger::size() const noexcept { return count_; }

bool CombatDefeatLedger::overflowed() const noexcept { return overflowed_; }

}  // namespace arpg::combat

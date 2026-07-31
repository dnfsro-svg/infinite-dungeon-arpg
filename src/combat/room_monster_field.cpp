#include "combat/room_monster_field.hpp"

#include "combat/monster_catalog.hpp"

#include <algorithm>
#include <cstddef>

namespace arpg::combat {
namespace {

[[nodiscard]] constexpr MonsterHandle invalid_handle() noexcept {
    return {0xFFFFU, 0U};
}

[[nodiscard]] bool same_region(
    const RoomStreamingRegion left,
    const RoomStreamingRegion right) noexcept {
    return left.first_column == right.first_column
        && left.column_count == right.column_count
        && left.first_row == right.first_row
        && left.row_count == right.row_count;
}

}  // namespace

RoomMonsterField::RoomMonsterField() noexcept {
    ordinal_to_handle_.fill(invalid_handle());
}

RoomMonsterField::RoomMonsterField(MonsterPool& active_pool) noexcept
    : active_pool_(&active_pool) {
    ordinal_to_handle_.fill(invalid_handle());
}

RoomMonsterPlan& RoomMonsterField::plan_storage_for_construction() noexcept {
    return plan_;
}

bool RoomMonsterField::plan_shape_valid(
    const std::uint16_t expected_count) const noexcept {
    if (expected_count != plan_.monster_count
        || plan_.monster_count > limits::kRoomMonsterCapacity
        || plan_.cell_offsets.front() != 0U
        || plan_.cell_offsets.back() != plan_.monster_count) {
        return false;
    }
    for (std::size_t cell = 0U; cell < kRoomMonsterCellCount; ++cell) {
        const std::uint16_t begin = plan_.cell_offsets[cell];
        const std::uint16_t end = plan_.cell_offsets[cell + 1U];
        if (begin > end || end > plan_.monster_count
            || end - begin != plan_.cell_counts[cell]) {
            return false;
        }
        for (std::uint16_t ordinal = begin; ordinal < end; ++ordinal) {
            const RoomMonsterBlueprint& blueprint = plan_.monsters[ordinal];
            if (blueprint.spawn_ordinal != ordinal
                || blueprint.home_cell != cell
                || blueprint.id >= MonsterId::count) {
                return false;
            }
        }
    }
    return true;
}

RoomMonsterFieldFault RoomMonsterField::seal_plan(
    const std::uint16_t expected_count) noexcept {
    active_pool_->clear();
    clear_residency();
    for (MonsterPersistentState& state : states_) {
        state = MonsterPersistentState{};
    }
    defeated_count_ = 0U;
    abyss_config_ = {};
    sealed_ = false;
    fault_ = RoomMonsterFieldFault::none;
    if (!plan_shape_valid(expected_count)) {
        fault_ = RoomMonsterFieldFault::invalid_plan;
        return fault_;
    }

    if (!initialize_persistent_states()) {
        fault_ = RoomMonsterFieldFault::active_pool_failure;
        return fault_;
    }
    sealed_ = true;
    return RoomMonsterFieldFault::none;
}

bool RoomMonsterField::initialize_persistent_states() noexcept {
    active_pool_->clear();
    for (MonsterPersistentState& state : states_) {
        state = MonsterPersistentState{};
    }
    for (MonsterOrdinal ordinal = 0U; ordinal < plan_.monster_count;
         ++ordinal) {
        const RoomMonsterBlueprint& blueprint = plan_.monsters[ordinal];
        MonsterSpawnSpec spec{};
        spec.id = blueprint.id;
        spec.position = blueprint.initial_position;
        spec.affixes = blueprint.affixes;
        spec.spawn_ordinal = ordinal;
        const std::optional<MonsterHandle> handle = active_pool_->spawn(
            spec, abyss_config_);
        if (!handle.has_value()) {
            active_pool_->clear();
            for (MonsterPersistentState& state : states_) {
                state = MonsterPersistentState{};
            }
            return false;
        }
        MonsterRuntime* runtime = active_pool_->get(*handle);
        if (runtime == nullptr) {
            active_pool_->clear();
            for (MonsterPersistentState& state : states_) {
                state = MonsterPersistentState{};
            }
            return false;
        }
        store_monster_persistent_state(*runtime, states_[ordinal], false);
        static_cast<void>(active_pool_->destroy(*handle));
    }
    active_pool_->clear();
    return true;
}

RoomMonsterFieldFault RoomMonsterField::apply_abyss_config_for_construction(
    const abyss::AbyssCombatConfig& config) noexcept {
    if (!sealed_ || region_active_ || defeated_count_ != 0U) {
        fault_ = RoomMonsterFieldFault::invalid_plan;
        return fault_;
    }
    for (MonsterOrdinal ordinal = 0U; ordinal < plan_.monster_count;
         ++ordinal) {
        if (states_[ordinal].touched || states_[ordinal].defeated) {
            fault_ = RoomMonsterFieldFault::invalid_plan;
            return fault_;
        }
    }
    abyss_config_ = config;
    if (!initialize_persistent_states()) {
        sealed_ = false;
        fault_ = RoomMonsterFieldFault::active_pool_failure;
        return fault_;
    }
    fault_ = RoomMonsterFieldFault::none;
    return fault_;
}

RoomResidentOrdinals RoomMonsterField::required_residents(
    const RoomStreamingRegion region) const noexcept {
    RoomResidentOrdinals result{};
    if (!sealed_ || region.column_count == 0U || region.row_count == 0U
        || static_cast<std::size_t>(region.first_column)
                + region.column_count > room_spatial::columns
        || static_cast<std::size_t>(region.first_row)
                + region.row_count > room_spatial::rows) {
        result.fault = RoomMonsterFieldFault::invalid_plan;
        return result;
    }
    for (std::size_t row = region.first_row;
         row < static_cast<std::size_t>(region.first_row) + region.row_count;
         ++row) {
        for (std::size_t column = region.first_column;
             column < static_cast<std::size_t>(region.first_column)
                    + region.column_count;
             ++column) {
            const std::size_t cell = row * room_spatial::columns + column;
            const std::uint16_t begin = plan_.cell_offsets[cell];
            const std::uint16_t end = plan_.cell_offsets[cell + 1U];
            if (begin > end || end > plan_.monster_count) {
                result.fault = RoomMonsterFieldFault::invalid_plan;
                result.count = 0U;
                return result;
            }
            for (std::uint16_t ordinal = begin; ordinal < end; ++ordinal) {
                if (states_[ordinal].defeated) continue;
                if (result.count >= result.ordinals.size()) {
                    result.fault =
                        RoomMonsterFieldFault::monster_residency_capacity;
                    result.count = 0U;
                    return result;
                }
                result.ordinals[result.count++] = ordinal;
            }
        }
    }
    if (result.count > room_spatial::maximum_streaming_monsters) {
        result.fault = RoomMonsterFieldFault::monster_residency_capacity;
        result.count = 0U;
        return result;
    }
    std::sort(result.ordinals.begin(),
        result.ordinals.begin() + result.count);
    for (std::size_t index = 1U; index < result.count; ++index) {
        if (result.ordinals[index - 1U] == result.ordinals[index]) {
            result.fault = RoomMonsterFieldFault::invalid_plan;
            result.count = 0U;
            return result;
        }
    }
    return result;
}

void RoomMonsterField::write_back_active() noexcept {
    for (std::size_t index = 0U; index < residents_.count; ++index) {
        const MonsterOrdinal ordinal = residents_.ordinals[index];
        if (ordinal >= plan_.monster_count) continue;
        const MonsterHandle handle = ordinal_to_handle_[ordinal];
        const MonsterRuntime* runtime = active_pool_->get(handle);
        if (runtime != nullptr) {
            const bool changed = !monster_persistent_state_matches(
                *runtime, states_[ordinal]);
            store_monster_persistent_state(
                *runtime, states_[ordinal], changed);
        }
    }
}

void RoomMonsterField::clear_residency() noexcept {
    for (std::size_t index = 0U; index < residents_.count; ++index) {
        const MonsterOrdinal ordinal = residents_.ordinals[index];
        if (ordinal < ordinal_to_handle_.size()) {
            ordinal_to_handle_[ordinal] = invalid_handle();
        }
    }
    residents_ = {};
    region_active_ = false;
}

bool RoomMonsterField::synchronize_active_region(
    const RoomStreamingRegion region) noexcept {
    if (region_active_ && same_region(region, active_region_)) return true;
    const RoomResidentOrdinals desired = required_residents(region);
    if (desired.fault != RoomMonsterFieldFault::none) {
        fault_ = desired.fault;
        return false;
    }

    write_back_active();
    active_pool_->clear();
    clear_residency();
    for (std::size_t index = 0U; index < desired.count; ++index) {
        const MonsterOrdinal ordinal = desired.ordinals[index];
        const MonsterPersistentState& state = states_[ordinal];
        MonsterSpawnSpec spec{};
        spec.id = state.id;
        spec.position = state.position;
        spec.affixes = state.affixes;
        spec.spawn_ordinal = ordinal;
        const std::optional<MonsterHandle> handle = active_pool_->spawn(
            spec, abyss_config_);
        if (!handle.has_value()) {
            active_pool_->clear();
            clear_residency();
            fault_ = RoomMonsterFieldFault::active_pool_failure;
            return false;
        }
        MonsterRuntime* runtime = active_pool_->get(*handle);
        if (runtime == nullptr) {
            active_pool_->clear();
            clear_residency();
            fault_ = RoomMonsterFieldFault::active_pool_failure;
            return false;
        }
        restore_monster_persistent_state(state, *runtime);
        ordinal_to_handle_[ordinal] = *handle;
        residents_.ordinals[residents_.count++] = ordinal;
    }
    active_region_ = region;
    region_active_ = true;
    fault_ = RoomMonsterFieldFault::none;
    return true;
}

bool RoomMonsterField::mark_defeated(const MonsterOrdinal ordinal) noexcept {
    if (!sealed_ || ordinal >= plan_.monster_count) {
        fault_ = RoomMonsterFieldFault::invalid_ordinal;
        return false;
    }
    MonsterPersistentState& state = states_[ordinal];
    if (state.defeated) return false;
    state.defeated = true;
    state.touched = true;
    state.hp = 0;
    state.reaction = ReactionState::defeated;
    state.ai_phase = MonsterAiPhase::defeated;
    if (MonsterRuntime* runtime = active_runtime(ordinal)) {
        runtime->hp = 0;
        runtime->reaction = ReactionState::defeated;
        runtime->ai_phase = MonsterAiPhase::defeated;
    }
    ++defeated_count_;
    return true;
}

std::uint32_t RoomMonsterField::total_count() const noexcept {
    return sealed_ ? plan_.monster_count : 0U;
}

std::uint32_t RoomMonsterField::living_count() const noexcept {
    return total_count() - defeated_count_;
}

std::uint32_t RoomMonsterField::defeated_count() const noexcept {
    return defeated_count_;
}

RoomMonsterFieldFault RoomMonsterField::fault() const noexcept {
    return fault_;
}

const RoomMonsterPlan& RoomMonsterField::plan() const noexcept { return plan_; }

const MonsterPersistentState* RoomMonsterField::persistent_state(
    const MonsterOrdinal ordinal) const noexcept {
    return sealed_ && ordinal < plan_.monster_count
        ? &states_[ordinal] : nullptr;
}

MonsterPersistentState* RoomMonsterField::persistent_state(
    const MonsterOrdinal ordinal) noexcept {
    return const_cast<MonsterPersistentState*>(
        static_cast<const RoomMonsterField&>(*this).persistent_state(ordinal));
}

std::optional<MonsterHandle> RoomMonsterField::resident_handle(
    const MonsterOrdinal ordinal) const noexcept {
    if (ordinal >= plan_.monster_count) return std::nullopt;
    const MonsterHandle handle = ordinal_to_handle_[ordinal];
    return active_pool_->get(handle) == nullptr
        ? std::nullopt : std::optional<MonsterHandle>{handle};
}

MonsterRuntime* RoomMonsterField::active_runtime(
    const MonsterOrdinal ordinal) noexcept {
    const std::optional<MonsterHandle> handle = resident_handle(ordinal);
    return handle.has_value() ? active_pool_->get(*handle) : nullptr;
}

const MonsterRuntime* RoomMonsterField::active_runtime(
    const MonsterOrdinal ordinal) const noexcept {
    const std::optional<MonsterHandle> handle = resident_handle(ordinal);
    return handle.has_value() ? active_pool_->get(*handle) : nullptr;
}

modifiers::EffectSet* RoomMonsterField::active_effects(
    const MonsterOrdinal ordinal) noexcept {
    MonsterRuntime* runtime = active_runtime(ordinal);
    if (runtime == nullptr) return nullptr;
    runtime->effects_touched = true;
    return &runtime->effects;
}

const modifiers::EffectSet* RoomMonsterField::active_effects(
    const MonsterOrdinal ordinal) const noexcept {
    const MonsterRuntime* runtime = active_runtime(ordinal);
    return runtime == nullptr ? nullptr : &runtime->effects;
}

RoomResidentOrdinals RoomMonsterField::resident_ordinals() const noexcept {
    return residents_;
}

bool RoomMonsterField::clamp_to_home_leash(
    const MonsterOrdinal ordinal, Vec3& position) const noexcept {
    if (!sealed_ || ordinal >= plan_.monster_count) return false;
    const RoomMonsterBlueprint& blueprint = plan_.monsters[ordinal];
    const std::size_t column = blueprint.home_cell % room_spatial::columns;
    const std::size_t row = blueprint.home_cell / room_spatial::columns;
    const std::size_t leash = (std::min<std::size_t>)(
        blueprint.roaming_leash_cells, room_spatial::roaming_halo_cells);
    const std::size_t first_column = column > leash ? column - leash : 0U;
    const std::size_t last_column = (std::min)(
        column + leash, room_spatial::columns - 1U);
    const std::size_t first_row = row > leash ? row - leash : 0U;
    const std::size_t last_row = (std::min)(
        row + leash, room_spatial::rows - 1U);
    const float minimum_x = room_bounds::min_x
        + static_cast<float>(first_column) * room_spatial::cell_width;
    const float maximum_x = room_bounds::min_x
        + static_cast<float>(last_column + 1U) * room_spatial::cell_width;
    const float minimum_y = room_bounds::min_y
        + static_cast<float>(first_row) * room_spatial::cell_depth;
    const float maximum_y = room_bounds::min_y
        + static_cast<float>(last_row + 1U) * room_spatial::cell_depth;
    position.x = std::clamp(position.x, minimum_x, maximum_x);
    position.y = std::clamp(position.y, minimum_y, maximum_y);
    return true;
}

void RoomMonsterField::rebind_active_pool(MonsterPool& active_pool) noexcept {
    write_back_active();
    active_pool_->clear();
    clear_residency();
    active_pool_ = &active_pool;
    active_pool_->clear();
}

void RoomMonsterField::retarget_moved_active_pool(
    MonsterPool& active_pool) noexcept {
    active_pool_ = &active_pool;
}

void RoomMonsterField::discard_active_residency() noexcept {
    active_pool_->clear();
    clear_residency();
}

MonsterPool& RoomMonsterField::active_pool() noexcept { return *active_pool_; }

const MonsterPool& RoomMonsterField::active_pool() const noexcept {
    return *active_pool_;
}

}  // namespace arpg::combat

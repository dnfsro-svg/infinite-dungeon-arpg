#include "test_framework.hpp"

#include "dungeon_test_support.hpp"

#include "core/deterministic_rng.hpp"
#include "combat/room_bounds.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"
#include "allocation_probe.hpp"

#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>

namespace {

using arpg::core::DeterministicRng;
using arpg::dungeon::DungeonRules;
using arpg::dungeon::DungeonRunState;
using arpg::dungeon::DungeonSession;

constexpr std::uint64_t kDropChanceDomain = 0x44524F505F43484EULL;
constexpr std::uint64_t kDropSlotDomain = 0x44524F505F534C54ULL;
constexpr std::uint64_t kDropContentDomain = 0x44524F505F49544DULL;
constexpr std::uint64_t kDropItemIdDomain = 0x44524F505F49445FULL;

DeterministicRng drop_stream(
    std::uint64_t seed,
    std::uint16_t ordinal,
    std::uint64_t domain) noexcept {
    auto ordinal_stream = DeterministicRng::derive_stream(seed, ordinal);
    return DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), domain);
}

std::uint64_t expected_item_id(
    const DungeonRunState& state,
    std::uint16_t ordinal) noexcept {
    auto room_stream = DeterministicRng::derive_stream(
        state.root_seed, state.current_room.index);
    auto stream = drop_stream(
        room_stream.next_u64(), ordinal, kDropItemIdDomain);
    const std::uint64_t id = stream.next_u64();
    return id == 0U ? 1U : id;
}

std::optional<arpg::items::ItemInstance> expected_drop(
    const DungeonRunState& state,
    std::uint16_t ordinal) noexcept {
    auto chance = drop_stream(
        state.current_room.seed, ordinal, kDropChanceDomain);
    if (chance.next_bounded(100U).value() != 0U) {
        return std::nullopt;
    }
    auto slot = drop_stream(
        state.current_room.seed, ordinal, kDropSlotDomain);
    auto content = drop_stream(
        state.current_room.seed, ordinal, kDropContentDomain);
    return arpg::items::generate_item({
        content.next_u64(),
        static_cast<arpg::items::ItemSlot>(
            slot.next_bounded(6U).value()),
        static_cast<std::uint8_t>(
            state.current_room.depth > 100U
                ? 100U : state.current_room.depth),
        expected_item_id(state, ordinal),
        std::nullopt,
    });
}

DungeonRunState trace_state() noexcept {
    DungeonRules rules;
    for (std::uint64_t root = 1U; root < 10000U; ++root) {
        DungeonRunState state =
            arpg::dungeon::make_initial_run_state(root, rules).state;
        std::size_t drops = 0U;
        for (std::uint16_t ordinal = 0U; ordinal < 192U; ++ordinal) {
            drops += expected_drop(state, ordinal).has_value() ? 1U : 0U;
        }
        if (drops >= 2U) return state;
    }
    return {};
}

bool same_item(
    const arpg::items::ItemInstance& left,
    const arpg::items::ItemInstance& right) noexcept {
    if (left.id != right.id || left.base_id != right.base_id
            || left.rarity != right.rarity
            || left.item_level != right.item_level
            || left.required_level != right.required_level
            || left.affix_count != right.affix_count
            || left.reserved != right.reserved) {
        return false;
    }
    for (std::size_t index = 0U; index < left.affixes.size(); ++index) {
        if (left.affixes[index].affix_id != right.affixes[index].affix_id
                || left.affixes[index].tier != right.affixes[index].tier
                || left.affixes[index].variant
                    != right.affixes[index].variant) {
            return false;
        }
    }
    return true;
}

std::uint16_t first_drop_ordinal(const DungeonRunState& state) noexcept {
    for (std::uint16_t ordinal = 0U; ordinal < 192U; ++ordinal) {
        if (expected_drop(state, ordinal).has_value()) return ordinal;
    }
    return 192U;
}

std::uint16_t second_drop_ordinal(const DungeonRunState& state) noexcept {
    bool found_first = false;
    for (std::uint16_t ordinal = 0U; ordinal < 192U; ++ordinal) {
        if (!expected_drop(state, ordinal).has_value()) continue;
        if (found_first) return ordinal;
        found_first = true;
    }
    return 192U;
}

bool inject_drop(
    DungeonSession& session,
    std::uint16_t ordinal,
    arpg::combat::Vec3 position) noexcept {
    if (!arpg::test::relay_defeated(
            session,
            static_cast<std::uint8_t>(ordinal / 96U),
            static_cast<std::uint8_t>(ordinal % 96U),
            position)) {
        return false;
    }
    return session.try_pop_combat_event().has_value();
}

bool resolve_committed(DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return false;
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        pending->expected_generation,
        pending->next_state,
    });
    return true;
}

arpg::items::ItemInstance normal_item(
    std::uint64_t id,
    arpg::items::ItemSlot slot = arpg::items::ItemSlot::helmet) noexcept {
    const auto item = arpg::items::generate_item({
        id ^ 0xA5A5A5A5ULL,
        slot,
        1U,
        id,
        arpg::items::ItemRarity::normal,
    });
    return item.value_or(arpg::items::ItemInstance{});
}

arpg::items::ItemInstance item_with_rarity(
    std::uint64_t id,
    arpg::items::ItemRarity rarity) noexcept {
    const auto item = arpg::items::generate_item({
        id ^ 0xA5A5A5A5ULL,
        arpg::items::ItemSlot::helmet,
        20U,
        id,
        rarity,
    });
    return item.value_or(arpg::items::ItemInstance{});
}

arpg::test::Failure two_wave_trace_rolls_each_ordinal_once() noexcept {
    const DungeonRules rules;
    const DungeonRunState state = trace_state();
    ARPG_REQUIRE(state.root_seed != 0U);
    DungeonSession session{rules, state};

    std::size_t expected_count = 0U;
    for (std::uint16_t ordinal = 0U; ordinal < 192U; ++ordinal) {
        const auto expected = expected_drop(state, ordinal);
        expected_count += expected.has_value() ? 1U : 0U;
        const std::uint8_t wave = static_cast<std::uint8_t>(ordinal / 96U);
        const std::uint8_t target = static_cast<std::uint8_t>(ordinal % 96U);
        const arpg::combat::Vec3 position{
            static_cast<float>(ordinal),
            -static_cast<float>(ordinal),
            77.0F,
        };
        ARPG_REQUIRE(arpg::test::relay_defeated(
            session, wave, target, position));
        ARPG_REQUIRE(session.try_pop_combat_event().has_value());
    }

    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.ground_item_count == expected_count);
    ARPG_REQUIRE((arpg::test::rolled_drop_bits(session)
        == std::array<std::uint64_t, 3>{{
            (std::numeric_limits<std::uint64_t>::max)(),
            (std::numeric_limits<std::uint64_t>::max)(),
            (std::numeric_limits<std::uint64_t>::max)(),
        }}));
    std::size_t packed = 0U;
    for (std::uint16_t ordinal = 0U; ordinal < 192U; ++ordinal) {
        const auto expected = expected_drop(state, ordinal);
        if (!expected.has_value()) continue;
        const auto& ground = snapshot.ground_items[packed++];
        ARPG_REQUIRE(ground.ordinal == ordinal);
        ARPG_REQUIRE(ground.position.x == static_cast<float>(ordinal));
        ARPG_REQUIRE(ground.position.y == -static_cast<float>(ordinal));
        ARPG_REQUIRE(ground.position.z == 0.0F);
        ARPG_REQUIRE(ground.item_id == expected->id);
        const auto* base = arpg::items::base_definition(expected->base_id);
        ARPG_REQUIRE(base != nullptr);
        ARPG_REQUIRE(ground.slot == base->slot);
        ARPG_REQUIRE(ground.rarity == expected->rarity);
    }
    for (; packed < snapshot.ground_items.size(); ++packed) {
        const auto& ground = snapshot.ground_items[packed];
        ARPG_REQUIRE(ground.ordinal == 0U);
        ARPG_REQUIRE(ground.item_id == 0U);
        ARPG_REQUIRE(ground.position.x == 0.0F);
        ARPG_REQUIRE(ground.position.y == 0.0F);
        ARPG_REQUIRE(ground.position.z == 0.0F);
    }

    ARPG_REQUIRE(arpg::test::relay_defeated(
        session, 0U, 0U, {999.0F, 999.0F, 999.0F}));
    ARPG_REQUIRE(session.try_pop_combat_event().has_value());
    ARPG_REQUIRE(session.snapshot().ground_item_count == expected_count);
    return {};
}

arpg::test::Failure nearby_tick_prepares_atomic_pickup() noexcept {
    const DungeonRules rules;
    const DungeonRunState state = trace_state();
    const std::uint16_t ordinal = first_drop_ordinal(state);
    ARPG_REQUIRE(ordinal < 192U);
    const auto expected = expected_drop(state, ordinal);
    ARPG_REQUIRE(expected.has_value());
    DungeonSession session{rules, state};
    const auto before = session.snapshot();
    ARPG_REQUIRE(before.phase == arpg::dungeon::RoomPhase::locked);
    ARPG_REQUIRE(before.combat.has_value());
    const auto player = before.combat->player.position;
    ARPG_REQUIRE(arpg::test::relay_defeated(
        session,
        static_cast<std::uint8_t>(ordinal / 96U),
        static_cast<std::uint8_t>(ordinal % 96U),
        player));
    ARPG_REQUIRE(session.try_pop_combat_event().has_value());
    ARPG_REQUIRE(session.snapshot().ground_item_count == 1U);

    session.tick({});

    const auto pending_snapshot = session.snapshot();
    ARPG_REQUIRE(pending_snapshot.phase
        == arpg::dungeon::RoomPhase::committing);
    ARPG_REQUIRE(pending_snapshot.pending_save_kind
        == arpg::dungeon::PendingSaveKind::loot_pickup);
    ARPG_REQUIRE(pending_snapshot.ground_item_count == 1U);
    ARPG_REQUIRE(session.item_state().items.empty());
    const auto pending = session.pending_save();
    ARPG_REQUIRE(pending.has_value());
    ARPG_REQUIRE(pending->resume_phase == arpg::dungeon::RoomPhase::combat);
    ARPG_REQUIRE(pending->next_state.item_ownership.items.size() == 1U);
    ARPG_REQUIRE(same_item(
        pending->next_state.item_ownership.items[0], *expected));
    const std::size_t word = ordinal / 64U;
    const std::uint8_t bit = static_cast<std::uint8_t>(ordinal % 64U);
    ARPG_REQUIRE((pending->next_state.item_ownership.claimed_drop_bits[word]
        & (std::uint64_t{1U} << bit)) != 0U);
    ARPG_REQUIRE(pending->expected_generation == state.commit_generation + 1U);
    return {};
}

arpg::test::Failure auto_pickup_precedes_same_tick_exit_request() noexcept {
    const DungeonRunState state = trace_state();
    DungeonSession session{DungeonRules{}, state};
    const arpg::combat::Vec3 doorway{
        arpg::combat::room_bounds::max_x, 0.0F, 0.0F};
    arpg::test::set_player_position(session, doorway);
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::awaiting_exit);
    constexpr std::uint16_t kNormalOrdinal = 3U;
    constexpr std::uint16_t kMagicOrdinal = 7U;
    arpg::test::install_ground_item(session, kNormalOrdinal,
        item_with_rarity(0xA001U, arpg::items::ItemRarity::normal), doorway);
    arpg::test::install_ground_item(session, kMagicOrdinal,
        item_with_rarity(0xA002U, arpg::items::ItemRarity::magic), doorway);

    session.tick({1, 0}, {arpg::items::ItemRarity::magic});

    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::committing);
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->kind
        == arpg::dungeon::PendingSaveKind::loot_pickup);
    ARPG_REQUIRE(session.pending_save()->pickup_ordinal == kMagicOrdinal);
    ARPG_REQUIRE(arpg::test::ground_items(session)[kNormalOrdinal].active);
    ARPG_REQUIRE(!session.pending_transition().has_value());
    return {};
}

arpg::test::Failure explicit_pickup_uses_inclusive_xy_radius() noexcept {
    const DungeonRules rules;
    const DungeonRunState state = trace_state();
    const std::uint16_t ordinal = first_drop_ordinal(state);
    DungeonSession outside{rules, state};
    const auto player = outside.snapshot().combat->player.position;
    ARPG_REQUIRE(inject_drop(outside, ordinal,
        {player.x + 1.5001F, player.y, 90.0F}));
    ARPG_REQUIRE(outside.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::rejected);
    ARPG_REQUIRE(!outside.pending_save().has_value());

    DungeonSession boundary{rules, state};
    ARPG_REQUIRE(inject_drop(boundary, ordinal,
        {player.x + 1.5F, player.y, -90.0F}));
    ARPG_REQUIRE(boundary.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    ARPG_REQUIRE(boundary.pending_save()->pickup_ordinal == ordinal);
    return {};
}

arpg::test::Failure nearby_pickup_chooses_lowest_ordinal_only() noexcept {
    const DungeonRunState state = trace_state();
    const std::uint16_t first = first_drop_ordinal(state);
    const std::uint16_t second = second_drop_ordinal(state);
    ARPG_REQUIRE(first < second && second < 192U);
    DungeonSession session{DungeonRules{}, state};
    const auto player = session.snapshot().combat->player.position;
    ARPG_REQUIRE(inject_drop(session, second, player));
    ARPG_REQUIRE(inject_drop(session, first, player));
    session.request_nearby_pickups(player);
    ARPG_REQUIRE(session.pending_save().has_value());
    ARPG_REQUIRE(session.pending_save()->pickup_ordinal == first);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 2U);

    constexpr std::uint16_t kNormalOrdinal = 4U;
    constexpr std::uint16_t kMagicOrdinal = 5U;
    constexpr std::uint16_t kRareOrdinal = 6U;
    const auto normal = item_with_rarity(
        0xB001U, arpg::items::ItemRarity::normal);
    const auto magic = item_with_rarity(
        0xB002U, arpg::items::ItemRarity::magic);
    const auto rare = item_with_rarity(
        0xB003U, arpg::items::ItemRarity::rare);

    DungeonSession magic_or_better{DungeonRules{}, state};
    arpg::test::install_ground_item(
        magic_or_better, kNormalOrdinal, normal, player);
    arpg::test::install_ground_item(
        magic_or_better, kMagicOrdinal, magic, player);
    arpg::test::install_ground_item(
        magic_or_better, kRareOrdinal, rare, player);
    ARPG_REQUIRE(arpg::dungeon::auto_pickup_eligible(
        arpg::test::ground_items(magic_or_better)[kNormalOrdinal], {}));
    ARPG_REQUIRE(!arpg::dungeon::auto_pickup_eligible(
        arpg::test::ground_items(magic_or_better)[kNormalOrdinal],
        {arpg::items::ItemRarity::magic}));
    magic_or_better.request_nearby_pickups(
        player, {arpg::items::ItemRarity::magic});
    ARPG_REQUIRE(arpg::test::ground_items(
        magic_or_better)[kNormalOrdinal].active);
    ARPG_REQUIRE(magic_or_better.pending_save_view() != nullptr);
    ARPG_REQUIRE(magic_or_better.pending_save_view()->pickup_ordinal
        == kMagicOrdinal);
    ARPG_REQUIRE(resolve_committed(magic_or_better));
    ARPG_REQUIRE(!arpg::test::ground_items(
        magic_or_better)[kMagicOrdinal].active);
    magic_or_better.request_nearby_pickups(player);
    ARPG_REQUIRE(magic_or_better.pending_save_view() != nullptr);
    ARPG_REQUIRE(magic_or_better.pending_save_view()->pickup_ordinal
        == kNormalOrdinal);
    ARPG_REQUIRE(resolve_committed(magic_or_better));
    ARPG_REQUIRE(!arpg::test::ground_items(
        magic_or_better)[kNormalOrdinal].active);

    DungeonSession rare_only{DungeonRules{}, state};
    arpg::test::install_ground_item(rare_only, kNormalOrdinal, normal, player);
    arpg::test::install_ground_item(rare_only, kMagicOrdinal, magic, player);
    arpg::test::install_ground_item(rare_only, kRareOrdinal, rare, player);
    rare_only.request_nearby_pickups(
        player, {arpg::items::ItemRarity::rare});
    ARPG_REQUIRE(rare_only.pending_save_view() != nullptr);
    ARPG_REQUIRE(rare_only.pending_save_view()->pickup_ordinal
        == kRareOrdinal);
    ARPG_REQUIRE(resolve_committed(rare_only));
    ARPG_REQUIRE(!arpg::test::ground_items(rare_only)[kRareOrdinal].active);
    rare_only.request_nearby_pickups(player);
    ARPG_REQUIRE(rare_only.pending_save_view() != nullptr);
    ARPG_REQUIRE(rare_only.pending_save_view()->pickup_ordinal
        == kNormalOrdinal);
    ARPG_REQUIRE(resolve_committed(rare_only));
    ARPG_REQUIRE(!arpg::test::ground_items(rare_only)[kNormalOrdinal].active);

    DungeonSession default_policy{DungeonRules{}, state};
    arpg::test::install_ground_item(
        default_policy, kNormalOrdinal, normal, player);
    default_policy.request_nearby_pickups(player);
    ARPG_REQUIRE(default_policy.pending_save_view() != nullptr);
    ARPG_REQUIRE(default_policy.pending_save_view()->pickup_ordinal
        == kNormalOrdinal);

    DungeonSession explicit_pickup{DungeonRules{}, state};
    arpg::test::install_ground_item(
        explicit_pickup, kNormalOrdinal, normal, player);
    ARPG_REQUIRE(explicit_pickup.request_pickup(kNormalOrdinal)
        == arpg::dungeon::RequestResult::accepted);
    ARPG_REQUIRE(explicit_pickup.pending_save_view() != nullptr);
    ARPG_REQUIRE(explicit_pickup.pending_save_view()->pickup_ordinal
        == kNormalOrdinal);
    return {};
}

arpg::test::Failure pickup_save_outcomes_are_atomic() noexcept {
    const DungeonRunState state = trace_state();
    const std::uint16_t ordinal = first_drop_ordinal(state);

    DungeonSession rollback{DungeonRules{}, state};
    const auto player = rollback.snapshot().combat->player.position;
    ARPG_REQUIRE(inject_drop(rollback, ordinal, player));
    ARPG_REQUIRE(rollback.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    const auto failed = *rollback.pending_save();
    rollback.resolve_pending_save({
        arpg::dungeon::SaveDisposition::not_committed,
        failed.expected_generation,
        failed.next_state,
    });
    ARPG_REQUIRE(rollback.snapshot().phase
        == arpg::dungeon::RoomPhase::locked);
    ARPG_REQUIRE(rollback.snapshot().ground_item_count == 1U);
    ARPG_REQUIRE(rollback.item_state().items.empty());
    ARPG_REQUIRE(!rollback.pending_save().has_value());

    DungeonSession indeterminate{DungeonRules{}, state};
    ARPG_REQUIRE(inject_drop(indeterminate, ordinal, player));
    ARPG_REQUIRE(indeterminate.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    indeterminate.resolve_pending_save({
        arpg::dungeon::SaveDisposition::indeterminate, 0U, {}});
    ARPG_REQUIRE(indeterminate.snapshot().phase
        == arpg::dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(indeterminate.snapshot().diagnostics.fault
        == arpg::dungeon::DungeonFault::save_commit_indeterminate);
    ARPG_REQUIRE(indeterminate.snapshot().ground_item_count == 1U);
    ARPG_REQUIRE(indeterminate.item_state().items.empty());
    return {};
}

arpg::test::Failure pickup_commit_publishes_without_allocation() noexcept {
    const DungeonRunState state = trace_state();
    const std::uint16_t ordinal = first_drop_ordinal(state);
    DungeonSession session{DungeonRules{}, state};
    const auto player = session.snapshot().combat->player.position;
    ARPG_REQUIRE(inject_drop(session, ordinal, player));
    ARPG_REQUIRE(session.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    const auto pending = *session.pending_save();
    const arpg::dungeon::PendingSaveResult receipt{
        arpg::dungeon::SaveDisposition::committed,
        pending.expected_generation,
        pending.next_state,
    };
    const std::uint64_t before = arpg::test::allocation_count();
    {
        arpg::test::ScopedAllocationFailure fail{0U};
        session.resolve_pending_save(receipt);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::locked);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);
    ARPG_REQUIRE(session.item_state().items.size() == 1U);
    ARPG_REQUIRE(same_item(session.item_state().items[0],
        pending.next_state.item_ownership.items.back()));
    return {};
}

arpg::test::Failure disappeared_ground_rejects_committed_receipt() noexcept {
    const DungeonRunState state = trace_state();
    const std::uint16_t ordinal = first_drop_ordinal(state);
    DungeonSession session{DungeonRules{}, state};
    const auto player = session.snapshot().combat->player.position;
    ARPG_REQUIRE(inject_drop(session, ordinal, player));
    ARPG_REQUIRE(session.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    const auto pending = *session.pending_save();
    arpg::test::clear_ground_item(session, ordinal);
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        pending.expected_generation,
        pending.next_state,
    });
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(session.snapshot().diagnostics.fault
        == arpg::dungeon::DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(session.item_state().items.empty());
    return {};
}

arpg::test::Failure wrong_ordinal_or_replaced_ground_rejects_receipt() noexcept {
    const DungeonRunState state = trace_state();
    const std::uint16_t ordinal = first_drop_ordinal(state);
    const std::uint16_t wrong = ordinal == 191U ? 190U : ordinal + 1U;
    DungeonSession position_source{DungeonRules{}, state};
    const auto player = position_source.snapshot().combat->player.position;

    DungeonSession wrong_ordinal{DungeonRules{}, state};
    ARPG_REQUIRE(inject_drop(wrong_ordinal, ordinal, player));
    ARPG_REQUIRE(wrong_ordinal.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    const auto wrong_pending = *wrong_ordinal.pending_save();
    arpg::test::set_pending_pickup_ordinal(wrong_ordinal, wrong);
    wrong_ordinal.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        wrong_pending.expected_generation,
        wrong_pending.next_state,
    });
    ARPG_REQUIRE(wrong_ordinal.snapshot().diagnostics.fault
        == arpg::dungeon::DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(wrong_ordinal.item_state().items.empty());

    DungeonSession replaced{DungeonRules{}, state};
    ARPG_REQUIRE(inject_drop(replaced, ordinal, player));
    ARPG_REQUIRE(replaced.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    const auto replaced_pending = *replaced.pending_save();
    const std::uint64_t id = replaced_pending.next_state
        .item_ownership.items.back().id;
    const auto replacement = normal_item(id, arpg::items::ItemSlot::boots);
    ARPG_REQUIRE(!same_item(replacement,
        replaced_pending.next_state.item_ownership.items.back()));
    arpg::test::install_ground_item(
        replaced, ordinal, replacement, player);
    replaced.resolve_pending_save({
        arpg::dungeon::SaveDisposition::committed,
        replaced_pending.expected_generation,
        replaced_pending.next_state,
    });
    ARPG_REQUIRE(replaced.snapshot().diagnostics.fault
        == arpg::dungeon::DungeonFault::save_receipt_mismatch);
    ARPG_REQUIRE(replaced.item_state().items.empty());
    return {};
}

arpg::test::Failure pickup_allocation_failures_roll_back() noexcept {
    const DungeonRunState base = trace_state();
    const std::uint16_t ordinal = first_drop_ordinal(base);
    for (std::size_t failure_index = 0U; failure_index < 4U;
         ++failure_index) {
        DungeonRunState state = base;
        state.item_ownership.items.push_back(normal_item(0xCAFE0000U));
        DungeonSession session{DungeonRules{}, state};
        const auto player = session.snapshot().combat->player.position;
        ARPG_REQUIRE(inject_drop(session, ordinal, player));
        arpg::dungeon::RequestResult result{};
        {
            arpg::test::ScopedAllocationFailure fail{failure_index};
            result = session.request_pickup(ordinal);
        }
        ARPG_REQUIRE(result == arpg::dungeon::RequestResult::rejected);
        ARPG_REQUIRE(session.snapshot().phase
            == arpg::dungeon::RoomPhase::locked);
        ARPG_REQUIRE(session.snapshot().ground_item_count == 1U);
        ARPG_REQUIRE(session.item_state().items.size() == 1U);
        ARPG_REQUIRE(session.item_state().items[0].id == 0xCAFE0000U);
        ARPG_REQUIRE(session.snapshot().commit_generation
            == state.commit_generation);
        ARPG_REQUIRE(!session.pending_save().has_value());
    }
    return {};
}

arpg::test::Failure claimed_and_unclaimed_reload_semantics() noexcept {
    const DungeonRunState state = trace_state();
    const std::uint16_t ordinal = first_drop_ordinal(state);
    const auto expected = expected_drop(state, ordinal);
    ARPG_REQUIRE(expected.has_value());

    DungeonSession first{DungeonRules{}, state};
    const auto player = first.snapshot().combat->player.position;
    ARPG_REQUIRE(inject_drop(first, ordinal, player));
    const auto first_ground = first.snapshot().ground_items[0];

    DungeonSession unclaimed_reload{DungeonRules{}, state};
    ARPG_REQUIRE(inject_drop(unclaimed_reload, ordinal, player));
    const auto reloaded_ground = unclaimed_reload.snapshot().ground_items[0];
    ARPG_REQUIRE(reloaded_ground.item_id == first_ground.item_id);
    ARPG_REQUIRE(reloaded_ground.ordinal == first_ground.ordinal);
    ARPG_REQUIRE(unclaimed_reload.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    ARPG_REQUIRE(same_item(
        unclaimed_reload.pending_save()->next_state.item_ownership.items[0],
        *expected));

    ARPG_REQUIRE(first.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    const DungeonRunState claimed_state = first.pending_save()->next_state;
    ARPG_REQUIRE(resolve_committed(first));
    DungeonSession claimed_reload{DungeonRules{}, claimed_state};
    ARPG_REQUIRE(inject_drop(claimed_reload, ordinal, player));
    ARPG_REQUIRE(claimed_reload.snapshot().ground_item_count == 0U);
    return {};
}

arpg::test::Failure miss_is_rolled_once_until_room_reset() noexcept {
    DungeonRunState state = trace_state();
    std::uint16_t miss = 192U;
    for (std::uint16_t ordinal = 0U; ordinal < 192U; ++ordinal) {
        if (!expected_drop(state, ordinal).has_value()) {
            miss = ordinal;
            break;
        }
    }
    ARPG_REQUIRE(miss < 192U);
    DungeonSession session{DungeonRules{}, state};
    ARPG_REQUIRE(inject_drop(session, miss, {10.0F, 20.0F, 30.0F}));
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);

    std::uint64_t hit_seed = 0U;
    for (std::uint64_t seed = 1U; seed < 10000U; ++seed) {
        DungeonRunState changed = state;
        changed.current_room.seed = seed;
        if (expected_drop(changed, miss).has_value()) {
            hit_seed = seed;
            break;
        }
    }
    ARPG_REQUIRE(hit_seed != 0U);
    arpg::test::set_current_room_seed(session, hit_seed);
    ARPG_REQUIRE(inject_drop(session, miss, {10.0F, 20.0F, 30.0F}));
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);
    static_cast<void>(session.reset_current_room());
    ARPG_REQUIRE(inject_drop(session, miss, {10.0F, 20.0F, 30.0F}));
    ARPG_REQUIRE(session.snapshot().ground_item_count == 1U);
    return {};
}

arpg::test::Failure ground_pool_has_full_192_capacity() noexcept {
    DungeonSession session;
    for (std::uint16_t ordinal = 0U; ordinal < 192U; ++ordinal) {
        const auto item = normal_item(
            0x100000000ULL + ordinal,
            static_cast<arpg::items::ItemSlot>(ordinal % 6U));
        ARPG_REQUIRE(item.id != 0U);
        arpg::test::install_ground_item(session, ordinal, item,
            {static_cast<float>(ordinal), 0.0F, 0.0F});
    }
    const auto snapshot = session.snapshot();
    ARPG_REQUIRE(snapshot.ground_item_count == 192U);
    ARPG_REQUIRE(snapshot.ground_items[0].ordinal == 0U);
    ARPG_REQUIRE(snapshot.ground_items[95].ordinal == 95U);
    ARPG_REQUIRE(snapshot.ground_items[96].ordinal == 96U);
    ARPG_REQUIRE(snapshot.ground_items[191].ordinal == 191U);
    return {};
}

arpg::test::Failure drop_id_collision_and_invalid_depth_fault() noexcept {
    DungeonRunState collision = trace_state();
    const std::uint16_t ordinal = first_drop_ordinal(collision);
    collision.item_ownership.items.push_back(
        normal_item(expected_item_id(collision, ordinal)));
    DungeonSession collision_session{DungeonRules{}, collision};
    ARPG_REQUIRE(inject_drop(collision_session, ordinal,
        {10.0F, 20.0F, 30.0F}));
    ARPG_REQUIRE(collision_session.snapshot().diagnostics.fault
        == arpg::dungeon::DungeonFault::item_id_collision);
    ARPG_REQUIRE(collision_session.snapshot().ground_item_count == 0U);

    DungeonRunState ground_collision = trace_state();
    const std::uint16_t ground_ordinal = first_drop_ordinal(ground_collision);
    const auto colliding_ground = expected_drop(
        ground_collision, ground_ordinal);
    ARPG_REQUIRE(colliding_ground.has_value());
    DungeonSession ground_collision_session{DungeonRules{}, ground_collision};
    const std::uint16_t occupied = ground_ordinal == 191U
        ? 190U : ground_ordinal + 1U;
    arpg::test::install_ground_item(ground_collision_session, occupied,
        *colliding_ground, {0.0F, 0.0F, 0.0F});
    ARPG_REQUIRE(inject_drop(ground_collision_session, ground_ordinal,
        {10.0F, 20.0F, 30.0F}));
    ARPG_REQUIRE(ground_collision_session.snapshot().diagnostics.fault
        == arpg::dungeon::DungeonFault::item_id_collision);

    DungeonRunState invalid_loaded = trace_state();
    invalid_loaded.current_room.depth = 0U;
    DungeonSession invalid_loaded_session{DungeonRules{}, invalid_loaded};
    ARPG_REQUIRE(invalid_loaded_session.snapshot().phase
        == arpg::dungeon::RoomPhase::faulted);
    ARPG_REQUIRE(invalid_loaded_session.snapshot().diagnostics.fault
        == arpg::dungeon::DungeonFault::invalid_item_state);

    DungeonRunState invalid = trace_state();
    const std::uint16_t invalid_ordinal = first_drop_ordinal(invalid);
    DungeonSession invalid_session{DungeonRules{}, invalid};
    arpg::test::set_current_room_depth(invalid_session, 0U);
    ARPG_REQUIRE(inject_drop(invalid_session, invalid_ordinal,
        {10.0F, 20.0F, 30.0F}));
    ARPG_REQUIRE(invalid_session.snapshot().diagnostics.fault
        == arpg::dungeon::DungeonFault::invalid_item_state);
    ARPG_REQUIRE(invalid_session.snapshot().ground_item_count == 0U);
    return {};
}

arpg::test::Failure deep_room_caps_drop_item_level_at_100() noexcept {
    DungeonRunState state = trace_state();
    state.current_room.depth = 150U;
    const std::uint16_t ordinal = first_drop_ordinal(state);
    const auto expected = expected_drop(state, ordinal);
    ARPG_REQUIRE(expected.has_value());
    ARPG_REQUIRE(expected->item_level == 100U);
    DungeonSession session{DungeonRules{}, state};
    const auto player = session.snapshot().combat->player.position;
    ARPG_REQUIRE(inject_drop(session, ordinal, player));
    ARPG_REQUIRE(session.request_pickup(ordinal)
        == arpg::dungeon::RequestResult::accepted);
    ARPG_REQUIRE(session.pending_save()->next_state
        .item_ownership.items.back().item_level == 100U);
    return {};
}

arpg::test::Failure transition_clears_claimed_and_ground_only_on_commit() noexcept {
    DungeonRunState state = trace_state();
    const std::uint16_t ordinal = first_drop_ordinal(state);
    const std::uint16_t claimed = ordinal == 191U ? 190U : ordinal + 1U;
    state.item_ownership.claimed_drop_bits[claimed / 64U]
        |= std::uint64_t{1U} << (claimed % 64U);
    DungeonSession session{DungeonRules{}, state};
    ARPG_REQUIRE(inject_drop(session, ordinal, {100.0F, 100.0F, 0.0F}));
    ARPG_REQUIRE(session.snapshot().ground_item_count == 1U);
    arpg::test::set_phase(session, arpg::dungeon::RoomPhase::awaiting_exit);
    arpg::test::attempt_exit(session, arpg::dungeon::ExitDirection::right);
    const auto first = session.pending_save();
    ARPG_REQUIRE(first.has_value());
    ARPG_REQUIRE(first->kind == arpg::dungeon::PendingSaveKind::transition);
    ARPG_REQUIRE(first->pickup_ordinal == 0xFFFFU);
    ARPG_REQUIRE((first->next_state.item_ownership.claimed_drop_bits
        == std::array<std::uint64_t, 3>{}));
    session.resolve_pending_save({
        arpg::dungeon::SaveDisposition::not_committed,
        first->expected_generation,
        first->next_state,
    });
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::awaiting_exit);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 1U);
    ARPG_REQUIRE(session.item_state().claimed_drop_bits
        == state.item_ownership.claimed_drop_bits);

    arpg::test::attempt_exit(session, arpg::dungeon::ExitDirection::right);
    ARPG_REQUIRE(resolve_committed(session));
    ARPG_REQUIRE(session.snapshot().phase
        == arpg::dungeon::RoomPhase::transitioning);
    ARPG_REQUIRE(session.snapshot().ground_item_count == 0U);
    ARPG_REQUIRE((session.item_state().claimed_drop_bits
        == std::array<std::uint64_t, 3>{}));
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"two wave trace rolls each ordinal once",
        &two_wave_trace_rolls_each_ordinal_once},
    {"nearby tick prepares atomic pickup",
        &nearby_tick_prepares_atomic_pickup},
    {"auto pickup precedes exit",
        &auto_pickup_precedes_same_tick_exit_request},
    {"explicit pickup inclusive radius",
        &explicit_pickup_uses_inclusive_xy_radius},
    {"nearby pickup lowest ordinal only",
        &nearby_pickup_chooses_lowest_ordinal_only},
    {"pickup save outcomes atomic", &pickup_save_outcomes_are_atomic},
    {"pickup commit no allocation",
        &pickup_commit_publishes_without_allocation},
    {"disappeared ground rejects receipt",
        &disappeared_ground_rejects_committed_receipt},
    {"wrong ordinal or replaced ground rejects receipt",
        &wrong_ordinal_or_replaced_ground_rejects_receipt},
    {"pickup allocation rollback", &pickup_allocation_failures_roll_back},
    {"claimed and unclaimed reload",
        &claimed_and_unclaimed_reload_semantics},
    {"miss rolls once until reset", &miss_is_rolled_once_until_room_reset},
    {"ground pool full capacity", &ground_pool_has_full_192_capacity},
    {"drop collision and invalid depth",
        &drop_id_collision_and_invalid_depth_fault},
    {"deep room caps item level", &deep_room_caps_drop_item_level_at_100},
    {"transition cleanup commit semantics",
        &transition_clears_claimed_and_ground_only_on_commit},
};

}  // namespace

arpg::test::TestSuite dungeon_loot_drop_suite() noexcept {
    return arpg::test::make_suite("dungeon_loot_drop", kCases);
}

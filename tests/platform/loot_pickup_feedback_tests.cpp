#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "loot_pickup_feedback.hpp"

#include <cstdint>
#include <cstring>

namespace {

namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace platform = arpg::platform;

platform::DungeonRenderStatus status_with_receipt(
    std::uint64_t generation,
    std::uint64_t item_id,
    dungeon::GroundItemSource source =
        dungeon::GroundItemSource::monster_drop) noexcept {
    platform::DungeonRenderStatus status{};
    status.indicator = platform::SaveIndicator::saved;
    status.loot_pickup = {
        true, generation, item_id, 3U, 24U,
        items::ItemRarity::rare, source};
    return status;
}

arpg::test::Failure first_receipt_is_baselined_and_newer_receipt_emits_once()
    noexcept {
    platform::LootPickupFeedbackState state{};
    const auto old = status_with_receipt(7U, 101U);
    ARPG_REQUIRE(!state.observe(old).ready);

    const auto current = status_with_receipt(8U, 102U);
    const platform::LootPickupFeedback feedback = state.observe(current);
    ARPG_REQUIRE(feedback.ready);
    ARPG_REQUIRE(!feedback.abyss);
    ARPG_REQUIRE(feedback.item_id == 102U);
    ARPG_REQUIRE(std::strcmp(feedback.text.bytes.data(),
        u8"已拾取：稀有 Ward Coat · i24") == 0);
    ARPG_REQUIRE(!feedback.text.truncated);
    ARPG_REQUIRE(!state.observe(current).ready);
    return {};
}

arpg::test::Failure startup_empty_status_allows_first_real_pickup_to_emit()
    noexcept {
    platform::LootPickupFeedbackState state{};
    platform::DungeonRenderStatus empty{};
    empty.indicator = platform::SaveIndicator::none;
    for (std::size_t index = 0U; index < 3U; ++index) {
        ARPG_REQUIRE(!state.observe(empty).ready);
    }
    const auto feedback = state.observe(status_with_receipt(1U, 101U));
    ARPG_REQUIRE(feedback.ready);
    ARPG_REQUIRE(feedback.item_id == 101U);
    return {};
}

arpg::test::Failure abyss_receipt_uses_abyss_feedback_style() noexcept {
    platform::LootPickupFeedbackState state{};
    ARPG_REQUIRE(!state.observe(status_with_receipt(1U, 1U)).ready);
    const auto feedback = state.observe(status_with_receipt(
        2U, 2U, dungeon::GroundItemSource::abyss_chest));
    ARPG_REQUIRE(feedback.ready);
    ARPG_REQUIRE(feedback.abyss);
    ARPG_REQUIRE(feedback.item_id == 2U);
    return {};
}

arpg::test::Failure invalid_older_error_and_recovery_receipts_do_not_emit()
    noexcept {
    platform::LootPickupFeedbackState state{};
    ARPG_REQUIRE(!state.observe(status_with_receipt(10U, 10U)).ready);

    auto invalid = status_with_receipt(11U, 11U);
    invalid.loot_pickup.base_id = 0xFFU;
    ARPG_REQUIRE(!state.observe(invalid).ready);
    ARPG_REQUIRE(!state.observe(status_with_receipt(12U, 12U)).ready);

    platform::LootPickupFeedbackState backward_state{};
    ARPG_REQUIRE(!backward_state.observe(status_with_receipt(10U, 10U)).ready);
    ARPG_REQUIRE(!backward_state.observe(status_with_receipt(9U, 9U)).ready);
    ARPG_REQUIRE(!backward_state.observe(status_with_receipt(11U, 11U)).ready);

    auto error = status_with_receipt(12U, 12U);
    error.indicator = platform::SaveIndicator::error;
    ARPG_REQUIRE(!state.observe(error).ready);
    ARPG_REQUIRE(!state.observe(error).ready);

    auto recovery = status_with_receipt(13U, 13U);
    recovery.recovery_required = true;
    ARPG_REQUIRE(!state.observe(recovery).ready);
    ARPG_REQUIRE(!state.observe(recovery).ready);
    return {};
}

arpg::test::Failure unchanged_observation_allocates_nothing_100k() noexcept {
    platform::LootPickupFeedbackState state{};
    const auto status = status_with_receipt(77U, 88U);
    ARPG_REQUIRE(!state.observe(status).ready);
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t index = 0U; index < 100000U; ++index) {
        ARPG_REQUIRE(!state.observe(status).ready);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"baseline and one shot", &first_receipt_is_baselined_and_newer_receipt_emits_once},
    {"startup empty emits first pickup", &startup_empty_status_allows_first_real_pickup_to_emit},
    {"abyss style", &abyss_receipt_uses_abyss_feedback_style},
    {"invalid and blocked status", &invalid_older_error_and_recovery_receipts_do_not_emit},
    {"unchanged zero allocation 100k", &unchanged_observation_allocates_nothing_100k},
};

}  // namespace

arpg::test::TestSuite loot_pickup_feedback_suite() noexcept {
    return arpg::test::make_suite("loot_pickup_feedback", kCases);
}

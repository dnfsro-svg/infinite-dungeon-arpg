#include "test_framework.hpp"

#include "allocation_probe.hpp"
#include "ui_text_catalog.hpp"

#include <array>
#include <cstdint>
#include <cstring>

namespace {

namespace platform = arpg::platform;

void count_text(const char* text, void* context) noexcept {
    auto* const count = static_cast<std::size_t*>(context);
    if (text != nullptr && text[0] != '\0') ++(*count);
}

arpg::test::Failure fixed_messages_are_chinese() noexcept {
    ARPG_REQUIRE(std::strcmp(
        platform::ui_text(platform::UiTextId::pause_title), u8"暂停") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(1U),
        u8"精炼之刃") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_material_name(
        arpg::items::MaterialId::reinforcement_stone), u8"强化石") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_short_name(
        arpg::combat::MonsterAffixId::death_blast), u8"死爆") == 0);
    return {};
}

arpg::test::Failure all_stable_ids_have_named_mappings() noexcept {
    constexpr std::array<platform::UiTextId, 32U> texts{{
        platform::UiTextId::pause_title, platform::UiTextId::pause_continue,
        platform::UiTextId::pause_settings, platform::UiTextId::pause_quit,
        platform::UiTextId::settings_title, platform::UiTextId::settings_audio,
        platform::UiTextId::settings_display, platform::UiTextId::settings_bindings,
        platform::UiTextId::settings_restore_defaults, platform::UiTextId::settings_apply,
        platform::UiTextId::settings_cancel, platform::UiTextId::settings_saved_prefix,
        platform::UiTextId::settings_unknown, platform::UiTextId::hud_health,
        platform::UiTextId::hud_barrier, platform::UiTextId::hud_experience,
        platform::UiTextId::hud_level, platform::UiTextId::save_saving,
        platform::UiTextId::save_saved, platform::UiTextId::save_recovered,
        platform::UiTextId::save_error, platform::UiTextId::recovery_title,
        platform::UiTextId::recovery_invalid_slots, platform::UiTextId::inventory_title,
        platform::UiTextId::equipment_title, platform::UiTextId::item_detail_title,
        platform::UiTextId::passive_title, platform::UiTextId::passive_points,
        platform::UiTextId::passive_close, platform::UiTextId::debug_ready,
        platform::UiTextId::debug_unavailable, platform::UiTextId::unknown,
    }};
    for (const platform::UiTextId id : texts)
        ARPG_REQUIRE(platform::ui_text(id)[0] != '\0');

    for (std::uint8_t id = 1U; id <= 18U; ++id)
        ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(id), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 14U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_material_name(
            static_cast<arpg::items::MaterialId>(raw)), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 6U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_item_slot_name(
            static_cast<arpg::items::ItemSlot>(raw)), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 3U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_item_rarity_name(
            static_cast<arpg::items::ItemRarity>(raw)), u8"未知") != 0);
    for (std::uint16_t id = 1U; id <= 12U; ++id)
        ARPG_REQUIRE(std::strcmp(platform::ui_item_affix_name(id), u8"未知") != 0);
    for (std::uint16_t id = 101U; id <= 112U; ++id)
        ARPG_REQUIRE(std::strcmp(platform::ui_item_affix_name(id), u8"未知") != 0);
    for (std::uint8_t id = 0U; id < 64U; ++id)
        ARPG_REQUIRE(std::strcmp(platform::ui_passive_node_name(id), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 8U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_name(
            static_cast<arpg::combat::MonsterId>(raw)), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 12U; ++raw) {
        const auto id = static_cast<arpg::combat::MonsterAffixId>(raw);
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_name(id), u8"未知") != 0);
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_short_name(id), u8"未知") != 0);
    }
    for (std::uint8_t raw = 0U; raw < 10U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_setting_action_name(
            static_cast<arpg::settings::SettingAction>(raw)), u8"未知") != 0);
    for (std::uint8_t raw = 0U; raw < 2U; ++raw)
        ARPG_REQUIRE(std::strcmp(platform::ui_skill_name(
            static_cast<arpg::skills::ActiveSkillId>(raw)), u8"未知") != 0);
    return {};
}

arpg::test::Failure unknown_ids_have_a_single_safe_fallback() noexcept {
    ARPG_REQUIRE(std::strcmp(platform::ui_text(platform::UiTextId::count), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(255U), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_material_name(
        static_cast<arpg::items::MaterialId>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_item_slot_name(
        static_cast<arpg::items::ItemSlot>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_item_rarity_name(
        static_cast<arpg::items::ItemRarity>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_item_affix_name(65535U), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_passive_node_name(255U), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_monster_name(
        static_cast<arpg::combat::MonsterId>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_name(
        static_cast<arpg::combat::MonsterAffixId>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_short_name(
        static_cast<arpg::combat::MonsterAffixId>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_setting_action_name(
        static_cast<arpg::settings::SettingAction>(255U)), u8"未知") == 0);
    ARPG_REQUIRE(std::strcmp(platform::ui_skill_name(
        static_cast<arpg::skills::ActiveSkillId>(255U)), u8"未知") == 0);
    return {};
}

arpg::test::Failure visitor_covers_the_complete_static_catalog() noexcept {
    std::size_t count{};
    platform::visit_ui_text_catalog(&count_text, &count);
    ARPG_REQUIRE(count == 268U);
    return {};
}

arpg::test::Failure catalog_lookups_and_visiting_do_not_allocate() noexcept {
    std::size_t warmup_count{};
    platform::visit_ui_text_catalog(&count_text, &warmup_count);
    const std::uint64_t before = arpg::test::allocation_count();
    for (std::size_t iteration = 0U; iteration < 1000U; ++iteration) {
        std::size_t count{};
        platform::visit_ui_text_catalog(&count_text, &count);
        ARPG_REQUIRE(count == 268U);
        ARPG_REQUIRE(std::strcmp(platform::ui_item_base_name(18U), u8"未知") != 0);
        ARPG_REQUIRE(std::strcmp(platform::ui_passive_node_name(63U), u8"未知") != 0);
        ARPG_REQUIRE(std::strcmp(platform::ui_monster_affix_short_name(
            arpg::combat::MonsterAffixId::death_blast), u8"死爆") == 0);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == before);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"fixed messages are Chinese", &fixed_messages_are_chinese},
    {"all stable ids have named mappings", &all_stable_ids_have_named_mappings},
    {"unknown ids have a single safe fallback", &unknown_ids_have_a_single_safe_fallback},
    {"visitor covers the complete static catalog", &visitor_covers_the_complete_static_catalog},
    {"catalog lookups and visiting do not allocate", &catalog_lookups_and_visiting_do_not_allocate},
};

}  // namespace

arpg::test::TestSuite ui_text_catalog_suite() noexcept {
    return arpg::test::make_suite("ui_text_catalog", kCases);
}

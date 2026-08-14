#include "allocation_probe.hpp"
#include "test_framework.hpp"

#include "material_bag_renderer.hpp"
#include "material_manifest.hpp"
#include "ui_text_contrast.hpp"
#include "ui_text_catalog.hpp"
#include "ui_text_layout.hpp"
#include "ui_typography.hpp"
#include "ui_utf8.hpp"

#include <array>
#include <cmath>
#include <cstdio>
#include <cstring>
#include <string_view>

namespace {

namespace items = arpg::items;
namespace platform = arpg::platform;

bool inside(Rectangle child, Rectangle parent) noexcept {
    return child.width > 0.0F && child.height > 0.0F
        && child.x >= parent.x && child.y >= parent.y
        && child.x + child.width <= parent.x + parent.width
        && child.y + child.height <= parent.y + parent.height;
}

Vector2 center(Rectangle rectangle) noexcept {
    return {rectangle.x + rectangle.width * 0.5F,
        rectangle.y + rectangle.height * 0.5F};
}

platform::UiTextExtent measure_material_text(const char* text,
    float font_size, float, void*) noexcept {
    std::size_t codepoints{};
    for (const unsigned char* cursor =
             reinterpret_cast<const unsigned char*>(text);
         cursor != nullptr && *cursor != 0U; ++cursor) {
        if ((*cursor & 0xC0U) != 0x80U) ++codepoints;
    }
    return {static_cast<float>(codepoints) * font_size * 0.72F, font_size};
}

arpg::test::Failure material_bag_has_all_fourteen_slots() noexcept {
    const auto shell = platform::inventory_shell_layout(1280, 720);
    const platform::MaterialBagLayout layout = platform::material_bag_layout(
        shell.right_panel, shell.geometry_scale);
    ARPG_REQUIRE(layout.contains_all_slots());
    ARPG_REQUIRE(layout.slots.size() == items::kMaterialCount);

    const auto full_hd_shell = platform::inventory_shell_layout(1920, 1080);
    const platform::MaterialBagLayout full_hd = platform::material_bag_layout(
        full_hd_shell.right_panel, full_hd_shell.geometry_scale);
    ARPG_REQUIRE(full_hd.contains_all_slots());
    ARPG_REQUIRE(full_hd.slots[0].width >= 40.0F);
    ARPG_REQUIRE(full_hd.slots[2].x == full_hd.slots[0].x);
    ARPG_REQUIRE(full_hd.slots[2].y > full_hd.slots[0].y);

    const Rectangle text_backing = platform::material_bag_text_backing(
        full_hd.slots[0], 1.5F);
    ARPG_REQUIRE(text_backing.x >= full_hd.slots[0].x);
    ARPG_REQUIRE(text_backing.y >= full_hd.slots[0].y);
    ARPG_REQUIRE(text_backing.x + text_backing.width
        <= full_hd.slots[0].x + full_hd.slots[0].width);
    ARPG_REQUIRE(text_backing.y + text_backing.height
        <= full_hd.slots[0].y + full_hd.slots[0].height);
    ARPG_REQUIRE(text_backing.width >= 185.0F);

    constexpr Color kMaterialTextBacking{5, 9, 16, 255};
    constexpr Color kDangerBacking{29, 15, 18, 255};
    constexpr Color kConfirmBacking{74, 24, 27, 255};
    constexpr Color kCancelBacking{30, 42, 58, 255};
    constexpr auto kContrast = platform::ui_text_contrast_style();
    constexpr std::array<Color, 5> kOpaqueBackings{{
        kMaterialTextBacking, kDangerBacking, kDangerBacking,
        kConfirmBacking, kCancelBacking,
    }};
    constexpr std::array<Color, 5> kActualTextColors{{
        kContrast.primary, kContrast.danger, kContrast.primary,
        kContrast.primary, kContrast.primary,
    }};
    for (std::size_t index{}; index < kOpaqueBackings.size(); ++index) {
        ARPG_REQUIRE(kOpaqueBackings[index].a == 255U);
        ARPG_REQUIRE(platform::ui_luma_contrast_ratio(
            kActualTextColors[index], kOpaqueBackings[index]) >= 4.5F);
    }

    constexpr std::array<std::array<int, 2>, 3> kResolutions{{
        {{800, 450}}, {{1280, 720}}, {{1920, 1080}},
    }};
    for (const auto resolution : kResolutions) {
        const auto scaled_shell = platform::inventory_shell_layout(
            resolution[0], resolution[1]);
        const platform::MaterialBagLayout scaled =
            platform::material_bag_layout(
                scaled_shell.right_panel, scaled_shell.geometry_scale);
        ARPG_REQUIRE(scaled.contains_all_slots());
        const float scale = scaled_shell.geometry_scale;
        for (std::size_t index{}; index < items::kMaterialCount; ++index) {
            ARPG_REQUIRE(inside(scaled.slots[index], scaled_shell.right_panel));
            ARPG_REQUIRE(scaled.slots[index].width >= 40.0F);
            ARPG_REQUIRE(scaled.slots[index].height >= 32.0F);
            const auto id = static_cast<items::MaterialId>(index);
            ARPG_REQUIRE(std::string_view{platform::material_bag_display_name(id)}
                == platform::ui_material_name(id));
            ARPG_REQUIRE(std::strstr(
                platform::material_bag_display_name(id), u8"蜕") == nullptr);
            ARPG_REQUIRE(platform::ui_utf8_is_valid(
                platform::material_bag_display_name(id)));
            const Rectangle container = platform::material_bag_text_backing(
                scaled.slots[index], scale);
            char source[64]{};
            const int written = std::snprintf(source, sizeof(source),
                u8"%s ×1", platform::ui_material_name(id));
            ARPG_REQUIRE(written > 0);
            ARPG_REQUIRE(static_cast<std::size_t>(written) < sizeof(source));
            char fitted[64]{};
            const auto centered = platform::fit_centered_ui_text(
                fitted, sizeof(fitted), source, container,
                16.0F * scale, 9.0F * scale, 0.5F * scale,
                &measure_material_text, nullptr);
            ARPG_REQUIRE(centered.drawable);
            ARPG_REQUIRE(!centered.truncated);
            ARPG_REQUIRE(platform::ui_utf8_is_valid(centered.text));
            ARPG_REQUIRE(std::fabs(centered.position.x + centered.size.x * 0.5F
                - (container.x + container.width * 0.5F)) < 0.001F);
            ARPG_REQUIRE(std::fabs(centered.position.y + centered.size.y * 0.5F
                - (container.y + container.height * 0.5F)) < 0.001F);
        }
        const auto confirmation = platform::reinforcement_confirmation_layout(
            resolution[0], resolution[1]);
        ARPG_REQUIRE(inside(confirmation.confirm, confirmation.panel));
        ARPG_REQUIRE(inside(confirmation.cancel, confirmation.panel));
        ARPG_REQUIRE(confirmation.confirm.width >= 40.0F
            && confirmation.confirm.height >= 32.0F);
        ARPG_REQUIRE(confirmation.cancel.width >= 40.0F
            && confirmation.cancel.height >= 32.0F);
        ARPG_REQUIRE(confirmation.confirm.x + confirmation.confirm.width
            <= confirmation.cancel.x);

        const Rectangle danger_bounds{
            confirmation.panel.x + 14.0F * scale,
            confirmation.panel.y + 10.0F * scale,
            confirmation.panel.width - 28.0F * scale, 25.0F * scale};
        const Rectangle question_bounds{
            confirmation.panel.x + 14.0F * scale,
            confirmation.panel.y + 38.0F * scale,
            confirmation.panel.width - 28.0F * scale, 22.0F * scale};
        const std::array<const char*, 4> popup_texts{{
            u8"危险：强化失败，装备丢失", u8"确认强化石？",
            u8"确认", u8"取消",
        }};
        const std::array<Rectangle, 4> popup_bounds{{
            danger_bounds, question_bounds,
            confirmation.confirm, confirmation.cancel,
        }};
        const std::array<float, 4> popup_sizes{{
            16.0F, platform::ui_typography().kInventoryBodyFontSize,
            platform::ui_typography().kInventoryBodyFontSize,
            platform::ui_typography().kInventoryBodyFontSize,
        }};
        const std::array<float, 4> popup_minimum_sizes{{
            10.0F, 9.0F, 9.0F, 9.0F,
        }};
        for (std::size_t index{}; index < popup_texts.size(); ++index) {
            char popup_fitted[96]{};
            const auto centered = platform::fit_centered_ui_text(
                popup_fitted, sizeof(popup_fitted), popup_texts[index],
                popup_bounds[index], popup_sizes[index] * scale,
                popup_minimum_sizes[index] * scale, 0.5F * scale,
                &measure_material_text, nullptr);
            ARPG_REQUIRE(centered.drawable);
            ARPG_REQUIRE(!centered.truncated);
            ARPG_REQUIRE(platform::ui_utf8_is_valid(centered.text));
            ARPG_REQUIRE(std::fabs(centered.position.x
                + centered.size.x * 0.5F
                - (popup_bounds[index].x
                    + popup_bounds[index].width * 0.5F)) < 0.001F);
            ARPG_REQUIRE(std::fabs(centered.position.y
                + centered.size.y * 0.5F
                - (popup_bounds[index].y
                    + popup_bounds[index].height * 0.5F)) < 0.001F);
        }
    }

    char fitted[32]{};
    const Rectangle repeated_bounds{0.0F, 0.0F, 180.0F, 30.0F};
    std::uint64_t checksum{};
    const std::uint64_t allocations_before = arpg::test::allocation_count();
    for (std::size_t iteration{}; iteration < 10000U; ++iteration) {
        const auto centered = platform::fit_centered_ui_text(
            fitted, sizeof(fitted), u8"材料袋", repeated_bounds,
            18.0F, 12.0F, 0.5F, &measure_material_text, nullptr);
        ARPG_REQUIRE(centered.drawable);
        ARPG_REQUIRE(!centered.truncated);
        checksum += static_cast<unsigned char>(fitted[0]);
    }
    ARPG_REQUIRE(arpg::test::allocation_count() == allocations_before);
    ARPG_REQUIRE(checksum != 0U);
    return {};
}

arpg::test::Failure minimum_layout_keeps_shell_bag_valid() noexcept {
    const auto shell = platform::inventory_shell_layout(800, 450);
    const platform::MaterialBagLayout bag =
        platform::material_bag_layout(
            shell.right_panel, shell.geometry_scale);
    ARPG_REQUIRE(bag.contains_all_slots());
    ARPG_REQUIRE(inside(bag.panel, shell.right_panel));
    for (const Rectangle slot : bag.slots)
        ARPG_REQUIRE(inside(slot, shell.right_panel));
    return {};
}

arpg::test::Failure selected_material_requires_an_owned_material_slot() noexcept {
    items::ItemOwnershipState state{};
    const std::size_t chaos = items::material_index(items::MaterialId::chaos);
    const std::size_t exalt = items::material_index(items::MaterialId::exalt);
    const std::size_t directed = items::material_index(items::MaterialId::directed);
    state.materials[chaos] = 4U;
    state.materials[directed] = 2U;
    const auto shell = platform::inventory_shell_layout(1280, 720);
    const auto layout = platform::material_bag_layout(
        shell.right_panel, shell.geometry_scale);
    platform::MaterialBagRenderer renderer{};
    ARPG_REQUIRE(!renderer.selected_material().has_value());
    ARPG_REQUIRE(renderer.process_click(center(layout.slots[chaos]),
        state, layout));
    ARPG_REQUIRE(renderer.selected_material()
        == items::MaterialId::chaos);
    ARPG_REQUIRE(!renderer.process_click(center(layout.slots[exalt]),
        state, layout));
    ARPG_REQUIRE(renderer.selected_material()
        == items::MaterialId::chaos);

    ARPG_REQUIRE(renderer.process_click(center(layout.slots[directed]),
        state, layout));
    ARPG_REQUIRE(renderer.selected_material() == items::MaterialId::directed);
    ARPG_REQUIRE(renderer.directed_category() == items::DirectedCategory::damage);
    ARPG_REQUIRE(platform::material_bag_slot_visual_state(0U, true)
        == platform::MaterialBagSlotVisualState::disabled);
    ARPG_REQUIRE(platform::material_bag_slot_visual_state(1U, false)
        == platform::MaterialBagSlotVisualState::idle);
    ARPG_REQUIRE(platform::material_bag_slot_visual_state(1U, true)
        == platform::MaterialBagSlotVisualState::selected);
    ARPG_REQUIRE(renderer.cycle_directed_category(
        center(layout.slots[directed]), layout));
    ARPG_REQUIRE(renderer.directed_category() == items::DirectedCategory::defense);
    ARPG_REQUIRE(renderer.selected_material() == items::MaterialId::directed);
    ARPG_REQUIRE(state.materials[directed] == 2U);
    return {};
}

arpg::test::Failure selected_material_can_be_cancelled_without_changing_counts() noexcept {
    items::ItemOwnershipState state{};
    const std::size_t chaos = items::material_index(items::MaterialId::chaos);
    state.materials[chaos] = 4U;
    platform::MaterialBagRenderer renderer{};
    ARPG_REQUIRE(renderer.select_slot(chaos, state));
    ARPG_REQUIRE(renderer.clear_selection());
    ARPG_REQUIRE(!renderer.selected_material().has_value());
    ARPG_REQUIRE(state.materials[chaos] == 4U);
    ARPG_REQUIRE(!renderer.clear_selection());
    return {};
}

arpg::test::Failure reinforcement_stone_requires_explicit_destroy_confirmation() noexcept {
    platform::MaterialBagRenderer renderer{};
    ARPG_REQUIRE(!renderer.begin_reinforcement_confirmation(77U, 11U));
    ARPG_REQUIRE(renderer.begin_reinforcement_confirmation(77U, 12U));
    ARPG_REQUIRE(renderer.reinforcement_confirmation_item() == 77U);
    ARPG_REQUIRE(!renderer.resolve_reinforcement_confirmation(false).has_value());
    ARPG_REQUIRE(!renderer.reinforcement_confirmation_item().has_value());
    ARPG_REQUIRE(renderer.begin_reinforcement_confirmation(77U, 13U));
    ARPG_REQUIRE(renderer.resolve_reinforcement_confirmation(true) == 77U);
    ARPG_REQUIRE(!renderer.reinforcement_confirmation_item().has_value());
    return {};
}

arpg::test::Failure material_bag_and_skill_stones_use_authored_icon_resources() noexcept {
    for (std::size_t index{}; index < items::kMaterialCount; ++index) {
        const auto id = static_cast<items::MaterialId>(index);
        ARPG_REQUIRE(platform::material_bag_sprite(id)
            == platform::material_loot_sprite(id));
    }
    const auto active = platform::skill_stone_sprite(
        platform::SkillStoneVisualKind::active);
    const auto support = platform::skill_stone_sprite(
        platform::SkillStoneVisualKind::support);
    ARPG_REQUIRE(active != platform::MaterialSpriteId::missing);
    ARPG_REQUIRE(support != platform::MaterialSpriteId::missing);
    ARPG_REQUIRE(active != support);

    const platform::MaterialManifestDefinition manifest =
        platform::default_material_manifest();
    const auto* active_frame = platform::find_material_frame(manifest, active);
    const auto* support_frame = platform::find_material_frame(manifest, support);
    ARPG_REQUIRE(active_frame != nullptr);
    ARPG_REQUIRE(support_frame != nullptr);
    ARPG_REQUIRE(active_frame->atlas == platform::MaterialAtlasId::items_ui);
    ARPG_REQUIRE(support_frame->atlas == platform::MaterialAtlasId::items_ui);
    ARPG_REQUIRE(active_frame->perceptual_hash
        != support_frame->perceptual_hash);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"material bag contains all slots", &material_bag_has_all_fourteen_slots},
    {"minimum layout keeps shell bag valid", &minimum_layout_keeps_shell_bag_valid},
    {"material bag selects owned material", &selected_material_requires_an_owned_material_slot},
    {"material bag selection cancels", &selected_material_can_be_cancelled_without_changing_counts},
    {"reinforcement requires destroy confirmation", &reinforcement_stone_requires_explicit_destroy_confirmation},
    {"bag and skill stones use authored resources", &material_bag_and_skill_stones_use_authored_icon_resources},
};

}  // namespace

arpg::test::TestSuite material_bag_renderer_suite() noexcept {
    return arpg::test::make_suite("material_bag_renderer", kCases);
}

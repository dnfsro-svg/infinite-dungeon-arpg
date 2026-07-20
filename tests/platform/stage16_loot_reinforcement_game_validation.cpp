#include "dungeon/dungeon_progression.hpp"
#include "dungeon/dungeon_session.hpp"
#include "dungeon_test_support.hpp"
#include "items/item_catalog.hpp"
#include "items/item_generation.hpp"
#include "items/material_catalog.hpp"
#include "material_bag_renderer.hpp"

#include <raylib.h>

#include <algorithm>
#include <cstdint>
#include <filesystem>
#include <fstream>
#include <iostream>
#include <optional>
#include <string>

namespace {

namespace dungeon = arpg::dungeon;
namespace items = arpg::items;
namespace platform = arpg::platform;

struct ScenarioResult final {
    bool picked{};
    bool selected{};
    bool crafted{};
    bool coupon{};
    bool confirmed{};
    bool destroyed{};
    std::uint64_t pickup_count{};
};

bool commit(dungeon::DungeonSession& session) noexcept {
    const auto pending = session.pending_save();
    if (!pending.has_value()) return false;
    session.resolve_pending_save({dungeon::SaveDisposition::committed,
        pending->expected_generation, pending->next_state, pending->kind});
    return session.snapshot().phase != dungeon::RoomPhase::faulted;
}

std::optional<items::ItemInstance> item_for(std::uint64_t id) noexcept {
    const auto* const base = items::base_definition(1U);
    if (base == nullptr) return std::nullopt;
    for (std::uint64_t seed = 1U; seed < 4096U; ++seed) {
        const auto item = items::generate_item(
            {seed, base->slot, 90U, id, items::ItemRarity::rare});
        if (item.has_value() && item->base_id == 1U) return item;
    }
    return std::nullopt;
}

bool item_exists(const dungeon::DungeonSession& session,
    std::uint64_t id) noexcept {
    return std::any_of(session.item_state().items.begin(),
        session.item_state().items.end(), [&](const items::ItemInstance& item) {
            return item.id == id;
        });
}

std::optional<ScenarioResult> run_core_scenario() noexcept {
    const auto base_item = item_for(0x1601U);
    if (!base_item.has_value()) return std::nullopt;
    constexpr std::uint16_t kPickupOrdinal = 4U;
    const std::size_t transmute = items::material_index(items::MaterialId::transmute);
    const std::size_t chaos = items::material_index(items::MaterialId::chaos);
    const std::size_t coupon = items::material_index(items::MaterialId::coupon_12);
    const std::size_t stone = items::material_index(items::MaterialId::reinforcement_stone);

    // Choose a real deterministic root where the actual +12 -> +13 stone
    // transaction fails, so the confirmation-to-destruction state is observed.
    for (std::uint64_t root = 1U; root <= 4096U; ++root) {
        auto built = dungeon::make_initial_run_state(root, dungeon::DungeonRules{});
        if (built.fault != dungeon::DungeonFault::none) continue;
        auto state = built.state;
        state.root_seed = root;
        state.item_ownership.items = {*base_item};
        state.item_ownership.next_item_sequence = 0x1700U;
        state.item_ownership.materials[chaos] = 1U;
        state.item_ownership.materials[coupon] = 1U;
        state.item_ownership.materials[stone] = 1U;
        dungeon::DungeonSession session{dungeon::DungeonRules{}, state};
        if (!session.snapshot().combat.has_value()) continue;
        const auto player = session.snapshot().combat->player.position;
        arpg::test::install_ground_material(session, kPickupOrdinal,
            items::MaterialId::transmute, player);
        if (session.request_material_pickup(kPickupOrdinal)
                != dungeon::RequestResult::accepted || !commit(session)) continue;

        platform::MaterialBagRenderer bag{};
        const bool selected = bag.select_slot(transmute, session.item_state())
            && bag.selected_material() == items::MaterialId::transmute;
        if (!selected) continue;
        if (session.request_craft(items::MaterialId::chaos, base_item->id)
                != dungeon::RequestResult::accepted || !commit(session)) continue;
        if (session.request_coupon(items::MaterialId::coupon_12, base_item->id)
                != dungeon::RequestResult::accepted || !commit(session)) continue;
        const auto before = std::find_if(session.item_state().items.begin(),
            session.item_state().items.end(), [&](const items::ItemInstance& item) {
                return item.id == base_item->id;
            });
        if (before == session.item_state().items.end()
            || before->reinforcement != 12U
            || !bag.begin_reinforcement_confirmation(base_item->id, before->reinforcement)
            || bag.resolve_reinforcement_confirmation(true) != base_item->id) {
            continue;
        }
        if (session.request_reinforcement(base_item->id)
                != dungeon::RequestResult::accepted) continue;
        const auto pending = session.pending_save();
        if (!pending.has_value() || !item_exists(session, base_item->id)
            || std::any_of(pending->next_state.item_ownership.items.begin(),
                pending->next_state.item_ownership.items.end(),
                [&](const items::ItemInstance& item) { return item.id == base_item->id; })) {
            continue;
        }
        if (!commit(session)) continue;
        const bool destroyed = !item_exists(session, base_item->id);
        if (!destroyed) continue;
        return ScenarioResult{true, selected, true, true, true, destroyed,
            session.item_state().materials[transmute]};
    }
    return std::nullopt;
}

bool png_is_1280x720(const std::filesystem::path& path) noexcept {
    const Image image = LoadImage(path.string().c_str());
    const bool valid = image.data != nullptr && image.width == 1280
        && image.height == 720;
    if (image.data != nullptr) UnloadImage(image);
    return valid;
}

void row(int y, const char* label, const char* state, Color color) noexcept {
    DrawRectangle(78, y - 8, 1124, 54, Color{20, 29, 45, 235});
    DrawRectangle(78, y - 8, 8, 54, color);
    DrawText(label, 108, y + 3, 20, RAYWHITE);
    DrawText(state, 760, y + 3, 20, color);
}

bool capture(const std::filesystem::path& root,
    const ScenarioResult& result) noexcept {
    std::error_code error{};
    std::filesystem::create_directories(root, error);
    if (error) return false;
    const auto screenshot = root / "stage16-loot-reinforcement-1280x720.png";
    const auto state_file = root / "stage16-loot-reinforcement-state.txt";
    const auto original_directory = std::filesystem::current_path(error);
    if (error) return false;

    InitWindow(1280, 720, "Stage 16 Loot / Currency / Reinforcement Validation");
    if (!IsWindowReady()) return false;
    SetTargetFPS(60);
    for (int frame = 0; frame < 3; ++frame) {
        BeginDrawing();
        ClearBackground(Color{10, 16, 28, 255});
        DrawRectangle(54, 42, 1172, 98, Color{30, 43, 65, 255});
        DrawText("STAGE 16  |  REAL RAYLIB DETERMINISTIC ACCEPTANCE", 82, 63,
            29, Color{255, 213, 106, 255});
        DrawText("1280 x 720  -  core transaction state rendered after committed saves",
            82, 102, 19, Color{166, 190, 220, 255});
        row(174, "1. Ground material pickup: Transmutation Orb", result.picked ? "COMMITTED +1" : "FAILED", Color{92, 212, 255, 255});
        row(246, "2. Material bag selection: Transmutation Orb", result.selected ? "SELECTED" : "FAILED", Color{126, 233, 175, 255});
        row(318, "3. Currency craft: Chaos Orb on rare weapon", result.crafted ? "COMMITTED" : "FAILED", Color{213, 132, 255, 255});
        row(390, "4. Coupon: +12 scroll applied", result.coupon ? "ENHANCEMENT +12" : "FAILED", Color{255, 203, 96, 255});
        row(462, "5. Destroy confirmation: +12 to +13 stone", result.confirmed ? "CONFIRMED" : "FAILED", Color{255, 151, 88, 255});
        row(534, "6. Reinforcement transaction outcome", result.destroyed ? "FAILED ROLL - EQUIPMENT DESTROYED" : "FAILED", Color{255, 90, 104, 255});
        DrawRectangle(78, 625, 1124, 42, Color{16, 24, 38, 255});
        DrawText("Persistent material bag: transmutation count = 1 | coupon, chaos and stone consumed atomically",
            92, 636, 17, Color{171, 192, 218, 255});
        EndDrawing();
    }
    // raylib 6 prefixes Windows absolute paths with its own startup working
    // directory, so use a relative path from that directory to the isolated
    // evidence root instead.
    const auto raylib_path = std::filesystem::relative(screenshot,
        original_directory, error);
    if (!error) TakeScreenshot(raylib_path.generic_string().c_str());
    CloseWindow();

    std::ofstream output(state_file, std::ios::out | std::ios::trunc);
    output << "result=" << ((result.picked && result.selected && result.crafted
        && result.coupon && result.confirmed && result.destroyed) ? "pass" : "fail") << '\n'
           << "pickup=transmute committed count=" << result.pickup_count << '\n'
           << "bag_selection=transmute\n"
           << "craft=chaos committed\n"
           << "coupon=+12 committed\n"
           << "reinforcement_confirmation=+12_to_+13 accepted\n"
           << "reinforcement_outcome=failed destroyed\n"
           << "screenshot=stage16-loot-reinforcement-1280x720.png\n";
    return output && png_is_1280x720(screenshot);
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || argv[1] == nullptr) return 2;
    const auto result = run_core_scenario();
    const bool passed = result.has_value() && capture(
        std::filesystem::absolute(argv[1]), *result);
    std::cout << "stage16 raylib scenario=" << (passed ? "PASS" : "FAIL") << '\n';
    return passed ? 0 : 1;
}

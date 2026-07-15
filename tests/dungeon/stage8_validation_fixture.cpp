#include "core/deterministic_rng.hpp"
#include "dungeon/dungeon_progression.hpp"
#include "items/item_generation.hpp"
#include "persistence/save_store.hpp"

#include <array>
#include <cstdint>
#include <filesystem>
#include <iostream>
#include <optional>
#include <system_error>

namespace {

constexpr std::uint64_t kDropChanceDomain = 0x44524F505F43484EULL;

bool first_monster_drops(
    const arpg::dungeon::checkpoint::DungeonRunState& state) noexcept {
    auto ordinal_stream = arpg::core::DeterministicRng::derive_stream(
        state.current_room.seed, 0U);
    auto chance_stream = arpg::core::DeterministicRng::derive_stream(
        ordinal_stream.next_u64(), kDropChanceDomain);
    return chance_stream.next_bounded(100U).value() == 0U;
}

std::optional<arpg::dungeon::checkpoint::DungeonRunState>
make_validation_state() noexcept {
    const arpg::dungeon::DungeonRules rules{};
    for (std::uint64_t seed = 1U; seed <= 1000000U; ++seed) {
        auto generated = arpg::dungeon::make_initial_run_state(seed, rules);
        if (generated.fault != arpg::dungeon::DungeonFault::none
                || !first_monster_drops(generated.state)) {
            continue;
        }

        struct ItemSpec final {
            std::uint64_t seed{};
            arpg::items::ItemSlot slot{};
            arpg::items::ItemRarity rarity{};
        };
        constexpr std::array<ItemSpec, 7> kItems{{
            {0x1001U, arpg::items::ItemSlot::weapon,
                arpg::items::ItemRarity::rare},
            {0x1002U, arpg::items::ItemSlot::helmet,
                arpg::items::ItemRarity::magic},
            {0x1003U, arpg::items::ItemSlot::chest,
                arpg::items::ItemRarity::magic},
            {0x1004U, arpg::items::ItemSlot::gloves,
                arpg::items::ItemRarity::rare},
            {0x2001U, arpg::items::ItemSlot::boots,
                arpg::items::ItemRarity::normal},
            {0x2002U, arpg::items::ItemSlot::boots,
                arpg::items::ItemRarity::normal},
            {0x2003U, arpg::items::ItemSlot::boots,
                arpg::items::ItemRarity::normal},
        }};

        auto& ownership = generated.state.item_ownership;
        ownership.items.reserve(kItems.size());
        for (std::size_t index = 0U; index < kItems.size(); ++index) {
            const std::uint64_t id = static_cast<std::uint64_t>(index + 1U);
            const auto item = arpg::items::generate_item({
                kItems[index].seed,
                kItems[index].slot,
                1U,
                id,
                kItems[index].rarity,
            });
            if (!item.has_value()) return std::nullopt;
            ownership.items.push_back(*item);
        }
        ownership.next_item_sequence = kItems.size() + 1U;
        return generated.state;
    }
    return std::nullopt;
}

bool directory_is_safe(const std::filesystem::path& directory) noexcept {
    try {
        return !std::filesystem::exists(directory)
            || std::filesystem::is_empty(directory);
    } catch (...) {
        return false;
    }
}

}  // namespace

int main(int argc, char** argv) {
    if (argc != 2 || argv == nullptr || argv[1] == nullptr) {
        std::cerr << "usage: arpg_stage8_validation_fixture <empty-save-dir>\n";
        return 2;
    }
    const std::filesystem::path directory = std::filesystem::absolute(argv[1]);
    if (!directory_is_safe(directory)) {
        std::cerr << "refusing to overwrite a non-empty directory\n";
        return 3;
    }
    const auto state = make_validation_state();
    if (!state.has_value()) {
        std::cerr << "failed to construct the validation state\n";
        return 4;
    }

    arpg::persistence::SaveStore store({directory});
    const auto committed = store.commit(*state);
    if (committed.state != arpg::persistence::SaveCommitState::committed) {
        std::cerr << "save commit failed: "
                  << static_cast<unsigned>(committed.error) << '\n';
        return 5;
    }
    const auto loaded = store.load();
    if (loaded.state != arpg::persistence::SaveLoadState::ready
            || loaded.checkpoint.root_seed != state->root_seed
            || loaded.checkpoint.item_ownership.items.size() != 7U) {
        std::cerr << "save reload verification failed\n";
        return 6;
    }
    std::cout << "save_dir=" << directory.string() << '\n'
              << "root_seed=" << state->root_seed << '\n'
              << "room_seed=" << state->current_room.seed << '\n'
              << "first_monster_drop=1\n"
              << "inventory_items=7\n";
    return 0;
}

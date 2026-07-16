#include "combat/monster_affix_generation.hpp"

#include <raylib.h>

#include <array>
#include <cstdint>
#include <filesystem>
#include <string>

namespace {

struct FixedAffixDisplay final {
    const char* name{};
    arpg::combat::MonsterAffixId id{arpg::combat::MonsterAffixId::mighty};
    arpg::combat::MonsterAffixTier tier{arpg::combat::MonsterAffixTier::m1};
    Color color{};
};

arpg::combat::MonsterAffixSet one_affix(
    arpg::combat::MonsterAffixId id,
    arpg::combat::MonsterAffixTier tier) noexcept {
    arpg::combat::MonsterAffixSet result{};
    result.values[0] = {id, tier};
    result.count = 1U;
    return result;
}

}  // namespace

int main() {
    constexpr std::array<FixedAffixDisplay, 12> kAffixes{{
        {"MIGHTY", arpg::combat::MonsterAffixId::mighty,
            arpg::combat::MonsterAffixTier::m1, {235, 179, 81, 255}},
        {"FRENZY", arpg::combat::MonsterAffixId::frenzy,
            arpg::combat::MonsterAffixTier::m2, {245, 108, 94, 255}},
        {"SWIFT", arpg::combat::MonsterAffixId::swift,
            arpg::combat::MonsterAffixTier::m3, {121, 208, 255, 255}},
        {"ARMORED", arpg::combat::MonsterAffixId::armored,
            arpg::combat::MonsterAffixTier::m1, {177, 190, 203, 255}},
        {"SHIELD", arpg::combat::MonsterAffixId::shielding,
            arpg::combat::MonsterAffixTier::m2, {96, 217, 186, 255}},
        {"MULTISHOT", arpg::combat::MonsterAffixId::multishot,
            arpg::combat::MonsterAffixTier::m3, {255, 123, 90, 255}},
        {"BURNING", arpg::combat::MonsterAffixId::burning_ground,
            arpg::combat::MonsterAffixTier::m1, {255, 144, 62, 255}},
        {"CHILLING", arpg::combat::MonsterAffixId::chilling,
            arpg::combat::MonsterAffixTier::m2, {112, 199, 255, 255}},
        {"CHAIN", arpg::combat::MonsterAffixId::chain_lightning,
            arpg::combat::MonsterAffixTier::m3, {255, 219, 92, 255}},
        {"CORROSION", arpg::combat::MonsterAffixId::chaos_corrosion,
            arpg::combat::MonsterAffixTier::m1, {200, 108, 255, 255}},
        {"BLINK", arpg::combat::MonsterAffixId::blink_assault,
            arpg::combat::MonsterAffixTier::m2, {255, 112, 176, 255}},
        {"DEATH BLAST", arpg::combat::MonsterAffixId::death_blast,
            arpg::combat::MonsterAffixTier::m3, {255, 73, 73, 255}},
    }};

    InitWindow(1280, 720, "Infinite Dungeon - Stage 9 Fixed Affix Validation");
    if (!IsWindowReady()) return 2;
    SetTargetFPS(60);
    std::error_code directory_error;
    const auto evidence_directory = std::filesystem::path{
        GetApplicationDirectory()}.parent_path().parent_path()
        / "docs" / "validation" / "evidence" / "stage9";
    std::filesystem::create_directories(evidence_directory, directory_error);
    const std::string automatic_capture = (evidence_directory
        / "01-fixed-affix-room-render.png").string();
    int rendered_frames = 0;
    while (!WindowShouldClose()) {
        BeginDrawing();
        ClearBackground({13, 17, 27, 255});
        DrawText("STAGE 9 - TEST-ONLY FIXED AFFIX ROOM", 44, 34, 30, RAYWHITE);
        DrawText("12 affixes / M1 M2 M3 / no production save or dungeon rule", 44,
            76, 20, {170, 190, 210, 255});
        for (std::size_t index = 0U; index < kAffixes.size(); ++index) {
            const int column = static_cast<int>(index % 4U);
            const int row = static_cast<int>(index / 4U);
            const int x = 44 + column * 302;
            const int y = 132 + row * 172;
            const auto& affix = kAffixes[index];
            const auto score = arpg::combat::monster_affix_danger_score(
                one_affix(affix.id, affix.tier));
            DrawRectangleRounded({static_cast<float>(x), static_cast<float>(y),
                270.0F, 138.0F}, 0.09F, 8, {28, 35, 53, 255});
            DrawRectangleRoundedLines({static_cast<float>(x), static_cast<float>(y),
                270.0F, 138.0F}, 0.09F, 8, affix.color);
            DrawText(affix.name, x + 18, y + 18, 24, affix.color);
            DrawText(affix.tier == arpg::combat::MonsterAffixTier::m1 ? "M1"
                : affix.tier == arpg::combat::MonsterAffixTier::m2 ? "M2" : "M3",
                x + 18, y + 56, 32, RAYWHITE);
            DrawText(TextFormat("danger %u", static_cast<unsigned>(score)),
                x + 18, y + 100, 18, {190, 202, 216, 255});
        }
        DrawText("Esc closes | F12 captures validation screen", 44, 666, 18,
            {170, 190, 210, 255});
        if (++rendered_frames == 60 || IsKeyPressed(KEY_F12)) {
            TakeScreenshot(automatic_capture.c_str());
        }
        EndDrawing();
    }
    CloseWindow();
    return 0;
}

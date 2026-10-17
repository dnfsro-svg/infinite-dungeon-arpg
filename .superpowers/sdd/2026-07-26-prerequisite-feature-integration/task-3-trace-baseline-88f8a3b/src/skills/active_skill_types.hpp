#pragma once

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::skills {

enum class ActiveSkillId : std::uint8_t {
    draw_slash = 0,
    storm_swords = 1,
    count = 2,
    none = 0xFF,
};

enum class SupportSkillId : std::uint8_t {
    count = 0,
    none = 0xFF,
};

inline constexpr std::size_t kActiveSkillCount = 2U;
inline constexpr std::size_t kActiveSkillSlotCount = 5U;
inline constexpr std::size_t kSupportSlotsPerActive = 5U;

struct ActiveSkillSlot final {
    ActiveSkillId active{ActiveSkillId::none};
    std::array<SupportSkillId, kSupportSlotsPerActive> supports{{
        SupportSkillId::none, SupportSkillId::none,
        SupportSkillId::none, SupportSkillId::none,
        SupportSkillId::none,
    }};
};

struct SkillLoadoutState final {
    std::array<ActiveSkillSlot, kActiveSkillSlotCount> slots{};
    std::uint64_t owned_active_bits{};
};

}  // namespace arpg::skills

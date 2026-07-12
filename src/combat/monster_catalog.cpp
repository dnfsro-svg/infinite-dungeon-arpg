#include "combat/monster_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::combat {
namespace {

constexpr std::uint16_t tag(MonsterTag value) noexcept {
    return static_cast<std::uint16_t>(value);
}

constexpr std::uint16_t tags(
    MonsterTag first,
    MonsterTag second) noexcept {
    return static_cast<std::uint16_t>(tag(first) | tag(second));
}

constexpr std::uint16_t tags(
    MonsterTag first,
    MonsterTag second,
    MonsterTag third) noexcept {
    return static_cast<std::uint16_t>(tags(first, second) | tag(third));
}

constexpr std::uint16_t tags(
    MonsterTag first,
    MonsterTag second,
    MonsterTag third,
    MonsterTag fourth) noexcept {
    return static_cast<std::uint16_t>(tags(first, second, third) | tag(fourth));
}

constexpr std::array<MonsterDefinition, 8> kCatalog{{
    {MonsterId::fire_bomber, 0, tags(MonsterTag::high_priority,
         MonsterTag::direct_target), 3, 220, 0, 0.040F, 1.2F,
     60, 1, 0, 0, 120, 0.0F, 0, FeedbackLevel::heavy},
    {MonsterId::fire_charger, 0, tags(MonsterTag::melee,
         MonsterTag::high_priority, MonsterTag::direct_target),
     4, 420, 60, 0.030F, 5.0F, 45, 18, 35, 100, 90, 0.0F, 0,
     FeedbackLevel::heavy},
    {MonsterId::water_bulwark, 1, tags(MonsterTag::melee,
         MonsterTag::direct_target), 4, 700, 120, 0.025F, 1.0F,
     24, 6, 30, 72, 70, 0.0F, 0, FeedbackLevel::heavy},
    {MonsterId::water_support, 1, tag(MonsterTag::support),
     3, 300, 0, 0.030F, 4.0F, 36, 1, 24, 120, 0, 0.0F, 0,
     FeedbackLevel::medium},
    {MonsterId::lightning_shooter, 2, tags(MonsterTag::ranged,
         MonsterTag::direct_target), 3, 240, 0, 0.035F, 4.5F,
     30, 1, 20, 75, 40, 0.0F, 0, FeedbackLevel::medium},
    {MonsterId::lightning_dasher, 2, tags(MonsterTag::melee,
         MonsterTag::high_priority, MonsterTag::direct_target),
     3, 280, 0, 0.050F, 3.0F, 24, 12, 30, 80, 60, 0.0F, 0,
     FeedbackLevel::medium},
    {MonsterId::chaos_chaser, 3, tags(MonsterTag::melee,
         MonsterTag::direct_target), 2, 260, 0, 0.045F, 0.9F,
     12, 4, 18, 42, 45, 0.0F, 0, FeedbackLevel::light},
    {MonsterId::chaos_hazard, 3, tags(MonsterTag::ranged,
         MonsterTag::high_priority, MonsterTag::ground_hazard,
         MonsterTag::direct_target), 4, 320, 0, 0.030F, 4.0F,
     45, 1, 25, 120, 35, 0.0F, 180, FeedbackLevel::heavy},
}};

constexpr bool has_tag(
    const MonsterDefinition& definition,
    MonsterTag value) noexcept {
    return (definition.tags & tag(value)) != 0U;
}

constexpr bool validate_catalog() noexcept {
    bool has_direct_target = false;
    for (std::size_t index = 0; index < kCatalog.size(); ++index) {
        const auto& definition = kCatalog[index];
        if (static_cast<std::size_t>(definition.id) != index ||
            definition.preferred_ecology > 3U ||
            definition.threat_cost == 0U || definition.max_hp <= 0) {
            return false;
        }
        has_direct_target = has_direct_target ||
            has_tag(definition, MonsterTag::direct_target);
    }
    return has_direct_target;
}

static_assert(kCatalog.size() == static_cast<std::size_t>(MonsterId::count));
static_assert(validate_catalog());

}  // namespace

const MonsterDefinition* monster_definition(MonsterId id) noexcept {
    const auto index = static_cast<std::size_t>(id);
    return index < kCatalog.size() ? &kCatalog[index] : nullptr;
}

bool monster_catalog_valid() noexcept {
    return validate_catalog();
}

}  // namespace arpg::combat

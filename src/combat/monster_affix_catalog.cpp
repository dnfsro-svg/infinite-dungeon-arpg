#include "combat/monster_affix_catalog.hpp"

#include <array>
#include <cstddef>
#include <cstdint>

namespace arpg::combat {
namespace {

constexpr std::uint16_t tag(MonsterTag value) noexcept {
    return static_cast<std::uint16_t>(value);
}

constexpr std::uint16_t kKnownMonsterTags = tag(MonsterTag::melee)
    | tag(MonsterTag::ranged) | tag(MonsterTag::support)
    | tag(MonsterTag::high_priority) | tag(MonsterTag::ground_hazard)
    | tag(MonsterTag::direct_target) | tag(MonsterTag::projectile_capable);

constexpr std::array<MonsterAffixDefinition, 12> kCatalog{{
    {MonsterAffixId::mighty, "Mighty", "MGT", MonsterAffixDanger::low, 100U, 0U, 0U, 0U, {{{13000, 8500}, {16000, 7000}, {20000, 5500}}}},
    {MonsterAffixId::frenzy, "Frenzy", "FRZ", MonsterAffixDanger::medium, 100U, 0U, 0U, 0U, {{{11500, 9000}, {13000, 8000}, {15000, 7000}}}},
    {MonsterAffixId::swift, "Swift", "SWF", MonsterAffixDanger::medium, 100U, 0U, 0U, 0U, {{{11500, 9200}, {13000, 8400}, {14500, 7600}}}},
    {MonsterAffixId::armored, "Armored", "ARM", MonsterAffixDanger::low, 100U, 0U, 0U, 0U, {{{75}, {325}, {775}}}},
    {MonsterAffixId::shielding, "Shielding", "SHD", MonsterAffixDanger::medium, 100U, 0U, 0U, 0U, {{{2000, 0, 0, 180}, {3500, 0, 0, 150}, {5000, 0, 0, 120}}}},
    {MonsterAffixId::multishot, "Multishot", "MULTI", MonsterAffixDanger::high, 100U, tag(MonsterTag::projectile_capable), 0U, 0U, {{{7500, 0, 0, 0, 0, 0.0F, 2U}, {6000, 0, 0, 0, 0, 0.0F, 3U}, {5000, 0, 0, 0, 0, 0.0F, 4U}}}},
    {MonsterAffixId::burning_ground, "Burning Ground", "BURN", MonsterAffixDanger::high, 100U, 0U, 0U, 0U, {{{0, 0, 180, 120, 25, 0.75F}, {0, 0, 150, 180, 35, 0.90F}, {0, 0, 120, 240, 45, 1.05F}}}},
    {MonsterAffixId::chilling, "Chilling", "CHILL", MonsterAffixDanger::medium, 100U, tag(MonsterTag::direct_target), 0U, 0U, {{{1500, 1500, 0, 60}, {2500, 2500, 0, 90}, {3500, 3500, 0, 120}}}},
    {MonsterAffixId::chain_lightning, "Chain Lightning", "CHAIN", MonsterAffixDanger::high, 100U, tag(MonsterTag::direct_target), 0U, 0U, {{{0, 0, 42, 0, 70, 0.65F}, {0, 0, 42, 0, 110, 0.80F}, {0, 0, 42, 0, 160, 0.95F}}}},
    {MonsterAffixId::chaos_corrosion, "Chaos Corrosion", "CORR", MonsterAffixDanger::medium, 100U, tag(MonsterTag::direct_target), 0U, 0U, {{{0, 0, 0, 120, 20}, {0, 0, 0, 180, 30}, {0, 0, 0, 240, 45}}}},
    {MonsterAffixId::blink_assault, "Blink Assault", "BLINK", MonsterAffixDanger::high, 100U, tag(MonsterTag::melee), 0U, 0U, {{{12000, 0, 480, 42}, {13500, 0, 360, 36}, {15000, 0, 240, 30}}}},
    {MonsterAffixId::death_blast, "Death Blast", "DEATH", MonsterAffixDanger::high, 100U, 0U, 0U, 0U, {{{0, 0, 66, 0, 120, 0.90F}, {0, 0, 54, 0, 190, 1.15F}, {0, 0, 45, 0, 280, 1.40F}}}},
}};

constexpr bool catalog_valid() noexcept {
    for (std::size_t index = 0U; index < kCatalog.size(); ++index) {
        const MonsterAffixDefinition& definition = kCatalog[index];
        if (static_cast<std::size_t>(definition.id) != index
            || definition.name.empty() || definition.short_name.empty()
            || definition.weight == 0U
            || static_cast<std::uint8_t>(definition.danger)
                > static_cast<std::uint8_t>(MonsterAffixDanger::high)
            || (definition.required_tags & ~kKnownMonsterTags) != 0U
            || (definition.forbidden_tags & ~kKnownMonsterTags) != 0U
            || (definition.required_tags & definition.forbidden_tags) != 0U)
            return false;
    }
    return true;
}

static_assert(kCatalog.size() == static_cast<std::size_t>(MonsterAffixId::count));
static_assert(catalog_valid());

}  // namespace

const MonsterAffixDefinition* monster_affix_definition(MonsterAffixId id) noexcept {
    const std::size_t index = static_cast<std::size_t>(id);
    return index < kCatalog.size() ? &kCatalog[index] : nullptr;
}

bool monster_affix_catalog_valid() noexcept { return catalog_valid(); }

}  // namespace arpg::combat

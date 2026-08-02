#include "persistence/room_progress_codec.hpp"

#include "abyss/abyss_rewards.hpp"
#include "checkpoint/room_checkpoint_validation.hpp"
#include "items/item_catalog.hpp"
#include "persistence/crc32.hpp"

#include <algorithm>
#include <array>
#include <cmath>
#include <cstring>
#include <limits>
#include <memory>

namespace arpg::persistence {
namespace {

namespace checkpoint = arpg::checkpoint;

constexpr std::array<std::uint8_t, 8U> kMagic{{
    'A', 'R', 'P', 'G', 'S', 'V', '9', '\0'}};
constexpr std::size_t kHeaderSize = 32U;

class Writer final {
public:
    Writer(std::uint8_t* data, std::size_t capacity, std::size_t offset) noexcept
        : data_(data), capacity_(capacity), offset_(offset) {}

    bool u8(std::uint8_t value) noexcept {
        if (!reserve(1U)) return false;
        data_[offset_++] = value;
        return true;
    }
    bool boolean(bool value) noexcept { return u8(value ? 1U : 0U); }
    bool u16(std::uint16_t value) noexcept {
        if (!reserve(2U)) return false;
        data_[offset_++] = static_cast<std::uint8_t>(value);
        data_[offset_++] = static_cast<std::uint8_t>(value >> 8U);
        return true;
    }
    bool u32(std::uint32_t value) noexcept {
        if (!reserve(4U)) return false;
        for (std::size_t index = 0U; index < 4U; ++index) {
            data_[offset_++] = static_cast<std::uint8_t>(value >> (index * 8U));
        }
        return true;
    }
    bool u64(std::uint64_t value) noexcept {
        if (!reserve(8U)) return false;
        for (std::size_t index = 0U; index < 8U; ++index) {
            data_[offset_++] = static_cast<std::uint8_t>(value >> (index * 8U));
        }
        return true;
    }
    bool i32(std::int32_t value) noexcept {
        std::uint32_t bits{};
        std::memcpy(&bits, &value, sizeof(bits));
        return u32(bits);
    }
    bool i64(std::int64_t value) noexcept {
        std::uint64_t bits{};
        std::memcpy(&bits, &value, sizeof(bits));
        return u64(bits);
    }
    bool f32(float value) noexcept {
        if (!std::isfinite(value)) return false;
        std::uint32_t bits{};
        static_assert(sizeof(bits) == sizeof(value));
        std::memcpy(&bits, &value, sizeof(bits));
        return u32(bits);
    }
    bool bytes(const std::uint8_t* source, std::size_t size) noexcept {
        if (source == nullptr || !reserve(size)) return false;
        std::memcpy(data_ + offset_, source, size);
        offset_ += size;
        return true;
    }
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }
    [[nodiscard]] std::uint8_t* current() noexcept {
        return data_ == nullptr || offset_ > capacity_ ? nullptr
            : data_ + offset_;
    }
    bool skip(std::size_t count) noexcept {
        if (!reserve(count)) return false;
        offset_ += count;
        return true;
    }

private:
    bool reserve(std::size_t count) const noexcept {
        return data_ != nullptr && offset_ <= capacity_
            && count <= capacity_ - offset_;
    }
    std::uint8_t* data_{};
    std::size_t capacity_{};
    std::size_t offset_{};
};

class Reader final {
public:
    Reader(const std::uint8_t* data, std::size_t size, std::size_t offset) noexcept
        : data_(data), size_(size), offset_(offset) {}

    bool u8(std::uint8_t& value) noexcept {
        if (!reserve(1U)) return false;
        value = data_[offset_++];
        return true;
    }
    bool boolean(bool& value) noexcept {
        std::uint8_t encoded{};
        if (!u8(encoded) || encoded > 1U) return false;
        value = encoded != 0U;
        return true;
    }
    bool u16(std::uint16_t& value) noexcept {
        if (!reserve(2U)) return false;
        value = static_cast<std::uint16_t>(data_[offset_])
            | static_cast<std::uint16_t>(data_[offset_ + 1U] << 8U);
        offset_ += 2U;
        return true;
    }
    bool u32(std::uint32_t& value) noexcept {
        if (!reserve(4U)) return false;
        value = 0U;
        for (std::size_t index = 0U; index < 4U; ++index) {
            value |= static_cast<std::uint32_t>(data_[offset_ + index])
                << (index * 8U);
        }
        offset_ += 4U;
        return true;
    }
    bool u64(std::uint64_t& value) noexcept {
        if (!reserve(8U)) return false;
        value = 0U;
        for (std::size_t index = 0U; index < 8U; ++index) {
            value |= static_cast<std::uint64_t>(data_[offset_ + index])
                << (index * 8U);
        }
        offset_ += 8U;
        return true;
    }
    bool i32(std::int32_t& value) noexcept {
        std::uint32_t bits{};
        if (!u32(bits)) return false;
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }
    bool i64(std::int64_t& value) noexcept {
        std::uint64_t bits{};
        if (!u64(bits)) return false;
        std::memcpy(&value, &bits, sizeof(value));
        return true;
    }
    bool f32(float& value) noexcept {
        std::uint32_t bits{};
        if (!u32(bits)) return false;
        std::memcpy(&value, &bits, sizeof(value));
        return std::isfinite(value);
    }
    [[nodiscard]] const std::uint8_t* current() const noexcept {
        return reserve(0U) ? data_ + offset_ : nullptr;
    }
    bool skip(std::size_t count) noexcept {
        if (!reserve(count)) return false;
        offset_ += count;
        return true;
    }
    [[nodiscard]] std::size_t offset() const noexcept { return offset_; }

private:
    bool reserve(std::size_t count) const noexcept {
        return data_ != nullptr && offset_ <= size_ && count <= size_ - offset_;
    }
    const std::uint8_t* data_{};
    std::size_t size_{};
    std::size_t offset_{};
};

class ComparingReader final {
public:
    ComparingReader(const std::uint8_t* data, std::size_t size,
        std::size_t offset) noexcept
        : reader_(data, size, offset) {}

    bool u8(std::uint8_t expected) noexcept {
        std::uint8_t actual{};
        return reader_.u8(actual) && actual == expected;
    }
    bool boolean(bool expected) noexcept {
        std::uint8_t actual{};
        return reader_.u8(actual) && actual <= 1U
            && (actual != 0U) == expected;
    }
    bool u16(std::uint16_t expected) noexcept {
        std::uint16_t actual{};
        return reader_.u16(actual) && actual == expected;
    }
    bool u32(std::uint32_t expected) noexcept {
        std::uint32_t actual{};
        return reader_.u32(actual) && actual == expected;
    }
    bool u64(std::uint64_t expected) noexcept {
        std::uint64_t actual{};
        return reader_.u64(actual) && actual == expected;
    }
    bool i32(std::int32_t expected) noexcept {
        std::int32_t actual{};
        return reader_.i32(actual) && actual == expected;
    }
    bool i64(std::int64_t expected) noexcept {
        std::int64_t actual{};
        return reader_.i64(actual) && actual == expected;
    }
    bool f32(float expected) noexcept {
        float actual{};
        if (!std::isfinite(expected) || !reader_.f32(actual)) return false;
        std::uint32_t actual_bits{};
        std::uint32_t expected_bits{};
        std::memcpy(&actual_bits, &actual, sizeof(actual_bits));
        std::memcpy(&expected_bits, &expected, sizeof(expected_bits));
        return actual_bits == expected_bits;
    }
    [[nodiscard]] std::size_t offset() const noexcept {
        return reader_.offset();
    }

private:
    Reader reader_;
};

template <typename Sink>
bool write_vec(Sink& writer, checkpoint::CheckpointVec3 value) noexcept {
    return writer.f32(value.x) && writer.f32(value.y) && writer.f32(value.z);
}

bool read_vec(Reader& reader, checkpoint::CheckpointVec3& value) noexcept {
    return reader.f32(value.x) && reader.f32(value.y) && reader.f32(value.z);
}

template <typename Sink>
bool write_modifier(Sink& writer,
    const modifiers::Modifier& value) noexcept {
    return writer.u32(value.id)
        && writer.u16(static_cast<std::uint16_t>(value.stat))
        && writer.u8(static_cast<std::uint8_t>(value.operation))
        && writer.i64(value.value) && writer.u64(value.required_tags)
        && writer.u64(value.forbidden_tags)
        && writer.u64(value.required_conditions)
        && writer.u16(value.priority)
        && writer.u16(static_cast<std::uint16_t>(value.conversion_target));
}

bool read_modifier(Reader& reader, modifiers::Modifier& value) noexcept {
    std::uint16_t stat{};
    std::uint8_t operation{};
    std::uint16_t target{};
    if (!reader.u32(value.id) || !reader.u16(stat) || !reader.u8(operation)
            || !reader.i64(value.value) || !reader.u64(value.required_tags)
            || !reader.u64(value.forbidden_tags)
            || !reader.u64(value.required_conditions)
            || !reader.u16(value.priority) || !reader.u16(target)) return false;
    value.stat = static_cast<modifiers::StatId>(stat);
    value.operation = static_cast<modifiers::ModifierOperation>(operation);
    value.conversion_target = static_cast<modifiers::StatId>(target);
    return true;
}

template <typename Sink>
bool write_effects(Sink& writer,
    const modifiers::EffectSetCheckpoint& value) noexcept {
    for (const auto& effect : value.effects) {
        if (!writer.u32(effect.id) || !writer.i32(effect.remaining_ticks)
                || !writer.u8(effect.stacks) || !writer.u8(effect.max_stacks)
                || !writer.u8(static_cast<std::uint8_t>(effect.refresh_rule))
                || !writer.i64(effect.strength)
                || !write_modifier(writer, effect.modifier)
                || !writer.boolean(effect.has_modifier)
                || !writer.u8(static_cast<std::uint8_t>(
                    effect.on_expire.kind))
                || !writer.i64(effect.on_expire.value)
                || !writer.boolean(effect.occupied)) return false;
    }
    for (const auto& command : value.commands) {
        if (!writer.u8(static_cast<std::uint8_t>(command.kind))
                || !writer.i64(command.value)
                || !writer.u32(command.effect_id)) return false;
    }
    return writer.u8(value.command_head) && writer.u8(value.command_count)
        && writer.u32(value.diagnostics.effect_overflows)
        && writer.u32(value.diagnostics.command_overflows);
}

bool read_effects(Reader& reader,
    modifiers::EffectSetCheckpoint& value) noexcept {
    for (auto& effect : value.effects) {
        std::uint8_t refresh{};
        std::uint8_t expire{};
        if (!reader.u32(effect.id) || !reader.i32(effect.remaining_ticks)
                || !reader.u8(effect.stacks) || !reader.u8(effect.max_stacks)
                || !reader.u8(refresh) || !reader.i64(effect.strength)
                || !read_modifier(reader, effect.modifier)
                || !reader.boolean(effect.has_modifier)
                || !reader.u8(expire) || !reader.i64(effect.on_expire.value)
                || !reader.boolean(effect.occupied)) return false;
        effect.refresh_rule = static_cast<modifiers::RefreshRule>(refresh);
        effect.on_expire.kind =
            static_cast<modifiers::EffectCommandKind>(expire);
    }
    for (auto& command : value.commands) {
        std::uint8_t kind{};
        if (!reader.u8(kind) || !reader.i64(command.value)
                || !reader.u32(command.effect_id)) return false;
        command.kind = static_cast<modifiers::EffectCommandKind>(kind);
    }
    return reader.u8(value.command_head) && reader.u8(value.command_count)
        && reader.u32(value.diagnostics.effect_overflows)
        && reader.u32(value.diagnostics.command_overflows);
}

template <typename Sink>
bool write_item(Sink& writer, const items::ItemInstance& item) noexcept {
    if (!writer.u64(item.id) || !writer.u8(item.base_id)
            || !writer.u8(static_cast<std::uint8_t>(item.rarity))
            || !writer.u8(item.item_level) || !writer.u8(item.required_level)
            || !writer.u8(item.affix_count)) return false;
    for (const std::uint8_t value : item.reserved) {
        if (!writer.u8(value)) return false;
    }
    for (const auto& affix : item.affixes) {
        if (!writer.u16(affix.affix_id) || !writer.u8(affix.tier)
                || !writer.u8(affix.variant)
                || !writer.u16(affix.value_roll_bp)) return false;
    }
    if (!writer.u32(item.reinforcement)) return false;
    for (const std::uint8_t value : item.extension_reserved) {
        if (!writer.u8(value)) return false;
    }
    return true;
}

bool read_item(Reader& reader, items::ItemInstance& item) noexcept {
    std::uint8_t rarity{};
    if (!reader.u64(item.id) || !reader.u8(item.base_id)
            || !reader.u8(rarity) || !reader.u8(item.item_level)
            || !reader.u8(item.required_level)
            || !reader.u8(item.affix_count)) return false;
    item.rarity = static_cast<items::ItemRarity>(rarity);
    for (std::uint8_t& value : item.reserved) {
        if (!reader.u8(value)) return false;
    }
    for (auto& affix : item.affixes) {
        if (!reader.u16(affix.affix_id) || !reader.u8(affix.tier)
                || !reader.u8(affix.variant)
                || !reader.u16(affix.value_roll_bp)) return false;
    }
    if (!reader.u32(item.reinforcement)) return false;
    for (std::uint8_t& value : item.extension_reserved) {
        if (!reader.u8(value)) return false;
    }
    return true;
}

template <typename Sink>
bool write_affixes(Sink& writer,
    const checkpoint::MonsterAffixSet& value) noexcept {
    if (!writer.u8(value.count)) return false;
    for (const auto& affix : value.values) {
        if (!writer.u8(static_cast<std::uint8_t>(affix.id))
                || !writer.u8(static_cast<std::uint8_t>(affix.tier))) {
            return false;
        }
    }
    return true;
}

bool read_affixes(Reader& reader, checkpoint::MonsterAffixSet& value) noexcept {
    if (!reader.u8(value.count)) return false;
    for (auto& affix : value.values) {
        std::uint8_t id{};
        std::uint8_t tier{};
        if (!reader.u8(id) || !reader.u8(tier)) return false;
        affix.id = static_cast<checkpoint::MonsterAffixId>(id);
        affix.tier = static_cast<checkpoint::MonsterAffixTier>(tier);
    }
    return true;
}

template <typename Sink>
bool write_source(Sink& writer,
    const checkpoint::PlayerDamageSource& source) noexcept {
    return writer.u8(static_cast<std::uint8_t>(source.kind))
        && writer.u8(static_cast<std::uint8_t>(source.monster))
        && writer.u16(source.detail_id);
}

bool read_source(Reader& reader,
    checkpoint::PlayerDamageSource& source) noexcept {
    std::uint8_t kind{};
    std::uint8_t monster{};
    if (!reader.u8(kind) || !reader.u8(monster)
            || !reader.u16(source.detail_id)) return false;
    source.kind = static_cast<checkpoint::PlayerDamageSourceKind>(kind);
    source.monster = static_cast<checkpoint::MonsterId>(monster);
    return true;
}

template <typename Sink>
bool write_player(Sink& writer,
    const checkpoint::PlayerCombatCheckpoint& value) noexcept {
    if (!write_vec(writer, value.position) || !write_vec(writer, value.velocity)
            || !writer.u8(static_cast<std::uint8_t>(value.facing))
            || !writer.u8(static_cast<std::uint8_t>(value.state))
            || !writer.u8(value.combo_stage)
            || !writer.boolean(value.air_attack_available)
            || !writer.i32(value.hp) || !writer.i32(value.max_hp)
            || !writer.i32(value.barrier) || !writer.i32(value.max_barrier)) {
        return false;
    }
    for (const std::int32_t amount : value.damage_reduction) {
        if (!writer.i32(amount)) return false;
    }
    for (const std::int32_t amount : value.damage_reduction_cap) {
        if (!writer.i32(amount)) return false;
    }
    if (!writer.i64(value.armor) || !writer.i64(value.evasion)
            || !writer.i32(value.armor_reduction_bp)
            || !writer.i32(value.evasion_rate_bp)
            || !writer.u16(value.hurt_ticks)
            || !writer.u16(value.invulnerability_ticks)
            || !writer.i32(value.status.slow_bp)
            || !writer.u16(value.status.slow_ticks)
            || !writer.i32(value.status.corrosion_damage_per_second)
            || !writer.u16(value.status.corrosion_ticks)
            || !writer.u8(value.status.corrosion_tick_phase)
            || !write_source(writer, value.status.corrosion_source)) {
        return false;
    }
    for (const std::uint16_t ticks : value.skill_cooldowns) {
        if (!writer.u16(ticks)) return false;
    }
    return true;
}

bool read_player(Reader& reader,
    checkpoint::PlayerCombatCheckpoint& value) noexcept {
    std::uint8_t facing{};
    std::uint8_t state{};
    if (!read_vec(reader, value.position) || !read_vec(reader, value.velocity)
            || !reader.u8(facing) || !reader.u8(state)
            || !reader.u8(value.combo_stage)
            || !reader.boolean(value.air_attack_available)
            || !reader.i32(value.hp) || !reader.i32(value.max_hp)
            || !reader.i32(value.barrier) || !reader.i32(value.max_barrier)) {
        return false;
    }
    value.facing = static_cast<checkpoint::Facing>(
        static_cast<std::int8_t>(facing));
    value.state = static_cast<checkpoint::PlayerState>(state);
    for (std::int32_t& amount : value.damage_reduction) {
        if (!reader.i32(amount)) return false;
    }
    for (std::int32_t& amount : value.damage_reduction_cap) {
        if (!reader.i32(amount)) return false;
    }
    if (!reader.i64(value.armor) || !reader.i64(value.evasion)
            || !reader.i32(value.armor_reduction_bp)
            || !reader.i32(value.evasion_rate_bp)
            || !reader.u16(value.hurt_ticks)
            || !reader.u16(value.invulnerability_ticks)
            || !reader.i32(value.status.slow_bp)
            || !reader.u16(value.status.slow_ticks)
            || !reader.i32(value.status.corrosion_damage_per_second)
            || !reader.u16(value.status.corrosion_ticks)
            || !reader.u8(value.status.corrosion_tick_phase)
            || !read_source(reader, value.status.corrosion_source)) {
        return false;
    }
    for (std::uint16_t& ticks : value.skill_cooldowns) {
        if (!reader.u16(ticks)) return false;
    }
    return true;
}

template <typename Sink>
bool write_attack(Sink& writer,
    const checkpoint::AttackCheckpoint& value) noexcept {
    if (!writer.u8(static_cast<std::uint8_t>(value.id))
            || !writer.u16(value.elapsed_ticks)
            || !writer.u16(value.startup_ticks)
            || !writer.u16(value.recovery_ticks)
            || !writer.boolean(value.connected)
            || !writer.boolean(value.impact_event_emitted)) return false;
    for (const std::uint64_t word : value.hit_targets.words) {
        if (!writer.u64(word)) return false;
    }
    return true;
}

bool read_attack(Reader& reader, checkpoint::AttackCheckpoint& value) noexcept {
    std::uint8_t id{};
    if (!reader.u8(id) || !reader.u16(value.elapsed_ticks)
            || !reader.u16(value.startup_ticks)
            || !reader.u16(value.recovery_ticks)
            || !reader.boolean(value.connected)
            || !reader.boolean(value.impact_event_emitted)) return false;
    value.id = static_cast<checkpoint::AttackId>(id);
    for (std::uint64_t& word : value.hit_targets.words) {
        if (!reader.u64(word)) return false;
    }
    return true;
}

template <typename Sink>
bool write_monster(Sink& writer,
    const checkpoint::MonsterCombatCheckpoint& value) noexcept {
    if (!writer.u16(value.ordinal)
            || !writer.u8(static_cast<std::uint8_t>(value.id))
            || !write_affixes(writer, value.affixes)
            || !writer.i32(value.affix_profile.max_hp)
            || !writer.i32(value.affix_profile.armor_rating)
            || !writer.i32(value.affix_profile.max_shield)
            || !writer.u16(value.affix_profile.shield_recharge_delay_ticks)
            || !writer.i32(value.affix_profile.damage_bp)
            || !writer.i32(value.affix_profile.attack_timing_bp)
            || !writer.i32(value.affix_profile.move_bp)
            || !writer.i32(value.affix_profile.cooldown_bp)
            || !writer.i32(value.affix_profile.horizontal_impulse_bp)
            || !writer.u8(static_cast<std::uint8_t>(value.kind))
            || !write_vec(writer, value.spawn) || !write_vec(writer, value.position)
            || !write_vec(writer, value.velocity)
            || !writer.u8(static_cast<std::uint8_t>(value.facing))
            || !writer.u8(static_cast<std::uint8_t>(value.reaction))
            || !writer.u8(static_cast<std::uint8_t>(value.armor))
            || !writer.u16(value.reaction_ticks)
            || !writer.u8(static_cast<std::uint8_t>(value.ai_phase))
            || !writer.u16(value.ai_ticks) || !writer.u32(value.attack_serial)
            || !writer.boolean(value.contact_attack_resolved)
            || !writer.i32(value.hp) || !writer.i32(value.max_hp)
            || !writer.i32(value.break_value) || !writer.i32(value.max_break)
            || !writer.i32(value.shield) || !writer.i32(value.max_shield)
            || !writer.u16(value.shield_ticks)
            || !writer.u16(value.max_shield_ticks)
            || !writer.u16(value.shield_recharge_ticks)
            || !writer.u16(value.break_window_ticks)
            || !writer.u16(value.owner_transient_counter)
            || !writer.u16(value.burning_ground_ticks)
            || !writer.u16(value.blink_assault_ticks)
            || !writer.boolean(value.blink_empowered)
            || !write_vec(writer, value.attack_target_position)
            || !write_vec(writer, value.attack_vector)
            || !writer.u8(static_cast<std::uint8_t>(value.affix_warning))
            || !writer.u16(value.affix_warning_ticks)
            || !write_effects(writer, value.effects)
            || !writer.boolean(value.effects_touched)) return false;
    return true;
}

bool read_monster(Reader& reader,
    checkpoint::MonsterCombatCheckpoint& value) noexcept {
    std::uint8_t id{};
    std::uint8_t kind{};
    std::uint8_t facing{};
    std::uint8_t reaction{};
    std::uint8_t armor{};
    std::uint8_t phase{};
    std::uint8_t warning{};
    if (!reader.u16(value.ordinal) || !reader.u8(id)
            || !read_affixes(reader, value.affixes)
            || !reader.i32(value.affix_profile.max_hp)
            || !reader.i32(value.affix_profile.armor_rating)
            || !reader.i32(value.affix_profile.max_shield)
            || !reader.u16(value.affix_profile.shield_recharge_delay_ticks)
            || !reader.i32(value.affix_profile.damage_bp)
            || !reader.i32(value.affix_profile.attack_timing_bp)
            || !reader.i32(value.affix_profile.move_bp)
            || !reader.i32(value.affix_profile.cooldown_bp)
            || !reader.i32(value.affix_profile.horizontal_impulse_bp)
            || !reader.u8(kind) || !read_vec(reader, value.spawn)
            || !read_vec(reader, value.position)
            || !read_vec(reader, value.velocity) || !reader.u8(facing)
            || !reader.u8(reaction) || !reader.u8(armor)
            || !reader.u16(value.reaction_ticks) || !reader.u8(phase)
            || !reader.u16(value.ai_ticks) || !reader.u32(value.attack_serial)
            || !reader.boolean(value.contact_attack_resolved)
            || !reader.i32(value.hp) || !reader.i32(value.max_hp)
            || !reader.i32(value.break_value) || !reader.i32(value.max_break)
            || !reader.i32(value.shield) || !reader.i32(value.max_shield)
            || !reader.u16(value.shield_ticks)
            || !reader.u16(value.max_shield_ticks)
            || !reader.u16(value.shield_recharge_ticks)
            || !reader.u16(value.break_window_ticks)
            || !reader.u16(value.owner_transient_counter)
            || !reader.u16(value.burning_ground_ticks)
            || !reader.u16(value.blink_assault_ticks)
            || !reader.boolean(value.blink_empowered)
            || !read_vec(reader, value.attack_target_position)
            || !read_vec(reader, value.attack_vector)
            || !reader.u8(warning) || !reader.u16(value.affix_warning_ticks)
            || !read_effects(reader, value.effects)
            || !reader.boolean(value.effects_touched)) return false;
    value.id = static_cast<checkpoint::MonsterId>(id);
    value.kind = static_cast<checkpoint::DummyKind>(kind);
    value.facing = static_cast<checkpoint::Facing>(
        static_cast<std::int8_t>(facing));
    value.reaction = static_cast<checkpoint::ReactionState>(reaction);
    value.armor = static_cast<checkpoint::ArmorState>(armor);
    value.ai_phase = static_cast<checkpoint::MonsterAiPhase>(phase);
    value.affix_warning =
        static_cast<checkpoint::MonsterAffixWarning>(warning);
    return true;
}

template <typename Sink>
bool write_abyss_runtime(Sink& writer,
    const checkpoint::AbyssEnvironmentRuntime& value) noexcept {
    return writer.u8(static_cast<std::uint8_t>(value.rule))
        && writer.u16(value.cycle_tick) && writer.u16(value.stage_tick)
        && write_vec(writer, value.locked_center)
        && writer.u8(value.expansion_stage) && writer.boolean(value.warning)
        && writer.boolean(value.active);
}

bool read_abyss_runtime(Reader& reader,
    checkpoint::AbyssEnvironmentRuntime& value) noexcept {
    std::uint8_t rule{};
    if (!reader.u8(rule) || !reader.u16(value.cycle_tick)
            || !reader.u16(value.stage_tick)
            || !read_vec(reader, value.locked_center)
            || !reader.u8(value.expansion_stage)
            || !reader.boolean(value.warning)
            || !reader.boolean(value.active)) return false;
    value.rule = static_cast<abyss::AbyssRuleId>(rule);
    return true;
}

template <typename Sink>
bool write_defense(Sink& writer,
    const checkpoint::PlayerDefenseSnapshot& value) noexcept {
    if (!writer.i32(value.hp) || !writer.i32(value.max_hp)
            || !writer.i32(value.barrier) || !writer.i32(value.max_barrier)
            || !writer.i64(value.armor) || !writer.i64(value.evasion)
            || !writer.i32(value.armor_reduction_bp)
            || !writer.i32(value.evasion_rate_bp)) return false;
    for (const std::int32_t amount : value.damage_reduction) {
        if (!writer.i32(amount)) return false;
    }
    for (const std::int32_t amount : value.damage_reduction_cap) {
        if (!writer.i32(amount)) return false;
    }
    return true;
}

bool read_defense(Reader& reader,
    checkpoint::PlayerDefenseSnapshot& value) noexcept {
    if (!reader.i32(value.hp) || !reader.i32(value.max_hp)
            || !reader.i32(value.barrier) || !reader.i32(value.max_barrier)
            || !reader.i64(value.armor) || !reader.i64(value.evasion)
            || !reader.i32(value.armor_reduction_bp)
            || !reader.i32(value.evasion_rate_bp)) return false;
    for (std::int32_t& amount : value.damage_reduction) {
        if (!reader.i32(amount)) return false;
    }
    for (std::int32_t& amount : value.damage_reduction_cap) {
        if (!reader.i32(amount)) return false;
    }
    return true;
}

template <typename Sink>
bool write_death(Sink& writer,
    const checkpoint::CombatDeathSnapshot& value) noexcept {
    if (!writer.u64(value.tick) || !write_source(writer, value.source)
            || !writer.u8(static_cast<std::uint8_t>(value.primary_type))
            || !writer.u64(value.raw_damage)
            || !writer.u64(value.barrier_loss)
            || !writer.u64(value.health_loss)
            || !writer.u64(value.final_damage)) return false;
    for (const std::uint64_t amount : value.recent_damage) {
        if (!writer.u64(amount)) return false;
    }
    return write_defense(writer, value.defense);
}

bool read_death(Reader& reader,
    checkpoint::CombatDeathSnapshot& value) noexcept {
    std::uint8_t type{};
    if (!reader.u64(value.tick) || !read_source(reader, value.source)
            || !reader.u8(type) || !reader.u64(value.raw_damage)
            || !reader.u64(value.barrier_loss)
            || !reader.u64(value.health_loss)
            || !reader.u64(value.final_damage)) return false;
    value.primary_type = static_cast<checkpoint::DamageType>(type);
    for (std::uint64_t& amount : value.recent_damage) {
        if (!reader.u64(amount)) return false;
    }
    return read_defense(reader, value.defense);
}

template <typename Sink>
bool write_room_combat(Sink& writer,
    const checkpoint::RoomCombatCheckpoint& value) noexcept {
    if (!writer.u64(value.tick)) return false;
    for (const std::uint64_t word : value.evasion_rng_state) {
        if (!writer.u64(word)) return false;
    }
    if (!write_player(writer, value.player)
            || !write_abyss_runtime(writer, value.abyss_environment)
            || !write_attack(writer, value.attack)
            || !writer.u16(value.monster_count)) return false;
    for (std::uint16_t index = 0U; index < value.monster_count; ++index) {
        if (!write_monster(writer, value.monsters[index])) return false;
    }
    if (!writer.u16(value.obstacle_count)) return false;
    for (std::uint16_t index = 0U; index < value.obstacle_count; ++index) {
        const auto& obstacle = value.obstacles[index];
        if (!writer.u16(obstacle.ordinal) || !writer.u16(obstacle.hp)
                || !writer.u16(obstacle.max_hp)
                || !writer.u64(obstacle.broken_tick)
                || !writer.boolean(obstacle.intact)
                || !write_effects(writer, obstacle.effects)) return false;
    }
    if (!writer.u8(value.fire_crate_count)) return false;
    for (std::uint8_t index = 0U; index < value.fire_crate_count; ++index) {
        const auto& crate = value.fire_crates[index];
        if (!write_vec(writer, crate.position) || !writer.u64(crate.broken_tick)
                || !writer.boolean(crate.intact)) return false;
    }
    if (!writer.boolean(value.player_damage_history.initialized)
            || !writer.u64(value.player_damage_history.active_tick)) {
        return false;
    }
    for (const auto& bucket : value.player_damage_history.buckets) {
        for (const std::uint64_t amount : bucket) {
            if (!writer.u64(amount)) return false;
        }
    }
    return writer.boolean(value.has_death_snapshot)
        && (!value.has_death_snapshot || write_death(writer, value.death_snapshot));
}

bool read_room_combat(Reader& reader,
    checkpoint::RoomCombatCheckpoint& value) noexcept {
    if (!reader.u64(value.tick)) return false;
    for (std::uint64_t& word : value.evasion_rng_state) {
        if (!reader.u64(word)) return false;
    }
    if (!read_player(reader, value.player)
            || !read_abyss_runtime(reader, value.abyss_environment)
            || !read_attack(reader, value.attack)
            || !reader.u16(value.monster_count)
            || value.monster_count > value.monsters.size()) return false;
    for (std::uint16_t index = 0U; index < value.monster_count; ++index) {
        if (!read_monster(reader, value.monsters[index])) return false;
    }
    if (!reader.u16(value.obstacle_count)
            || value.obstacle_count > value.obstacles.size()) return false;
    for (std::uint16_t index = 0U; index < value.obstacle_count; ++index) {
        auto& obstacle = value.obstacles[index];
        if (!reader.u16(obstacle.ordinal) || !reader.u16(obstacle.hp)
                || !reader.u16(obstacle.max_hp)
                || !reader.u64(obstacle.broken_tick)
                || !reader.boolean(obstacle.intact)
                || !read_effects(reader, obstacle.effects)) return false;
    }
    if (!reader.u8(value.fire_crate_count)
            || value.fire_crate_count > value.fire_crates.size()) return false;
    for (std::uint8_t index = 0U; index < value.fire_crate_count; ++index) {
        auto& crate = value.fire_crates[index];
        if (!read_vec(reader, crate.position) || !reader.u64(crate.broken_tick)
                || !reader.boolean(crate.intact)) return false;
    }
    if (!reader.boolean(value.player_damage_history.initialized)
            || !reader.u64(value.player_damage_history.active_tick)) {
        return false;
    }
    for (auto& bucket : value.player_damage_history.buckets) {
        for (std::uint64_t& amount : bucket) {
            if (!reader.u64(amount)) return false;
        }
    }
    if (!reader.boolean(value.has_death_snapshot)) return false;
    value.death_snapshot = {};
    return !value.has_death_snapshot || read_death(reader, value.death_snapshot);
}

template <typename Sink>
bool write_room_progress(Sink& writer,
    const checkpoint::RoomProgressCheckpoint& value) noexcept {
    if (!writer.u8(static_cast<std::uint8_t>(value.lifecycle))
            || !writer.u64(value.room_index) || !writer.u64(value.room_seed)
            || !writer.u32(value.monster_generator_version)
            || !writer.u64(value.monster_blueprint_hash)
            || !writer.u32(value.environment_generator_version)
            || !writer.u64(value.environment_blueprint_hash)
            || !writer.u32(value.generated_monsters)
            || !writer.u32(value.defeated_monsters)
            || !writer.u32(value.required_kills)
            || !writer.boolean(value.exits_unlocked)
            || !writer.boolean(value.full_clear)
            || !writer.boolean(value.reward_committed)) return false;
    for (const std::uint64_t word : value.defeat_bits) {
        if (!writer.u64(word)) return false;
    }
    for (const std::uint64_t word : value.equipment_claim_bits) {
        if (!writer.u64(word)) return false;
    }
    for (const std::uint64_t word : value.secondary_claim_bits) {
        if (!writer.u64(word)) return false;
    }
    if (!write_room_combat(writer, value.combat)
            || !writer.u16(value.equipment_ground_count)) return false;
    for (std::uint16_t index = 0U; index < value.equipment_ground_count;
            ++index) {
        const auto& ground = value.equipment_ground[index];
        if (!writer.u16(ground.ordinal) || !writer.u8(ground.source)
                || !writer.u8(ground.reward_ordinal)
                || !write_vec(writer, ground.position)
                || !write_item(writer, ground.item)) return false;
    }
    if (!writer.u16(value.secondary_ground_count)) return false;
    for (std::uint16_t index = 0U; index < value.secondary_ground_count;
            ++index) {
        const auto& ground = value.secondary_ground[index];
        if (!writer.u8(static_cast<std::uint8_t>(ground.tag))
                || !writer.u16(ground.ordinal) || !writer.u8(ground.source)
                || !write_vec(writer, ground.position)
                || !writer.u8(static_cast<std::uint8_t>(ground.material))) {
            return false;
        }
    }
    return true;
}

bool read_room_progress(Reader& reader,
    checkpoint::RoomProgressCheckpoint& value) noexcept {
    std::uint8_t lifecycle{};
    if (!reader.u8(lifecycle) || !reader.u64(value.room_index)
            || !reader.u64(value.room_seed)
            || !reader.u32(value.monster_generator_version)
            || !reader.u64(value.monster_blueprint_hash)
            || !reader.u32(value.environment_generator_version)
            || !reader.u64(value.environment_blueprint_hash)
            || !reader.u32(value.generated_monsters)
            || !reader.u32(value.defeated_monsters)
            || !reader.u32(value.required_kills)
            || !reader.boolean(value.exits_unlocked)
            || !reader.boolean(value.full_clear)
            || !reader.boolean(value.reward_committed)) return false;
    value.lifecycle = static_cast<checkpoint::RoomProgressLifecycle>(lifecycle);
    for (std::uint64_t& word : value.defeat_bits) {
        if (!reader.u64(word)) return false;
    }
    for (std::uint64_t& word : value.equipment_claim_bits) {
        if (!reader.u64(word)) return false;
    }
    for (std::uint64_t& word : value.secondary_claim_bits) {
        if (!reader.u64(word)) return false;
    }
    if (!read_room_combat(reader, value.combat)
            || !reader.u16(value.equipment_ground_count)
            || value.equipment_ground_count > value.equipment_ground.size()) {
        return false;
    }
    for (std::uint16_t index = 0U; index < value.equipment_ground_count;
            ++index) {
        auto& ground = value.equipment_ground[index];
        if (!reader.u16(ground.ordinal) || !reader.u8(ground.source)
                || !reader.u8(ground.reward_ordinal)
                || !read_vec(reader, ground.position)
                || !read_item(reader, ground.item)) return false;
    }
    if (!reader.u16(value.secondary_ground_count)
            || value.secondary_ground_count > value.secondary_ground.size()) {
        return false;
    }
    for (std::uint16_t index = 0U; index < value.secondary_ground_count;
            ++index) {
        auto& ground = value.secondary_ground[index];
        std::uint8_t tag{};
        std::uint8_t material{};
        if (!reader.u8(tag) || !reader.u16(ground.ordinal)
                || !reader.u8(ground.source)
                || !read_vec(reader, ground.position)
                || !reader.u8(material)) return false;
        ground.tag = static_cast<checkpoint::SecondaryGroundTag>(tag);
        ground.material = static_cast<items::MaterialId>(material);
    }
    return true;
}

[[nodiscard]] std::uint32_t crc(
    const std::uint8_t* bytes, std::size_t payload_size) noexcept {
    std::uint32_t value = crc32_update(0U, bytes + 8U, 20U);
    return crc32_update(value, bytes + kHeaderSize, payload_size);
}

template <std::size_t Size>
void set_bit(std::array<std::uint64_t, Size>& bits,
    const std::uint16_t ordinal) noexcept {
    bits[ordinal / 64U] |= std::uint64_t{1U} << (ordinal % 64U);
}

template <std::size_t Size>
[[nodiscard]] bool bit_is_set(
    const std::array<std::uint64_t, Size>& bits,
    const std::uint16_t ordinal) noexcept {
    return ordinal < bits.size() * 64U
        && (bits[ordinal / 64U]
            & (std::uint64_t{1U} << (ordinal % 64U))) != 0U;
}

[[nodiscard]] bool same_checkpoint_position(
    const checkpoint::CheckpointVec3& left,
    const checkpoint::CheckpointVec3& right) noexcept {
    std::array<std::uint32_t, 3U> left_bits{};
    std::array<std::uint32_t, 3U> right_bits{};
    static_assert(sizeof(left_bits) == sizeof(left));
    std::memcpy(left_bits.data(), &left, sizeof(left));
    std::memcpy(right_bits.data(), &right, sizeof(right));
    return left_bits == right_bits;
}

enum class SecondaryOrdinalSemantics : std::uint8_t {
    unknown,
    task5,
    canonical,
};

[[nodiscard]] bool add_secondary_ordinal_evidence(
    SecondaryOrdinalSemantics& semantics,
    const SecondaryOrdinalSemantics evidence) noexcept {
    if (evidence == SecondaryOrdinalSemantics::unknown) return true;
    if (semantics == SecondaryOrdinalSemantics::unknown) {
        semantics = evidence;
        return true;
    }
    return semantics == evidence;
}

[[nodiscard]] bool spawn_has_exact_drop_position(
    const checkpoint::RoomProgressCheckpoint& room,
    const std::uint16_t spawn,
    const checkpoint::CheckpointVec3& position,
    const SecondaryOrdinalSemantics semantics) noexcept {
    if (spawn >= room.generated_monsters) return false;
    for (std::uint16_t index = 0U;
            index < room.combat.monster_count; ++index) {
        const auto& monster = room.combat.monsters[index];
        if (monster.ordinal == spawn
                && same_checkpoint_position(monster.position, position)) {
            return true;
        }
    }
    for (std::uint16_t index = 0U;
            index < room.equipment_ground_count; ++index) {
        const auto& equipment = room.equipment_ground[index];
        if (equipment.source == 0U && equipment.ordinal == spawn
                && same_checkpoint_position(equipment.position, position)) {
            return true;
        }
    }
    const std::uint16_t potion_ordinal =
        checkpoint::health_potion_claim_ordinal(spawn);
    const std::uint16_t coupon_ordinal =
        semantics == SecondaryOrdinalSemantics::task5
        ? static_cast<std::uint16_t>(potion_ordinal * 2U)
        : potion_ordinal;
    for (std::uint16_t index = 0U;
            index < room.secondary_ground_count; ++index) {
        const auto& secondary = room.secondary_ground[index];
        if (same_checkpoint_position(secondary.position, position)
                && ((secondary.tag
                        == checkpoint::SecondaryGroundTag::health_potion
                        && secondary.ordinal == potion_ordinal)
                    || (secondary.tag
                        == checkpoint::SecondaryGroundTag::material
                        && secondary.source == 1U
                        && secondary.ordinal == coupon_ordinal))) {
            return true;
        }
    }
    return false;
}

[[nodiscard]] SecondaryOrdinalSemantics
ambiguous_common_ordinal_position_evidence(
    const checkpoint::RoomProgressCheckpoint& room,
    const checkpoint::SecondaryGroundCheckpoint& ground) noexcept {
    const std::uint16_t task5_spawn = static_cast<std::uint16_t>(
        ground.ordinal / 4U);
    const std::uint16_t canonical_spawn = static_cast<std::uint16_t>(
        ground.ordinal / 2U);
    const bool task5_position = spawn_has_exact_drop_position(
        room, task5_spawn, ground.position,
        SecondaryOrdinalSemantics::task5);
    const bool canonical_position = spawn_has_exact_drop_position(
        room, canonical_spawn, ground.position,
        SecondaryOrdinalSemantics::canonical);
    if (task5_position != canonical_position) {
        return task5_position ? SecondaryOrdinalSemantics::task5
                              : SecondaryOrdinalSemantics::canonical;
    }
    const bool task5_defeated = bit_is_set(room.defeat_bits, task5_spawn);
    const bool canonical_defeated =
        bit_is_set(room.defeat_bits, canonical_spawn);
    if (task5_defeated != canonical_defeated) {
        return task5_defeated ? SecondaryOrdinalSemantics::task5
                              : SecondaryOrdinalSemantics::canonical;
    }
    return SecondaryOrdinalSemantics::unknown;
}

[[nodiscard]] bool migrate_task5_secondary_ordinals(
    checkpoint::SaveCheckpointSlot& slot) noexcept {
    auto& room = slot.room_progress;
    std::array<std::uint64_t, limits::kRoomSecondaryClaimWords>
        expected_task5_claims{};
    std::array<std::uint64_t, limits::kRoomSecondaryClaimWords>
        canonical_claims{};
    const auto& durable_claims =
        slot.state.item_ownership.material_claimed_drop_bits;
    for (std::uint16_t ordinal = 0U;
            ordinal < durable_claims.size() * 64U; ++ordinal) {
        if ((durable_claims[ordinal / 64U]
                & (std::uint64_t{1U} << (ordinal % 64U))) == 0U) {
            continue;
        }
        const std::uint16_t task5_ordinal =
            checkpoint::checkpoint_material_ordinal(ordinal);
        set_bit(expected_task5_claims, task5_ordinal);
        if ((ordinal & 1U) != 0U
                && ordinal < checkpoint::kHealthPotionGroundCapacity * 2U) {
            set_bit(expected_task5_claims, ordinal);
        }
        set_bit(canonical_claims,
            ordinal >= checkpoint::kAbyssMaterialOrdinalBegin
                ? task5_ordinal : ordinal);
    }
    const bool claims_match_task5 =
        room.secondary_claim_bits == expected_task5_claims;
    const bool claims_match_canonical =
        room.secondary_claim_bits == canonical_claims;
    if (!claims_match_task5 && !claims_match_canonical) return false;

    SecondaryOrdinalSemantics semantics = SecondaryOrdinalSemantics::unknown;
    if (claims_match_task5 != claims_match_canonical
            && !add_secondary_ordinal_evidence(semantics,
                claims_match_task5 ? SecondaryOrdinalSemantics::task5
                                   : SecondaryOrdinalSemantics::canonical)) {
        return false;
    }

    bool has_semantic_sensitive_ground = false;
    for (std::uint16_t index = 0U;
            index < room.secondary_ground_count; ++index) {
        const auto& ground = room.secondary_ground[index];
        if (ground.tag != checkpoint::SecondaryGroundTag::material) continue;
        if (ground.source == 2U) continue;
        if (ground.source > 1U) return false;

        const std::uint32_t ordinary_end = room.generated_monsters * 2U;
        const bool canonical_possible = ground.ordinal < ordinary_end
            && ((ground.source == 0U && (ground.ordinal & 1U) == 0U)
                || (ground.source == 1U && (ground.ordinal & 1U) != 0U));
        const std::uint16_t task5_canonical =
            static_cast<std::uint16_t>(ground.ordinal / 2U);
        const bool task5_possible =
            ground.ordinal < checkpoint::kOrdinarySecondaryOrdinalEnd
            && task5_canonical < ordinary_end
            && ((ground.source == 0U && (task5_canonical & 1U) == 0U)
                || (ground.source == 1U
                    && (task5_canonical & 1U) != 0U));
        if (!canonical_possible && !task5_possible) return false;
        if (canonical_possible != task5_possible) {
            if (!add_secondary_ordinal_evidence(semantics,
                    canonical_possible
                        ? SecondaryOrdinalSemantics::canonical
                        : SecondaryOrdinalSemantics::task5)) {
                return false;
            }
            continue;
        }
        if (ground.ordinal != 0U) {
            has_semantic_sensitive_ground = true;
            if (!add_secondary_ordinal_evidence(semantics,
                    ambiguous_common_ordinal_position_evidence(
                        room, ground))) {
                return false;
            }
        }
    }

    if (semantics == SecondaryOrdinalSemantics::unknown) {
        if (has_semantic_sensitive_ground) return false;
        semantics = SecondaryOrdinalSemantics::canonical;
    }
    if (semantics == SecondaryOrdinalSemantics::canonical) return true;

    for (std::uint16_t index = 0U;
            index < room.secondary_ground_count; ++index) {
        auto& ground = room.secondary_ground[index];
        if (ground.tag == checkpoint::SecondaryGroundTag::material
                && ground.source <= 1U) {
            ground.ordinal = static_cast<std::uint16_t>(ground.ordinal / 2U);
        }
    }
    std::sort(room.secondary_ground.begin(),
        room.secondary_ground.begin() + room.secondary_ground_count,
        [](const checkpoint::SecondaryGroundCheckpoint& left,
            const checkpoint::SecondaryGroundCheckpoint& right) noexcept {
            return left.ordinal < right.ordinal;
        });
    for (std::uint16_t index = 1U;
            index < room.secondary_ground_count; ++index) {
        if (room.secondary_ground[index - 1U].ordinal
                == room.secondary_ground[index].ordinal) {
            return false;
        }
    }
    room.secondary_claim_bits = canonical_claims;
    return true;
}

[[nodiscard]] bool valid_resolution_lifecycle_extension(
    const checkpoint::LastAbyssResolution& value) noexcept {
    if (value.lifecycle == abyss::AbyssLifecycle::none) return true;
    return value.lifecycle == abyss::AbyssLifecycle::failed
        && value.valid && value.generated == 0U && value.claimed == 0U
        && value.abandoned == value.total;
}

[[nodiscard]] bool legacy_v9_abyss_death_requires_failed_resolution(
    const checkpoint::DungeonRunState& state) noexcept {
    const auto& death = state.death;
    const auto& resolution = state.last_abyss_resolution;
    if (death.lifecycle != checkpoint::DeathLifecycle::pending_continue
            || !death.death_was_abyss
            || state.commit_generation < 2U
            || state.death_sequence == 0U
            || state.current_room.is_abyss
            || state.biases != std::array<std::uint32_t, 4>{}
            || state.last_transition
                != checkpoint::TransitionKind::death_retreat
            || state.last_direction != checkpoint::ExitDirection::none
            || death.death_depth != state.current_room.depth
            || death.death_floor_room_index
                != state.current_room.floor_room_index
            || death.death_ecology != state.current_room.ecology
            || state.abyss.lifecycle != abyss::AbyssLifecycle::failed
            || !resolution.valid
            || resolution.lifecycle != abyss::AbyssLifecycle::none
            || resolution.room_seed != state.current_room.seed
            || resolution.rule != state.abyss.rule) {
        return false;
    }
    const std::uint8_t total = abyss::reward_profile_for(
        state.abyss.danger, 1U).item_count;
    return resolution.total == total
        && resolution.generated == 0U
        && resolution.claimed == 0U
        && resolution.abandoned == total;
}

}  // namespace

CodecError inspect_checkpoint_v9_envelope(const std::uint8_t* const bytes,
    const std::size_t size, std::uint64_t& revision) noexcept {
    revision = 0U;
    if (bytes == nullptr || size < kHeaderSize) return CodecError::wrong_size;
    if (size > kMaximumEncodedCheckpointBytes) {
        return CodecError::bad_payload_length;
    }
    if (!std::equal(kMagic.begin(), kMagic.end(), bytes)) {
        return CodecError::bad_magic;
    }
    Reader header{bytes, size, 8U};
    std::uint32_t format{};
    std::uint32_t rules{};
    std::uint64_t parsed_revision{};
    std::uint32_t payload_size{};
    std::uint32_t expected_crc{};
    if (!header.u32(format) || !header.u32(rules)
            || !header.u64(parsed_revision) || !header.u32(payload_size)
            || !header.u32(expected_crc)) {
        return CodecError::wrong_size;
    }
    if (format != kCheckpointFormatVersionV9) {
        return CodecError::unsupported_format;
    }
    if (rules != kCheckpointRulesVersion) return CodecError::unsupported_rules;
    if (parsed_revision == 0U
            || static_cast<std::size_t>(payload_size) != size - kHeaderSize) {
        return CodecError::bad_payload_length;
    }
    if (expected_crc != crc(bytes, payload_size)) return CodecError::bad_crc;
    revision = parsed_revision;
    return CodecError::none;
}

CodecError encode_checkpoint_v9_into(
    const checkpoint::SaveCheckpointSlot& source,
    std::uint8_t* bytes,
    std::size_t capacity,
    std::size_t& written) noexcept {
    written = 0U;
    if (bytes == nullptr || capacity < kHeaderSize
            || capacity > kMaximumEncodedCheckpointBytes
            || source.persistence_revision == 0U
            || !checkpoint::valid_room_progress_checkpoint_structural(
                source.room_progress, source.state)) {
        return CodecError::invalid_state;
    }
    std::fill(bytes, bytes + capacity, std::uint8_t{0U});
    std::copy(kMagic.begin(), kMagic.end(), bytes);
    Writer header{bytes, capacity, 8U};
    if (!header.u32(kCheckpointFormatVersionV9)
            || !header.u32(kCheckpointRulesVersion)
            || !header.u64(source.persistence_revision)
            || !header.u32(0U) || !header.u32(0U)) {
        return CodecError::wrong_size;
    }
    Writer payload{bytes, capacity, kHeaderSize};
    if (!payload.u32(0U)) return CodecError::wrong_size;
    std::size_t durable_size{};
    const CodecError durable_error = encode_checkpoint_v9_durable_into(
        source.state,
        payload.current(), capacity - payload.offset(), durable_size);
    if (durable_error != CodecError::none) return durable_error;
    if (durable_size > (std::numeric_limits<std::uint32_t>::max)()
            || !payload.skip(durable_size)
            || !write_room_progress(payload, source.room_progress)
            || !payload.u8(static_cast<std::uint8_t>(
                source.state.last_abyss_resolution.lifecycle))
            || !payload.u8(kV9CanonicalSecondaryOrdinalMarker)) {
        return CodecError::wrong_size;
    }
    Writer durable_length{bytes, capacity, kHeaderSize};
    if (!durable_length.u32(static_cast<std::uint32_t>(durable_size))) {
        return CodecError::wrong_size;
    }
    const std::size_t payload_size = payload.offset() - kHeaderSize;
    if (payload_size > (std::numeric_limits<std::uint32_t>::max)()
            || payload.offset() > kMaximumEncodedCheckpointBytes) {
        return CodecError::bad_payload_length;
    }
    Writer lengths{bytes, capacity, 24U};
    if (!lengths.u32(static_cast<std::uint32_t>(payload_size))
            || !lengths.u32(0U)) return CodecError::wrong_size;
    Writer checksum{bytes, capacity, 28U};
    if (!checksum.u32(crc(bytes, payload_size))) return CodecError::wrong_size;
    written = payload.offset();
    return CodecError::none;
}

CodecError decode_checkpoint_v9_into_scratch(
    const std::uint8_t* bytes,
    std::size_t size,
    checkpoint::SaveCheckpointSlot& destination,
    bool& migrated) noexcept {
    migrated = false;
    if (bytes == nullptr || size < kHeaderSize) return CodecError::wrong_size;
    if (!std::equal(kMagic.begin(), kMagic.end(), bytes)) {
        DecodeResult legacy = decode_checkpoint(bytes, size);
        if (legacy.error != CodecError::none) return legacy.error;
        checkpoint::clear_save_checkpoint_slot(destination);
        try {
            destination.state = legacy.state;
        } catch (...) {
            return CodecError::allocation_failure;
        }
        destination.state.item_ownership.claimed_drop_bits = {};
        destination.state.item_ownership.material_claimed_drop_bits = {};
        destination.persistence_revision = destination.state.commit_generation;
        migrated = true;
        return CodecError::none;
    }
    if (size > kMaximumEncodedCheckpointBytes) {
        return CodecError::bad_payload_length;
    }
    Reader header{bytes, size, 8U};
    std::uint32_t format{};
    std::uint32_t rules{};
    std::uint64_t revision{};
    std::uint32_t payload_size{};
    std::uint32_t expected_crc{};
    if (!header.u32(format) || !header.u32(rules) || !header.u64(revision)
            || !header.u32(payload_size) || !header.u32(expected_crc)) {
        return CodecError::wrong_size;
    }
    if (format != kCheckpointFormatVersionV9) {
        return CodecError::unsupported_format;
    }
    if (rules != kCheckpointRulesVersion) return CodecError::unsupported_rules;
    if (revision == 0U || payload_size > kMaximumEncodedCheckpointBytes
            || static_cast<std::size_t>(payload_size) != size - kHeaderSize) {
        return CodecError::bad_payload_length;
    }
    if (expected_crc != crc(bytes, payload_size)) return CodecError::bad_crc;

    Reader payload{bytes, size, kHeaderSize};
    std::uint32_t durable_size{};
    if (!payload.u32(durable_size) || durable_size > payload_size
            || durable_size > size - payload.offset()) {
        return CodecError::bad_payload_length;
    }
    const DecodeResult durable = decode_checkpoint(
        payload.current(), durable_size);
    if (durable.error != CodecError::none) return durable.error;
    checkpoint::clear_save_checkpoint_slot(destination);
    try {
        destination.state = durable.state;
    } catch (...) {
        return CodecError::allocation_failure;
    }
    destination.persistence_revision = revision;
    if (!payload.skip(durable_size)
            || !read_room_progress(payload, destination.room_progress)) {
        return CodecError::bad_payload_length;
    }
    const std::size_t extension_size = size - payload.offset();
    if (extension_size > 2U) return CodecError::bad_payload_length;
    bool canonical_secondary_ordinals = false;
    if (extension_size == 0U
            && legacy_v9_abyss_death_requires_failed_resolution(
                destination.state)) {
        destination.state.last_abyss_resolution.lifecycle =
            abyss::AbyssLifecycle::failed;
    } else if (extension_size >= 1U) {
        std::uint8_t resolution_lifecycle{};
        if (!payload.u8(resolution_lifecycle)) {
            return CodecError::bad_payload_length;
        }
        if (resolution_lifecycle
                    != static_cast<std::uint8_t>(
                        abyss::AbyssLifecycle::none)
                && resolution_lifecycle
                    != static_cast<std::uint8_t>(
                        abyss::AbyssLifecycle::failed)) {
            return CodecError::invalid_enum;
        }
        destination.state.last_abyss_resolution.lifecycle =
            static_cast<abyss::AbyssLifecycle>(resolution_lifecycle);
        if (extension_size == 2U) {
            std::uint8_t secondary_marker{};
            if (!payload.u8(secondary_marker)) {
                return CodecError::bad_payload_length;
            }
            if (secondary_marker != kV9CanonicalSecondaryOrdinalMarker) {
                return CodecError::invalid_enum;
            }
            canonical_secondary_ordinals = true;
        }
    }
    if (!valid_resolution_lifecycle_extension(
            destination.state.last_abyss_resolution)) {
        return CodecError::invalid_state;
    }
    if (!canonical_secondary_ordinals) {
        if (!migrate_task5_secondary_ordinals(destination)) {
            return CodecError::invalid_state;
        }
        migrated = true;
    }
    if (!checkpoint::valid_room_progress_checkpoint_structural(
            destination.room_progress, destination.state)) {
        return CodecError::invalid_state;
    }
    return CodecError::none;
}

namespace {

void publish_room_combat_checkpoint(
    checkpoint::RoomCombatCheckpoint& destination,
    const checkpoint::RoomCombatCheckpoint& source) noexcept {
    destination.tick = source.tick;
    destination.evasion_rng_state = source.evasion_rng_state;
    destination.player = source.player;
    destination.abyss_environment = source.abyss_environment;
    destination.attack = source.attack;
    destination.monsters = source.monsters;
    destination.monster_count = source.monster_count;
    destination.obstacles = source.obstacles;
    destination.obstacle_count = source.obstacle_count;
    destination.fire_crates = source.fire_crates;
    destination.fire_crate_count = source.fire_crate_count;
    destination.player_damage_history = source.player_damage_history;
    destination.has_death_snapshot = source.has_death_snapshot;
    destination.death_snapshot = source.death_snapshot;
}

void publish_room_progress_checkpoint(
    checkpoint::RoomProgressCheckpoint& destination,
    const checkpoint::RoomProgressCheckpoint& source) noexcept {
    destination.lifecycle = source.lifecycle;
    destination.room_index = source.room_index;
    destination.room_seed = source.room_seed;
    destination.monster_generator_version = source.monster_generator_version;
    destination.monster_blueprint_hash = source.monster_blueprint_hash;
    destination.environment_generator_version =
        source.environment_generator_version;
    destination.environment_blueprint_hash = source.environment_blueprint_hash;
    destination.generated_monsters = source.generated_monsters;
    destination.defeated_monsters = source.defeated_monsters;
    destination.required_kills = source.required_kills;
    destination.exits_unlocked = source.exits_unlocked;
    destination.full_clear = source.full_clear;
    destination.reward_committed = source.reward_committed;
    destination.defeat_bits = source.defeat_bits;
    destination.equipment_claim_bits = source.equipment_claim_bits;
    destination.secondary_claim_bits = source.secondary_claim_bits;
    publish_room_combat_checkpoint(destination.combat, source.combat);
    destination.equipment_ground = source.equipment_ground;
    destination.equipment_ground_count = source.equipment_ground_count;
    destination.secondary_ground = source.secondary_ground;
    destination.secondary_ground_count = source.secondary_ground_count;
}

}  // namespace

CodecError decode_checkpoint_v9_into(
    const std::uint8_t* bytes,
    const std::size_t size,
    checkpoint::SaveCheckpointSlot& destination,
    bool& migrated) noexcept {
    migrated = false;
    std::unique_ptr<checkpoint::SaveCheckpointSlot> scratch{
        new (std::nothrow) checkpoint::SaveCheckpointSlot{}};
    if (scratch == nullptr) return CodecError::allocation_failure;
    try {
        scratch->state.item_ownership.items.reserve(
            kMaximumCheckpointItemCount);
    } catch (...) {
        return CodecError::allocation_failure;
    }
    bool decoded_migrated{};
    const CodecError decoded = decode_checkpoint_v9_into_scratch(
        bytes, size, *scratch, decoded_migrated);
    if (decoded != CodecError::none) return decoded;
    try {
        // Reserve before publishing any scalar state. vector::reserve has the
        // strong guarantee here, and the subsequent aggregate assignment is
        // allocation-free for the validated fixed-size item records.
        destination.state.item_ownership.items.reserve(
            scratch->state.item_ownership.items.size());
        destination.state = scratch->state;
    } catch (...) {
        return CodecError::allocation_failure;
    }
    destination.persistence_revision = scratch->persistence_revision;
    publish_room_progress_checkpoint(
        destination.room_progress, scratch->room_progress);
    migrated = decoded_migrated;
    return CodecError::none;
}

CodecError verify_checkpoint_v9_readback(
    const std::uint8_t* const bytes,
    const std::size_t size,
    const checkpoint::SaveCheckpointSlot& expected,
    const std::uint8_t* const canonical_bytes,
    const std::size_t canonical_size) noexcept {
    if (bytes == nullptr || canonical_bytes == nullptr || size != canonical_size
            || size < kHeaderSize || size > kMaximumEncodedCheckpointBytes
            || !std::equal(bytes, bytes + size, canonical_bytes)
            || !std::equal(kMagic.begin(), kMagic.end(), bytes)
            || expected.persistence_revision == 0U
              || !checkpoint::valid_room_progress_checkpoint_structural(
                  expected.room_progress, expected.state)) {
        return CodecError::invalid_state;
    }
    Reader header{bytes, size, 8U};
    std::uint32_t format{};
    std::uint32_t rules{};
    std::uint64_t revision{};
    std::uint32_t payload_size{};
    std::uint32_t expected_crc{};
    if (!header.u32(format) || !header.u32(rules) || !header.u64(revision)
            || !header.u32(payload_size) || !header.u32(expected_crc)
            || format != kCheckpointFormatVersionV9
            || rules != kCheckpointRulesVersion
            || revision != expected.persistence_revision
            || static_cast<std::size_t>(payload_size) != size - kHeaderSize
            || expected_crc != crc(bytes, payload_size)) {
        return CodecError::invalid_state;
    }
    Reader payload{bytes, size, kHeaderSize};
    std::uint32_t durable_size{};
    if (!payload.u32(durable_size) || durable_size < kCheckpointHeaderSize
            || durable_size > size - payload.offset()) {
        return CodecError::bad_payload_length;
    }
    const std::uint8_t* const durable = payload.current();
    constexpr std::array<std::uint8_t, 8U> kV8Magic{{
        'A', 'R', 'P', 'G', 'S', 'V', '8', '\0'}};
    if (durable == nullptr
            || !std::equal(kV8Magic.begin(), kV8Magic.end(), durable)) {
        return CodecError::bad_magic;
    }
    Reader durable_header{durable, durable_size, 8U};
    std::uint32_t durable_format{};
    std::uint32_t durable_rules{};
    std::uint64_t generation{};
    std::uint32_t durable_payload_size{};
    std::uint32_t durable_crc{};
    if (!durable_header.u32(durable_format)
            || !durable_header.u32(durable_rules)
            || !durable_header.u64(generation)
            || !durable_header.u32(durable_payload_size)
            || !durable_header.u32(durable_crc)
            || durable_format != kCheckpointFormatVersion
            || durable_rules != kCheckpointRulesVersion
            || generation != expected.state.commit_generation
            || static_cast<std::size_t>(durable_payload_size)
                != durable_size - kCheckpointHeaderSize) {
        return CodecError::invalid_state;
    }
    const std::uint32_t inner_header_crc = crc32_update(0U, durable + 8U, 20U);
    if (durable_crc != crc32_update(inner_header_crc,
            durable + kCheckpointHeaderSize, durable_payload_size)) {
        return CodecError::bad_crc;
    }
    if (verify_checkpoint_v9_durable_readback_fields(
            durable, durable_size, expected.state) != CodecError::none) {
        return CodecError::invalid_state;
    }
    if (!payload.skip(durable_size) || payload.offset() >= size) {
        return CodecError::bad_payload_length;
    }
    ComparingReader room_fields{bytes, size, payload.offset()};
    if (!write_room_progress(room_fields, expected.room_progress)) {
        return CodecError::invalid_state;
    }
    const std::size_t extension_size = size - room_fields.offset();
    if (extension_size == 2U) {
        ComparingReader extension{bytes, size, room_fields.offset()};
        if (!extension.u8(static_cast<std::uint8_t>(
                expected.state.last_abyss_resolution.lifecycle))
                || !extension.u8(kV9CanonicalSecondaryOrdinalMarker)
                || extension.offset() != size) {
            return CodecError::invalid_state;
        }
    } else {
        return CodecError::bad_payload_length;
    }
    return CodecError::none;
}

}  // namespace arpg::persistence

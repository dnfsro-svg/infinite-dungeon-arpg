#include "combat_feedback.hpp"

#include <algorithm>
#include <cmath>

namespace arpg::platform {
namespace {

struct ShakeParameters final {
    float amplitude{};
    float duration{};
};

ShakeParameters shake_for(combat::FeedbackLevel level) noexcept {
    switch (level) {
    case combat::FeedbackLevel::light:
        return {2.0F, 0.10F};
    case combat::FeedbackLevel::medium:
        return {5.0F, 0.14F};
    case combat::FeedbackLevel::heavy:
        return {9.0F, 0.20F};
    }
    return {};
}

}  // namespace

void CombatFeedback::consume(const combat::CombatEvent& event) noexcept {
    if (event.kind == combat::CombatEventKind::reset) {
        clear();
        return;
    }

    if (event.kind == combat::CombatEventKind::swing) {
        VisualEffect trail{};
        trail.active = true;
        trail.kind = VisualEffectKind::weapon_trail;
        trail.position = event.position;
        trail.lifetime_seconds = 0.12F;
        static_cast<void>(try_spawn(trail));
        return;
    }

    if (event.kind == combat::CombatEventKind::landing) {
        VisualEffect dust{};
        dust.active = true;
        dust.kind = VisualEffectKind::dust;
        dust.position = event.position;
        dust.lifetime_seconds = 0.24F;
        static_cast<void>(try_spawn(dust));
        return;
    }

    if (event.kind == combat::CombatEventKind::defeated) {
        VisualEffect marker{};
        marker.active = true;
        marker.kind = VisualEffectKind::defeat_marker;
        marker.position = event.position;
        marker.lifetime_seconds = 0.65F;
        static_cast<void>(try_spawn(marker));
        return;
    }

    if (event.kind != combat::CombatEventKind::hit
        && event.kind != combat::CombatEventKind::player_hit) {
        return;
    }

    if (event.kind == combat::CombatEventKind::hit
        && event.target_index < flash_seconds_.size()) {
        flash_seconds_[event.target_index] = 2.0F / 60.0F;
    }
    if (event.kind == combat::CombatEventKind::player_hit) {
        player_hit_source_ = event.position;
        player_hit_indicator_seconds_ = 0.55F;
    }

    VisualEffect spark{};
    spark.active = true;
    spark.kind = VisualEffectKind::spark;
    spark.position = event.position;
    spark.lifetime_seconds = 0.18F;
    spark.value = event.value;
    static_cast<void>(try_spawn(spark));

    VisualEffect number = spark;
    number.kind = VisualEffectKind::damage_number;
    number.lifetime_seconds = 0.45F;
    static_cast<void>(try_spawn(number));

    const ShakeParameters shake = shake_for(event.feedback);
    shake_amplitude_ = std::max(shake_amplitude_, shake.amplitude);
    shake_time_ = std::max(shake_time_, shake.duration);
}

void CombatFeedback::update(float frame_seconds) noexcept {
    const float dt = std::clamp(frame_seconds, 0.0F, 0.1F);
    for (VisualEffect& effect : effects_) {
        if (!effect.active) {
            continue;
        }
        effect.age_seconds += dt;
        if (effect.age_seconds >= effect.lifetime_seconds) {
            effect = VisualEffect{};
        }
    }

    for (float& flash : flash_seconds_) {
        flash = std::max(0.0F, flash - dt);
    }
    player_hit_indicator_seconds_ = std::max(0.0F,
        player_hit_indicator_seconds_ - dt);

    if (shake_time_ > 0.0F) {
        shake_time_ = std::max(0.0F, shake_time_ - dt);
        shake_phase_ += dt;
        if (shake_time_ == 0.0F) {
            shake_amplitude_ = 0.0F;
        }
    }
}

void CombatFeedback::clear() noexcept {
    effects_.fill(VisualEffect{});
    flash_seconds_.fill(0.0F);
    shake_amplitude_ = 0.0F;
    shake_time_ = 0.0F;
    shake_phase_ = 0.0F;
    player_hit_indicator_seconds_ = 0.0F;
    player_hit_source_ = {};
    dropped_count_ = 0;
}

bool CombatFeedback::try_spawn(const VisualEffect& effect) noexcept {
    for (VisualEffect& slot : effects_) {
        if (slot.active) {
            continue;
        }
        slot = effect;
        slot.active = true;
        slot.age_seconds = 0.0F;
        return true;
    }
    ++dropped_count_;
    return false;
}

std::size_t CombatFeedback::active_count() const noexcept {
    std::size_t count = 0;
    for (const VisualEffect& effect : effects_) {
        if (effect.active) {
            ++count;
        }
    }
    return count;
}

std::uint32_t CombatFeedback::dropped_count() const noexcept {
    return dropped_count_;
}

float CombatFeedback::shake_amplitude() const noexcept {
    return shake_amplitude_;
}

CameraOffset CombatFeedback::camera_offset() const noexcept {
    if (shake_time_ <= 0.0F || shake_amplitude_ <= 0.0F) {
        return {};
    }
    return {
        std::sin(shake_phase_ * 91.0F) * shake_amplitude_,
        std::cos(shake_phase_ * 73.0F) * shake_amplitude_,
    };
}

float CombatFeedback::target_flash_seconds(
    std::size_t target_index) const noexcept {
    return target_index < flash_seconds_.size()
        ? flash_seconds_[target_index]
        : 0.0F;
}

float CombatFeedback::player_hit_indicator_seconds() const noexcept {
    return player_hit_indicator_seconds_;
}

combat::Vec3 CombatFeedback::player_hit_source() const noexcept {
    return player_hit_source_;
}

const std::array<VisualEffect, CombatFeedback::kCapacity>&
CombatFeedback::effects() const noexcept {
    return effects_;
}

}  // namespace arpg::platform

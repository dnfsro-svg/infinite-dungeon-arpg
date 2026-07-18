#include "hud_notice_state.hpp"

#include <cmath>
#include <cstdarg>
#include <cstdio>
#include <cstring>
#include <limits>

namespace arpg::platform {
namespace {

constexpr float kTransientSeconds = 3.0F;
constexpr std::uint8_t kSaveErrorPriority = 110U;
constexpr std::uint8_t kRecoveryPriority = 109U;
constexpr std::uint8_t kAbyssPriority = 100U;
constexpr std::uint8_t kHolePriority = 90U;
constexpr std::uint8_t kExitPriority = 80U;
constexpr std::uint8_t kRoomClearPriority = 70U;
constexpr std::uint8_t kRewardPriority = 60U;
constexpr std::uint8_t kLevelPriority = 50U;
constexpr std::uint8_t kPassivePointsPriority = 40U;
constexpr std::uint8_t kInventoryPriority = 30U;
constexpr std::uint8_t kPassiveTreePriority = 20U;

[[nodiscard]] std::size_t bounded_length(
    const std::array<char, 160>& text) noexcept {
    std::size_t length{};
    while (length < text.size() && text[length] != '\0') {
        ++length;
    }
    return length;
}

[[nodiscard]] bool action_key(char* output,
    std::size_t output_size,
    const ControlHints& hints,
    const char* action) noexcept {
    if (output_size == 0U) {
        return false;
    }
    output[0] = '\0';
    const std::size_t action_length = std::strlen(action);
    const std::size_t length = bounded_length(hints.secondary);
    if (action_length == 0U || action_length + 1U > length) {
        return false;
    }

    for (std::size_t cursor{}; cursor + action_length <= length; ++cursor) {
        if (std::memcmp(hints.secondary.data() + cursor,
                action, action_length) != 0) {
            continue;
        }
        if (cursor == 0U || hints.secondary[cursor - 1U] != ' ') {
            continue;
        }
        std::size_t end = cursor - 1U;
        while (end > 0U && hints.secondary[end - 1U] == ' ') {
            --end;
        }
        std::size_t begin{};
        for (std::size_t index = end; index > 1U; --index) {
            if (hints.secondary[index - 1U] == ' '
                && hints.secondary[index - 2U] == ' ') {
                begin = index;
                break;
            }
        }
        if (begin == end) {
            return false;
        }
        const std::size_t count = end - begin;
        const std::size_t copied = count < output_size - 1U
            ? count : output_size - 1U;
        std::memcpy(output, hints.secondary.data() + begin, copied);
        output[copied] = '\0';
        return copied != 0U;
    }
    return false;
}

void format_notice(HudNotice& notice, const char* format, ...) noexcept {
    va_list arguments;
    va_start(arguments, format);
    const int written = std::vsnprintf(notice.text.bytes.data(),
        notice.text.bytes.size(), format, arguments);
    va_end(arguments);
    notice.text.bytes.back() = '\0';
    notice.text.truncated = written < 0
        || static_cast<std::size_t>(written) >= notice.text.bytes.size();
}

[[nodiscard]] bool is_room_context(HudNoticeKind kind) noexcept {
    return kind != HudNoticeKind::none
        && kind != HudNoticeKind::save_error
        && kind != HudNoticeKind::recovery_required;
}

[[nodiscard]] bool has_open_exit(
    const dungeon::DungeonSnapshot& snapshot) noexcept {
    for (const bool open : snapshot.exits_open) {
        if (open) {
            return true;
        }
    }
    return false;
}

void erase_kind(std::array<HudNotice, 4>& notices,
    HudNoticeKind kind) noexcept {
    std::size_t write{};
    for (const HudNotice& notice : notices) {
        if (notice.kind != HudNoticeKind::none && notice.kind != kind) {
            notices[write++] = notice;
        }
    }
    while (write < notices.size()) {
        notices[write++] = {};
    }
}

[[nodiscard]] HudNotice* find_notice(std::array<HudNotice, 4>& notices,
    HudNoticeKind kind) noexcept {
    for (HudNotice& notice : notices) {
        if (notice.kind == kind) {
            return &notice;
        }
    }
    return nullptr;
}

void enqueue(std::array<HudNotice, 4>& notices,
    std::uint32_t& dropped_count,
    HudNotice notice) noexcept {
    if (HudNotice* const existing = find_notice(notices, notice.kind)) {
        *existing = notice;
        return;
    }

    std::size_t count{};
    while (count < notices.size()
        && notices[count].kind != HudNoticeKind::none) {
        ++count;
    }
    if (count == notices.size()) {
        if (notice.priority <= notices.back().priority) {
            ++dropped_count;
            return;
        }
        --count;
        ++dropped_count;
    }

    std::size_t insert_at{};
    while (insert_at < count && notices[insert_at].priority >= notice.priority) {
        ++insert_at;
    }
    for (std::size_t index = count; index > insert_at; --index) {
        notices[index] = notices[index - 1U];
    }
    notices[insert_at] = notice;
    if (count + 1U < notices.size()) {
        notices[count + 1U] = {};
    }
}

void set_persistent(std::array<HudNotice, 4>& notices,
    std::uint32_t& dropped_count,
    HudNoticeKind kind,
    std::uint8_t priority,
    bool active,
    const char* text) noexcept {
    if (!active) {
        erase_kind(notices, kind);
        return;
    }
    HudNotice notice{};
    notice.kind = kind;
    notice.priority = priority;
    notice.seconds_left = (std::numeric_limits<float>::infinity)();
    format_notice(notice, "%s", text);
    enqueue(notices, dropped_count, notice);
}

void add_transient(std::array<HudNotice, 4>& notices,
    std::uint32_t& dropped_count,
    HudNoticeKind kind,
    std::uint8_t priority,
    const char* text) noexcept {
    HudNotice notice{};
    notice.kind = kind;
    notice.priority = priority;
    notice.seconds_left = kTransientSeconds;
    format_notice(notice, "%s", text);
    enqueue(notices, dropped_count, notice);
}

void add_action_notice(std::array<HudNotice, 4>& notices,
    std::uint32_t& dropped_count,
    HudNoticeKind kind,
    std::uint8_t priority,
    const ControlHints& hints,
    const char* action,
    const char* suffix) noexcept {
    char key[32]{};
    if (action_key(key, sizeof(key), hints, action)) {
        HudNotice notice{};
        notice.kind = kind;
        notice.priority = priority;
        notice.seconds_left = kTransientSeconds;
        format_notice(notice, "%s %s", key, suffix);
        enqueue(notices, dropped_count, notice);
        return;
    }
    add_transient(notices, dropped_count, kind, priority, suffix);
}

}  // namespace

void HudNoticeState::observe(const dungeon::DungeonSnapshot& previous,
    const dungeon::DungeonSnapshot& current,
    const DungeonRenderStatus& status,
    const ControlHints& hints,
    bool recovery_required) noexcept {
    const bool new_commit = current.commit_generation != last_commit_generation_;
    const bool new_room = last_room_index_ != 0U
        && current.room_index != last_room_index_;
    const bool context_changed = new_commit
        || last_room_index_ != current.room_index;
    if (new_room) {
        clear_room_context();
    }

    set_persistent(notices_, dropped_count_, HudNoticeKind::save_error,
        kSaveErrorPriority, status.indicator == SaveIndicator::error,
        "Save failed");
    set_persistent(notices_, dropped_count_, HudNoticeKind::recovery_required,
        kRecoveryPriority, recovery_required, "Recovery required");

    if (context_changed && current.abyss_exit_confirmation_armed) {
        add_action_notice(notices_, dropped_count_, HudNoticeKind::abyss_abandon,
            kAbyssPriority, hints, "Interact", "again to abandon rewards");
    }
    const bool awaiting_exit = current.has_active_room
        && current.phase == dungeon::RoomPhase::awaiting_exit;
    if (context_changed && awaiting_exit && current.has_hole) {
        add_action_notice(notices_, dropped_count_, HudNoticeKind::hole_interact,
            kHolePriority, hints, "Interact", "to descend");
    }
    if (context_changed && awaiting_exit && has_open_exit(current)) {
        add_action_notice(notices_, dropped_count_, HudNoticeKind::exit_ready,
            kExitPriority, hints, "Interact", "to enter exit");
    }
    if (current.phase == dungeon::RoomPhase::cleared
        && current.remaining_targets == 0U
        && previous.phase != dungeon::RoomPhase::cleared
        && (new_commit || last_room_index_ != current.room_index)) {
        add_transient(notices_, dropped_count_, HudNoticeKind::room_clear,
            kRoomClearPriority, "Room clear");
    }
    if (current.last_room_experience != 0U
        && current.last_room_experience != last_room_experience_) {
        HudNotice notice{};
        notice.kind = HudNoticeKind::reward;
        notice.priority = kRewardPriority;
        notice.seconds_left = kTransientSeconds;
        format_notice(notice, "Reward +%llu XP",
            static_cast<unsigned long long>(current.last_room_experience));
        enqueue(notices_, dropped_count_, notice);
    }
    if (current.progression.level > previous.progression.level
        && current.progression.level != last_level_) {
        HudNotice notice{};
        notice.kind = HudNoticeKind::level_up;
        notice.priority = kLevelPriority;
        notice.seconds_left = kTransientSeconds;
        format_notice(notice, "Level %u", static_cast<unsigned>(current.progression.level));
        enqueue(notices_, dropped_count_, notice);
    }
    if (context_changed && current.progression.unspent_passive_points != 0U) {
        add_transient(notices_, dropped_count_, HudNoticeKind::passive_points,
            kPassivePointsPriority, "Passive points available");
    }
    if (context_changed && current.inventory_count != 0U) {
        add_action_notice(notices_, dropped_count_, HudNoticeKind::inventory,
            kInventoryPriority, hints, "Inventory", "to open inventory");
    }
    if (context_changed && awaiting_exit
        && current.progression.unspent_passive_points != 0U) {
        add_action_notice(notices_, dropped_count_, HudNoticeKind::passive_tree,
            kPassiveTreePriority, hints, "Passive Tree", "to open passive tree");
    }

    last_commit_generation_ = current.commit_generation;
    last_room_index_ = current.room_index;
    last_room_experience_ = current.last_room_experience;
    last_level_ = current.progression.level;
}

void HudNoticeState::update(float frame_seconds, bool paused) noexcept {
    if (paused || !(frame_seconds > 0.0F)) {
        return;
    }
    std::size_t write{};
    for (HudNotice notice : notices_) {
        if (notice.kind == HudNoticeKind::none) {
            continue;
        }
        if (!std::isinf(notice.seconds_left)) {
            if (frame_seconds >= notice.seconds_left) {
                continue;
            }
            notice.seconds_left -= frame_seconds;
        }
        notices_[write++] = notice;
    }
    while (write < notices_.size()) {
        notices_[write++] = {};
    }
}

void HudNoticeState::clear_room_context() noexcept {
    std::size_t write{};
    for (const HudNotice& notice : notices_) {
        if (notice.kind != HudNoticeKind::none && !is_room_context(notice.kind)) {
            notices_[write++] = notice;
        }
    }
    while (write < notices_.size()) {
        notices_[write++] = {};
    }
    last_room_index_ = 0U;
}

HudNoticeView HudNoticeState::view() const noexcept {
    HudNoticeView result{};
    result.primary = notices_[0];
    result.secondary = notices_[1];
    return result;
}

std::uint32_t HudNoticeState::dropped_count() const noexcept {
    return dropped_count_;
}

}  // namespace arpg::platform

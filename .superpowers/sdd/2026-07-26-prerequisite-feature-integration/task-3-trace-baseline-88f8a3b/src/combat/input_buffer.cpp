#include "combat/input_buffer.hpp"

namespace arpg::combat {

bool InputBuffer::push(Action action) noexcept {
    if (size_ == kCapacity) {
        ++overflow_count_;
        return false;
    }

    entries_[size_] = Entry{action, kLifetimeTicks};
    ++size_;
    return true;
}

bool InputBuffer::consume(Action action) noexcept {
    for (std::size_t index = 0; index < size_; ++index) {
        if (entries_[index].action != action) {
            continue;
        }

        for (std::size_t next = index + 1; next < size_; ++next) {
            entries_[next - 1] = entries_[next];
        }
        --size_;
        entries_[size_] = Entry{};
        return true;
    }
    return false;
}

void InputBuffer::age(bool paused) noexcept {
    if (paused) {
        return;
    }

    std::size_t write_index = 0;
    for (std::size_t read_index = 0; read_index < size_; ++read_index) {
        Entry entry = entries_[read_index];
        --entry.remaining_ticks;
        if (entry.remaining_ticks == 0) {
            ++expired_count_;
            continue;
        }

        entries_[write_index] = entry;
        ++write_index;
    }

    for (std::size_t index = write_index; index < size_; ++index) {
        entries_[index] = Entry{};
    }
    size_ = write_index;
}

void InputBuffer::clear() noexcept {
    for (std::size_t index = 0; index < size_; ++index) {
        entries_[index] = Entry{};
    }
    size_ = 0;
}

void InputBuffer::reset_diagnostics() noexcept {
    expired_count_ = 0;
    overflow_count_ = 0;
}

std::size_t InputBuffer::size() const noexcept {
    return size_;
}

std::uint32_t InputBuffer::expired_count() const noexcept {
    return expired_count_;
}

std::uint32_t InputBuffer::overflow_count() const noexcept {
    return overflow_count_;
}

}  // namespace arpg::combat

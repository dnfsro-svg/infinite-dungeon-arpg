#pragma once

#include <array>
#include <cassert>
#include <cstddef>
#include <cstdint>
#include <limits>
#include <optional>
#include <type_traits>
#include <utility>

namespace arpg::core {

template <typename T, std::size_t N>
class FixedPool final {
public:
    using index_type = std::uint32_t;
    using generation_type = std::uint32_t;

    static constexpr index_type invalid_index =
        std::numeric_limits<index_type>::max();

    struct Handle final {
        index_type index{invalid_index};
        generation_type generation{0};

        friend constexpr bool operator==(
            Handle lhs,
            Handle rhs) noexcept {
            return lhs.index == rhs.index &&
                lhs.generation == rhs.generation;
        }

        friend constexpr bool operator!=(
            Handle lhs,
            Handle rhs) noexcept {
            return !(lhs == rhs);
        }
    };

    FixedPool() noexcept {
        static_assert(N > 0, "FixedPool capacity must be positive");
        static_assert(
            N <= std::numeric_limits<index_type>::max(),
            "FixedPool capacity exceeds index range");
        static_assert(
            std::is_nothrow_destructible_v<T>,
            "FixedPool values must be nothrow destructible");

        for (std::size_t index = 0; index < N; ++index) {
            slots_[index].next_free =
                index + 1 < N
                ? static_cast<index_type>(index + 1)
                : invalid_index;
        }
    }

    ~FixedPool() noexcept = default;

    FixedPool(const FixedPool&) = delete;
    FixedPool& operator=(const FixedPool&) = delete;
    FixedPool(FixedPool&&) = delete;
    FixedPool& operator=(FixedPool&&) = delete;

    template <
        typename... Args,
        std::enable_if_t<
            std::is_nothrow_constructible_v<T, Args...>,
            int> = 0>
    [[nodiscard]] std::optional<Handle> try_emplace(
        Args&&... args) noexcept {
        if (free_head_ == invalid_index) {
            return std::nullopt;
        }

        assert(free_head_ < N);
        const index_type index = free_head_;
        Slot& slot = slot_at(index);
        free_head_ = slot.next_free;
        slot.next_free = invalid_index;
        slot.value.emplace(std::forward<Args>(args)...);
        ++size_;
        assert(size_ <= N);
        return Handle{index, slot.generation};
    }

    [[nodiscard]] bool release(Handle handle) noexcept {
        if (!contains(handle)) {
            return false;
        }

        Slot& slot = slot_at(handle.index);
        slot.value.reset();
        ++slot.generation;
        if (slot.generation == 0) {
            ++slot.generation;
        }
        slot.next_free = free_head_;
        free_head_ = handle.index;
        --size_;
        assert(size_ <= N);
        return true;
    }

    [[nodiscard]] T* get(Handle handle) noexcept {
        return contains(handle)
            ? &(*slot_at(handle.index).value)
            : nullptr;
    }

    [[nodiscard]] const T* get(Handle handle) const noexcept {
        return contains(handle)
            ? &(*slot_at(handle.index).value)
            : nullptr;
    }

    [[nodiscard]] bool contains(Handle handle) const noexcept {
        return handle.index < N &&
            handle.generation != 0 &&
            slot_at(handle.index).generation == handle.generation &&
            slot_at(handle.index).value.has_value();
    }

    [[nodiscard]] std::size_t size() const noexcept {
        return size_;
    }

    [[nodiscard]] bool empty() const noexcept {
        return size_ == 0;
    }

    [[nodiscard]] bool full() const noexcept {
        return size_ == N;
    }

    [[nodiscard]] static constexpr std::size_t capacity() noexcept {
        return N;
    }

private:
    struct Slot final {
        std::optional<T> value;
        generation_type generation{1};
        index_type next_free{invalid_index};
    };

    [[nodiscard]] Slot& slot_at(index_type index) noexcept {
        assert(index < N);
        return slots_[index];
    }

    [[nodiscard]] const Slot& slot_at(index_type index) const noexcept {
        assert(index < N);
        return slots_[index];
    }

    std::array<Slot, N> slots_{};
    index_type free_head_{0};
    std::size_t size_{0};
};

}  // namespace arpg::core

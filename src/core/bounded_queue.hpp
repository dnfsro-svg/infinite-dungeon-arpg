#pragma once

#include <array>
#include <cstddef>
#include <optional>
#include <type_traits>
#include <utility>

namespace arpg::core {

template <typename T, std::size_t N>
class BoundedQueue final {
public:
    static_assert(N > 0, "BoundedQueue capacity must be positive");
    static_assert(
        std::is_nothrow_move_constructible_v<T>,
        "BoundedQueue values must be nothrow move constructible");
    static_assert(
        std::is_nothrow_destructible_v<T>,
        "BoundedQueue values must be nothrow destructible");

    BoundedQueue() noexcept = default;
    ~BoundedQueue() noexcept = default;

    BoundedQueue(const BoundedQueue&) = delete;
    BoundedQueue& operator=(const BoundedQueue&) = delete;
    BoundedQueue(BoundedQueue&&) = delete;
    BoundedQueue& operator=(BoundedQueue&&) = delete;

    template <
        typename... Args,
        std::enable_if_t<
            std::is_nothrow_constructible_v<T, Args...>,
            int> = 0>
    [[nodiscard]] bool try_emplace(Args&&... args) noexcept {
        if (full()) {
            return false;
        }

        slots_[tail_].emplace(std::forward<Args>(args)...);
        tail_ = next_index(tail_);
        ++size_;
        return true;
    }

    template <
        typename Value = T,
        std::enable_if_t<
            std::is_nothrow_copy_constructible_v<Value>,
            int> = 0>
    [[nodiscard]] bool try_push(const T& value) noexcept {
        return try_emplace(value);
    }

    [[nodiscard]] bool try_push(T&& value) noexcept {
        return try_emplace(std::move(value));
    }

    [[nodiscard]] std::optional<T> try_pop() noexcept {
        if (empty()) {
            return std::nullopt;
        }

        std::optional<T> result{
            std::move(*slots_[head_])};
        slots_[head_].reset();
        head_ = next_index(head_);
        --size_;
        return result;
    }

    [[nodiscard]] const T* front() const noexcept {
        return empty() ? nullptr : &(*slots_[head_]);
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
    [[nodiscard]] static constexpr std::size_t next_index(
        std::size_t index) noexcept {
        return (index + 1U) % N;
    }

    std::array<std::optional<T>, N> slots_{};
    std::size_t head_{0};
    std::size_t tail_{0};
    std::size_t size_{0};
};

}  // namespace arpg::core

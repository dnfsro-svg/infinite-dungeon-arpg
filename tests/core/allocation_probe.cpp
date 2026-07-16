#include "allocation_probe.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <limits>
#include <new>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {

std::atomic<std::uint64_t> g_allocations{0};
thread_local std::int64_t g_allocations_before_failure = -1;

[[nodiscard]] bool should_fail_allocation() noexcept {
    if (g_allocations_before_failure < 0) return false;
    if (g_allocations_before_failure == 0) {
        g_allocations_before_failure = -1;
        return true;
    }
    --g_allocations_before_failure;
    return false;
}

[[nodiscard]] void* allocate_unaligned(std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (should_fail_allocation()) throw std::bad_alloc{};
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc{};
}

[[nodiscard]] void* allocate_aligned(
    std::size_t size,
    std::size_t alignment) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (should_fail_allocation()) throw std::bad_alloc{};
#if defined(_MSC_VER)
    if (void* memory =
            _aligned_malloc(size == 0 ? 1 : size, alignment)) {
        return memory;
    }
#else
    void* memory = nullptr;
    if (posix_memalign(
            &memory,
            alignment,
            size == 0 ? 1 : size) == 0) {
        return memory;
    }
#endif
    throw std::bad_alloc{};
}

void free_aligned(void* memory) noexcept {
#if defined(_MSC_VER)
    _aligned_free(memory);
#else
    std::free(memory);
#endif
}

}  // namespace

namespace arpg::test {

std::uint64_t allocation_count() noexcept {
    return g_allocations.load(std::memory_order_relaxed);
}

ScopedAllocationFailure::ScopedAllocationFailure(
    std::size_t successful_allocations_before_failure) noexcept
    : previous_(g_allocations_before_failure) {
    const auto maximum = static_cast<std::size_t>(
        (std::numeric_limits<std::int64_t>::max)());
    g_allocations_before_failure = successful_allocations_before_failure
        > maximum ? (std::numeric_limits<std::int64_t>::max)()
                  : static_cast<std::int64_t>(
                      successful_allocations_before_failure);
}

ScopedAllocationFailure::~ScopedAllocationFailure() noexcept {
    g_allocations_before_failure = previous_;
}

}  // namespace arpg::test

void* operator new(std::size_t size) {
    return allocate_unaligned(size);
}

void* operator new[](std::size_t size) {
    return allocate_unaligned(size);
}

void operator delete(void* memory) noexcept {
    std::free(memory);
}

void operator delete[](void* memory) noexcept {
    std::free(memory);
}

void operator delete(void* memory, std::size_t) noexcept {
    std::free(memory);
}

void operator delete[](void* memory, std::size_t) noexcept {
    std::free(memory);
}

void* operator new(
    std::size_t size,
    const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](
    std::size_t size,
    const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](size);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(
    void* memory,
    const std::nothrow_t&) noexcept {
    ::operator delete(memory);
}

void operator delete[](
    void* memory,
    const std::nothrow_t&) noexcept {
    ::operator delete[](memory);
}

void* operator new(
    std::size_t size,
    std::align_val_t alignment) {
    return allocate_aligned(
        size,
        static_cast<std::size_t>(alignment));
}

void* operator new[](
    std::size_t size,
    std::align_val_t alignment) {
    return allocate_aligned(
        size,
        static_cast<std::size_t>(alignment));
}

void operator delete(
    void* memory,
    std::align_val_t) noexcept {
    free_aligned(memory);
}

void operator delete[](
    void* memory,
    std::align_val_t) noexcept {
    free_aligned(memory);
}

void operator delete(
    void* memory,
    std::size_t,
    std::align_val_t) noexcept {
    free_aligned(memory);
}

void operator delete[](
    void* memory,
    std::size_t,
    std::align_val_t) noexcept {
    free_aligned(memory);
}

void* operator new(
    std::size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    try {
        return ::operator new(size, alignment);
    } catch (...) {
        return nullptr;
    }
}

void* operator new[](
    std::size_t size,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    try {
        return ::operator new[](size, alignment);
    } catch (...) {
        return nullptr;
    }
}

void operator delete(
    void* memory,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    ::operator delete(memory, alignment);
}

void operator delete[](
    void* memory,
    std::align_val_t alignment,
    const std::nothrow_t&) noexcept {
    ::operator delete[](memory, alignment);
}

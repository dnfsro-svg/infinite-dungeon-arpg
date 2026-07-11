#include "allocation_probe.hpp"

#include <atomic>
#include <cstddef>
#include <cstdlib>
#include <new>

#if defined(_MSC_VER)
#include <malloc.h>
#endif

namespace {

std::atomic<std::uint64_t> g_allocations{0};

[[nodiscard]] void* allocate_unaligned(std::size_t size) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
    if (void* memory = std::malloc(size == 0 ? 1 : size)) {
        return memory;
    }
    throw std::bad_alloc{};
}

[[nodiscard]] void* allocate_aligned(
    std::size_t size,
    std::size_t alignment) {
    g_allocations.fetch_add(1, std::memory_order_relaxed);
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

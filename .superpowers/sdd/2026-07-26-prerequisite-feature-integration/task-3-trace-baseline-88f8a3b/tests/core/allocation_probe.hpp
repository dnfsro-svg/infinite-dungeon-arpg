#pragma once

#include <cstddef>
#include <cstdint>

namespace arpg::test {

[[nodiscard]] std::uint64_t allocation_count() noexcept;

class ScopedAllocationFailure final {
public:
    explicit ScopedAllocationFailure(
        std::size_t successful_allocations_before_failure) noexcept;
    ~ScopedAllocationFailure() noexcept;

    ScopedAllocationFailure(const ScopedAllocationFailure&) = delete;
    ScopedAllocationFailure& operator=(
        const ScopedAllocationFailure&) = delete;

private:
    std::int64_t previous_{};
};

}  // namespace arpg::test

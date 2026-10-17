#include "test_framework.hpp"

#include "allocation_probe.hpp"

#include <new>

namespace {

arpg::test::Failure nested_failure_scopes_restore_previous_state() noexcept {
    {
        arpg::test::ScopedAllocationFailure outer{0U};
        {
            arpg::test::ScopedAllocationFailure inner{1U};
            void* first = ::operator new(1U, std::nothrow);
            ARPG_REQUIRE(first != nullptr);
            ::operator delete(first);
            ARPG_REQUIRE(::operator new(1U, std::nothrow) == nullptr);
        }
        ARPG_REQUIRE(::operator new(1U, std::nothrow) == nullptr);
    }
    void* after = ::operator new(1U, std::nothrow);
    ARPG_REQUIRE(after != nullptr);
    ::operator delete(after);
    return {};
}

constexpr arpg::test::TestCase kCases[] = {
    {"nested failure scopes restore state",
        &nested_failure_scopes_restore_previous_state},
};

}  // namespace

arpg::test::TestSuite allocation_probe_suite() noexcept {
    return arpg::test::make_suite("allocation_probe", kCases);
}

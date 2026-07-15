#include "test_framework.hpp"

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#include <cstdlib>
#endif

arpg::test::TestSuite checkpoint_codec_suite() noexcept;
arpg::test::TestSuite passive_tree_checkpoint_suite() noexcept;
arpg::test::TestSuite save_store_suite() noexcept;
arpg::test::TestSuite save_store_fault_suite() noexcept;
arpg::test::TestSuite dungeon_save_integration_suite() noexcept;

int main() {
#if defined(_WIN32) && defined(_DEBUG)
    _set_error_mode(_OUT_TO_STDERR);
    _CrtSetReportMode(_CRT_ASSERT, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ASSERT, _CRTDBG_FILE_STDERR);
    _CrtSetReportMode(_CRT_ERROR, _CRTDBG_MODE_FILE);
    _CrtSetReportFile(_CRT_ERROR, _CRTDBG_FILE_STDERR);
    _set_abort_behavior(_WRITE_ABORT_MSG,
        _WRITE_ABORT_MSG | _CALL_REPORTFAULT);
#endif
    const arpg::test::TestSuite suites[] = {
        checkpoint_codec_suite(),
        passive_tree_checkpoint_suite(),
        save_store_suite(),
        save_store_fault_suite(),
        dungeon_save_integration_suite(),
    };

    return arpg::test::run_suites(suites, 48, "stage 8 task 8 persistence");
}

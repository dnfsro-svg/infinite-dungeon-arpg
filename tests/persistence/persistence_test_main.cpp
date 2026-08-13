#include "test_framework.hpp"

#include <cstdlib>

#if defined(_WIN32) && defined(_DEBUG)
#include <crtdbg.h>
#include <cstdlib>
#endif

arpg::test::TestSuite checkpoint_codec_suite() noexcept;
arpg::test::TestSuite checkpoint_v8_skill_loadout_suite() noexcept;
arpg::test::TestSuite checkpoint_v9_suite() noexcept;
arpg::test::TestSuite checkpoint_v9_unlock_suite() noexcept;
arpg::test::TestSuite death_checkpoint_codec_suite() noexcept;
arpg::test::TestSuite passive_tree_checkpoint_suite() noexcept;
arpg::test::TestSuite save_store_suite() noexcept;
arpg::test::TestSuite save_store_fault_suite() noexcept;
arpg::test::TestSuite save_commit_worker_suite() noexcept;
arpg::test::TestSuite save_directory_lease_suite() noexcept;
arpg::test::TestSuite dungeon_save_integration_suite() noexcept;

namespace {

bool cplay038_save_directory_only() noexcept {
#if defined(_WIN32)
    char* value = nullptr;
    std::size_t length = 0U;
    const errno_t error = _dupenv_s(&value, &length,
        "ARPG_CPLAY038_SAVE_DIRECTORY_ONLY");
    const bool enabled = error == 0 && value != nullptr;
    std::free(value);
    return enabled;
#else
    return std::getenv("ARPG_CPLAY038_SAVE_DIRECTORY_ONLY") != nullptr;
#endif
}

}  // namespace

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
    if (cplay038_save_directory_only()) {
        const arpg::test::TestSuite focused[] = {
            save_directory_lease_suite(),
        };
        return arpg::test::run_suites(focused, 1,
            "CPLAY-038 save directory lease");
    }
    const arpg::test::TestSuite suites[] = {
        checkpoint_codec_suite(),
        checkpoint_v8_skill_loadout_suite(),
        death_checkpoint_codec_suite(),
        passive_tree_checkpoint_suite(),
        save_store_suite(),
        save_store_fault_suite(),
        save_commit_worker_suite(),
        save_directory_lease_suite(),
        dungeon_save_integration_suite(),
        checkpoint_v9_suite(),
        checkpoint_v9_unlock_suite(),
    };

    return arpg::test::run_suites(suites, 118, "checkpoint persistence");
}

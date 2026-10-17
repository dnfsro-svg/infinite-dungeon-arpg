#define UNICODE
#define _UNICODE
#include <windows.h>
#include <iostream>
#include <string>
#include <vector>

static std::wstring source_name(const DISPLAYCONFIG_PATH_SOURCE_INFO& source) {
    DISPLAYCONFIG_SOURCE_DEVICE_NAME name{};
    name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_SOURCE_NAME;
    name.header.size = sizeof(name);
    name.header.adapterId = source.adapterId;
    name.header.id = source.id;
    return DisplayConfigGetDeviceInfo(&name.header) == ERROR_SUCCESS
        ? std::wstring(name.viewGdiDeviceName)
        : L"<source-name-error>";
}

static std::wstring target_name(const DISPLAYCONFIG_PATH_TARGET_INFO& target) {
    DISPLAYCONFIG_TARGET_DEVICE_NAME name{};
    name.header.type = DISPLAYCONFIG_DEVICE_INFO_GET_TARGET_NAME;
    name.header.size = sizeof(name);
    name.header.adapterId = target.adapterId;
    name.header.id = target.id;
    return DisplayConfigGetDeviceInfo(&name.header) == ERROR_SUCCESS
        ? std::wstring(name.monitorFriendlyDeviceName)
        : L"<target-name-error>";
}

static LONG list_paths() {
    UINT32 path_count = 0, mode_count = 0;
    LONG rc = GetDisplayConfigBufferSizes(QDC_ALL_PATHS, &path_count, &mode_count);
    if (rc != ERROR_SUCCESS) {
        std::wcout << L"GetDisplayConfigBufferSizes=" << rc << L"\n";
        return rc;
    }
    std::vector<DISPLAYCONFIG_PATH_INFO> paths(path_count);
    std::vector<DISPLAYCONFIG_MODE_INFO> modes(mode_count);
    rc = QueryDisplayConfig(QDC_ALL_PATHS, &path_count, paths.data(), &mode_count,
                            modes.data(), nullptr);
    std::wcout << L"QueryDisplayConfig=" << rc << L" paths=" << path_count
               << L" modes=" << mode_count << L"\n";
    if (rc != ERROR_SUCCESS) return rc;
    for (UINT32 i = 0; i < path_count; ++i) {
        const auto& p = paths[i];
        std::wcout << L"path[" << i << L"] active="
                   << ((p.flags & DISPLAYCONFIG_PATH_ACTIVE) ? 1 : 0)
                   << L" available=" << (p.targetInfo.targetAvailable ? 1 : 0)
                   << L" source=" << source_name(p.sourceInfo)
                   << L" target=" << target_name(p.targetInfo)
                   << L" sourceId=" << p.sourceInfo.id
                   << L" targetId=" << p.targetInfo.id
                   << L" sourceModeIdx=" << p.sourceInfo.modeInfoIdx
                   << L" targetModeIdx=" << p.targetInfo.modeInfoIdx << L"\n";
        if (p.sourceInfo.modeInfoIdx != DISPLAYCONFIG_PATH_MODE_IDX_INVALID &&
            p.sourceInfo.modeInfoIdx < mode_count) {
            const auto& m = modes[p.sourceInfo.modeInfoIdx];
            if (m.infoType == DISPLAYCONFIG_MODE_INFO_TYPE_SOURCE) {
                std::wcout << L"  sourceMode=" << m.sourceMode.width << L"x"
                           << m.sourceMode.height << L" pos="
                           << m.sourceMode.position.x << L"," << m.sourceMode.position.y
                           << L"\n";
            }
        }
    }
    return rc;
}

int wmain(int argc, wchar_t** argv) {
    if (argc == 1 || std::wstring(argv[1]) == L"list") return static_cast<int>(list_paths());
    if (std::wstring(argv[1]) == L"extend") {
        const UINT32 flags = SDC_TOPOLOGY_EXTEND | SDC_APPLY |
                             SDC_ALLOW_CHANGES | SDC_SAVE_TO_DATABASE;
        LONG rc = SetDisplayConfig(0, nullptr, 0, nullptr, flags);
        std::wcout << L"SetDisplayConfig(EXTEND)=" << rc << L"\n";
        Sleep(3000);
        LONG list_rc = list_paths();
        return rc == ERROR_SUCCESS ? static_cast<int>(list_rc) : static_cast<int>(rc);
    }
    std::wcerr << L"usage: task2_display_tool [list|extend]\n";
    return 64;
}

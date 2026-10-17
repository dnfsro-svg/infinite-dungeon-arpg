# Task 1 Report: 动态材质常驻请求与差异同步

## 实现内容

- 新增固定容量 `MaterialAtlasMask`/`MaterialResidencyRequest`；基础请求包含公共、玩家、物品和 UI 图集，活动房间按生态选择一组环境/背景图集，活动怪物按 ID 选择其图集（不按 HP 过滤）。
- 新增按请求统计双纹理常驻字节数，采用饱和加法；验证峰值改为“基础 + 全部八种怪物 + 最大环境/房间背景”请求，默认清单峰值为 `199540736` 字节。
- `MaterialPack::synchronize_residency` 实现固定数组差异同步：相同失败请求不重试 I/O、先加载新增图集再卸载移除图集、半对失败立即清理、成功图集保留、超预算无 I/O、每图集最多一次带 ID/路径的警告。
- 兼容 `load(MaterialEcology)`，并将生产初始化、正常绘制和背景单绘制改为选择性常驻请求。
- 增加四个 C++ 用例，平台总数从 443 更新为 447；fake texture API 的纹理句柄与调用历史分离，并使用固定容量调用历史。

## 文件

- 新增：`src/platform/raylib/material_residency.hpp`、`src/platform/raylib/material_residency.cpp`
- 修改：`src/platform/raylib/material_pack.hpp`、`src/platform/raylib/material_pack.cpp`、`src/platform/raylib/material_asset_validation.cpp`、`src/platform/raylib/combat_renderer.cpp`、`src/platform/raylib/CMakeLists.txt`、`tests/platform/material_asset_validation_tests.cpp`、`tests/platform/platform_test_main.cpp`
- 额外授权修改：`src/platform/raylib/room_renderer.cpp`；该文件承载 Step 5 明确要求的 `draw_room_background_only()`，但原 Files 清单遗漏，已由主任务确认授权。

## RED

1. `./scripts/Configure.ps1 -Preset windows-msvc-debug`：退出 0。
2. 初始 `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1` 被未加载的 MSVC 标准库环境阻断：`fatal error C1083: 无法打开包括文件: “cstdint”`。
3. 加载 `vcvars64.bat` 后以同一串行构建重跑，退出 1，且唯一任务相关错误为：`fatal error C1083: 无法打开包括文件: “material_residency.hpp”: No such file or directory`。这是预期的 RED：新请求接口尚不存在。

## GREEN 与最终验证

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

最终输出：构建退出 0；`platform.units` 通过；`100% tests passed, 0 tests failed out of 1`；平台测试程序报告 `447 cases, 0 failures`。

## 自审

- 掩码容量由 `static_assert(count <= 64)` 固定；热路径仅遍历固定清单/数组，不分配堆内存。
- 用例覆盖请求组成、HP 为 0 的活动怪物、非活动槽位、无操作同步 10,000 次零分配、失败隔离与不重试、加载先于卸载、超预算零调用。
- `git diff --check` 退出 0；变更仅涉及 brief 文件以及已授权的 `room_renderer.cpp` 和本报告。

## Concerns

- 原始 Files 清单遗漏 `room_renderer.cpp`；主任务已明确授权其仅用于 Step 5 的背景常驻请求改造。除此之外无未解决 concern。

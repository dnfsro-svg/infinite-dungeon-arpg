# Stage 11-D Task 7 报告：已提交拾取回执与 HUD 反馈

## 范围

- 起点：`417f58a`。
- 仅实现 Task 7：runtime 已确认拾取回执、固定容量表现 observer、HUD 三秒通知和深渊紫色样式。
- 未实现 Task 8 架构守卫扩展、Task 9 正式截图或 Task 10 文档门禁。

## TDD RED

测试先行修改：

- `tests/platform/loot_pickup_feedback_tests.cpp`
- `tests/platform/dungeon_runtime_tests.cpp`
- `tests/platform/hud_notice_state_tests.cpp`
- `tests/platform/hud_host_integration_tests.cpp`
- 平台测试注册与 case count。

RED 命令：

```powershell
cmd.exe /d /s /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out/build/windows-msvc-debug --target arpg_platform_tests -- -j1"
```

真实失败：

- 首次 RED：`loot_pickup_feedback_tests.cpp(4): fatal error C1083: loot_pickup_feedback.hpp: No such file or directory`。
- 补齐 runtime 事务测试后的 RED：`DungeonRenderStatus::loot_pickup` 不存在。
- 两次失败均来自 Task 7 API 尚未实现；测试夹具的 abyss rewards include 错误已先修正并重新确认 RED。

## 实现与事务不变量

- `DungeonRuntime::service_pending_save()` 只为 `loot_pickup` / `abyss_reward_claim` 捕获生产 pending ordinal 对应的地面快照。
- store 必须返回 committed，verified state 必须与 pending next state 完全一致，Session 必须接受结果且进入对应 commit generation。
- captured item ID 与 captured ordinal 均必须从提交后 ground snapshot 消失，才更新持久保留的 `LootPickupReceipt`。
- 保存失败、非拾取保存、错误 ordinal、同 ordinal 被替换、Session fault 均不覆盖旧回执。
- capture、commit/resolve、confirm 分为独立栈帧，避免 Debug 大型 checkpoint + snapshot 同帧造成栈溢出。
- observer 只读取 `DungeonRenderStatus`，不读取 Session 或背包。
- 首次直接附着已有 valid 回执只建立 baseline；先观察正常空状态后，第一次真实回执会发布。连续空帧幂等。
- same receipt 不重发；malformed、backward、save error、recovery 清 baseline 且不发布。
- HUD 在普通 notice observation 前消费 feedback，以 reward priority 发布三秒；深渊反馈携带 abyss 标记并使用 chaos/紫色主题。
- unchanged observer 100,000 次 allocation probe 为零。

## GREEN 与回归

构建与平台 GREEN：

```powershell
cmd.exe /d /s /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build out/build/windows-msvc-debug --target arpg_platform_tests -- -j1 && out\build\windows-msvc-debug\bin\arpg_platform_tests.exe"
```

结果：`270 cases, 0 failures`。

相关 CTest：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|stage11c\.hud_stress\.zero_alloc_100k|stage11c\.architecture\.hud_boundaries(_self_test)?|stage11d\.renderer_integration_guard(_self_test)?)$' --output-on-failure
```

结果：`6/6 passed`，包括：

- `platform.units`
- `stage11c.hud_stress.zero_alloc_100k`
- Stage 11-C HUD architecture guard + self-test
- Stage 11-D renderer integration guard + self-test

最终执行 `git diff --check`，无 whitespace error。

## 修改文件

生产：

- `src/platform/raylib/dungeon_runtime.hpp/.cpp`
- `src/platform/raylib/loot_pickup_feedback.hpp/.cpp`
- `src/platform/raylib/hud_notice_state.hpp/.cpp`
- `src/platform/raylib/hud_view_model.hpp/.cpp`
- `src/platform/raylib/hud_renderer.hpp/.cpp`
- `src/platform/raylib/combat_renderer.hpp/.cpp`
- `src/platform/raylib/CMakeLists.txt`

测试：

- `tests/platform/loot_pickup_feedback_tests.cpp`
- `tests/platform/dungeon_runtime_tests.cpp`
- `tests/platform/hud_notice_state_tests.cpp`
- `tests/platform/hud_host_integration_tests.cpp`
- `tests/platform/hud_render_plan_tests.cpp`
- `tests/platform/hud_stress_tests.cpp`
- `tests/platform/platform_test_main.cpp`
- `tests/platform/CMakeLists.txt`

## 剩余风险

- Task 7 使用现有 catalog 英文基底名，和 Stage 11-D 地面标签保持一致；本任务未重命名 catalog。
- 正式 raylib 截图与证据验证属于 Task 9。

# Stage 11-C Task 8 Brief — Allocation Stress and Architecture Mutation Guards

## Scope

只实现 Task 8 的测试/门禁，不改生产功能，不做 Task9正式截图或Task10文档全门禁。

## Files

- Create: `tests/platform/hud_stress_tests.cpp`
- Create: `tests/platform/stage11c_hud_architecture_guard_test.cmake`
- Create: `tests/platform/stage11c_hud_architecture_guard_self_test.cmake`
- Create: `tests/platform/stage11c_hud_bad_source.txt`
- Modify: `tests/platform/CMakeLists.txt`

## Required outputs

- Executable `arpg_stage11c_hud_stress` linked with `tests/core/allocation_probe.cpp`.
- CTest `stage11c.hud_stress.zero_alloc_100k`.
- CTest `stage11c.architecture.hud_boundaries`.
- CTest `stage11c.architecture.hud_boundaries_self_test`.

## Required RED and behavior

- 先注册/调用新名称取得目标或fixture缺失的有效 RED。
- 压力 100,000 次：unchanged、low-health、status-heavy、max-counter、priority-overflow；覆盖 ViewModel build、notice observe/update/view、layout、所有纯draw plan；排除GPU/raylib draw。
- allocation probe 必须证明循环内0 heap alloc；同时用状态hash/等价比较证明输入快照不变。
- stress进程仅在环境 `ARPG_STAGE11C_HUD_STRESS=1` 时运行，否则明确命名失败；CTest必须设置该环境与300s timeout。
- 主guard禁止 HUD 边界内：`GetKeyPressed`、`DungeonSession`、`fixed_tick`、`SettingsStore`、`std::string`、`std::vector`、正常HUD旧`Budget`文本、第四个可见status tag。
- self-test 必须复制生产HUD源并逐一独立注入8类mutation，每类调用主guard并验证命名拒绝原因；不能只匹配fixture文本。
- Task7不注册重复guard，本任务名称必须唯一。

## Required GREEN

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage11c_hud_stress arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "^stage11c\.(hud_stress|architecture)" --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|combat.units|dungeon.units)$" --output-on-failure
git diff --check
```

## Commit and report

- Commit: `test: stress stage 11c hud boundaries`
- Write ignored `.superpowers/sdd/stage11c-task-8-report.md` with RED, mutation matrix, 100k allocation/hash evidence, exact GREEN, commit, self-review, concerns.
- 不提交 `.superpowers/sdd`。

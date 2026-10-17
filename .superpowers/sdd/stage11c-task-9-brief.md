# Stage 11-C Task 9 Brief — Real Raylib HUD Evidence and Anti-Injection Guard

## Scope

只实现 Task 9 的生产路径正式场景、6 张真实 raylib 截图、validator 和 anti-injection guard。不要改游戏玩法、质量设置、物品过滤或 Task10 文档/全门禁。

## Files

- Modify: `src/platform/raylib/raylib_host.hpp/.cpp`
- Create: `tests/platform/stage11c_hud_formal_game_validation.cpp`
- Create: `tests/platform/stage11c_hud_formal_validator.ps1`
- Create: `tests/platform/stage11c_hud_evidence_guard_test.cmake`
- Create: `tests/platform/stage11c_hud_evidence_guard_self_test.cmake`
- Create: `tests/platform/stage11c_hud_bad_formal_input.txt`
- Modify: `tests/platform/CMakeLists.txt`

## Required scenario interface

```cpp
enum class Stage11CHudValidationScenario : std::uint8_t {
    none, normal_combat, low_health_status, cleared_exit,
    abyss_abandon, level_up_points, debug_overlay
};
```

## Required RED/evidence

- 先注册 formal/validator/guard/self-test 名称，运行 `^stage11c\.hud_(formal|evidence)` 得到 missing target/report 的真实 RED。
- 必须生成 fresh 1280x720 PNG：`combat.png`、`low-health.png`、`cleared.png`、`abyss-warning.png`、`level-up.png`、`debug.png`。
- 每场景独立子进程，防 raylib 重初始化泄漏。
- summary 必含 scenario、HUD rects、player values、status tags、objective、notice kinds、navigation values、font readiness、F1、production snapshot hash、`result=pass`。

## Production-only constraints

- 场景只能通过 fixed seed、真实 Session/host、物理键 edge 和 production fixture seed 驱动。
- 禁止直接设置 `HudViewModel`、`HudNoticeState`、combat snapshot、room phase、logical action；禁止 TestAccess。
- 捕获只经现有唯一 post-Present helper，在 `EndDrawing()` 后；每场景新鲜文件。
- low-health/status、cleared、abyss confirmation、level-up/passive points 必须由生产模拟/Session 行为达到，不可篡改报告代替画面。

## Validator/guards

- PowerShell验证 PNG signature、1280x720、freshness、所有summary字段、区域像素确定性样本，且中文字体 `font_ready=1`。
- guard绑定 `run_raylib_host`、physical sample→map→pause gate→submit顺序、production snapshot hash、post-Present capture。
- self-test逐一注入并命名拒绝：direct ViewModel assignment、TestAccess、logical action queue、pre-Present capture、fake font-ready、fake result=pass、bypassed Session progression。
- 手工/程序目检六图：中文、面板布局、低血+状态、出口、深渊警告、升级点数、F1完整诊断。

## Required GREEN

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage11c_hud_formal arpg_game
ctest --test-dir out/build/windows-msvc-debug -R "^stage11c\.hud_(formal|evidence)" --output-on-failure
git diff --check
```

## Commit and report

- Commit: `test: validate stage 11c hud in raylib`
- Write ignored `.superpowers/sdd/stage11c-task-9-report.md` with RED, scenario production path, hashes/fields, exact GREEN, image paths and inspection notes, commit, concerns.
- 不提交 `.superpowers/sdd`，但提交测试源码/脚本/guard；按现有项目策略决定正式PNG是build evidence还是tracked fixture，不擅自加入大型生成物。

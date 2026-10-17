# Stage 11-B Task 5 实现报告

## 状态

- 基线：`84f53348b9ba7a5940060863351b95ccc0f3cddc`。
- 功能提交：`33f544d9c9d0fe80a4ced240d52d91a5210254fe`。
- 提交标题：`feat: model pause and settings navigation`。
- 范围：仅纯暂停/设置状态机、平台无窗口测试、零分配压力和源码依赖门禁；未实现 Task 6 UI 或 Task 8 host integration。

## TDD 证据

### RED：接口缺失

先新增并接入 `pause_menu_state_tests.cpp`，未创建任何生产接口，运行：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
```

结果：exit `1`，MSVC `C1083`，无法打开 `pause_menu_state.hpp`。失败原因是 Task 5 接口缺失，属于有效 RED。

首次补接口后的测试构建还暴露测试夹具误用了 C++20 designated initializer；没有放宽 C++17 约束，只把测试输入改为 C++17 工厂函数。该次旧 `platform.units` 二进制的通过结果未计作 GREEN。

### GREEN

最小实现完成后，focused 验证：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.pause_menu_state_boundary)$" --output-on-failure
```

结果：新测试二进制构建成功，`2/2` CTest 通过；平台 suite `143 cases, 0 failures`，其中 Task 5 新增 `28` cases。

## 已实现契约

- `closed/root/settings/capture_binding/quit_confirm` 五状态与 `none/preview/apply/rollback/resume/quit` 六命令。
- closed 打开时严格阻断 recovery、death、inventory、passive 与 pending-save 上下文；正常 Esc 只打开 root，同帧其他输入不继续解释。
- root 固定 3 行、settings 固定 16 行、quit confirm 固定 2 行，均支持 wrap navigation；Enter 与 activate 等价。
- Volume 以 5 调整并 clamp 到 `0..100`，边界 no-op 不 preview；Window Mode/VSync 仅实际 toggle 时返回 preview。
- 十个绑定行严格映射 `SettingAction` 枚举顺序；有效键 assign/swap 后回 settings 且不 preview，V/非法键保持捕获并使用静态错误消息。
- Esc、focus_lost、window unfocused 三条捕获取消路径不改 draft。
- Reset Defaults 替换 draft 但保留当前 revision，并返回 preview。
- Apply 调用 settings validation，拒绝 committed revision 最大值，以 `committed + 1` 生成 draft revision；不发布 committed、不关闭 settings，并保留 host 提供的 retry message。
- Cancel/Settings Esc 恢复 committed、清 message、回 root 并返回 rollback。
- quit confirm 默认 Back；仅 row 0 确认返回 quit，Back/Esc 回 root。

## 测试与边界

Task 5 的 28 cases 覆盖：所有固定行与 wrap navigation、所有命令、上下文优先级、pending save、同帧打开 gate、volume clamp/no-op、window/vsync preview、10 个绑定 action、V/越界 key、swap、三种 capture cancel、defaults revision、Apply validation/overflow/retry、两种 rollback、quit default/back/quit，以及 1000 次状态机调用零堆分配。

`platform.pause_menu_state_boundary` 同时用 mutation self-check 与真实文件扫描拒绝 raylib、SettingsStore、Combat、Dungeon、filesystem 和常见文件 I/O 依赖。

最终相关门禁：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.pause_menu_state_boundary|platform.host_input_source|platform.module_boundary|settings.units|stage11.death_evidence.mutation_self_test)$" --output-on-failure
```

结果：platform/game 构建成功；平台 executable `143 cases, 0 failures`；相关 CTest `6/6` 通过、`0` 失败。

## 残余风险与停止边界

- Task 5 只建模状态与命令；暂停冻结、同帧玩法输入实际丢弃、preview/apply I/O 编排由明确后续 Task 8 host integration 完成。
- 静态错误消息当前为英文内部文本；Task 6 view 可直接显示或按既定产品文案调整，但不得改成动态所有权。
- `PauseMenuState` 的 committed/draft 需要 host 用已加载或默认 settings 初始化；本任务按范围未接 host。
- 未开始 UI、renderer、window settings、持久化或完整 HUD。

## Review Minor 修复追加记录

- 修复提交：`60a9188f5e310cd8fca0813df53e05f769bbc7eb`。
- 提交标题：`test: strengthen pause state boundaries`。
- 范围：仅修改 `pause_menu_state_boundary_test.cmake` 与 `pause_menu_state_tests.cpp`；生产代码未变，未开始 Task 6。

### Boundary guard RED/GREEN

先在旧全文子串 guard 加入合法 negative control，再运行：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^platform.pause_menu_state_boundary$" --output-on-failure
```

结果：`0/1`，明确错误为合法注释 `// raylib combat dungeon file...` 被普通 `raylib` 子串拒绝，复现 review 指出的误报。

随后将 guard 改为两层小型结构门禁：

- 只把行首可带空白的真实 `#include` 作为 include，白名单精确限制为自身头、settings types 与所需 C++ 标准头；普通 `// #include` 不计。
- API 门禁只匹配明确 raylib device/window 调用、SettingsStore/settings_store token、combat/dungeon/filesystem namespace、C++/C/Windows 文件 I/O 类型与调用。
- 7 类 forbidden include、12 类 forbidden API mutation 必须命中；11 个包含普通架构词和相似子串的 clean controls 必须通过。

focused 结果：`platform.units` 与 `platform.pause_menu_state_boundary` 为 `2/2`，平台 suite 仍为 `143 cases, 0 failures`。

### 零分配覆盖扩展

同一 no-allocation case 改为 64 轮固定状态重置，固定数组/循环覆盖：

- 5 个 closed owner 阻断与正常打开；
- root navigation、resume、settings、quit；
- Volume/Window/VSync 三类 preview、defaults、Apply valid/invalid/overflow/retry、cancel；
- 10 个 action 的有效捕获，以及 swap、V、越界 key、失焦、Esc；
- quit yes/back。

测试自身不使用动态容器，只在所有 transition families 完成后统一比较 allocation counter。

### 最终 GREEN

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.pause_menu_state_boundary|platform.host_input_source|platform.module_boundary|settings.units|stage11.death_evidence.mutation_self_test)$" --output-on-failure
git diff --check
```

结果：platform/game 构建成功；平台 `143 cases, 0 failures`；相关 CTest `6/6`、`0` 失败；diff check 无错误，仅显示既有 LF/CRLF 转换提示。

## Syntax-aware boundary 复审修复记录

- 修复提交：`248d502ff800450b243702f8bb91c245bf11bd68`。
- 提交标题：`test: share syntax-aware pause boundary guard`。
- 范围：仅共享 CMake C++ source sanitizer、core boundary 接线与 pause boundary 门禁；生产 pause 代码未变，未开始 Task 6。

### RED

先给旧 pause 逐行 regex guard 加入复杂 directive mutation：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^platform.pause_menu_state_boundary$" --output-on-failure
```

结果：`0/1`，明确漏检 `#/**/include <raylib.h>`。旧实现也会对 `// IsKeyPressed(1)`、普通/Raw string 内的 `SettingsStore`、`FILE`、`fopen(...)` 产生 API-shaped 误报。

### 共享 sanitizer

- 将 `core_boundary_test.cmake` 已验证的六状态 scanner 机械抽取为 `cpp_source_lexer.cmake::arpg_sanitize_cpp_source()`；core 与 pause 共同调用唯一实现，不再存在 pause 简化 lexer 与 core scanner 双实现。
- sanitizer 清空 line/block comment、ordinary/raw string 与 char，同时保留有效代码和 include/define quoted header；非 raw 状态按 translation phase 处理 LF/CRLF splice。
- `core_boundary_test.cmake` 原 exact self-tests 全部保留，仍覆盖 comment-separated directive、comment before header、continued directive/header、macro angle/quoted header、block/line comment、ordinary/raw string、char 与 splice-shaped tokens；另增加显式 CRLF continued directive/line-comment 对照。

### Pause boundary GREEN

- 所有 mutation/control/实际 hpp/cpp 都先 sanitize。
- 有效 preprocessor 只允许精确 `#pragma once` 与自身/settings/所需标准头白名单；其他 include、define、macro header、macro include 和其他 pragma 全部拒绝。
- API token guard 只扫描 sanitized code，不再扫描注释或普通/Raw string。
- 新增 API-shaped negatives：line/block comment、ordinary string、custom raw string 内的 `IsKeyPressed`、`SettingsStore`、`FILE`、`fopen`；block/raw include 也必须 clean。
- 新增 comment-separated、continued LF/CRLF、macro header forbidden includes，以及 LF/CRLF spliced API token positives。

### 最终验证

```powershell
ctest --test-dir out/build/windows-msvc-debug -L architecture --output-on-failure
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|settings.units|stage11.death_evidence.mutation_self_test)$" --output-on-failure
git diff --check
```

结果：architecture 标签 `23/23`、0 失败；platform/game 构建成功；平台 `143 cases, 0 failures`；platform/settings/death `3/3`、0 失败；diff check 无错误，仅有既有 LF/CRLF 转换提示。

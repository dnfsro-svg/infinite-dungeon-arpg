# Stage 11-B Task 4 实现报告

## 状态

- 基线：`a03bf2e`（Task 4 开始时 HEAD）。
- 功能提交：`bb80cf71f13ca63f7dd376ec8eb60c31dfdb2f28`。
- 提交标题：`feat: map configurable keys to frame input`。
- 范围：仅 stable-key/raylib 适配、单帧物理快照、纯逻辑映射、combat action submit、生产 host 临时默认绑定接线与平台测试；未实现 Task 5 的暂停/设置状态、持久化设置加载或动态 HUD。

## TDD 证据

### RED：接口与源文件缺失

先用 `host_input_tests.cpp` 替换旧 `combat_key_bindings_tests.cpp`，覆盖 45 键适配、单次采样、逻辑映射、R/N/V、零分配和真实 session submit，再运行：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
```

结果：exit `1`，MSVC `C1083`，无法打开 `host_input.hpp`。这是新接口缺失导致的有效 RED。

同时先加入 `platform.host_input_source` 门禁并运行：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^platform.host_input_source$" --output-on-failure
```

结果：`0/1`，门禁因 `host_input.cpp` 不存在失败，确认生产采样模块尚未实现。

### GREEN

实现四个新文件并替换 host 私有采样后，提交前 fresh 验证：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(core.units|combat.units|platform.units|platform.input_latency_source|platform.host_input_source|platform.module_boundary|settings.units)$" --output-on-failure
```

结果：构建 exit `0`，`arpg_game.exe` 已链接；平台 executable `114 cases, 0 failures`；相关 CTest `7/7` 通过、`0` 失败。

## 已实现契约

- `stable_key_raylib.*` 提供 45 个 `StableKey` 到 raylib code 的完整双向表与稳定、唯一显示标签；非法稳定键返回 `KEY_NULL` / `Unknown`，未知 raylib code 返回空 optional。
- `sample_physical_keys()` 对 45 个稳定键各调用一次 pressed、一次 down；`Esc/Enter/F1/F12` 各采样一次 pressed，鼠标与焦点各采样一次。
- `V` 只由 `snapshot.pressed[StableKey::v]` 派生；`R`、`N` 也只消费同一稳定数组，不在 host 二次轮询。
- `map_host_frame_input()` 只读 `SettingsData + PhysicalKeySnapshot`：移动使用 down，动作/交互/背包/星盘使用 pressed；无 raylib 设备调用、无堆分配。
- 有效绑定占用 `R` 时，绑定玩法输入优先，`keys.reset == false`；默认未占用时保留 legacy room reset。
- `submit_frame_actions()` 固定把三项逻辑 action 映射到 `light/jump/launcher` 并提交到真实 `DungeonSession`。
- `raylib_host.cpp` 每 presented frame 先生成唯一 `PhysicalKeySnapshot`，再用一次 `map_host_frame_input()`；Task 4 临时 binding authority 为启动时构造的 `const default_settings()`。
- 删除 `combat_key_bindings.hpp` 与旧测试，生产 host 不再直接采样 `W/A/S/D/J/K/L/E/I/P/R/N/V`。

## 测试覆盖

平台 suite 新增 12 cases，替换旧 2 cases，平台总数从 104 增至 114：

1. 45 键完整 round-trip、raylib code 唯一、标签唯一且黄金值稳定；
2. 非法稳定键/未知 raylib key 拒绝；
3. 注入式 sampler 证明每个稳定键 pressed/down 各一次，V/R/N 无二次采样；
4. 默认 movement/combat/interact/inventory/passive 映射；
5. movement↔combat、interact↔inventory、inventory↔passive 的 occupied-key swap；
6. movement 只看 down、动作只看 pressed；
7. Esc/Enter/F1/F12/V/mouse/focus 全局映射不依赖玩法绑定；
8. R 绑定抑制 legacy reset；
9. N recovery 从稳定键 pressed 边沿派生；
10. 合法 swap 后一个物理键只产生一个 combat action；
11. mapper 1000 次调用 allocation counter 不增长；
12. 三个 submit action 均由真实 DungeonSession 行为验证。

`platform.host_input_source` 另锁定生产采样调用点数量、禁止 V/R/N 直接采样、禁止 host 设备轮询与玩法键常量、要求 host 存在 sample→map 链。

## 源码与边界门禁

```powershell
rg -n "KEY_[WASDJKLEIP]" src/platform/raylib/raylib_host.cpp src/platform/raylib/hud_renderer.cpp
rg -n "KEY_(W|A|S|D|J|K|L|E|I|P|R|N|V)([^A-Z0-9_]|$)|platform_key_(pressed|down)|IsKey(Pressed|Down)" src/platform/raylib/raylib_host.cpp
git diff --cached --check
```

结果：前两项无匹配；cached diff check 通过。

## 残余风险与后续边界

- `hud_renderer.cpp:106` 和 `:109` 仍有 Stage 8/11-A 的 `WASD/J/K/L/E/I/P/R` 文本提示。它不是 `KEY_*` 设备硬编码，且动态 HUD 明确属于本计划 Task 9；本任务按 controller clarification 只记录，不跨卡修改。
- host 当前刻意使用 `default_settings()`，尚未消费磁盘加载或 committed settings；按 brief 由后续设置接线任务替换，不在 Task 4 提前实现。
- 本任务没有新增 pause、settings UI、settings I/O、固定步冻结或持久化行为。
- 未发现其他阻塞风险。

## 审查修复追加记录

- 修复提交：`ffea5e7d124293a30487f5f6c30e60616379acf4`。
- 提交标题：`fix: keep inventory input on frame snapshot`。
- 修复范围：2 个 Important（背包同帧输入、十动作穷举覆盖）与 1 个 Minor（source guard 精确/稳健性）；未开始 Task 5。

### RED：背包仍直接采样设备

先扩展测试期望的 `PhysicalKeySnapshot`、`PhysicalKeySource` 与 `HostFrameInput` 字段，并让 source guard 扫描 `inventory_renderer.cpp`，运行：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "^platform.host_input_source$" --output-on-failure
```

结果：平台目标 exit `1`，首批有效错误为 `PhysicalKeySource` 初始化字段不匹配，以及 `PhysicalKeySnapshot::mouse_right/mouse_wheel`、`HostFrameInput::mouse_left_pressed/mouse_right_pressed/mouse_wheel/control_down` 缺失；source guard `0/1`，明确报错 `inventory renderer must consume mapped frame input`。

### 修复内容

- 生产采样层每帧各采样一次左键 pressed、右键 pressed、鼠标位置与滚轮；Ctrl 继续来自 45 键循环中已经取得的 left/right control down 状态。
- mapper 把左右鼠标、位置、滚轮和合并后的 `control_down` 发布到 `HostFrameInput`。
- `InventoryRenderer::process_input` 接收同一帧的 `HostFrameInput`；删除背包文件内的 `IsMouseButtonPressed`、`GetMousePosition`、`GetMouseWheelMove` 与 `platform_key_down` 调用。`GetTime`、`GetScreenWidth/Height` 保留，它们不是物理输入边沿。
- 新增表驱动 case，逐项激活全部十个 `SettingAction` 的当前 binding：四方向锁定轴和符号，三 combat/交互/背包/星盘锁定唯一对应输出。原 swap、R ownership 和 no-double-action cases 保留。
- source guard 在匹配前剥离块注释、行注释与普通字符串；host 的 `sample_physical_keys()` 和 `map_host_frame_input()` 都要求精确一次；删除 `new/malloc` 原始文本正则，mapper 零分配继续由 1000 次 runtime allocation probe 证明。
- source guard 同时要求 host 把 `frame_input` 传入 inventory，并禁止 inventory renderer 的四类设备输入调用。

### 最终 GREEN

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_game
out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.host_input_source|platform.input_latency_source|settings.units|platform.module_boundary)$" --output-on-failure
git diff --check
git diff --cached --check
```

结果：`arpg_game.exe` 链接成功；平台 executable `115 cases, 0 failures`；指定 CTest `5/5` 通过、`0` 失败；工作区与 cached diff check 均通过。修复提交仅包含 5 个 raylib 生产文件与 3 个平台测试/门禁文件。

## 复审 Minor 修复追加记录

- 修复提交：`3fc9450d02a8f2b6999c5236e6b14bb81e9d544a`。
- 提交标题：`test: harden frame input source guard`。
- 修复范围：仅强化 `host_input_source_test.cmake` 的 C++ 非代码屏蔽、direct-input 禁止表与 guard self-test；未修改生产代码，未开始 Task 5。

### RED：字符串注释标记吞掉真实调用

先保留旧 `strip_non_code`，加入合成源码：普通字符串中的 `//` 后同一行放真实 `IsKeyDown`，字符串化 `/*` 与 `*/` 之间放真实 `platform_key_pressed`，并加入字符/raw string 和注释内伪调用。运行：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^platform.host_input_source$" --output-on-failure
```

结果：`0/1`，明确失败为 `guard self-test lost real call after // in string`；旧实现先剥注释，错误地把字符串内 `//` 当成行注释并吞掉后续真实代码。

### GREEN：逐字符两阶段屏蔽与统一禁止表

- `mask_cpp_literals` 逐字符处理普通字符串、反斜杠转义、字符字面量与最多 16 字符自定义 delimiter 的 raw string；非代码替换为空格但保留换行。普通字符串遇到未转义换行立即结束屏蔽，不能跨行吞后续真实源码。
- `strip_cpp_comments` 只在 literal 已屏蔽后处理 line/block comments，字符串内 `//`、`/*`、`*/` 不再影响代码。
- self-test 精确要求 6 个真实调用保留，并要求字符串、raw string、行注释、块注释中的 6 类伪调用全部消失；其中包含未闭合单行字面量后的下一行真实 `IsKeyUp`，锁定“不跨换行”。
- host 与 inventory 共用 `DIRECT_INPUT_PATTERN`，覆盖 `platform_key_pressed/down`、`IsKeyPressed/PressedRepeat/Down/Released/Up`、`GetKeyPressed/GetCharPressed`、四类 mouse button 查询、`GetMouseX/Y/Position/Delta/WheelMove/WheelMoveV`。
- `host_input.cpp` 同时按单项黄金数量和统一禁止表总数 `6` 锁定批准采样点；新增任何其他 direct-input 查询都会失败。host/inventory 统一要求零 direct-input 查询。

最终验证：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.host_input_source|platform.input_latency_source|settings.units|platform.module_boundary)$" --output-on-failure
git diff --check
git diff --cached --check
```

结果：构建成功；指定 CTest `5/5` 通过、`0` 失败，其中强化后的 source guard 自身约 `2.69s`；工作区与 cached diff check 通过。提交仅包含一个平台 guard 文件。

## 最终复审 line-splicing 修复记录

- 修复提交：`ec295798fd59a0652ae632a37ff5fabd183632c9`。
- 提交标题：`test: cover spliced input guard lines`。
- 修复范围：仅补齐 C++ translation phase line splicing 与 focus direct-input 门禁；未修改生产代码，未开始 Task 5。

### RED：未执行 phase-2 拼行

先在旧 parser 上增加 LF 与显式 CRLF 合成 self-test。两组都包含：反斜杠续行的 string/char/raw 内伪 direct call、反斜杠续行的 `//` 注释内伪调用，以及把真实函数名拆在续行两侧的调用。同时把 `IsWindowFocused` 加入统一 `DIRECT_INPUT_PATTERN`，但保留旧批准总数。

运行：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^platform.host_input_source$" --output-on-failure
```

结果：`0/1`，明确报错 `guard LF splice lost a real direct-input call`；旧 parser 无法识别 `IsKey\` + LF + `Released(...)` 经 translation phase 应形成的真实调用。

### GREEN：LF/CRLF phase-2 拼行与 focus 门禁

- 新增 `splice_cpp_lines`，在 literal/comment 处理之前逐字符执行 translation phase 2：删除 backslash+LF 与 backslash+CRLF，其他字符原样保留。
- 拼行发生在所有 token 识别之前，因此 string、character、raw string 与 line comment 使用相同的 phase-2 结果；CRLF 的 CR/LF 一次共同移除，不会把残留 LF 误判为 literal/comment 结束。
- LF self-test 要求真实 `IsKeyReleased`、`GetMouseX` 被发现，并排除 string/char/raw/续行注释中的 `IsKeyDown`、`IsKeyUp`、`GetCharPressed`、`platform_key_down`。
- CRLF 使用 `string(ASCII 13)` 构造真实 CRLF，要求真实 `IsMouseButtonReleased`、`GetMouseY` 被发现，并排除四类续行伪调用。
- `IsWindowFocused` 纳入 host/inventory 共用的 direct-input 禁止表；`host_input.cpp` 仍单独要求 focus 调用恰好一次，批准 direct-input 总数由 `6` 更新为 `7`。host/inventory 出现新的 focus 直采会被拒绝。

最终验证：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.host_input_source|platform.input_latency_source|settings.units|platform.module_boundary)$" --output-on-failure
git diff --check
git diff --cached --check
```

结果：构建成功；指定 CTest `5/5`、`0` 失败，其中 source guard 约 `3.26s`；工作区与 cached diff check 通过。提交仅包含一个平台 guard 文件。

## 最终复审 raw literal 拼行语义修复记录

- 修复提交：`35eaf6b1a7ef71def04213f0d9166fe1c332e2df`。
- 提交标题：`test: preserve raw input guard literals`。
- 修复范围：仅修正 `host_input_source_test.cmake` 的 raw string line-splicing 语义并补充合成测试；未修改生产代码，未开始 Task 5。

### RED：raw 内容中的伪 terminator 被错误拼合

先保留旧的全局 `splice_cpp_lines`，加入有区分力的 LF 与 CRLF 合成源码。raw 内容包含拆开的伪 terminator：`)tag\\`（或 `)crlf\\`）后接物理换行和 `"`；伪 direct-input 调用位于该伪 terminator 之后，真实调用位于真正 custom-delimiter terminator 之后。

运行：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^platform.host_input_source$" --output-on-failure
```

结果：`0/1`，明确报错 `guard raw LF real calls: expected 1 source matches, found 2`。旧实现删除 raw 内容内的 backslash+LF，把拆开的伪 terminator 拼成真实 terminator，导致 raw 内的伪 `IsKeyDown` 被当成代码。

### GREEN：保留 raw 的物理内容，只在 raw 外拼行

- 提取共享的 `find_cpp_raw_literal_end`，从 `R"` 位置按最多 16 字符的 custom delimiter 查找物理源码中的真实 `)delimiter"`；扫描走到 `R` 时即可识别 `u8R`、`LR`、`uR`、`UR` 等常见 encoding prefix。
- `splice_cpp_lines` 在遇到有效 raw literal 时，从开头到真实 terminator 原样复制并整体跳过，raw 内的 backslash+LF/CRLF 不参与拼行；raw 外仍删除 backslash+LF 与 backslash+CRLF。
- `mask_cpp_literals` 复用同一 terminator helper，避免 splice 与 mask 对 raw 边界的解释分叉。
- LF 测试锁定 `u8R"tag(...)tag"` 内的拆分伪 terminator与伪 `IsKeyDown` 仍被 mask，真实 terminator 后的 `IsKeyReleased` 必须检出；CRLF 测试以 `LR"crlf(...)crlf"` 锁定同样行为，分别检查伪 `GetMousePosition` 与真实 `GetMouseDelta`。
- 调试中发现 CMake regex 的转义语义会把 delimiter 中的普通 `t`/`r` 误判为禁用字符；改为逐字符 `STREQUAL` 判断空白、`)` 和 backslash。随后只在当前字符确为 `R` 且下一字符为 `"` 时调用 helper，source guard 时间由约 `5.39s` 降至约 `3.38s`。

最终验证：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.host_input_source|platform.input_latency_source|settings.units|platform.module_boundary)$" --output-on-failure
git diff --check
```

结果：配置和构建成功（`ninja: no work to do`）；指定 CTest `5/5`、`0` 失败，source guard `3.36s`，总计 `7.88s`；`git diff --check` 无错误，仅有工作区 LF/CRLF 转换提示。提交仅包含一个平台 guard 文件。

## 最终复审 lexer context 修复记录

- 修复提交：`68fbab80396a7994e1db1afbf7d4450512e216d5`。
- 提交标题：`test: respect lexer context in input guard`。
- 修复范围：仅重构 `host_input_source_test.cmake` 的 phase-2 scanner 并增加上下文 self-test；未修改生产代码，未开始 Task 5。

### RED：注释中的 raw-lookalike 错误进入 raw

先保留旧 scanner，加入 LF 与显式 CRLF 合成源码。续行 `//` 注释中放入 `R"tag(...)tag"` raw-lookalike，terminator 后仍放伪 direct-input 调用；同时在 block comment、ordinary string 与 char 中加入带 backslash+换行的 raw-lookalike，并用 normal-code raw 对照验证物理换行必须保留。

运行：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^platform.host_input_source$" --output-on-failure
```

结果：`0/1`，首个具体错误为 `guard LF splice treated comment text raw-lookalike as raw`。旧实现不跟踪词法上下文，只要看到 `R"` 就跳到 raw terminator，错误保留了 line comment 内本应被 phase 2 删除的 backslash+LF。

### GREEN：六状态单遍 phase-2 scanner

- `splice_cpp_lines` 改为 `normal`、`line_comment`、`block_comment`、`ordinary_string`、`char`、`raw` 六状态单遍 scanner。
- 只有 `normal` 状态识别 raw token、`//`、`/*`、普通字符串和字符字面量；注释或普通字面量中的 `R"` 不再改变 scanner 状态。
- 除 raw 外，各状态统一删除 backslash+LF 与 backslash+CRLF；line comment 删除续行后保持 comment 状态，遇到普通 LF/CRLF 才回到 normal。
- block comment 只由 `*/` 结束；ordinary string/char 正确跳过反斜杠转义，由未转义的对应引号或普通换行结束。
- raw 状态把从 `R` 到真实 custom-delimiter terminator 的物理内容原样复制，保留 raw 内的 backslash+LF/CRLF。
- LF/CRLF self-test 均验证 line comment terminator 后的伪调用仍属于注释，block/string/char raw-lookalike 内的续行被删除，normal-code raw 的物理续行保留，并且注释后的真实调用仍被检出。

最终验证：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.host_input_source|platform.input_latency_source|settings.units|platform.module_boundary)$" --output-on-failure
git diff --check
git diff --cached --check
```

结果：配置和构建成功（`ninja: no work to do`）；指定 CTest `5/5`、`0` 失败，source guard `9.68s`，总计 `14.21s`；工作区与 cached diff check 无错误，仅有 LF/CRLF 转换提示。提交仅包含一个平台 guard 文件。

## 编译期输入权限边界架构替换记录

- 替换提交：`b0c51f5cdee4ad40291d3b58bd9b0b0048295e47`。
- 提交标题：`test: enforce frame input authority at compile time`。
- 范围：用编译期 poison 替换自制 C++ lexer；只影响 Task 4 的 raylib consumer 输入权限与测试门禁，未开始 Task 5。

### RED：旧方案没有编译期权限边界

先把 `host_input_source_test.cmake` 改为新的结构要求，但尚未添加 poison：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^platform.host_input_source$" --output-on-failure
```

结果：`0/1`，明确报错 `direct input poison header is required`。

随后加入配置期正向 sentinel/负向调用编译探针，仍保留旧生产结构并重新配置：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
```

结果：配置失败，明确报错 `direct input poison must expose a compilable active sentinel`，证明旧 lexer 门禁不满足新的编译期架构约束。

初版 poison 使用 deleted 函数模板后，键盘、鼠标、focus、platform wrapper 四类调用都按预期编译失败，但 `ADDRESS` 探针意外编译成功。最终改为 `Blocked` sentinel 对象，同时删除模板 `operator()` 与 `operator&`，使直接调用和常见 `&IsKeyDown` 取地址都在编译期失败。

### GREEN：deleted sentinel + consumer 末尾锚点

- 新增 `direct_input_poison.hpp`，将 20 个统一 API token 映射到同一个 `Blocked` sentinel：两个 platform wrapper、7 个键盘查询、4 个 mouse-button 查询、6 个鼠标位置/滚轮查询与 `IsWindowFocused`。
- `Blocked::operator()` 和 `Blocked::operator&` 均为 deleted；直接调用与常见取地址不能编译。头文件暴露 `inline constexpr bool active = true`。
- `raylib_host.cpp` 与 `inventory_renderer.cpp` 在全部正常 include 之后包含 poison，并各自用 `static_assert` 锁定 active sentinel；`host_input.cpp` 不包含 poison，继续作为唯一物理采样权威。
- 配置期编译自测先正向证明 header/sentinel 可编译，再分别要求键盘、鼠标、focus、platform wrapper 与 address 五个负向 probe 编译失败。配置日志确认调用命中 deleted `operator()`，取地址命中 deleted `operator&`。
- `host_input_source_test.cmake` 删除 raw/string/comment lexer 与全部 token 边界 self-test，仅保留固定结构门禁：两个 consumer 的末尾 include + sentinel、authority 不含 poison、host 的 sample/map 各一次、poison header 的完整 20 项精确宏表。
- 该替换从 guard 文件移除 626 行复杂 parser/self-test；source guard 从上一版约 `9.68s` 降至 `0.02s`。

最终验证：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.host_input_source|platform.input_latency_source|settings.units|platform.module_boundary)$" --output-on-failure
git diff --check
git diff --cached --check
```

结果：配置成功；active probe 成功，五个负编译 probe 均按预期失败；`arpg_game` 与 `arpg_platform_tests` 构建成功；指定 CTest `5/5`、`0` 失败，总计 `4.48s`；工作区与 cached diff check 无错误，仅有 LF/CRLF 转换提示。提交包含 5 个明确文件，未开始 Task 5。

## Important 复审：独立 poison probes 与锚定结构门禁

- 修复提交：`84f53348b9ba7a5940060863351b95ccc0f3cddc`。
- 提交标题：`test: make input poison probes independent`。
- 范围：只强化 Task 4 的配置期编译探针与轻量结构门禁；未修改生产代码，未恢复 lexer，未开始 Task 5。

### RED：负探针缺少独立 control，非锚定模式接受注释

先加入 comment-only synthetic structure，继续使用旧的非锚定模式：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R "^platform.host_input_source$" --output-on-failure
```

结果：`0/1`，首个错误为 `commented poison include lines: expected 0 matches, found 1`，证明普通 `// #include` 可以满足旧 guard。

随后为每个旧负探针加入“不 include poison”的正向 control，但暂不提供目标 API 声明：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
```

结果：配置失败，首项报 `direct input control probe must compile for KEY`。这证明旧负探针的失败可能只来自未声明标识符，不能独立证明 poison 生效。

### GREEN：同声明/同表达式的成对编译实验

- KEY、MOUSE、FOCUS、PLATFORM_WRAPPER、ADDRESS 五项各自获得匹配名称、参数和返回类型的可编译声明；platform wrapper 使用 `arpg::platform` 声明和 `using` 引入，address 使用真实 `bool IsKeyDown(int)` 声明。
- 每项生成唯一 `ARPG_DIRECT_INPUT_CONTROL_<LABEL>_COMPILES` 与 `ARPG_DIRECT_INPUT_POISON_<LABEL>_COMPILES` 结果，并在每次检查前 `unset(... CACHE)`。
- control 与负探针复用完全相同的声明和表达式；唯一差异是负探针在声明后 include poison。最终五个 control 全部成功，五个 poison probe 全部因 deleted sentinel 失败。

### GREEN：行首锚定的小型结构 guard

- consumer include 只接受行首可带空白的独立 `#include "direct_input_poison.hpp"` 预处理行；其后的 active `static_assert` 必须从语句行首开始，且 poison 仍必须是最后一个真实 include。
- host 的 snapshot/map 分别锚定实际 `const PhysicalKeySnapshot physical_keys = sample_physical_keys()` 与 `HostFrameInput frame_input = map_host_frame_input(` 赋值行。
- 20 个 API 宏逐项要求从行首 `#define` 开始且独占预处理行；active sentinel 同样锚定语句起始。
- lightweight synthetic self-test 对 include、static_assert、sample、map、macro 五类结构逐项证明：普通 `//` 注释计数为 `0`，对应真实代码行计数为 `1`。
- 调试时发现 CMake `MATCHALL` 的单个匹配若包含语句分号，`list(LENGTH)` 会把分号解释为列表分隔符；sample 与 sentinel 模式保留行首和完整目标表达式锚定，但不把分号纳入匹配结果，消除错误双计数。

最终验证：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "^(platform.units|platform.host_input_source|platform.input_latency_source|settings.units|platform.module_boundary)$" --output-on-failure
git diff --check
git diff --cached --check
```

结果：配置期五个 control 均成功、五个 poison probe 均按预期失败；game/platform 构建成功；指定 CTest `5/5`、`0` 失败，source guard `0.03s`，总计 `4.54s`；工作区与 cached diff check 无错误，仅有 LF/CRLF 转换提示。提交仅包含两个平台测试/门禁文件。

# Task 7 Report: Stage 0 Merge and Completion Gate

## 集成结果

- 工作树：`E:\game\.worktrees\m00-foundation`
- 分支：`milestone/m00-foundation`
- 起始 SHA：`cc4b2da224388d8f6d73cdafa425a78c46e18018`
- 最终 HEAD：`b4f4f89d2ca90592852b0fe6d14b4ab66dd162a9`
- `main` 保持：`f8bbe5ec21e5f36f88dd18924d5962d8f2d26817`
- 未创建 Stage 1 分支，未把 milestone 合并到 `main`。
- 两次合并均无冲突，也没有产生任何集成修复提交。
- 最终审查后新增六个独立测试加固提交；没有修改 `main`。

## 最终审查加固：测试计数与 Core/raylib 边界

### Important 1：Stage 0 测试用例数门禁

RED：临时把 `kExpectedCaseCount` 设为 `23`，运行：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

- 退出状态：非零（CTest `8`）。
- 关键输出：`[FAIL] stage 0 case count: actual 22 != expected 23`。
- 汇总：`22 cases, 1 failures`。

GREEN：用 `apply_patch` 把期望值改回固定的 `22` 后：

- `windows-msvc-core-debug`：CTest `2/2` 通过。
- 直接运行 `arpg_core_tests.exe`：`22 cases, 0 failures`。
- 独立提交：`661e3be1e74c5f01dd3bba2a453ca3a7a3226cc4 test: enforce stage 0 core case count`。

### Important 2：递归 Core/raylib 架构边界

旧守卫假绿复现：临时建立下列依赖链：

```text
arpg_core PRIVATE -> arpg_boundary_probe_mid INTERFACE
                  -> arpg_boundary_probe_leaf INTERFACE -> raylib
```

- `windows-msvc-debug -Fresh` 错误放行，CTest 仍为 `2/2`；证明旧实现只检查直接属性字符串，不能遍历 target 闭包。
- 假绿探针随后用 `apply_patch` 完整撤销。

修复内容：

- 配置期递归遍历 `LINK_LIBRARIES` 与 `INTERFACE_LINK_LIBRARIES`，带 visited 集合防止循环。
- 解析普通 target、namespaced target、`LINK_ONLY` 与逗号生成器表达式中的候选 target。
- 检测 `raylib`、`rlgl`、`raymath`、`raylib-cpp`、绝对 `.lib`/`.a` 路径和 `_deps/raylib` include 路径。
- 配置期永久正负自检覆盖 `m00-raylib-host` 负例、断开的 `leaf -> mid` target 闭包和递归 include 闭包；不增加 CTest。
- 源码边界扫描新增 `.hh/.hxx/.ipp/.tpp/.cxx/.ixx/.cppm`，只匹配真实 `#include` 行；永久正负正则自检覆盖注释与字符串假阳性。

修复后 RED：再次临时链接 `arpg_core PRIVATE arpg_boundary_probe_mid`，运行 `windows-msvc-debug -Fresh`：

- 配置按预期非零退出。
- `FATAL_ERROR` 路径明确显示 `arpg_core -> arpg_boundary_probe_mid -> arpg_boundary_probe_leaf`，并报告其 `INTERFACE_LINK_LIBRARIES` 中的 `raylib`。
- 临时生产链接随后用 `apply_patch` 撤销。

修复后 GREEN：

- Debug Fresh：CTest `2/2`，`0` failures。
- Release Fresh：CTest `2/2`，`0` failures。
- Debug 与 Release 直接测试程序均输出 `22 cases, 0 failures`。
- 两个配置的 `ctest -N` 均准确列出 `2` 项测试。
- CMake 语法修复次数：`0`。
- 独立提交：`49717eabf61086e8bfda1853c7f4c6122daed7dd test: close core raylib boundary gaps`。

### Important 2 复审：路径、imported target 与源码词法边缘

修复前分别完成三个 RED：

1. 临时加入 `E:/raylib-host/src` 负例断言，Core Fresh 配置非零退出并报告：
   `RED A: boundary detector falsely matched E:/raylib-host/src`。
2. 临时建立 neutral `UNKNOWN IMPORTED` target，设置
   `IMPORTED_LOCATION=E:/vendor/raylib.lib`，再经 alias、leaf、mid 递归；
   检测函数错误返回 false，配置由 RED 断言非零退出并报告：
   `RED B: detector missed neutral imported target at raylib.lib`。
3. 临时 `.hxx` 同时包含跨行块注释与自定义 delimiter raw string 中的独立
   `#include <raylib.h>`；`architecture.core_no_raylib` 错误失败。

根因与修复：

- 分离 link 与 include 检测：link 只检查 forbidden target/token 和真实
  `.lib`/`.a` 文件名；include 只检查固定依赖 `_deps/raylib...`。
- `E:/raylib-host/src` 与 `E:/game/.worktrees/m00-raylib-host/src` 成为永久负例。
- alias 解析后检查 canonical target 名；imported target 检查
  `IMPORTED_LOCATION`、`IMPORTED_IMPLIB` 及全部 `IMPORTED_CONFIGURATIONS`
  变体，并保留 visited 防环。
- 永久配置自测新增 neutral imported alias 闭包、配置化 imported location、
  forbidden canonical target 名与循环依赖。
- 源码扫描改为轻量逐字符状态机，区分代码、行注释、块注释、普通字符串、
  字符字面量、quoted include 与任意合法 delimiter 的 raw string；仅将代码态
  的真实预处理 include 行交给 forbidden header 正则。
- 永久源码自测覆盖真实 angle/quoted include 正例，以及行/块注释、普通字符串、
  char、自定义 raw delimiter、相似头文件等负例；扩展后缀列表保持不变。

实现验证：

- 尝试 1 首个错误为
  `Recursive boundary self-test missed leaf -> mid -> raylib`；根因为 link/include
  matcher 调用位置互换，不是 CMake 语法错误。
- 仅交换两处调用后尝试 2 Core Fresh：CTest `2/2` 通过。
- 修复后临时 `arpg_core -> imported mid` 配置正确 FATAL，路径显示 alias 闭包，
  并明确报告 `IMPORTED_LOCATION contains 'raylib.lib' in 'E:/vendor/raylib.lib'`。
- 原 block comment/raw string 临时 `.hxx`：architecture CTest 通过。
- 真实 `#include <raylib.h>` 临时 `.hxx`：architecture CTest 按预期失败。
- 所有临时文件与生产链接均用 `apply_patch` 撤销，残留数量为 `0`。
- CMake 语法修复次数：`0`；功能实现尝试：`2/3`。
- 独立提交：`04d39198cb248d2e9715913f2b2b438e74877805 test: harden boundary detector edge cases`。

该提交后的完整 Task 7 复验：

- Debug Fresh 与 Release Fresh 均 CTest `2/2`、`0` failures。
- 两份 `arpg_core_tests.exe` 均输出 `22 cases, 0 failures`。
- 两份 `ctest -N` 均只列出 `core.units` 与
  `architecture.core_no_raylib`，`Total Tests: 2`。
- Debug/Release 缓存均为 MSVC 14.44、正确 build type、
  `BUILD_SHARED_LIBS=OFF`；静态 `arpg_core.lib`、`raylib.lib` 与
  `arpg_game.exe` 均存在，两个 `bin` 目录 DLL 数量均为 `0`。
- `/out/` ignore 核验继续命中 `.gitignore:2:/out/`。

### Important 2 最后一轮：phase-2、legacy link 与常见变体

所有新自测先调用现有生产函数并完成修前 RED。

源码词法脚本修前一次汇总：

- missed：continued directive token、continued header、macro angle header、
  macro quoted header。
- false-positive：continued line comment。
- `#include/**/<raylib.h>` 正例已由既有注释清洗正确识别。

Core Fresh 配置期修前一次汇总 10 项漏检：

- link：`raylib_static`、`E:/vendor/raylibd.lib`、`-lraylib`。
- include：`E:/vendor/raylib/include`、`E:/vendor/raylib-cpp/include`。
- imported：legacy interface、configured dependent、configured objects。
- target 属性：`/DEFAULTLIB:raylib.lib` 与
  `RL_HEADER=<raylib.h>` compile definition。

定向修复：

- 在除 `RAW_STRING` 外的所有词法状态统一执行 translation phase 2，跳过
  `\\\n` 与 `\\\r\n` 且不结算逻辑行；续行行注释保持在注释态。
- 清洗后的简单 `#define NAME <raylib.h>` 与 quoted raymath header 也视为
  forbidden，因此 `#include NAME` 不再形成绕过。
- imported 闭包的 generic 与所有配置变体新增
  `IMPORTED_LINK_INTERFACE_LIBRARIES`、
  `IMPORTED_LINK_DEPENDENT_LIBRARIES`、`IMPORTED_OBJECTS`；值同时做直接检测、
  target 提取与递归。
- target 属性新增 `LINK_OPTIONS`、`INTERFACE_LINK_OPTIONS`、
  `COMPILE_DEFINITIONS`、`INTERFACE_COMPILE_DEFINITIONS` 扫描。
- link 分类新增 `_static`/`-static`/`d` target 变体、debug/static 库 basename、
  `.obj/.o` 与 `-lraylib`；普通 `raylib-host` 父目录继续为负例。
- include 分类新增精确 `raylib/include` 与 `raylib-cpp/include` 路径组件，
  `_deps/raylib-*` 保持；两个 `raylib-host` 负例继续通过。
- 源码后缀新增 `.inc`、`.tcc`、`.mpp`。

实现与生产链验证：

- 功能尝试 `1/2` 即 GREEN；生产 lexer 直接运行退出 `0`，Core Fresh CTest
  `2/2`。CMake 语法修复次数为 `0`。
- 临时 `arpg_core -> legacy_mid -> leaf -> alias`：配置正确 FATAL，明确命中
  `IMPORTED_LINK_INTERFACE_LIBRARIES contains 'raylib' in 'raylib'`。
- 临时 `arpg_core -> defaultlib_probe`：配置正确 FATAL，明确命中
  `INTERFACE_LINK_OPTIONS` 中的 `/DEFAULTLIB:raylib.lib`。
- 临时 `arpg_core -> definition_probe`：配置正确 FATAL，明确命中
  `INTERFACE_COMPILE_DEFINITIONS` 中的 `RL_HEADER=<raylib.h>`。
- 三条临时生产链接均用 `apply_patch` 撤销；临时链接数与临时文件数均为 `0`。
- 独立提交：`6068bbaee5453a2fc0c64963791a8bf10a8085dd test: close remaining core boundary bypasses`。

该提交后的完整 Task 7 复验：

- Debug Fresh 与 Release Fresh 均 CTest `2/2`、`0` failures。
- 两份测试 EXE 均输出 `22 cases, 0 failures`；两份 `ctest -N` 均为
  `Total Tests: 2`。
- Debug/Release 缓存继续为 MSVC 14.44、正确 build type、
  `BUILD_SHARED_LIBS=OFF`。
- 两配置的静态 `arpg_core.lib`、`raylib.lib` 与 `arpg_game.exe` 均存在；
  两个 `bin` 目录 DLL 数量均为 `0`；`/out/` ignore 继续命中。

### Lexer 收口：splice 跨界 token 与 MSVC 小场景

修前永久生产函数自测 RED：

- 源码 lexer 汇总且仅汇总：
  `missed:block comment opener across splice` 与
  `missed:block comment closer across splice`。
- 自定义 delimiter raw string 中包含相同 `/\\\n*`、`*\\\n/` 字符序列的
  负例保持通过，没有 raw 内容假阳性。
- 配置期额外汇总 5 项漏检：quoted `/DEFAULTLIB:"raylib.lib"`、
  `raylib/src`、`raylib-cpp/src`、forced-include compile option 与
  `_deps/raylib` compile option。

最小通用修复：

- lexer 增加 phase-2 logical-next/pending-token 视图；在非 raw 状态寻找下一个
  逻辑字符时跨过 `\\\n`/`\\\r\n`，使续行两侧 `/`、`*` 重新参与
  `//`、`/*`、`*/` 与 raw opener 识别。
- raw string 内容继续使用物理字符和原始 delimiter closer，不删除其中的
  splice-shaped 序列；raw opener delimiter 建立前则按 phase 2 跳过续行。
- link tokenizer 新增单/双引号边界，因此 quoted DEFAULTLIB 正确命中。
- include 精确组件扩展到 `raylib/src` 与 `raylib-cpp/src`，既有
  `raylib-host` 负例保持。
- 闭包新增 `COMPILE_OPTIONS`/`INTERFACE_COMPILE_OPTIONS`，剥离 MSVC
  `/I`、`/FI`、`/D` 前缀后复用现有 link/header/include detector；永久
  neutral INTERFACE 探针覆盖 forced include 与 `_deps/raylib`。

验证与提交：

- 功能尝试 `1/2` 即 GREEN；CMake 语法修复次数 `0`。
- 生产 lexer 直接脚本退出 `0`，提交前 Core Fresh CTest `2/2`。
- 临时探针文件数量为 `0`。
- 独立提交：`2e789bf88dda5282a2bef04459d8305337f56c20 test: preserve tokens across spliced lines`。
- 提交后 Core Fresh、Debug Fresh、Release Fresh 均 CTest `2/2`。
- Core/Debug/Release 三份测试 EXE 均输出 `22 cases, 0 failures`；Debug 与
  Release 的 `ctest -N` 均为 `Total Tests: 2`。
- MSVC 14.44、build type、`BUILD_SHARED_LIBS=OFF`、全部静态库/EXE、
  两配置 DLL 数量 `0` 与 `/out/` ignore 再次通过。

### Lexer 最终收口：跨 splice 的 STRING/CHAR 逻辑转义

修前永久生产函数自测 RED：

- 普通字符串与字符字面量中，反斜杠后紧接物理续行，再接转义字符时，
  lexer 使用物理下一字符判断转义，可能提前结束字面量并把后续文本误判为代码。
- 新增 STRING 与 CHAR 两个永久负例后，生产脚本在修复前按预期失败。

最小修复：

- STRING/CHAR 状态统一通过 phase-2 logical-next 视图取得被转义字符；
  使用 `_arpg_logical_next_index + 1` 前进，跨过 `\\\n`/`\\\r\n` 后仍正确
  消费一个逻辑字符。
- raw string 保持物理字符语义，不受本修复影响。

验证与提交：

- 独立提交：`b4f4f89d2ca90592852b0fe6d14b4ab66dd162a9 test: handle logical escapes across splices`。
- 两名最终复审者均批准，Critical/Important/Minor 均为 `0`。
- 主代理重新执行 Debug Fresh 与 Release Fresh：两者 CTest 均为 `2/2`、
  `0` failures。
- Debug/Release 两份 `arpg_core_tests.exe` 均输出 `22 cases, 0 failures`。
- Release `arpg_game.exe` 与静态 `raylib.lib` 均存在，raylib DLL 数量为 `0`；
  `git diff --check` 无输出，工作树状态仅为分支名。

## Step 1：Core 分支合并与即时门禁

来源分支核验：

- `task/m00-core-runtime`：`ca6aaeb9901848dfe9a45477988ee8d936451531`
- 指定基线是该来源分支祖先。

合并命令：

```powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation -c user.name=Codex -c user.email=codex@local merge --no-ff task/m00-core-runtime -m "merge: integrate stage 0 core runtime"
```

- 退出状态：`0`
- 结果：无冲突，创建 merge commit `98122022a8947eb1242a752bf2209ae8a2a0777c`
- 提交信息：`merge: integrate stage 0 core runtime`

即时测试命令：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

- 退出状态：`0`
- `core.units`：通过
- `architecture.core_no_raylib`：通过
- CTest：`2/2`，`0` failures

直接运行：

```powershell
.\out\build\windows-msvc-core-debug\bin\arpg_core_tests.exe
```

- 退出状态：`0`
- 输出：`22 cases, 0 failures`

`ctest --test-dir .\out\build\windows-msvc-core-debug -N` 受控权限重跑退出状态 `0`，列出：

1. `core.units`
2. `architecture.core_no_raylib`

总数：`2`。首次在受限权限下运行时仅因不能写 `Testing/Temporary/LastTest.log` 失败；这不是构建、测试或集成失败。

## Step 2：Host 分支合并

来源分支核验：

- `task/m00-raylib-host`：`f76bb66e441a4d714ef01bc70bea259840411bd8`
- 该分支包含 `effb6bd` architecture 正则修复、`3b9f925` Task 6 实现和 `f76bb66` 侧墙绕序修复。

合并命令：

```powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation -c user.name=Codex -c user.email=codex@local merge --no-ff task/m00-raylib-host -m "merge: integrate stage 0 raylib host"
```

- 退出状态：`0`
- 结果：无冲突，创建 merge commit `cabbf746a9e0726a775ccdefe7ad76a8078dc6c1`
- 提交信息：`merge: integrate stage 0 raylib host`

## Step 3：完整 Debug / Release Fresh 门禁

### Debug

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
```

- 退出状态：`0`
- Fresh configure/build 成功，`arpg_game.exe` 成功链接。
- `core.units` 与 `architecture.core_no_raylib`：`2/2` 通过，`0` failures。
- `out/build/windows-msvc-debug/bin/arpg_core_tests.exe`：退出状态 `0`，`22 cases, 0 failures`。
- 未报告项目 `/W4 /permissive-` 编译警告。

### Release

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
```

- 退出状态：`0`
- Fresh configure/build 成功，`raylib.lib` 与 `arpg_game.exe` 成功链接。
- `core.units` 与 `architecture.core_no_raylib`：`2/2` 通过，`0` failures。
- `out/build/windows-msvc-release/bin/arpg_core_tests.exe`：退出状态 `0`，`22 cases, 0 failures`。
- 未报告项目 `/W4 /permissive-` 编译警告。

### CTest 清单

以下两个命令均以受控权限退出 `0`：

```powershell
ctest --test-dir .\out\build\windows-msvc-debug -N
ctest --test-dir .\out\build\windows-msvc-release -N
```

Debug 与 Release 均准确列出：

1. `core.units`
2. `architecture.core_no_raylib`

每个配置总数均为 `2`。

## Step 4：工具链、依赖与产物

缓存检查命令均退出 `0`。

Debug `CMakeCache.txt`：

- `CMAKE_CXX_COMPILER:FILEPATH=C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`
- `CMAKE_BUILD_TYPE:STRING=Debug`
- `BUILD_SHARED_LIBS:BOOL=OFF`

Release `CMakeCache.txt`：

- `CMAKE_CXX_COMPILER:FILEPATH=C:/Program Files (x86)/Microsoft Visual Studio/2022/BuildTools/VC/Tools/MSVC/14.44.35207/bin/Hostx64/x64/cl.exe`
- `CMAKE_BUILD_TYPE:STRING=Release`
- `BUILD_SHARED_LIBS:BOOL=OFF`

raylib 固定依赖：

- 固定提交：`dbc56a87da87d973a9c5baa4e7438a9d20121d28`
- 下载源码宏：`RAYLIB_VERSION_MAJOR=6`、`MINOR=0`、`PATCH=0`
- Host 同时具有对应的三项编译期 `static_assert`。
- Release 静态库存在：`out/build/windows-msvc-release/lib/raylib.lib`，大小 `5,918,934` bytes。

Release 可执行文件：

- `out/build/windows-msvc-release/bin/arpg_game.exe`
- 存在，大小 `611,840` bytes。

动态库扫描：

```powershell
Get-ChildItem .\out\build\windows-msvc-release\bin -Filter 'raylib*.dll'
```

- 退出状态：`0`
- 匹配数量：`0`

Ignore 核验：

```powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation check-ignore -v out/build/windows-msvc-debug/CMakeCache.txt
```

- 退出状态：`0`
- 命中 `.gitignore:2:/out/`。

## Step 5：可见 GUI 与退出路径

主代理完成了可见窗口与退出验证：

- raylib 自身通过临时、未提交的三帧探针导出 OpenGL 帧缓冲：
  `E:/game/.worktrees/m00-raylib-host/.superpowers/sdd/task-6-green-f76bb66.png`。
- 图像尺寸为 1280×720；左右侧墙、透视地板、横纵网格均可见。
- HUD 七项完整显示：raylib 6.0、root seed、tick、fixed steps、alpha、dropped seconds、invalid dt。
- 截图时 tick 已从初值推进到 `2`，alpha 为 `0.000`，位于 `[0, 1)`；dropped 与 invalid 均为 `0`。
- 临时探针随后用 `apply_patch` 撤销；最终 Release EXE 已重新构建并再次 CTest `2/2`，宿主工作树干净。

集成版 Release 第一次启动后，通过 Computer Use 向窗口 `Infinite Dungeon - Stage 0` 发送 Esc：

- 窗口关闭；窗口查询返回 `stillOpen: false`。
- `Get-Process arpg_game -ErrorAction SilentlyContinue` 返回进程数量 `0`。

第二次启动后，UI Automation 树明确返回 `6 按钮 关闭`。刷新窗口截图后点击标题栏关闭按钮：

- 窗口关闭；窗口查询返回 `stillOpen: false`。
- 最终 `Get-Process arpg_game -ErrorAction SilentlyContinue` 再次返回进程数量 `0`。

Step 5 完成。

## Step 6：仓库状态与历史

```powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation diff --check
```

- 退出状态：`0`
- 无输出。

```powershell
git -c safe.directory=E:/game/.worktrees/m00-foundation status --short --branch
```

- 退出状态：`0`
- 输出仅为 `## milestone/m00-foundation`，没有修改或未跟踪文件。

```text
b4f4f89 (HEAD -> milestone/m00-foundation) test: handle logical escapes across splices
2e789bf test: preserve tokens across spliced lines
6068bba test: close remaining core boundary bypasses
04d3919 test: harden boundary detector edge cases
49717ea test: close core raylib boundary gaps
661e3be test: enforce stage 0 core case count
cabbf74 merge: integrate stage 0 raylib host
9812202 merge: integrate stage 0 core runtime
f76bb66 (task/m00-raylib-host) fix: correct graybox wall winding
3b9f925 feat: render stage 0 raylib graybox
effb6bd fix: avoid raylib worktree path false positive
ca6aaeb (task/m00-core-runtime) feat: add bounded event queue
8b087c5 feat: add generational fixed-capacity pool
ea2c2a4 feat: add deterministic random streams
```

最终 host merge 的两个父提交为 `98122022a8947eb1242a752bf2209ae8a2a0777c` 与 `f76bb66e441a4d714ef01bc70bea259840411bd8`；core merge 的两个父提交为 `cc4b2da224388d8f6d73cdafa425a78c46e18018` 与 `ca6aaeb9901848dfe9a45477988ee8d936451531`。

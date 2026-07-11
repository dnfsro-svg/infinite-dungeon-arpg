# raylib 6.0 阶段 0 工程骨架设计

状态：已确认

确认日期：2026-07-10

适用里程碑：阶段 0——工程骨架

## 1. 目标

为单人、低资源、DNF 式 2.5D 房间动作 ARPG 建立一个可构建、可运行、可无窗口测试的 C++17 基础工程。阶段 0 只验证技术底座，不实现任何正式玩法。

交付结果必须包括：

- 使用官方 raylib 6.0 的 Windows 可执行程序。
- 60 Hz 固定时间步，渲染帧率不改变逻辑更新频率。
- 显式种子的确定性随机数工具及可派生随机流。
- 固定容量、代数句柄对象池。
- 固定容量、有界环形队列。
- 核心规则代码与 raylib 表现层之间的单向依赖边界。
- 使用 CMake、Ninja 和 CTest 的 Debug/Release 构建与测试流程。
- 一个可见的低成本 2.5D 灰盒房间和基础运行诊断信息。

## 2. 本阶段不做什么

阶段 0 不实现：

- 玩家、怪物、移动、攻击或受击。
- 三段普攻、重击、上挑及任何战斗状态机。
- 四扇元素门、房间清理、奖励或房间切换。
- 下坠洞、无限深度、1% 深渊房或随机防重骰存档。
- 等级、经验、星盘、装备、词缀、背包或掉落。
- 正式美术、动画、音效或资源流水线。
- 联机、服务器、ECS、通用刚体物理或多个房间并行模拟。
- 为后续系统预建没有调用者的空接口。

阶段 0 完成并报告后必须停止；未经用户确认不得进入阶段 1。

## 3. 技术基线

- 语言：C++17。
- 图形与输入：raylib 6.0。
- 构建系统：CMake 3.25 或更高版本。
- 构建器：Ninja。
- 编译器：MSVC 19.44，x64 host/x64 target。
- 测试入口：CTest。
- 固定逻辑频率：60 Hz，单步时长 `1.0 / 60.0` 秒。
- Windows SDK：10.0.26100.0。

raylib 依赖通过 CMake `FetchContent` 获取官方提交归档：

`https://github.com/raysan5/raylib/archive/dbc56a87da87d973a9c5baa4e7438a9d20121d28.tar.gz`

该提交对应 raylib 6.0 正式版。项目静态链接 raylib，不提交下载源码、构建缓存或第三方构建产物。首次配置需要联网；下载完成后允许复用本机 CMake 缓存。依赖不可用时配置必须失败，不得静默回退到其他 raylib 版本。

## 4. 架构与依赖方向

工程包含四个构建目标：

1. `arpg_core`：纯 C++17 静态库，不包含、不链接 raylib。
2. `arpg_raylib`：raylib 平台适配静态库，负责窗口、帧时间、输入采样和绘制。
3. `arpg_game`：唯一组合入口，只负责装配 `arpg_core` 与 `arpg_raylib`。
4. `arpg_core_tests`：无窗口测试程序，由 CTest 调用。

允许的依赖方向为：

```text
arpg_game -> arpg_raylib -> arpg_core

arpg_core_tests --------> arpg_core
```

`arpg_core` 不得出现 `raylib.h`、raylib 类型、键盘常量、颜色或纹理句柄。平台层负责把平台数据转换成核心层可接受的标准 C++ 值。阶段 0 不创建 `combat`、`actors`、`dungeon`、`items` 或 `progression` 空模块。

## 5. 核心组件

### 5.1 FixedStepRunner

`FixedStepRunner` 接收真实帧间隔，使用双精度累加器生成固定逻辑步。每次渲染帧最多执行 8 个逻辑步；达到上限后，丢弃累加器中剩余的完整逻辑步，只保留不足一个逻辑步的余数，并把丢弃量累计到诊断统计中。负数或非有限帧间隔按 0 秒处理并计入无效输入计数，防止卡顿或异常计时导致无限追帧。

输出至少包含：

- 本帧执行的固定步数量。
- 总逻辑 tick 数。
- `[0, 1)` 范围的渲染插值比例。
- 累计丢弃时间与无效输入计数。

阶段 0 的实际调用者是灰盒程序中的 tick 计数状态，不创建未来玩法接口。

### 5.2 DeterministicRng

随机工具使用 `xoshiro256**` 生成 64 位随机值，使用 `SplitMix64` 展开初始状态，不依赖标准库分布器的实现差异。相同种子与相同调用顺序必须产生相同的逐值序列。

工具支持从一个根种子和稳定的 64 位整数流 ID 派生子流。子流种子按 `SplitMix64(root_seed ^ SplitMix64(stream_id))` 计算，再用四次连续的 `SplitMix64` 输出填满 `xoshiro256**` 状态。派生一个子流或增加该子流的调用次数，不得改变其他子流结果。阶段 0 只提供通用派生能力，不预建深渊、洞口、怪物、词缀或掉落对象。

### 5.3 FixedPool<T, N>

对象池在构造时获得全部容量，运行期间不扩容。句柄由固定宽度的槽位索引与代数号组成。释放槽位会使旧句柄失效；槽位复用后旧句柄不能访问新对象。

容量耗尽时创建操作返回失败。无效或过期句柄查询返回空指针；Debug 断言只用于对象池内部不变量，不因正常的失败查询终止进程。池操作不抛异常，不在正常热路径分配堆内存。

### 5.4 BoundedQueue<T, N>

有界队列使用固定数组环形缓冲区，提供非阻塞的入队、出队和只读查询。满队列入队返回失败；空队列出队返回失败。队列不扩容、不丢弃旧元素、不覆盖尚未消费的数据。

## 6. 平台层与帧数据流

帧循环的数据流为：

```text
raylib 获取真实帧间隔
        -> FixedStepRunner 累积时间
        -> 每帧执行 0 到 8 个固定更新
        -> 生成只读诊断快照
        -> raylib 绘制灰盒房间与诊断 HUD
```

窗口默认尺寸为 1280×720。场景只绘制低成本的 2.5D 房间地面、背景和调试文字，不绘制玩家、怪物或门。HUD 显示：

- raylib 编译期版本。
- 根随机种子。
- 总逻辑 tick。
- 本帧固定步数。
- 插值比例。
- 累计丢弃时间。
- 无效帧间隔计数。

Esc 与窗口关闭按钮均可正常退出。平台适配层在编译期拒绝 raylib 主版本或次版本不是 6.0 的依赖。

## 7. 工作树与文件所有权

采用“里程碑集成工作树 + 独立任务工作树”的混合方式。仓库内 `.worktrees/` 已列入 `.gitignore`。

### 7.1 分支与路径

| 用途 | 分支 | 工作树路径 |
|---|---|---|
| 已验收基线 | `main` | `E:\game` |
| 阶段 0 集成 | `milestone/m00-foundation` | `E:\game\.worktrees\m00-foundation` |
| 核心运行时 | `task/m00-core-runtime` | `E:\game\.worktrees\m00-core-runtime` |
| raylib 宿主 | `task/m00-raylib-host` | `E:\game\.worktrees\m00-raylib-host` |

### 7.2 集成顺序

1. 在集成工作树建立可编译、可启动的最小 CMake/raylib 基线和实际被调用的核心边界。
2. 验证基线构建后，从同一个集成提交创建两个任务工作树。
3. 两个任务工作树只能修改各自拥有的目录，并各自携带测试。
4. 先把 `task/m00-core-runtime` 合并进集成分支并运行核心测试。
5. 再把 `task/m00-raylib-host` 合并进集成分支并运行完整测试与窗口验证。
6. 阶段 0 经用户验收后才能合并到 `main`。
7. 合并完成后移除任务工作树；未获确认不得创建阶段 1 工作树。

### 7.3 文件所有权

阶段 0 集成工作树独占：

- 根 `CMakeLists.txt`。
- `CMakePresets.json`。
- `cmake/`。
- `scripts/`。
- 跨组件公共构建配置与阶段报告。

核心运行时工作树独占：

- `src/core/`。
- `tests/core/`。
- 上述目录内的局部 `CMakeLists.txt`。

raylib 宿主工作树独占：

- `src/app/`。
- `src/platform/raylib/`。
- `tests/platform/`。
- 上述目录内的局部 `CMakeLists.txt`。

任务工作树不得修改根 CMake、公共依赖配置或另一个任务拥有的目录。测试不得与对应实现拆分到不同工作树。

## 8. 预期文件结构

```text
E:\game
|-- CMakeLists.txt
|-- CMakePresets.json
|-- cmake/
|   `-- Dependencies.cmake
|-- scripts/
|   |-- Configure.ps1
|   |-- Build.ps1
|   `-- Test.ps1
|-- src/
|   |-- core/
|   |   |-- CMakeLists.txt
|   |   |-- fixed_step.hpp
|   |   |-- fixed_step.cpp
|   |   |-- deterministic_rng.hpp
|   |   |-- deterministic_rng.cpp
|   |   |-- fixed_pool.hpp
|   |   `-- bounded_queue.hpp
|   |-- platform/raylib/
|   |   |-- CMakeLists.txt
|   |   |-- raylib_host.hpp
|   |   `-- raylib_host.cpp
|   `-- app/
|       |-- CMakeLists.txt
|       `-- main.cpp
|-- tests/
|   |-- core/
|   |   |-- CMakeLists.txt
|   |   |-- test_main.cpp
|   |   |-- fixed_step_tests.cpp
|   |   |-- deterministic_rng_tests.cpp
|   |   |-- fixed_pool_tests.cpp
|   |   `-- bounded_queue_tests.cpp
|   `-- platform/
|       |-- CMakeLists.txt
|       `-- core_boundary_test.cmake
`-- docs/
    `-- superpowers/
```

## 9. 构建流程

PowerShell 脚本通过 `vswhere.exe` 定位包含 x64 C++ 工具的 Visual Studio 2022 Build Tools，导入 `Microsoft.VisualStudio.DevShell.dll`，再以 x64 host/x64 target 初始化 MSVC。

标准流程为：

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug
.\scripts\Build.ps1 -Preset windows-msvc-debug
.\scripts\Test.ps1 -Preset windows-msvc-debug

.\scripts\Configure.ps1 -Preset windows-msvc-release
.\scripts\Build.ps1 -Preset windows-msvc-release
.\scripts\Test.ps1 -Preset windows-msvc-release
```

项目目标启用 MSVC `/W4 /permissive-`。这些警告选项只应用于本项目目标，不传递给 raylib。构建输出统一写入 `out/`，不污染源码目录。

## 10. 错误处理

- 找不到符合要求的 MSVC 工具链时，配置脚本输出明确错误并返回非零退出码。
- raylib 下载、配置或版本检查失败时停止构建，不使用系统中其他版本替代。
- raylib 窗口初始化失败时记录错误并返回非零退出码。
- 对象池或有界队列容量耗尽时返回失败结果，不扩容、不覆盖数据。
- 无效或过期对象句柄永远不能返回对象指针或引用。
- 固定时间步超过追帧上限时记录被丢弃时间，程序继续运行。
- 任一测试失败、工作树存在不明改动或合并冲突时停止集成并报告，不绕过验证。

## 11. 自动测试

CTest 至少覆盖以下行为：

1. 相同根种子产生逐值一致的黄金随机序列。
2. 不同子流结果可重复，且一个子流的额外调用不影响其他子流。
3. 相同总时间使用 30 FPS、60 FPS 和混合帧切分时产生相同逻辑 tick。
4. 长帧只执行最多 8 个固定步，且正确记录丢弃时间。
5. 负数、无穷大和非数字帧间隔不推进逻辑，并增加无效输入计数。
6. 对象池达到容量后创建失败。
7. 对象释放、槽位复用和代数号变化正确。
8. 已释放句柄和旧代数句柄均无法访问对象。
9. 队列保持 FIFO，正确处理空、满和索引绕回。
10. 对象池和队列的压力循环不发生堆分配。
11. 核心源码和公开头文件不包含 raylib 头文件。
12. Debug 与 Release 配置均可完成构建并运行全部无窗口测试。

测试框架使用项目内的轻量测试入口，不增加 Catch2、GoogleTest 或其他大型依赖。

## 12. 人工验收

1. 启动 Release 版 `arpg_game.exe`。
2. 确认出现 1280×720 的低成本 2.5D 灰盒房间。
3. 确认 HUD 显示 raylib 6.0、根种子、逻辑 tick 和固定步统计。
4. 改变窗口刷新节奏后，逻辑仍按 60 Hz 固定步推进。
5. 分别使用 Esc 和窗口关闭按钮退出。
6. 确认退出后不存在残留游戏进程。

## 13. 完成与停止条件

只有同时满足以下条件，阶段 0 才算完成：

- 三个阶段 0 工作树中的预定内容全部进入集成分支。
- Debug 与 Release 均配置和构建成功。
- 全部 CTest 测试通过。
- 可执行程序实际启动并通过人工验收。
- `arpg_core` 不依赖 raylib。
- 实际使用的 raylib 版本为 6.0。
- 阶段完成报告列出构建命令、测试结果、人工验收结果和遗留问题。

达到以上条件后停止。阶段 1 的战斗实验室必须另行设计、确认和创建工作树。

# 行为保持型代码精简设计

## 目标

在不改变战斗数值、输入语义、固定步长、事件顺序、绘制顺序和公开接口的前提下，删除无效状态并集中测试重复代码。目标是让代码来源更单一、测试入口更短，同时保持 Stage 1 的所有可观察行为不变。

## 方案比较

### A. 小步删除和去重（采用）

- 将三个测试入口的执行循环集中到 `test_framework.hpp`。
- 用 `make_suite()` 推导测试用例数量，删除手写 `sizeof` 表达式。
- 提取战斗测试的 `tick_n`、`finish_attack`、`drain_events`。
- 用一张 `constexpr` 表表达压力测试动作周期和位掩码。
- 删除从未读取的运行时字段，复用现有 `attack_phase_at()` 判定攻击结束。

优点是变更机械、可由现有测试完整保护；缺点是会触及多个测试文件。

### B. 只精简生产代码（不采用）

只删除无效状态和重复阶段计算。变更最小，但保留大部分已确认的测试重复，收益有限。

### C. 同时拆分渲染器和 CMake 守卫（不采用）

能缩短单个大文件，但总行数不会明显减少，还会扩大绘制顺序和配置作用域的回归面，不符合本轮低风险目标。

## 设计边界

- 保留 C++17、raylib 6.0.0、MSVC 19.44 x64、Windows SDK 10.0.26100.0、CMake、Ninja 和 CTest 基线。
- 不调整 J、K、L 的输入含义，不恢复重击，不修改上挑、击飞或浮空时长。
- 不修改攻击目录数值、碰撞、受击状态机、事件容量或无分配约束。
- 不合并渲染器的阴影遍历和角色遍历，不数组化枚举 `switch`。
- 不重构 `FixedStep`、RNG、固定池或有界队列。

## 组件设计

### 测试框架

`make_suite(name, cases)` 从数组类型推导用例数。`run_suites(suites, expected_count, label)` 统一运行、失败打印、用例总数守卫和退出码。三个测试程序继续分别守卫 22、30、7 个用例。

### 战斗测试支持

新增头文件 `tests/combat/combat_test_support.hpp`，只提供三个内联辅助函数。`finish_attack` 的最大 tick 数必须由调用点显式传入，继续保留原来的 80、100、128 边界；各测试自己的 `CombatLabConfig` 仍留在原文件中。

### 压力测试调度

四项调度按固定数组顺序保存：37 tick 的轻击、181 tick 的跳跃、251 tick 的上挑、307 tick 的第二个独立上挑位。一次循环同时产生 requested 和 accepted 掩码，保持 tick 0 的 J、K、L、L 入队顺序。

### 生产运行时

删除只有写入、从未读取的 `AttackRuntime::serial`、`DummyRuntime::pending_impact` 和 `DummyRuntime::has_pending_impact` 及其赋值。攻击推进在递增 elapsed tick 后只调用一次 `attack_phase_at()`；当结果为 `finished` 时执行原有结束路径。

## 验证

1. 修改前运行官方 Debug `scripts/Test.ps1`，建立 5/5 CTest 通过基线。
2. 每个精简任务后构建并运行对应测试程序。
3. 完成后运行 Debug、Release 和 core-only Debug 全量 CTest。
4. 直接运行 core/combat/platform 三个测试程序，确认分别为 22/30/7 用例、0 失败。
5. 隐藏启动 `arpg_game.exe` 做短时启动冒烟，确认进程能启动且不会立即异常退出。
6. 独立审查最终差异，确认没有行为扩张或绘制/输入语义改变。

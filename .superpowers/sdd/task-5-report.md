# Task 5 实现报告：Fixed-Capacity Bounded Queue

## 提交

- 分支：`task/m00-core-runtime`
- 基线 SHA：`8b087c5e88127660423a676d530526a8b642d5cf`
- 最终 SHA：`ca6aaeb9901848dfe9a45477988ee8d936451531`
- 提交信息：`feat: add bounded event queue`

## 文件

- 新增 `src/core/bounded_queue.hpp`
- 新增 `tests/core/bounded_queue_tests.cpp`
- 修改 `tests/core/CMakeLists.txt`
- 修改 `tests/core/test_main.cpp`

## RED 证据

先只加入队列测试和 suite 注册，未创建实现头文件。首次运行
`./scripts/Test.ps1 -Preset windows-msvc-core-debug` 时，沙箱拒绝写入既有
CMake 构建缓存；未删除缓存，按要求用受控权限重跑同一命令。

受控重跑退出码为 1，MSVC 在 `/W4` 编译阶段报告预期错误：

```text
tests/core/bounded_queue_tests.cpp(4): fatal error C1083:
无法打开包括文件: “core/bounded_queue.hpp”: No such file or directory
```

该 RED 由缺失目标功能导致，不是测试拼写或配置错误。

## GREEN 证据

创建最小 `BoundedQueue<T, N>` 实现后，运行同一 core-only 命令：

```text
Test #1: core.units ....................... Passed
Test #2: architecture.core_no_raylib ...... Passed
100% tests passed, 0 tests failed out of 2
```

直接运行 `out/build/windows-msvc-core-debug/bin/arpg_core_tests.exe`：

```text
22 cases, 0 failures
```

## 分配与约束验证

- 存储为 `std::array<std::optional<T>, N>`，无动态增长、覆盖或阻塞路径。
- 满队列在构造前返回 `false`；空队列返回 `nullptr` 或 `std::nullopt`。
- FIFO 环形索引、析构、10,000 次压力循环和分配计数均由测试覆盖。
- `front()` 只返回 `const T*`。
- `N > 0`、nothrow 移动构造和 nothrow 析构由类级静态断言约束；
  `try_emplace` 与复制 `try_push` 使用 nothrow 构造约束。
- 运行
  `rg -n "malloc|calloc|realloc|new " src/core/fixed_pool.hpp src/core/bounded_queue.hpp`
  退出码为 1 且无输出，表示两份核心头文件没有直接分配调用。

## 自检

- `/W4 /permissive-` 编译通过。
- `git diff --check`：退出码 0。
- `git diff --cached --check`：退出码 0。
- 提交后 `git status --short`：无输出。
- 提交仅含上述 4 个任务 5 文件；未实现任务 6+，未删除或重置文件。

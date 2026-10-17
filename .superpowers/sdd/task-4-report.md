# Task 4 Report: Generational Fixed-Capacity Object Pool

## 提交

- 分支：`task/m00-core-runtime`
- 基线 SHA：`ea2c2a4a91eb02a5aecfa8c8a2e545e6c54cb8f3`
- 最终 SHA：`8b087c5e88127660423a676d530526a8b642d5cf`
- 提交信息：`feat: add generational fixed-capacity pool`

## 改动文件

- `src/core/fixed_pool.hpp`
- `tests/core/allocation_probe.hpp`
- `tests/core/allocation_probe.cpp`
- `tests/core/fixed_pool_tests.cpp`
- `tests/core/CMakeLists.txt`
- `tests/core/test_main.cpp`

提交仅包含上述 6 个任务 4 文件，共 `541 insertions(+), 1 deletion(-)`。

## RED

命令：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
```

首次在受限沙箱内运行时，现有 CMake 构建目录写入被拒；该环境失败不计为有效 RED。按受控权限流程以同一命令重跑后取得有效 RED：

- MSVC 在 `tests/core/fixed_pool_tests.cpp(4)` 报 `fatal error C1083`。
- 关键原因：无法打开 `core/fixed_pool.hpp`，文件不存在。
- Ninja 因该编译错误退出，正好证明新增 suite 已进入构建且因待实现接口失败。

取得上述 RED 前，`src/core/fixed_pool.hpp` 尚未创建。

## GREEN

命令：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
```

提交后最终结果：退出码 `0`，CTest `2/2` 通过：

- `core.units`：通过。
- `architecture.core_no_raylib`：通过。
- `100% tests passed, 0 tests failed out of 2`。
- MSVC `/W4 /permissive-` 构建无编译警告。

测试可执行文件确认命令：

```powershell
.\out\build\windows-msvc-core-debug\bin\arpg_core_tests.exe
```

结果：退出码 `0`，`17 cases, 0 failures`。其中新增 `fixed_pool` suite 为 5 例，既有 suite 为 12 例。

## 行为与分配验证

- `FixedPool<T, N>` 使用编译期固定的 `std::array` 槽位、`std::optional<T>` 原地生命周期和索引自由链表；运行期间不持有动态容器。
- `Handle` 的 `index_type` 与 `generation_type` 均静态验证为 32 位，整个句柄静态验证为 8 字节。
- 用例覆盖非默认构造类型、满池拒绝且保留既有对象、const/non-const `get`、稳定地址、释放后槽位/地址复用、陈旧句柄失效、越界/零代际/错误代际句柄、重复释放失败和析构恰好一次。
- 代际在释放时递增；32 位回绕为 `0` 时再次递增，跳过保留的无效代际 `0`。压力循环同时验证返回句柄代际始终非零。
- 64 字节对齐对象通过地址模 `alignof(T)` 为 0 的检查。
- 全局分配探针覆盖普通/数组、sized delete、nothrow、aligned 及 aligned+nothrow 的标准 `new/delete` 重载；MSVC 对齐路径使用 `_aligned_malloc/_aligned_free`。
- 分配计数在测试框架启动后、压力段入口采样基线，避免把框架自身初始化分配算入对象池；10,000 次 `try_emplace/get/release` 和对齐对象池构造/使用/析构后再次采样，`after == before` 断言通过，即额外动态分配增量为 `0`。

## 自检

- `git -c safe.directory=E:/game/.worktrees/m00-core-runtime diff --check`：退出码 `0`，无错误。
- `git -c safe.directory=E:/game/.worktrees/m00-core-runtime status --short`：无输出，工作树干净。
- `git show --name-status HEAD`：仅列出任务 4 的 6 个目标文件。
- 最终 HEAD：`8b087c5e88127660423a676d530526a8b642d5cf`；其父提交为指定基线 `ea2c2a4a91eb02a5aecfa8c8a2e545e6c54cb8f3`。

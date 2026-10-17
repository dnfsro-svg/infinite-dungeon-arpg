# Task 3 Report: Deterministic Random Streams

## 提交

- 分支：`task/m00-core-runtime`
- 基线 SHA：`cc4b2da224388d8f6d73cdafa425a78c46e18018`
- 最终 SHA：`ea2c2a4a91eb02a5aecfa8c8a2e545e6c54cb8f3`
- 提交信息：`feat: add deterministic random streams`

## 改动文件

- `src/core/deterministic_rng.hpp`
- `src/core/deterministic_rng.cpp`
- `src/core/CMakeLists.txt`
- `tests/core/deterministic_rng_tests.cpp`
- `tests/core/CMakeLists.txt`
- `tests/core/test_main.cpp`

## RED

命令：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

有效 RED 结果：退出码 `1`。编译完成后链接按预期失败：

- `LNK2019`：未定义 `DeterministicRng::DeterministicRng(std::uint64_t)`
- `LNK2019`：未定义 `DeterministicRng::next_u64()`
- `LNK2019`：未定义 `DeterministicRng::derive_stream(std::uint64_t, std::uint64_t)`
- `LNK1120`：共 3 个未解析外部符号

首次在受限沙箱内运行时，CMake 构建目录创建被拒；按权限流程允许写入隔离工作树的 `out/build` 后，以同一命令重跑并取得上述功能性 RED。

## GREEN

命令：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
```

结果：退出码 `0`，CTest `2/2` 通过：

- `core.units`：通过
- `architecture.core_no_raylib`：通过
- `100% tests passed, 0 tests failed out of 2`

测试用例数量确认命令：

```powershell
.\out\build\windows-msvc-core-debug\bin\arpg_core_tests.exe
```

结果：退出码 `0`，`12 cases, 0 failures`。

## 自检

- `git status --short`：无输出，工作树干净。
- `git -c safe.directory=E:/game/.worktrees/m00-core-runtime diff --check`：退出码 `0`，无错误。
- 最终提交仅含简报指定的 6 个文件，共 `219 insertions(+), 2 deletions(-)`。

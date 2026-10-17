# Task 6：96 字节版本化检查点编解码报告

## TDD RED

先添加 8 个 codec 用例、测试目标和构建接线，未添加生产实现时运行：

```text
.\scripts\Build.ps1 -Preset windows-msvc-core-debug -Fresh
```

Fresh configure/generate 失败，原因是 `src/persistence/CMakeLists.txt` 引用的
`crc32.cpp` 尚不存在，输出为 `Cannot find source file: crc32.cpp` 与
`No SOURCES given to target: arpg_persistence`。这确认测试接线先于 codec 实现进入 RED。

## TDD GREEN

实现 CRC32/ISO-HDLC、显式 little-endian 读写、96 字节 wire layout、CRC 覆盖区域和解码校验后运行：

```text
.\scripts\Build.ps1 -Preset windows-msvc-core-debug -Fresh
```

结果：配置、生成和编译成功。

```text
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
```

结果：`100% tests passed, 0 tests failed out of 12`。

```text
.\out\build\windows-msvc-core-debug\bin\arpg_persistence_tests.exe
```

结果：`8 cases, 0 failures`。

## 变更文件

- `CMakeLists.txt`
- `src/persistence/CMakeLists.txt`
- `src/persistence/crc32.hpp`
- `src/persistence/crc32.cpp`
- `src/persistence/checkpoint_codec.hpp`
- `src/persistence/checkpoint_codec.cpp`
- `tests/persistence/CMakeLists.txt`
- `tests/persistence/persistence_test_main.cpp`
- `tests/persistence/checkpoint_codec_tests.cpp`
- `tests/platform/CMakeLists.txt`
- `tests/platform/module_boundary_test.cmake`
- `tests/platform/persistence_boundary_test.cmake`

## 自审

- 测试 fixture 逐字段手工构造 `arpg::dungeon::checkpoint::DungeonRunState`，未调用 dungeon progression。
- persistence 只包含 `dungeon/dungeon_checkpoint.hpp`，未链接 `arpg_dungeon` 或 `arpg_combat`。
- header 为 32 字节、payload 为 64 字节、总长为 96 字节；多字节字段均显式 little-endian。
- CRC32 覆盖 bytes 8..27 与 32..95，跳过 magic 和 CRC 字段；已验证 `123456789 -> 0xCBF43926`。
- 新增目标闭包断言和 5 个架构测试，Core-only 总数为 12。
- `git diff --check` 在提交前通过。

## Concerns

- 规则版本当前固定为 1（`kCheckpointRulesVersion`），后续规则升级需同步版本策略。
- 本任务未实现双槽 SaveStore 或端到端 Session 集成，留给 Task 7/8。

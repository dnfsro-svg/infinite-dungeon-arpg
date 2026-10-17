# Task 4 最终复审报告：检查点格式 3 与星盘位图

## Spec Compliance

- 符合：格式 3 使用 `IARPGS04`、payload 88、总长 120，且在 offset 106 以小端序写入 64 位 `allocated_bits`；CRC 覆盖 header 后 20 字节与完整 payload 88 字节。`src/persistence/checkpoint_codec.cpp:17-24, 133-161`
- 符合：解码将 magic、format、payload size 作为一致的三元组校验；format 3 只能配新魔数，format 1/2 只能配旧魔数。`src/persistence/checkpoint_codec.cpp:173-213`
- 符合：格式 1/2 解码均迁移至仅起点位图，格式 3 读取 bitset；随后统一执行 progression 与连通性/点数校验，因此能拒绝 bit0 缺失、保留尾字节非零、断链、超点和无效 progression。`src/persistence/checkpoint_codec.cpp:256-295`; `src/passives/passive_tree_rules.cpp:58-65`
- 符合：A/B 槽的状态比较已经纳入位图，临时文件验证与最终扫描都会使用该比较。`src/persistence/save_slot.cpp:24-45`; `src/persistence/save_transaction.cpp:50-55`; `src/persistence/save_recovery.cpp:230-245`
- 符合：故障恢复用例现在以不同且合法的位图提交新槽：初始 `{0,8,9,10}` 与新状态 `{0,1,2,3}` 都花费 3 点，匹配 `earned=9/unspent=6`，且均与起点连通；加载断言要求返回新位图。`tests/persistence/passive_tree_checkpoint_tests.cpp:40-43`; `tests/persistence/passive_tree_checkpoint_tests.cpp:136-155`
- 符合：格式 1 的迁移用例已明确断言结果为仅起点位图。`tests/persistence/checkpoint_codec_tests.cpp:261-270`
- 符合：持久化 target 只新增 `arpg_passives` 与 `arpg_progression`，没有引入 raylib、combat 或 dungeon 库依赖；dungeon checkpoint 仅携带纯状态类型。`src/persistence/CMakeLists.txt:1-11`; `src/dungeon/dungeon_checkpoint.hpp:1-62`

## Strengths

- 防御性边界正确：进入 payload 读取前，代码先校验长度、magic、规则版本、payload 长度和 CRC；不同旧格式只复制各自 payload 长度，未发生 format 1/2 越界读取。`src/persistence/checkpoint_codec.cpp:165-214`
- 格式 3 对新增位图尾部的 6 个保留字节要求全零，避免将非格式字段静默接受为有效存档。`src/persistence/checkpoint_codec.cpp:271-289`
- 旧的 format 2 静态 fixture 仍以 `IARPGS03`/112 字节编码并被独立读取；这证实本次修复不是单纯把测试一起换成新格式。`tests/persistence/checkpoint_codec_tests.cpp:77-109`
- 新增用例覆盖了 offset 106 小端 round-trip、format 2 迁移、位图包含在 CRC 内、以及使用可区分位图的最终扫描故障后 load 路径。`tests/persistence/passive_tree_checkpoint_tests.cpp:78-157`

## Issues

### Critical

- 无。`src/persistence/checkpoint_codec.cpp:165-295`

### Important

- 无。`tests/persistence/passive_tree_checkpoint_tests.cpp:136-155`

### Minor

- `baseline_checkpoint_bytes_are_preserved` 对同一 V2 fixture 做了两次 decode，第一处的 `same_state` 已间接覆盖默认 bit0，第二次 decode 只是重复读取，可合并为一次以提高用例意图清晰度。`tests/persistence/checkpoint_codec_tests.cpp:101-108`

## Assessment

最终复审通过：格式 3、旧格式迁移、CRC、状态校验、A/B 位图保留和依赖边界均满足约定；未发现阻止合并的问题。A/B 用例已能区分恢复旧位图与新位图，format 1 也已具备 bit0 回归断言。`tests/persistence/passive_tree_checkpoint_tests.cpp:126-157`; `tests/persistence/checkpoint_codec_tests.cpp:261-270`

## 审查修复验证

- A/B 故障恢复测试的 `next` 改为中心路径 `0,1,2,3`，与 `{earned=9, unspent=6}` 的 3 点分配匹配，并断言与 initial 位图不同后再检查 load 结果。
- format 1 迁移测试显式断言 `allocated_bits == 1ULL`。
- V2 fixture 的重复 decode 已合并为一次，单次 decode 同时验证完整状态与起点迁移位图。

修复后：`arpg_persistence_tests.exe` 37/37；指定 persistence/architecture CTest 3/3；`git diff --check` 通过。

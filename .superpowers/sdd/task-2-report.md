# Stage 17 Task 2 实现报告

## 结果

- 提交：`edd7468` (`feat: persist skill stone loadout in checkpoint v8`)
- 将技能石装配加入 V8 checkpoint。
- V8 固定 payload 为 756 字节，base encoded 为 788 字节。
- V7 固定 payload 716 字节、base encoded 748 字节及 item record 布局保持不变。
- V1～V7 成功解码后统一迁移为 `default_skill_loadout()`，并返回 `migrated=true`。
- V8 成功解码返回 `migrated=false`。

## RED

先新增 `checkpoint_v8_skill_loadout_tests.cpp` 并注册 7 个测试，未修改生产代码。

真实 RED 命令（普通 PowerShell 缺少 MSVC/SDK 环境，因此按仓库要求进入 VsDevCmd）：

```powershell
cmd /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=x64 -host_arch=x64 && cmake --build --preset windows-msvc-core-debug --target arpg_persistence_tests"
```

结果：exit 2。关键失败为：

- 缺少 `kV8SkillLoadoutPayloadSize`、`kV8BasePayloadSize`、`kV8BaseEncodedCheckpointSize`。
- 缺少 `kSeventhCheckpointFormatVersion`。
- `DungeonRunState` 缺少 `skill_loadout`。
- `kCheckpointFormatVersion == 8U` 静态断言失败。

随后按简报执行 ctest 时旧二进制仍通过；由于新目标构建失败，该结果明确标记为 stale，不作为 RED/GREEN 证据。

## GREEN

实现 V8 后首次构建成功；新 V8 suite 7/7 通过。旧 suite 最初有 13 项失败，均为旧测试仍将 current encoder 写死为 V7：

- 748/812/940 等尺寸未加 40 字节。
- current magic/version 仍断言 7。
- item record 仍从 748 开始。
- V8→V4/V6 测试 helper 仍从旧 item offset 取数据。
- passive CRC 仍只覆盖 716 字节。
- unsupported format 测试仍把 8 当作未知版本。

机械更新这些 current-version 断言后，保留并加强 V7 golden：从 V8 严格移除 40 字节装配段，固定 ownership 和 64 字节 item record 逐字节匹配旧 golden，并成功按 V7 解码、默认迁移。

最终验证：

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_persistence_tests
ctest --test-dir out/build/windows-msvc-core-debug -R '^(persistence\.units|architecture\.persistence_checkpoint_only)$' --output-on-failure
```

结果：

- `persistence.units`：通过，92 cases / 0 failures。
- `architecture.persistence_checkpoint_only`：通过。
- 合计 2/2 tests passed，exit 0。

额外依赖自检：

```powershell
cmake --build --preset windows-msvc-core-debug
```

结果：core-only 全目标构建成功，exit 0。

## 覆盖

- 两个主动石在五槽中的全部 20 种有序排列往返。
- 空槽、仅拥有单石、owned bits 为零往返。
- 固定 V1 样本和 V7 base/item golden 默认迁移。
- 5 个主动 ID 字节逐个拒绝非法 ID 2。
- 25 个辅助 ID 字节逐个拒绝非法 ID 0。
- 重复主动石、槽中未拥有主动石、owned 高位、两个非零保留字节拒绝。
- 编码前整体 `validate_skill_loadout` 拒绝非法状态。

## 修改文件

- `src/dungeon/dungeon_checkpoint.hpp`
- `src/dungeon/CMakeLists.txt`
- `src/persistence/checkpoint_codec.hpp`
- `src/persistence/checkpoint_codec.cpp`
- `src/persistence/CMakeLists.txt`
- `tests/persistence/CMakeLists.txt`
- `tests/persistence/persistence_test_main.cpp`
- `tests/persistence/checkpoint_v8_skill_loadout_tests.cpp`
- `tests/persistence/checkpoint_codec_tests.cpp`
- `tests/persistence/death_checkpoint_codec_tests.cpp`
- `tests/persistence/passive_tree_checkpoint_tests.cpp`
- `tests/persistence/save_store_tests.cpp`

## 自审

- V8 装配段顺序严格为 owned u64 LE、5 active、25 support、2 reserved zero。
- V8 item 起点为 788；V7 item 起点仍为 748。
- V7 material、reinforcement、6-roll item 解析条件显式保留。
- enum 在写入状态前逐 ID 验证；重复、未拥有和 owned 高位由整体 validator 返回 `invalid_state`。
- `arpg_persistence` PUBLIC 依赖 `arpg_skills`。
- 独立 reviewer 发现 `DungeonRunState` 公共头也要求 `arpg_dungeon` PUBLIC 依赖 `arpg_skills`；已修复并复核 Ready。
- `src/skills` 无 persistence 引用，依赖方向无反转。
- `git diff --check` 无 whitespace error。

## 顾虑与边界

- 按任务硬边界，未实现地下城装卸事务、输入、UI 或战斗改动。
- persistence 事务层的 `detail::same_state()` 已在审查修复中纳入完整技能装配比较；地下城运行时接入仍不在本任务范围。
- 工作树原有 `.superpowers/sdd/task-5-report.md` 修改未触碰、未暂存。

## 审查修复：AB 槽技能装配冲突

修复提交：`cc281d6` (`fix: detect skill loadout save conflicts`)

正式审查指出 `src/persistence/save_slot.cpp` 的
`detail::same_state()` 未比较 `skill_loadout`，可能让同 generation、仅装配不同的
两个合法 V8 AB 槽被静默视为相同。

### RED

先扩展 `same_state_includes_all_ownership_bytes_and_order()`：

- owned bits 差异必须不相同。
- 5 个 active 字段逐个差异必须不相同。
- 25 个 support 字段逐个差异必须不相同。

另新增两个真实文件场景：

- 同 generation、其他字段相同，只交换合法主动石槽位。
- 同 generation、其他字段相同，只把另一份档案改为合法空 owned/空装配。

命令：

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_persistence_tests
ctest --test-dir out/build/windows-msvc-core-debug -R '^persistence\.units$' --output-on-failure
```

真实 RED：94 cases / 3 failures，exit 1：

- `same state includes ownership bytes and order` 忽略 loadout 差异。
- `equal generation active slot difference conflicts` 未返回 recovery。
- `equal generation owned skill difference conflicts` 未返回 recovery。

### GREEN

在 `save_slot.cpp` 增加最小 `same_skill_loadout()`：比较
`owned_active_bits`，并逐槽比较 active 与完整 supports 数组；随后纳入
`detail::same_state()`。

第一次 GREEN 运行因测试夹具在新增逐字段检查后被重置为无物品状态，既有代码继续访问
`items[0]` 而触发 MSVC vector 越界断言。恢复原带物品夹具后重新执行，生产实现不需额外接口或改动。

最终结果：

- `persistence.units`：94/94，0 failures。
- `architecture.persistence_checkpoint_only`：通过。

同时将实际编码/断言 current format 8、base 788 的测试名称由 V7 更正为 V8；
通过 `as_v7_golden()`/`v7_from_v8()` 构造并验证旧格式的测试继续保留 V7 名称。

# Stage 10 Task 2 实现报告

## 状态

- 完成 Task 2：V5 稳定深渊 checkpoint、固定 codec、legacy migrated 传播、纯迁移 helper。
- 未开始 runtime 自动提交、raylib 或 session 深渊启动逻辑。
- 提交：`65bd81eebdc07d1a3d27ff0c6fee808f85a1e732`。

## TDD 证据

### RED

先新增 15 个测试用例（总数由 49 增至 64），再运行：

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cmake --build --preset windows-msvc-debug --target arpg_persistence_tests'
```

预期失败输出：

```text
checkpoint_codec_tests.cpp(4): fatal error C1083:
无法打开包括文件: dungeon/abyss_checkpoint_migration.hpp
save_store_tests.cpp: error C2039: migrated 不是 SaveLoadResult 的成员
ninja: build stopped: subcommand failed.
```

说明：第一次未初始化 VS 环境的命令还出现 `<array>`/`<cstdint>` 找不到；该环境失败不作为 RED 证据。初始化 `vcvars64.bat` 后，上述缺失 Task 2 API 的失败才是有效 RED。

### GREEN

最终新鲜验证：

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cmake --build --preset windows-msvc-debug --target arpg_persistence_tests'
& .\out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe
ctest --preset windows-msvc-debug -R "^(persistence\.units|abyss\.units|architecture\.(dungeon_no_persistence|persistence_checkpoint_only|persistence_no_raylib|dungeon_no_raylib))$" --output-on-failure
```

输出：

```text
[3/3] Linking CXX executable bin\arpg_persistence_tests.exe
64 cases, 0 failures
100% tests passed, 0 tests failed out of 6
```

必要 dungeon 回归单独运行：

```powershell
ctest --preset windows-msvc-debug -R "^dungeon\.units$" --output-on-failure
```

输出：

```text
1/1 Test #4: dungeon.units ... Passed 150.05 sec
100% tests passed, 0 tests failed out of 1
```

## 实现与固定布局

- `DungeonRunState` 新增 `AbyssCheckpoint abyss` 和 `LastAbyssResolution last_abyss_resolution`。
- V5 magic 为 `IARPGS06`，format 为 5；V4 `IARPGS05`、format 4 及 V1～V4 长度/布局保留。
- V5 base payload 为 204 bytes、base encoded size 为 236 bytes；物品记录仍为 40 bytes。
- 固定顺序：旧 88-byte dungeon/progression 区，abyss checkpoint，last resolution，V5 ownership header，物品记录。
- CRC 覆盖 header 的 format/rules/generation/payload length 及完整 payload。
- 解码明确拒绝非法 lifecycle/danger/rule、summary bool、保留字节、reward_total、位图关系、摘要计数、rules version、确定性 rule/danger 错配和所有既有 item/progression 非法状态。
- `available/started/cleared` 必须对应 `current_room.is_abyss=true`；`none` 保持空规则状态，`failed` 不保留奖励运行进度；所有非 none 状态都校验 seed+depth 确定性重建。

## 迁移与依赖边界

- V1～V4 decoder 只恢复旧字段并设置 `DecodeResult::migrated=true`，不调用深渊生成逻辑；V5 为 false。
- slot scan 将标记存入 `SlotInfo`，`ready_result` 只传播最终选中 A/B slot 的标记；损坏 slot 归档并重扫后仍保持 legacy 标记。
- `migrate_legacy_abyss_checkpoint` 是 dungeon 纯 helper：合法 door 目标按 room seed+depth 派生 `available`；initial/descent/非门或普通房清除旧 `is_abyss` 资格并置 `none`。
- helper 保留 root seed、commit generation、bias、room index/seed/depth/floor index/ecology/hole、progression、passive tree 和完整 items，仅重建深渊稳定字段。
- persistence 为执行 V5 语义校验，`PRIVATE` 链接纯规则库 `arpg_abyss`；未依赖 `arpg_dungeon`、combat runtime、session 或 raylib。`architecture.persistence_checkpoint_only`、`dungeon_no_persistence` 等边界测试通过。

## 测试覆盖

- V5 全字段/固定偏移/大小/小端/物品记录/摘要往返。
- started 载入可见；每类非法 enum、bool、保留位、mask、count、rules version、deterministic selection、CRC 拒绝。
- V1、V2、V3、V4（含物品记录）legacy bytes 可读且 migrated；V5 不 migrated。
- legacy door 确定性迁移；initial/descent/普通房清除且非深渊字段不漂移。
- A/B winner migrated 原样传播；损坏另一 slot 的 recovery 后不丢标记。
- 发布 final-scan 故障携带 V5 resolution 时仍为 indeterminate，不伪造 committed。
- `same_state` 覆盖所有 abyss checkpoint 与 last resolution 字段。

## 文件

- 新增：`src/dungeon/abyss_checkpoint_migration.hpp/.cpp`。
- 修改：`src/dungeon/dungeon_checkpoint.hpp`、`src/dungeon/CMakeLists.txt`。
- 修改：`src/persistence/checkpoint_codec.hpp/.cpp`、`save_store.hpp`、`save_store_detail.hpp`、`save_slot.cpp`、`save_recovery.cpp`、`CMakeLists.txt`。
- 修改测试：`checkpoint_codec_tests.cpp`、`save_store_tests.cpp`、`save_store_fault_tests.cpp`、`passive_tree_checkpoint_tests.cpp`、`persistence_test_main.cpp`。

## 自审与疑虑

- 自审确认无 runtime/raylib 修改、无 legacy decoder 内部规则派生、无 migrated OR/丢失、无 V4 item offset 漂移。
- `none` 不强制 `current_room.is_abyss=false`：任务不变量是单向的（available/started/cleared 必须为 true），且 Task 3 尚未替换旧 room flag 生成路径；none 本身仍严格禁止 rule/version/reward 状态。legacy helper 会把非法旧路径规范化为 false。
- 无阻塞疑虑。Task 4 仍需在创建 session 前原子发布 migrated helper 结果；本任务按约束未接入。

## 审查修复追加（Important + Minor）

- 修复提交：`41da4ad50176259e9b1e68937a022a77333f9cbf`。

### 修复内容

- V5 `cleared` 不再只接受任意 `1..3`：codec 使用确定性校验后的 `AbyssDanger` 和纯 `reward_profile_for`，严格要求 low=1、medium=2、high=3。
- `LastAbyssResolution` 不信任 payload 自报总数：由固定 `AbyssRuleId -> AbyssDanger -> reward count` 推导唯一 total，encode/decode 都拒绝错配。
- 为避免 persistence 与 migration 各自复制规则分组，最小修改 Task 1 的 `abyss_rules.hpp/.cpp`，把已有内部映射暴露为纯 `danger_for_rule(AbyssRuleId) -> optional<AbyssDanger>`；非法/none rule 返回空。persistence 仍只私有依赖 pure `arpg_abyss`。
- legacy migration 的合法门条件收紧为：`last_transition=door`、`last_direction!=none`，且入口必须是出口对侧：up→bottom、down→top、left→right、right→left；none 或错配一律清除 `is_abyss` 并规范化为普通房。

### 审查修复 TDD RED

先新增三档件数和门配对测试，再运行：

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cmake --build --preset windows-msvc-debug --target arpg_persistence_tests'
& .\out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe
```

输出：

```text
[FAIL] checkpoint_codec.v5 cleared reward total matches each danger:
  !persistence::encode_checkpoint(state).has_value()
[FAIL] checkpoint_codec.v5 resolution total matches each rule danger:
  !persistence::encode_checkpoint(state).has_value()
[FAIL] checkpoint_codec.legacy door migration rejects missing or mismatched direction:
  !migrated.current_room.is_abyss
67 cases, 3 failures
```

这三个失败分别证明旧实现接受 danger 件数错配、resolution rule 件数错配和非法门方向组合。

### 审查修复 GREEN 与回归

三档测试在固定 depth=40 下调用 `select_abyss_rule` 搜索明确的 low/medium/high seed；不再把任意 fixture 假设为 high。每档同时覆盖：正确 encode/decode、错误 encode 拒绝、篡改 payload 后刷新 CRC 的 decode 拒绝。门测试覆盖四个正确对侧组合以及 none/四个错配组合。

最终新鲜命令与输出：

```powershell
cmd.exe /d /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\VC\Auxiliary\Build\vcvars64.bat" && cmake --build --preset windows-msvc-debug --target arpg_persistence_tests arpg_abyss_tests arpg_dungeon_tests'
& .\out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe
ctest --preset windows-msvc-debug -R "^(persistence\.units|abyss\.units|architecture\.(dungeon_no_persistence|persistence_checkpoint_only|persistence_no_raylib|dungeon_no_raylib))$" --output-on-failure
ctest --preset windows-msvc-debug -R "^dungeon\.units$" --output-on-failure
```

```text
67 cases, 0 failures
100% tests passed, 0 tests failed out of 6
1/1 Test #4: dungeon.units ... Passed 147.26 sec
100% tests passed, 0 tests failed out of 1
```

### 审查修复自审

- 全部 9 个 rule ID 及 none/非法 ID 都直接覆盖共享 `danger_for_rule`；分组只在 pure abyss 模块维护一份。
- checkpoint 件数先校验 seed+depth 重建的 danger，再推导件数；resolution 只从 rule 推导，不读取当前房 danger。
- 错误件数测试同时验证 encoder 和带合法 CRC 的 decoder，无法借 CRC 提前失败掩盖语义缺陷。
- migration 的 API 仍只消费/返回 checkpoint 值类型；非法方向只修改深渊稳定状态，不漂移其他 legacy 字段。
- 无新增 runtime、raylib 或 session 行为；无阻塞疑虑。

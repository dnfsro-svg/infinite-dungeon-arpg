# Stage 11-D Task 4 报告

## 状态

完成 Production Snapshot Metadata and Runtime Mapping，并按真实 RED -> GREEN 执行 TDD。

## 实现

- `GroundItemSnapshot` 新增 `base_id`、`item_level`，快照直接复制已存在的生产 `ItemInstance` 字段。
- `DungeonSnapshot` 新增 `pending_pickup_ordinal`；仅 `loot_pickup` 与 `abyss_reward_claim` 从真实 `PendingSave::pickup_ordinal` 填值，其余 pending kind 保持 `nullopt`。
- `DungeonRuntime::fixed_tick` 接受可选 `AutoPickupPolicy` 并透传给 `DungeonSession::tick`；默认参数仍为 `normal`，兼容原有 show-all 调用者。
- host 边界新增 `loot_pickup_policy(settings::LootFilterMode)`，三档映射为 normal/magic/rare。
- 唯一 host fixed-tick 调用每步读取 `live_settings.loot_filter_mode`；未读取 `pause_menu.draft`。
- 未实现 ground label、renderer 或 feedback。

## TDD 证据

### RED

命令：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_platform_tests -- -j1
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
cmake -DSOURCE_ROOT=E:/game/.worktrees/stage11d-loot-filter -DGUARD_TEST_ROOT=E:/game/.worktrees/stage11d-loot-filter/out/build/windows-msvc-debug/tests/platform/stage11b-evidence-guard -P tests/platform/stage11b_settings_evidence_guard_test.cmake
```

预期失败：

- dungeon：`GroundItemSnapshot` 缺少 `base_id`/`item_level`，`DungeonSnapshot` 缺少 `pending_pickup_ordinal`。
- platform：缺少 `loot_pickup_policy`，`DungeonRuntime::fixed_tick` 不接受两个参数。
- host guard：`Stage11B evidence guard requires live loot policy behind host gate`。

说明：首次普通构建因工作树构建目录写权限/PDB 竞争失败，不计作 RED；加载 MSVC 开发环境并在沙箱外串行构建后取得上述有效 RED。

### GREEN

构建：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_platform_tests -- -j1
```

结果：成功链接 `arpg_dungeon_tests.exe` 与 `arpg_platform_tests.exe`。增量复验同样成功。

主 focused tests：

```powershell
ctest --preset windows-msvc-debug -R "^(dungeon.units|platform.units|stage11b.settings_evidence_guard)$"
```

提交前 fresh 结果：3/3 通过，0 失败；`dungeon.units` 180.84 秒，`platform.units` 3.20 秒，host evidence guard 0.01 秒。

补充 stress/architecture tests：

```powershell
ctest --preset windows-msvc-debug -R "^(stage10.abyss_stress.determinism_and_zero_alloc|platform.input_latency_source|stage11b.settings_evidence_guard_self_test|stage11c.architecture.hud_boundaries|stage11c.architecture.hud_boundaries_self_test)$"
```

结果：5/5 通过，0 失败。

## 文件

- `src/dungeon/dungeon_types.hpp`
- `src/dungeon/dungeon_snapshot.cpp`
- `src/platform/raylib/dungeon_runtime.hpp`
- `src/platform/raylib/dungeon_runtime.cpp`
- `src/platform/raylib/raylib_host.hpp`
- `src/platform/raylib/raylib_host.cpp`
- `tests/dungeon/dungeon_loot_drop_tests.cpp`
- `tests/dungeon/dungeon_abyss_reward_tests.cpp`
- `tests/dungeon/dungeon_stress_tests.cpp`
- `tests/platform/dungeon_runtime_tests.cpp`
- `tests/platform/platform_test_main.cpp`
- `tests/platform/stage11b_settings_evidence_guard_test.cmake`
- `.superpowers/sdd/task-4-report.md`

## 提交

主题：`feat: expose filtered ground loot snapshots`

## 自审

- 新 snapshot 字段只来自现有生产 ground item，不复制额外对象或新增分配。
- pending ordinal 分支严格限制为两种 pickup save kind；transition 等其他 save kind 已断言为无值。
- rare-only runtime 会保留 normal drop；省略 policy 的既有调用仍自动拾取 normal drop。
- source guard 同时要求 `live_settings` 映射并拒绝 `pause_menu.draft` 映射。
- dungeon stress 的 snapshot 等价比较已包含新增字段、source 与 abyss ordinal。
- 没有新增 dungeon -> platform 反向依赖；依赖方向仍为 platform 调用 dungeon。

## 顾虑

- 无已知功能顾虑。
- MSVC 构建必须加载 Visual Studio Developer Command Prompt；沙箱内现有 build 目录有写权限/PDB 锁限制，因此验证在获批的沙箱外串行执行。
- 构建日志中 `host_input_tests.cpp` 有既存 C4834 警告，本任务未修改该文件，测试仍全部通过。

## 审查修复：完整 host policy 调用守卫

### 发现与实现

- 原 guard 分别查找 `runtime.fixed_tick` 与 live-policy 片段，只验证位置顺序；`fixed_tick(step_movement, {})` 后放一个未使用的 live-policy 表达式即可通过。
- guard 现在先删除 host 源中的空白，再匹配完整调用：`runtime.fixed_tick(step_movement,loot_pickup_policy(live_settings.loot_filter_mode));`。
- 原有 fixed-tick 唯一路径计数和 host gate 顺序检查保持不变，没有放宽门禁。
- 完整 draft-policy 调用优先给出 `rejects draft loot policy in fixed_tick`，避免误报为缺少 live policy。
- self-test 新增两项 `HOST_OVERRIDE` mutation：draft policy；default `{}` policy + 未使用 live-policy 诱饵。
- mapper unit case 新增非法 `LootFilterMode(0xFF)` 回退到 `ItemRarity::normal` 的断言。

### 审查修复 RED

命令：

```powershell
ctest --preset windows-msvc-debug -R "^stage11b.settings_evidence_guard_self_test$" --output-on-failure
```

有效 RED：1/1 CTest 失败；旧 guard 错误接受 default-policy + live-policy 诱饵，self-test 报：

```text
evidence guard self-test accepted default policy with live-policy decoy
```

在调整 mutation 执行顺序前还确认：旧 guard 能拒绝 draft mutation，但错误报告 `requires live loot policy behind host gate`，未给出 draft 来源错误。

### 审查修复 GREEN

增量构建：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1
```

结果：2 个增量步骤成功，`arpg_platform_tests.exe` 链接成功。

focused tests：

```powershell
ctest --preset windows-msvc-debug -R "^(platform.units|stage11b.settings_evidence_guard|stage11b.settings_evidence_guard_self_test|platform.input_latency_source|stage11c.architecture.hud_boundaries|stage11c.architecture.hud_boundaries_self_test)$" --output-on-failure
```

结果：6/6 CTests 通过，0 失败，总耗时 22.92 秒：

- `platform.units`：通过，包含 244 个 unit cases。
- `stage11b.settings_evidence_guard`：通过。
- `stage11b.settings_evidence_guard_self_test`：通过；共覆盖 13 个 mutation scenarios，其中新增 2 个 loot-policy mutations。
- `platform.input_latency_source`：通过。
- `stage11c.architecture.hud_boundaries`：通过。
- `stage11c.architecture.hud_boundaries_self_test`：通过。

### 审查修复文件

- `tests/platform/dungeon_runtime_tests.cpp`
- `tests/platform/stage11b_settings_evidence_guard_test.cmake`
- `tests/platform/stage11b_settings_evidence_guard_self_test.cmake`
- `.superpowers/sdd/task-4-report.md`

### 审查修复提交

主题：`test: harden host loot policy evidence guard`

### 审查修复自审与顾虑

- 完整调用匹配绑定了 `fixed_tick` 第二参数与 `live_settings`，单独诱饵表达式不能满足门禁。
- draft 与 decoy mutations 都 fail closed，并校验具体失败原因。
- 未修改生产 runtime/snapshot 行为。
- 无已知功能顾虑；mutation 的输入替换按当前生产调用格式构造，若格式漂移导致替换失效，self-test 会失败而不是静默通过。

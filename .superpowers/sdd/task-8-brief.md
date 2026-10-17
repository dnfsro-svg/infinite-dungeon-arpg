### Task 8: 端到端回归、窗口验收和最终验证

**Files:**
- Modify: `tests/dungeon/dungeon_stress_tests.cpp`
- Modify: `tests/persistence/dungeon_save_integration_tests.cpp`
- Modify: `tests/platform/dungeon_runtime_tests.cpp`
- Create: `docs/stage7/2026-07-14-result.md`

**Interfaces:**
- Consumes: Tasks 1–7 的已提交接口。
- Produces: 1000 房间确定性星盘回放、保存恢复端到端证明、Debug/Release 绿灯和可视验收记录。

- [ ] **Step 1: 写端到端 RED 回归**

在 `dungeon_stress_tests.cpp` 增加固定根种子用例。先定义精确 trace 和相等比较：

```cpp
struct PassiveStressRecord final {
    std::uint64_t room_seed{};
    std::uint64_t allocated_bits{};
    std::uint64_t generation{};
    int hp{};
    int barrier{};
};

bool same_record(const PassiveStressRecord& left,
    const PassiveStressRecord& right) noexcept {
    return left.room_seed == right.room_seed
        && left.allocated_bits == right.allocated_bits
        && left.generation == right.generation
        && left.hp == right.hp && left.barrier == right.barrier;
}
```

用固定根种子运行两个独立 session 1000 房间；每 25 房清房后按 `8,9,10,22,23,24,36,37,38,50,51,52` 的循环选择下一合法节点并提交；每房记录上述字段。逐项断言两个 1000 项 trace 相等，并断言每条记录的 `allocated_bits` 与 generation 均非零。

在 `dungeon_save_integration_tests.cpp` 加入：分配 8→9→10，注入 before_publish/after_publish，重启后只能得到提交前或验证后的完整位图；不能得到“点数减少但 bit 未置位”或“bit 置位但点数未减少”。

- [ ] **Step 2: 运行 RED 测试**

Run:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_persistence_tests arpg_platform_tests
ctest --test-dir out/build/windows-msvc-debug -R "dungeon.units|persistence.units|platform.view_math" --output-on-failure
```

Expected: 在最终接线前，至少一个新端到端测试因缺少完整星盘 transaction 或 trace 记录接线而失败；记录该 RED 原因后继续。

- [ ] **Step 3: 补齐 trace 的确定性接线**

让压力 helper 每次加点都通过 `PendingSaveKind::passive_tree` 的 committed 收据，再进行门转换；记录必须从 `session.snapshot()` 读取，不能从待提交状态读取。为 trace 循环实现以下索引规则，避免随机或隐式容器顺序：

```cpp
constexpr std::array<passives::PassiveNodeId, 12> kRouteCycle{{
    8U, 9U, 10U, 22U, 23U, 24U, 36U, 37U, 38U, 50U, 51U, 52U}};
const passives::PassiveNodeId next = kRouteCycle[
    static_cast<std::size_t>((room_index / 25U) % kRouteCycle.size())];
```

当循环请求的节点已分配或不相邻时，按目录邻接表选择该节点的最小未分配邻居；若没有未分配邻居则不变更位图。测试必须仍完成 1000 房间，且两个 trace 完全相等。不得跳过 fault point、使用随机 seed 或降低房间数量。

- [ ] **Step 4: 运行完整 Debug 与 Release 验证**

Run:

```powershell
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-debug && cmake --build --preset windows-msvc-debug && ctest --preset windows-msvc-debug'
cmd /c 'call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && cmake --preset windows-msvc-release && cmake --build --preset windows-msvc-release && ctest --preset windows-msvc-release'
git diff --check
git status --short
```

Expected: Debug 与 Release 全部 CTest 通过，`git diff --check` 无输出；只允许本任务有关的已跟踪修改，保留用户已有未跟踪文件不处理。

- [ ] **Step 5: 可视验收、记录与提交 Task 8**

启动 `out/build/windows-msvc-release/bin/arpg_game.exe`，清房后按 P，点击路线入口 8，再点击 9、10；确认覆盖层显示已分配连线、未分配点减少、保存指示完成，点击 10 退款，确认点数返还；关闭 P 并选门，下一房显示相同构筑。再在重启后确认位图保留。

把 Debug/Release CTest 计数、1000 房间 trace 结果、窗口的 P 键/加点/退款/重启观察和已知限制写入 `docs/stage7/2026-07-14-result.md`。历史重构报告继续保持“当时未实施 Stage 7”的事实，不修改它。

```powershell
git add tests docs/stage7/2026-07-14-result.md
git commit -m "test: verify passive star chart integration"
```

## Plan Self-Review

- 规格覆盖：Task 1–3 覆盖五分量、抗性、通用属性与四关键节点；Task 2 覆盖 64 节点与连通；Task 4–6 覆盖存档迁移与双槽事务；Task 7 覆盖 P 键和全屏 UI；Task 8 覆盖压力、窗口、Debug/Release 验收。
- 占位扫描：没有未决占位词或含糊实现指令；每个代码任务列出路径、接口、RED、GREEN、命令与提交。
- 类型一致性：`passives` 只输出 `modifiers::PlayerModifierValues`；`combat` 只接收 `PlayerCombatBuild`；`dungeon` 持有 `PassiveTreeState`；`persistence` 只编码该状态；`platform` 只请求 Session mutation。

## Execution Handoff

计划已完成并保存至 `docs/superpowers/plans/2026-07-14-stage7-passive-star-chart.md`。执行方式二选一：

1. **子代理分任务执行（推荐）**：每个任务使用独立子代理，主代理在任务间审查与验证。
2. **当前会话串行执行**：使用 `executing-plans` 按任务分批执行并设检查点。

Which approach?

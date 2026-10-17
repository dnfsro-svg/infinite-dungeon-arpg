# Stage 11-A Task 11 实现报告

## 范围与提交

- 基线：`f4d37e2a806e86e59f783e6d063f96e4190e6c29`。
- 提交：`4bd191d1e645e21b2fb5bb8579bb8aefaad7aefa`（`test: stress death continue determinism`）。
- 独立审查修复：`5cf547b1d46b45defe8b983b8b7c223bb6512eb4`（`fix: strengthen death stress guards`）。
- 仅实现 1000 次死亡/继续确定性压力、零分配修复与架构护栏；未开始 Task 12，也未加入真实窗口验证钩子。

## RED

先注册专用 suite/CTest，并以未固定 golden 值运行，得到有效行为 RED；压力路径随后暴露 MSVC Debug 下死亡 prepare/validate/commit 的临时 `DungeonRunState`、`PendingSave` 与 item vector 分配。

首次零分配修复的完整 `dungeon.units` 回归又得到两个有效 RED：

```text
dungeon_loot_drop.pickup commit no allocation
dungeon_abyss_reward.abyss claim full oom overflow atomic
254 cases, 2 failures
```

原因是把 item capacity 补偿放在 commit 热路径，破坏了既有零分配和 OOM 原子性；随后将 capacity 准备前移到 request/prepare 阶段。

最终自审移除压力测试对既有 `DungeonSessionTestAccess` 包装器的依赖后，公开 Combat tick 首轮还得到两次有效 RED：一个随机浅层 encounter 不能保证 20000 tick 内致死，以及每 tick 构造完整 snapshot 导致 CTest 300 秒超时。修复为按浅层/深层使用已验证可致死的 production encounter seed，并用公开 `pending_save_view()` 判断死亡；没有提高 300 秒超时，也没有降低 1000 次或 17/31/43 节奏。

独立审查后新增 focused drop-stream 断言，先证明 seed=1、ordinal=0 时旧 golden helper 与生产的首个 ordinal key 复用规则不一致；生产等价值为 `7af3c1e15bc7b138/563976ed1fc9f0c6/a9e93dbac431f276`。guard mutation probe 也先得到 `failed to reject overlay_renderer mutation` 的有效 RED，再扩展真实扫描范围与 fixture 白名单。

## 实现

- `ARPG_STAGE11_DEATH_STRESS_ONLY=1` 只运行 Task 11 suite，CTest 名为 `stage11.death_stress.determinism_zero_alloc`，标签为 `headless;dungeon;stage11;stress`，`TIMEOUT 300`。
- 两条相同根轨迹各执行 1000 次完整死亡/继续：混合 depth-1 clamp、深层普通死亡和正式 started abyss 死亡。
- 死亡只由公开 `DungeonSession::tick()` 驱动真实 Combat 到 player defeat；之后走公开 pending save、committed receipt、V6 codec round-trip、Session reconstruction 和 death continue。
- restart 轨迹严格执行每 17 次 V6 round-trip、每 31 次 pending-death 重启、每 43 次 continue 后重启；计数为 `58/32/23`。
- 比较全部 1000 个完整 death checkpoint、target descriptor/seed 与累计 hash；drop golden 与生产 `drop_stream` 一样，为 chance/slot/content 三个 domain 复用同一个首个 ordinal key。door/descent/abyss/drop stream 在循环前后均为 `0xe999456db183d756`。
- 第 500 次通过最小 `DungeonDeathStressFixture` 只填满 192 个 production ground slots；fixture 不访问 Combat、stable/pending/death/phase/diagnostics，后续死亡与事务仍全部走公开 API。验证 ground 不进入 checkpoint、死亡提交后 ground 清空且 saturation diagnostic 不漂移。
- allocation probe 覆盖公开 Combat ledger/致死/死亡 prepare tick 以及 death/continue commit，最终为 `0/0/0`。working set 只打印诊断，不设阈值。
- 用可复用 pending storage 和 death validation scratch 消除死亡热路径临时动态容器；发布 stable state 时交换已预留 item vector。item 数增长所需 reserve 前移到 request/prepare，commit 继续保持零分配和 OOM 原子性。
- death target validator 改用固定字段 overload，death checkpoint equality 改为无临时 run state 的穷举字段比较。
- guard 覆盖 Combat、death codec、全部 `death_overlay*.hpp/cpp`、生产头文件与实现中的 TestAccess 调用，并直接扫描压力源，禁止既有 TestAccess/kill/fill/handle/private state token。
- guard 解析 `DungeonDeathStressFixture` body 中全部 `session.<identifier>`，只白名单 `session.ground_items_`；内置 mutation probes 证明 renderer SaveStore、overlay header stable state、production header TestAccess invocation 以及 fixture 的 combat/phase/diagnostics 私有访问都会被拒绝。

## GREEN

精确 Task 11 命令：

```text
stage11.death_stress.determinism_zero_alloc  Passed 123.52 sec
stage11.architecture.death_boundaries        Passed   0.29 sec
2/2 passed, total 123.82 sec
```

直接压力输出：

```text
[stage11-death-streams] hash=0xe999456db183d756
[stage11-death-stress] deaths=1000 hash=0x84d4dd1435805ece reloads=58/32/23 allocations=0/0/0 working-set=3796992->5410816
2 cases, 0 failures
```

相关回归：

- `dungeon.units`：254/254 passed，494.30 秒。
- `persistence.units` + `platform.units`：2/2 passed，3.40 秒。
- 既有 `architecture.*`：18/18 passed，94.01 秒。
- `git diff --cached --check`：无错误；提交后 tracked 工作树干净。

## 自审结论

- 没有降低 1000 次、17/31/43 cadence、零分配或 300 秒超时。
- 压力文件不 include `dungeon_test_support.hpp`，也不调用 `DungeonSessionTestAccess` / `CombatWorldTestAccess` 或其 kill/fill/handle 包装器。
- ground fixture 只建立无法由当前公开 gameplay API 确定性构造的满池前置条件，没有注入 death snapshot、stable state 或 pending save。
- 未发现 Task 12 范围漂移。

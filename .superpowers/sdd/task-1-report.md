# Stage 11-A Task 1 实现报告

## 状态与提交

- 状态：完成
- 提交：`d803f5f` (`feat: add fixed player damage history`)
- 范围：仅 Combat 承伤逐类型解析、旧总伤害兼容包装、300 tick 固定账本及其测试。

## 实现内容

1. 在 `combat_types.hpp` 新增 `ResolvedPlayerDamage` 与 `resolve_player_damage_packet()` 声明。
2. 将既有玩家承伤计算拆成五种伤害类型分别解析：
   - 护甲只处理物理伤害；
   - 火、水、电、混沌分别应用既有元素减伤、默认 7500/最高 9500 上限；
   - 每个减伤后分量最后应用 `damage_taken`；
   - `total` 是五个 `uint64_t` 分量的饱和和。
3. 保留 `resolve_player_damage()`，其调用新解析器并只在 `total <= INT_MAX` 时返回 `int`，保持既有调用 API。
4. 新增 `PlayerDamageHistory`：
   - 编译期固定 300 个桶，每桶固定五个 `uint64_t`；
   - 同 tick 的多次记录逐类型饱和累加；
   - 向前推进时逐一清理跨过的环形桶；
   - tick 倒退或一次跳跃达到 300 tick 时清空全部桶；
   - 汇总跨桶饱和累加；
   - 不使用动态容器、字符串、异常或堆分配。
5. 在 Combat 源与测试 CMake 中注册新文件，并在 Combat 测试入口注册 9 个新用例（总计 177）。

## 需求澄清

Task 1 简报同时要求说明资源上限裁剪，但规定的 `PlayerDamageHistory::record(const ResolvedPlayerDamage&)` 没有 HP/barrier 参数。经主代理确认采用边界 A：Task 1 的账本只累计调用方传入的、已经裁剪后的逐类型实际伤害；HP+barrier 裁剪和按比例分配由 Task 2 在调用 `record()` 前完成。本提交未新增裁剪 helper/overload，也未将历史接入 `CombatWorld`。

## TDD 证据

### RED 0：环境预检（不计功能 RED）

命令：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
```

普通 PowerShell 缺少 SDK 环境，CMake 在功能编译前退出：

```text
Stage 0 requires Windows SDK 10.0.26100.0; detected ''
```

随后使用仓库 `scripts/Configure.ps1` 进入固定 MSVC 19.44 / SDK 26100 环境。

### RED 1：新 API 缺少实现

命令：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
```

关键输出：

```text
player_damage_history_tests.cpp.obj : error LNK2019: unresolved external symbol resolve_player_damage_packet
player_damage_history_tests.cpp.obj : error LNK2019: unresolved external symbol PlayerDamageHistory::begin_tick
player_damage_history_tests.cpp.obj : error LNK2019: unresolved external symbol PlayerDamageHistory::record
player_damage_history_tests.cpp.obj : error LNK2019: unresolved external symbol PlayerDamageHistory::totals
fatal error LNK1120: 4 unresolved externals
```

失败原因与目标功能缺失一致，测试本身成功编译。

### RED 2：倒退/大跳清理分支

自审发现初始测试只证明逐 tick 300/301 淘汰，未单独驱动“倒退或跳跃 300 tick 全清”。先移除未被测试驱动的清空循环，加入两个测试后运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --preset windows-msvc-debug -R '^combat\.units$' --output-on-failure
```

关键输出：

```text
[FAIL] player_damage_history.backward tick resets history
[FAIL] player_damage_history.300 tick jump resets history
177 cases, 2 failures
```

恢复最小全桶清空逻辑后两项转绿。

### GREEN 与回归

最终新鲜验证命令：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --preset windows-msvc-debug -R '^combat\.units$' --output-on-failure
git diff --check
```

关键输出：

```text
ninja: no work to do.
1/1 Test #3: combat.units ... Passed
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 0.84 sec
```

此前直接运行 focused executable 的输出为：

```text
177 cases, 0 failures
```

`git diff --check` 退出 0；只有 Git 的 LF 将来可能转换为 CRLF 的提示，无空白错误。

## 文件列表

- `src/combat/player_damage_history.hpp`（新增）
- `src/combat/player_damage_history.cpp`（新增）
- `src/combat/combat_types.hpp`（修改）
- `src/combat/combat_world.cpp`（修改）
- `src/combat/CMakeLists.txt`（修改）
- `tests/combat/player_damage_history_tests.cpp`（新增）
- `tests/combat/combat_test_main.cpp`（修改）
- `tests/combat/CMakeLists.txt`（修改）

## 自审

- 逐项核对简报：五类型结果、物理/元素减伤边界、`damage_taken` 最后应用、饱和总和、同 tick 累计、300/301 淘汰、倒退/大跳全清、`INT_MAX`、重复累计饱和、零分配均有覆盖。
- `PlayerDamageHistory` 对象只含固定数组与标量；热路径均为有界循环，无动态分配。
- 旧 `resolve_player_damage()` API 保留，既有 Combat 测试全部通过。
- 未接入死亡来源/冻结，未修改 Dungeon、Persistence、raylib，也未触碰无关文件。

## 疑虑

无阻塞疑虑。唯一接口边界已澄清：Task 2 必须在写账本前完成 HP+barrier 上限裁剪与逐类型实际承伤分配。

---

## 审查修复：保持旧总伤害缩放语义

### 状态与提交

- 修复提交：`623d9f7` (`fix: preserve player damage total semantics`)
- 本节修正并取代上文“每个减伤后分量分别应用 `damage_taken`”的实现描述。

### Finding 核验

审查 finding 已在代码中复现：旧实现对两个减伤后均为 1 的分量分别应用 `damage_taken=5000`，两个分量各自向下截断为 0，导致 `total=0`；原有 `resolve_player_damage()` 的语义是先汇总为 2，再统一缩放 50%，结果应为 1。

### RED

先新增回归：物理和火焰的减伤后分量均为 1，`damage_taken=5000`；要求兼容总值为 1，逐类型总和为 1，并按物理→火→水→电→混沌的固定顺序把余数分给物理。

命令：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
ctest --preset windows-msvc-debug -R '^combat\.units$' --output-on-failure
```

关键输出：

```text
[FAIL] player_damage_history.damage taken distributes remainder:
resolved->total == 1U
178 cases, 1 failures
0% tests passed, 1 tests failed out of 1
```

失败点与审查 finding 精确一致，不是编译、链接或测试拼写错误。

### GREEN 实现

1. 五类伤害先分别完成护甲/元素减伤，保留尚未应用 `damage_taken` 的 `uint64_t` 分量。
2. 先对五个分量求和，再使用既有 `checked_multiply(int64_t)` 对总和统一应用 `damage_taken`，保持旧总伤害的截断与溢出语义。
3. 逐类型结果按 `floor(component * compatible_total / reduced_total)` 比例向下分配。
4. 为避免 `component * compatible_total` 溢出，使用固定、有界、零分配的商/余数倍增算法计算乘除结果，不依赖更宽整数或浮点数。
5. 比例向下分配后的剩余单位按物理→火→水→电→混沌顺序补给非零来源分量；最终 `by_type` 饱和和精确等于兼容 `total`。
6. 将测试名称从 `records clipped damage` 改为 `records caller-provided damage`，不暗示 Task 1 验证了资源裁剪。

### 最终验证

命令：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_combat_tests
& .\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
ctest --preset windows-msvc-debug -R '^combat\.units$' --output-on-failure
git diff --check
```

关键输出：

```text
ninja: no work to do.
178 cases, 0 failures
1/1 Test #3: combat.units ... Passed
100% tests passed, 0 tests failed out of 1
Total Test time (real) = 0.83 sec
```

`git diff --check` 退出 0；仅有 Git 的 LF 将来可能转换为 CRLF 的提示。

### 修复文件

- `src/combat/combat_world.cpp`
- `tests/combat/player_damage_history_tests.cpp`
- `tests/combat/combat_test_main.cpp`

### 自审与疑虑

- 旧包装现在对所有可表示输入恢复“先汇总减伤分量，再统一应用 `damage_taken`”的计算顺序；最终仍执行 `INT_MAX` 范围检查。
- 比例分配全程使用整数；五个向下取整份额的余数严格小于非零类型数量，固定顺序单次补余数即可结束。
- 统一缩放乘法继续走 checked overflow 路径并保持 `noexcept`；热路径未引入动态分配。
- 未扩大到 Task 2 的 HP/barrier 裁剪、死亡来源或冻结逻辑。
- 无阻塞疑虑。

# Stage 10 Task 1 实现报告

## 状态

DONE

## 实现内容

- 新增纯 `arpg_abyss` 静态库，依赖方向为 `arpg_abyss -> arpg_core`，位于顶层 CMake 的 `src/core` 之后、`src/combat` 之前。
- 固化 `kAbyssRulesVersion`、`AbyssDanger`、`AbyssRuleId`、`AbyssLifecycle`、`AbyssCombatConfig` 和 `AbyssSelection` 的稳定值。
- 使用既有 `ABYSS_01` 1% 命名域，并为规则版本、危险等级和规则槽建立独立 64 位命名域。
- 实现四段危险权重、每档三条规则等权抽取、预算 `ceil(1.5x)`（`uint8_t` 饱和）、深度词缀下限、资源比例映射和实际最大生命百分比向上取整。
- 实现九条规则的战斗倍率配置。
- 实现低/中/高危奖励件数、物品等级加成及 100 级上限。
- 实现保持总和 100 的稀有度权重转移；普通权重不足时按最大余数法按原比例重分配，平局稳定优先蓝装。
- 新增 28 个纯函数测试，覆盖任务简报要求的全部边界与确定性行为。

## 文件

- 修改：`CMakeLists.txt`
- 新增：`src/abyss/CMakeLists.txt`
- 新增：`src/abyss/abyss_types.hpp`
- 新增：`src/abyss/abyss_rules.hpp`
- 新增：`src/abyss/abyss_rules.cpp`
- 新增：`src/abyss/abyss_rewards.hpp`
- 新增：`src/abyss/abyss_rewards.cpp`
- 新增：`tests/abyss/CMakeLists.txt`
- 新增：`tests/abyss/abyss_test_main.cpp`
- 新增：`tests/abyss/abyss_rules_tests.cpp`
- 新增：`tests/abyss/abyss_rewards_tests.cpp`

## RED

首次直接配置发现当前普通 PowerShell 未加载 MSVC：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests
```

前置环境失败：`No CMAKE_C_COMPILER could be found` / `No CMAKE_CXX_COMPILER could be found`。加载 VS 2022 BuildTools 开发环境后重新执行同一配置与构建命令，得到预期 RED：

```text
error LNK2019: unresolved external symbol arpg::abyss::select_abyss_rule(...)
error LNK2019: unresolved external symbol arpg::abyss::map_resource_ratio(...)
error LNK2019: unresolved external symbol arpg::abyss::shift_abyss_rarity_weights(...)
fatal error LNK1120: 9 unresolved externals
```

预期原因：测试、公共类型、接口声明与 CMake 已接入，但 `abyss_rules.cpp` / `abyss_rewards.cpp` 尚无接口定义，证明测试先于实现失败。

## GREEN

聚焦构建与运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests
& .\out\build\windows-msvc-debug\bin\arpg_abyss_tests.exe
```

输出：

```text
[5/5] Linking CXX executable bin\arpg_abyss_tests.exe
28 cases, 0 failures
```

完成乘法中间值 Refactor 后的新鲜验证：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests arpg_core_tests arpg_combat_tests
& .\out\build\windows-msvc-debug\bin\arpg_abyss_tests.exe
ctest --test-dir .\out\build\windows-msvc-debug -R '^(core|combat|abyss)\.units$' --output-on-failure
```

输出：

```text
28 cases, 0 failures
100% tests passed, 0 tests failed out of 3
```

## 自审

- 确认稳定枚举数值与任务简报逐项一致，未重排。
- 确认四段权重边界覆盖 `1/9/10/19/20/39/40/UINT64_MAX`，每档三条规则均可达。
- 确认危险流和规则流分别派生，规则槽不受危险权重变化影响；同输入逐字段稳定。
- 确认所有生产代码乘法先提升到 `std::uint64_t`，资源映射和百分比伤害均使用整数公式，无浮点运算。
- 确认稀有度输入总和不是 100 或危险枚举非法时显式返回 `nullopt`。
- 确认 `src/abyss` 未包含 dungeon、persistence 或 raylib 头文件；`arpg_abyss` 只链接 `arpg_core`。
- 确认没有修改 combat/dungeon/persistence，未开始 Task 2。
- `git diff --check` 无空白错误；仅报告工作区现有 LF/CRLF 转换提示。

## 疑虑

- 简报的 `Consumes` 行列出 `modifiers::DamageType`，但 Task 1 给出的稳定类型和全部接口均没有 DamageType 参数或字段；正式计划把元素伤害包的使用放在后续 combat 环境规则任务。因此本任务没有添加未使用的 modifiers 头依赖。若后续任务需要把伤害类型下沉到规则配置，应由对应任务以测试先行扩展。
- 构建 combat 回归时，既有 `tests/core/allocation_probe.cpp` 对象出现 `/EHsc` 的 C4530 告警；本任务聚焦的 `arpg_abyss_tests` 构建无告警，且该既有告警不来自本次文件。

---

## 审查修复：完整已求值环境规则配置

### 修复内容

- 按正式设计的模块边界扩展 `AbyssCombatConfig`，加入从规则稳定派生的 `danger` 与 `environment` 值对象，后续 combat 不再需要按 rule ID 硬编码环境规格。
- 新增 abyss 自有稳定枚举 `AbyssDamageType`，明确表示 `none/fire/lightning/chaos`。它保持 `arpg_abyss -> arpg_core` 的唯一链接依赖；后续 combat 在消费边界显式转换为 `modifiers::DamageType`。
- 新增 trivially-copyable `AbyssEnvironmentConfig`，固定携带伤害 basis points、周期、预警、持续、伤害间隔、扩张间隔、半径档数，以及五档 `radius_milliunits` 固定数组。
- 雷暴完整配置：lightning、15%、180 tick 周期、45 tick 预警、0.8 半径。
- 追猎烈焰完整配置：fire、10%、240 tick 周期、45 tick 预警、1.0 半径、180 tick 持续、60 tick 伤害间隔。
- 混沌扩散完整配置：chaos、8%、1.0/2.3/3.6/4.9/6.2 五档半径、180 tick 扩张、60 tick 伤害间隔。
- 六条非环境规则使用 `AbyssDamageType::none` 且其他环境字段全零，形成明确 sentinel。
- 修正低/中/高规则可达性测试：不再用 `rule_id / 3` 只验证类别，而是逐档校验精确三条规则集合并确认每条均可达。
- 测试总数由 28 增至 32；新增四个独立环境配置测试，并保留既有规则、奖励和确定性覆盖。

### 审查修复 RED

先只增加精确测试并运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests
```

预期失败摘录：

```text
error C2039: "AbyssEnvironmentConfig": is not a member of "arpg::abyss"
error C2039: "environment": is not a member of "arpg::abyss::AbyssCombatConfig"
error C2039: "danger": is not a member of "arpg::abyss::AbyssCombatConfig"
error C2039: "AbyssDamageType": is not a member of "arpg::abyss"
ninja: build stopped: subcommand failed.
```

失败原因正是审查指出的公共纯值类型和已求值环境字段缺失；不是测试拼写或环境错误。

### 审查修复 GREEN

聚焦测试：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests
& .\out\build\windows-msvc-debug\bin\arpg_abyss_tests.exe
```

输出：

```text
[6/6] Linking CXX executable bin\arpg_abyss_tests.exe
32 cases, 0 failures
```

新鲜局部与必要回归：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests arpg_core_tests arpg_combat_tests
& .\out\build\windows-msvc-debug\bin\arpg_abyss_tests.exe
ctest --test-dir .\out\build\windows-msvc-debug -R '^(core|abyss|combat)\.units$' --output-on-failure
```

输出：

```text
32 cases, 0 failures
100% tests passed, 0 tests failed out of 3
```

### 审查修复自审与疑虑

- `AbyssEnvironmentConfig` 与扩展后的 `AbyssCombatConfig` 均由编译期测试确认 trivially copyable；只使用固定数组，无堆容器。
- `src/abyss` 未包含 modifiers、dungeon、persistence 或 raylib 头；`arpg_abyss` 仍只链接 `arpg_core`。
- 半径以千分之一世界单位存储，字段名 `radius_milliunits` 明确缩放，避免规则核引入浮点时序或平台差异。
- 原报告关于未携带元素语义的疑虑已由稳定 `AbyssDamageType` 和完整环境值对象解决；当前无遗留疑虑。

---

## 二次审查修复：直接消费 `modifiers::DamageType`

> 本节替代上一节关于 abyss 自有 `AbyssDamageType` 的实现说明；平行枚举已删除。

### 修复内容与文件

- 修改 `src/abyss/abyss_types.hpp`：直接包含稳定纯值头 `modifiers/damage_types.hpp`；删除 `AbyssDamageType`；`AbyssEnvironmentConfig::damage_type` 改为 `modifiers::DamageType`，增加 `bool active`。
- 修改 `src/abyss/abyss_rules.cpp`：雷暴、追猎烈焰和混沌扩散分别直接写入 `modifiers::DamageType::lightning/fire/chaos`，同时设置 `active=true`。
- 修改 `tests/abyss/abyss_rules_tests.cpp`：直接断言三种 `modifiers::DamageType`；六条非环境规则断言 `active=false`。
- 非环境配置的默认 damage type 为 `physical`，但 `active=false` 时该字段不允许被消费；其他环境数值仍全零。

### 二次审查 RED

先只修改测试，运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests
```

预期失败摘录：

```text
error C2039: "active": is not a member of "arpg::abyss::AbyssEnvironmentConfig"
error C2676: binary '==': 'const arpg::abyss::AbyssDamageType' does not define this operator
ninja: build stopped: subcommand failed.
```

失败原因：生产类型仍暴露平行 `AbyssDamageType` 且没有显式环境激活位，无法满足测试要求的 `modifiers::DamageType` API。

### 二次审查 GREEN

最小实现后聚焦运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests
& .\out\build\windows-msvc-debug\bin\arpg_abyss_tests.exe
```

输出：

```text
[6/6] Linking CXX executable bin\arpg_abyss_tests.exe
32 cases, 0 failures
```

新鲜局部与必要回归：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_abyss_tests arpg_core_tests arpg_combat_tests
& .\out\build\windows-msvc-debug\bin\arpg_abyss_tests.exe
ctest --test-dir .\out\build\windows-msvc-debug -R '^(core|abyss|combat)\.units$' --output-on-failure
```

输出：

```text
32 cases, 0 failures
100% tests passed, 0 tests failed out of 3
```

### 二次审查自审与疑虑

- `modifiers/damage_types.hpp` 只含稳定枚举和 constexpr 辅助函数，无链接符号；公共头包含不会要求链接 `arpg_modifiers`。
- `src/abyss/CMakeLists.txt` 未修改，`arpg_abyss` 仍只 `PRIVATE` 链接 `arpg_core`；未产生 `abyss -> modifiers` target 依赖或循环。
- `rg` 确认 `src/` 与 `tests/` 已无 `AbyssDamageType` 残留。
- `AbyssEnvironmentConfig` 和 `AbyssCombatConfig` 的 trivially-copyable 编译期测试继续通过；固定数组、无堆、无 raylib/dungeon/persistence 依赖。
- 当前无遗留疑虑。

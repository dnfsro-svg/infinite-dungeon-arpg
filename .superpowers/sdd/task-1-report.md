# Stage 4 Task 1 实施报告

## 状态

DONE。用户授权使用临时、数据化的 Stage 4 灰盒值，并由主控提供最终 8 行表。
这些值只用于建立稳定公共数据契约和后续预算接入，均可调，不是正式平衡。

## 实现

- 新建 `tests/combat/monster_catalog_tests.cpp`，包含简报指定的两条目录契约测试。
- 修改 `tests/combat/CMakeLists.txt` 注册测试源文件。
- 修改 `tests/combat/combat_test_main.cpp` 注册 suite，并将预期用例数从 32 更新为 34。
- 在 `combat_types.hpp` 增加稳定 ID、标签位、定义/刷怪/波次结构及 5 个固定容量常量。
- 在 `monster_catalog.cpp` 以单个 `constexpr std::array<MonsterDefinition, 8>` 保存全部灰盒值。
- `monster_definition` 对合法 ID 返回稳定条目指针，对 `count`/越界值返回 `nullptr`。
- 编译期与运行时共用目录校验：拒绝 ID/索引不一致、生态大于 3、零预算、非正 HP，
  并要求目录至少包含一个 `direct_target`。
- combat 未引入 dungeon、persistence、raylib、ECS 或 actors 依赖。

## 临时灰盒目录（可调，非正式平衡）

字段顺序：`id / ecology / tags / cost / hp / break / speed / range / telegraph / active /
recovery / cooldown / damage / projectile_speed / hazard_ticks / feedback`。

- fire_bomber / 0 / high_priority|direct_target / 3 / 220 / 0 / 0.040 / 1.2 / 60 / 1 / 0 / 0 / 120 / 0 / 0 / heavy
- fire_charger / 0 / melee|high_priority|direct_target / 4 / 420 / 60 / 0.030 / 5.0 / 45 / 18 / 35 / 100 / 90 / 0 / 0 / heavy
- water_bulwark / 1 / melee|direct_target / 4 / 700 / 120 / 0.025 / 1.0 / 24 / 6 / 30 / 72 / 70 / 0 / 0 / heavy
- water_support / 1 / support / 3 / 300 / 0 / 0.030 / 4.0 / 36 / 1 / 24 / 120 / 0 / 0 / 0 / medium
- lightning_shooter / 2 / ranged|direct_target / 3 / 240 / 0 / 0.035 / 4.5 / 30 / 1 / 20 / 75 / 40 / 0 / 0 / medium
- lightning_dasher / 2 / melee|high_priority|direct_target / 3 / 280 / 0 / 0.050 / 3.0 / 24 / 12 / 30 / 80 / 60 / 0 / 0 / medium
- chaos_chaser / 3 / melee|direct_target / 2 / 260 / 0 / 0.045 / 0.9 / 12 / 4 / 18 / 42 / 45 / 0 / 0 / light
- chaos_hazard / 3 / ranged|high_priority|ground_hazard|direct_target / 4 / 320 / 0 / 0.030 / 4.0 / 45 / 1 / 25 / 120 / 35 / 0 / 180 / heavy

## RED 命令与关键输出

命令：

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
```

结果：退出码 1，符合“API 尚不存在”的预期失败。

```text
[2/3] Building CXX object ... monster_catalog_tests.cpp.obj
FAILED: [code=2] ... monster_catalog_tests.cpp.obj
monster_catalog_tests.cpp(3): fatal error C1083:
无法打开包括文件: “combat/monster_catalog.hpp”: No such file or directory
ninja: build stopped: subcommand failed.
```

## GREEN / 完整测试结果

首次 GREEN 构建捕获两个机械问题：四标签 helper 的声明顺序，以及测试宏内模板逗号；
调整 helper 顺序并给断言表达式加括号后，以下验证全部通过。

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-core-debug
```

退出码 0；7 个编译/链接步骤完成，无编译警告。

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug
.\out\build\windows-msvc-core-debug\bin\arpg_combat_tests.exe
```

退出码 0；CTest 12/12 通过、0 失败；combat 可执行文件输出 `34 cases, 0 failures`。
8 个 architecture 测试全部通过，其中包括 combat 不依赖 raylib/dungeon/persistence。

## 当前文件

- `tests/combat/monster_catalog_tests.cpp`（新增）
- `tests/combat/CMakeLists.txt`（修改）
- `tests/combat/combat_test_main.cpp`（修改）
- `src/combat/monster_catalog.hpp`（新增）
- `src/combat/monster_catalog.cpp`（新增）
- `src/combat/combat_types.hpp`（修改）
- `src/combat/CMakeLists.txt`（修改）

## 自审与关注点

- RED 测试先于任何生产代码；预期缺失头文件编译失败已实际记录。
- 修改范围仅涉及简报允许文件；报告位于简报指定路径。
- 未删除文件、未重置 Git、未覆盖无关修改。
- `git diff --check` 无错误（仅 Git 提示未来可能执行 LF→CRLF 转换）。
- 明确文件依赖扫描无 dungeon/persistence/raylib/ECS/actor 命中。
- `water_support` 的权威 tags 仅为 support，因此“直接战斗目标”校验按整个目录至少存在一个
  direct_target 实现；其余 7 个条目提供直接目标。
- 数值为临时灰盒值，应在后续平衡阶段集中修改 `monster_catalog.cpp`，无需改公共契约。

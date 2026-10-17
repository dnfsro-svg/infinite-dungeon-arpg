# CPLAY-021 Task 1 报告：固定容量装备/材料吸入状态与轨迹

## 结果

- 新增 `LootSuctionState`：12 个固定槽位、无动态分配、`observe` / `update` / `build_plan` / `clear` / `active_count`。
- 装备仅在已提交回执更新、前帧存在同 `item_id`、当前帧消失时入列；错误、恢复与故障状态只推进高水位，不生成动画。
- 材料仅在已提交的新材料回执、相同房间实例、并且对应 ordinal 从前帧消失时入列；回执数量限制每种材料的入列数。
- 轨迹为 0.45 秒 smoothstep 收束并叠加正弦上拱；目标每次按传入的当前玩家插值位置投影，偏移到腰后，而非屏幕固定位置。
- 到达后回收槽位并产生短暂 destination pulse；暂停不推进时间。

## RED

执行：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 4
$env:ARPG_CPLAY021_LOOT_SUCTION_ONLY='1'
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

结果：预期失败，新增测试编译时直接报

```text
fatal error C1083: 无法打开包括文件: “loot_suction_animation.hpp”: No such file or directory
```

这是缺失目标接口导致的 RED；该命令在编译失败后仍调用了先前遗留的测试二进制，其全量输出不作为本任务验证结果。

## GREEN

执行：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 4
$env:ARPG_CPLAY021_LOOT_SUCTION_ONLY='1'
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
```

结果：构建成功，定向测试输出：

```text
6 cases, 0 failures
```

按任务边界未运行全量测试，也未启动游戏。

## 改动文件

- `src/platform/raylib/loot_suction_animation.hpp`
- `src/platform/raylib/loot_suction_animation.cpp`
- `src/platform/raylib/CMakeLists.txt`
- `tests/platform/loot_suction_animation_tests.cpp`
- `tests/platform/CMakeLists.txt`
- `tests/platform/platform_test_main.cpp`

## 自审

- `git diff --check` 通过；仅修改 task brief 列出的六个代码/测试文件，报告文件按任务要求单独保留。
- 未修改 `CombatRenderer`；未触及掉率、拾取事务、存档或药水逻辑。
- `ARPG_CPLAY021_LOOT_SUCTION_ONLY=1` 只运行本任务的 6 个用例。
- 12 槽上限、重复回执、保存错误、仍在地面、材料实际消失、暂停、回收脉冲及 100k 次稳态零分配均由定向测试覆盖。

## 审查修复：满槽确定性替换

审查发现首个实现满 12 槽后会直接丢弃后续飞行。容量测试已改为先生成 12 条拥有不同 elapsed 和不同 world 起点的飞行，再触发第 13 条；它断言第 1 条（elapsed 最大）被替换、第 13 条起点存在、活动数仍为 12。

### RED

执行同一 CPLAY-021 定向命令后，得到预期失败：

```text
[FAIL] loot_suction_animation.full capacity replacement and zero allocation:
!has_oldest_origin
6 cases, 1 failures
```

### GREEN

新增统一 `reserve_flight`：优先首个空槽；若已满，选择 elapsed 最大的槽位，elapsed 相同则保留最早数组槽位。装备和材料入列共用该 helper。

再次执行同一 CPLAY-021 定向命令，结果：

```text
6 cases, 0 failures
```

此次仅运行定向测试和 `git diff --check`；未运行全量测试、未启动游戏。

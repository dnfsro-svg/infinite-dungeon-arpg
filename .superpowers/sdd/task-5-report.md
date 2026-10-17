# Stage 17 Task 5 实现报告

## 状态

- 结果：PASS
- 实现提交：`5da5355 feat: implement storm swords awakening skill`
- 范围：仅实现极·鬼剑术（暴风式）的锁定中心、12 段剑阵、终结、取消、冷却、事件元数据和 combat 单元测试；未修改 dungeon、persistence、输入映射或 Raylib。

## RED

先仅新增 `storm_swords_skill_tests.cpp` 及测试接线，没有修改生产行为。

首次直接运行简报构建命令时，普通 PowerShell 未加载 VS 开发者环境，CMake 因 `WindowsSDKVersion` 为空而被项目 SDK 门禁拦截；该次不计作功能 RED。随后运行仓库 `scripts/Configure.ps1 -Preset windows-msvc-core-debug`，加载 MSVC x64 / Windows SDK 10.0.26100.0，再执行：

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests
$env:ARPG_STAGE17_STORM_SWORDS_ONLY='1'
& out/build/windows-msvc-core-debug/bin/arpg_combat_tests.exe
Remove-Item Env:ARPG_STAGE17_STORM_SWORDS_ONLY
```

观察结果：测试目标构建成功；`9 cases, 9 failures`。九项均在 `request_active_skill(ActiveSkillId::storm_swords) == SkillCastResult::accepted` 断言失败，证明 RED 来自暴风式运行时尚未实现，不是测试编译、拼写或环境错误。

## GREEN

最小实现包括：

- 冻结简报给定的 24 tick 前摇、12 段、6 tick 间隔、24 tick 后摇、中心前方 3.5、普通/终结半径与物理伤害、终结击退/浮空常量。
- 施放时按锁定朝向计算一次中心，之后玩家移动不再改变中心。
- 普通段发生于 elapsed 24、30、...、90；终结由 `24 + 12 * 6` 推导为 elapsed 96；恢复结束由同一终结边界加 24 推导，没有第二套硬编码总帧数。
- 每次普通段及终结前清空固定容量 `hit_latch`；遍历固定怪物池，无随机选目标、无动态容器。
- 普通段使用 `ImpactKind::light_hitstun`；终结使用 `ImpactKind::launch`、0.26 水平击退和 0.24 浮空速度。
- 普通段和终结都只调用 `CombatWorld::resolve_player_attack_hit()`，由其继续调用 `build_player_hit_packet()`；`active_skill_runtime.cpp` 没有直接写怪物 HP。
- `CombatEvent` 保持 `attack = AttackId::none`，新增独立 `skill`、`strike_index`、`finisher` 元数据；共享命中解析向 hit/break 事件填充这些字段。
- 死亡、`reset()`、`load_wave()` 沿用既有 `ActiveSkillRuntime{}` 清理路径，取消剩余段和冷却。
- 暴风式使用技能目录既有的 1800 world-tick 冷却。

定向 GREEN：`9 cases, 0 failures`。

## 测试证据

```text
定向暴风式：9 cases, 0 failures
完整 combat 可执行：228 cases, 0 failures
ctest -R '^combat\.units$' --repeat until-fail:3：3/3 通过，0 failures
git diff --check：通过
```

完整复跑命令：

```powershell
cmake --build --preset windows-msvc-core-debug --target arpg_combat_tests
$env:ARPG_STAGE17_STORM_SWORDS_ONLY='1'
& out/build/windows-msvc-core-debug/bin/arpg_combat_tests.exe
Remove-Item Env:ARPG_STAGE17_STORM_SWORDS_ONLY
ctest --test-dir out/build/windows-msvc-core-debug -R '^combat\.units$' --repeat until-fail:3 --output-on-failure
```

覆盖项：朝向前方固定中心、移动后中心不漂移、12 段/6 tick 节奏、每段每目标至多一次且下一段可再次命中、42 物理轻牵制、2.8 半径、360 物理终结、3.8 半径、击退与浮空、技能/段号/终结事件元数据、死亡/reset/load_wave 取消、1800 tick 冷却、阶段/段号快照、热路径零分配。

## 自审

- `AttackId` 枚举未改，暴风式没有放入普攻 ID。
- 普攻、J/K/L、WASD 和所有输入文件未改。
- 未修改 dungeon、persistence、Raylib 或平台层。
- 运行时没有直接 HP 扣减，没有复制 Task 4 已收束的伤害解析路径。
- 半径查询遍历固定 `MonsterPool` 槽位，闩锁为既有固定 `std::array<bool, kMonsterCapacity>`。
- `combat_snapshot.cpp` 已直接复制 `active_skill_.snapshot`，无需额外分支；测试实际验证了 startup/strikes/finisher/recovery、elapsed ticks、locked center 和 strike index，故未做无意义触碰。
- 为接入既有请求入口、共享命中元数据和私有方法声明，必要地修改了 `combat_world.cpp/.hpp` 与 `hit_resolution.cpp`；这些改动仅是 Task 5 的紧邻 plumbing。

## 顾虑

无已知功能顾虑。构建输出中的 CMake poison compile-check “Failed” 是项目预期的负向门禁探测，不是编译失败；最终构建、定向测试和三次 combat 回归均为退出码 0。

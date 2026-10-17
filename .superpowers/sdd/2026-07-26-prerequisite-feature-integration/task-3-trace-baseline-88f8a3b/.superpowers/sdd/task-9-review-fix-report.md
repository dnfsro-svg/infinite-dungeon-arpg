# Task 9 Review Fix Report

## 范围

- 仅修复 Stage 9 Task 9 的 raylib blink 词缀预警显示与预警音频限流边界。
- 未修改 combat 规则、快照写入逻辑或通用 `ai_phase` 预警；未进入 Task 10+。

## TDD 证据

先在 `monster_view_tests.cpp` 写入所需行为：

1. `affix_warning == blink` 且 `affix_warning_ticks > 0` 时，纯视图映射同时给出角色环与地面环半径；仅改变 `ai_phase` 不改变映射，ticks 为 0 时全部关闭。
2. 预警音频同 tick、相差 4 tick 均拒绝；`UINT64_MAX - 5` 后的
   `UINT64_MAX - 1` 拒绝，回绕后的 tick `2` 允许。

首次直接构建因当前 shell 未加载 MSVC 标准库环境而报 `<cmath>` 缺失；加载
`VsDevCmd.bat` 后重新运行，测试按预期在链接期 RED：
`blink_affix_warning_visible`、`blink_affix_warning_actor_radius` 和
`blink_affix_warning_ground_radius` 均未定义。

最小 GREEN：

- 新增只读取 `MonsterSnapshot::affix_warning` 和
  `MonsterSnapshot::affix_warning_ticks` 的三个纯视图函数。
- `actor_renderer.cpp` 调用该映射，并为 blink 绘制独立的角色圆环和地面椭圆；原有通用
  `ai_phase` 预警保持不变。
- `WarningAudioThrottle` 按“未播放或回绕允许，否则 `tick - last_tick >= 12`”限流，避免
  `last_tick + 12` 溢出。
- 平台固定用例计数因新增一例由 72 同步为 73。

## 验证

```text
cmake --build build-release --target arpg_platform_tests arpg_game
ctest --test-dir build-release --output-on-failure -R "^(platform\.units|architecture\.combat_no_raylib)$"
git diff --check
```

结果：`arpg_platform_tests` 与 `arpg_game` 构建通过；`platform.units`、
`architecture.combat_no_raylib` 通过（2/2）；差异检查通过。

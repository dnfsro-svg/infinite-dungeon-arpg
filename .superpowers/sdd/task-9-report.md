# Task 9 Report: raylib 徽标、预警与提示音

## 范围

- 在 raylib 平台层新增 `monster_affix_badge()`、`monster_affix_outline()` 和
  `hazard_color()`；展示仅读取战斗快照与既有词缀目录，不修改 combat 规则。
- 怪物血条下最多绘制三条 `SHORT M#` 词缀标签；轮廓为额外叠加层，保持生态本体色。
- 高危词缀以 30 tick 周期的红色透明度脉冲显示；native/burning/chain/death
  hazard 分别保留紫色、橙红、黄色和红色。
- blink、chain、death 预警分别路由到独立的程序化短音；每一类声音独立限流为
  12 tick，同 tick 同类只播放一次，tick 回绕后允许播放。

## TDD 证据

先在 `monster_view_tests.cpp` 添加 3 个失败用例，覆盖：

1. 12 个短名、M1/M2/M3 和目录危险等级；
2. 基础/防御/火/水/电/混沌颜色及高危脉冲；
3. 四种 hazard 颜色、三种预警 cue、12 tick 限流和回绕。

RED 使用 MSVC Developer Command Prompt 运行：

```text
cmake --build build-release --target arpg_platform_tests
```

编译按预期因缺少 `monster_affix_badge`、`monster_affix_outline`、
`hazard_color`、新 AudioCue 和 `WarningAudioThrottle` 失败。随后完成最小实现。

## 验证

```text
cmake --build build-release --target arpg_platform_tests arpg_game
ctest --test-dir build-release -R "platform.units|architecture.combat_no_raylib" --output-on-failure
git diff --check
```

结果：平台单元测试和 combat 无 raylib 架构检查均通过（2/2）；`arpg_game`
构建通过；差异检查通过。

备注：平台测试主程序使用固定 case count；新增 3 个用例后已将登记值从 69 同步为 72。

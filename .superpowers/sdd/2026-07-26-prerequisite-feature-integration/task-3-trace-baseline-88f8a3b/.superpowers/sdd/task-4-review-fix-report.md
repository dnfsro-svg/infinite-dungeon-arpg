# Stage 9 Task 4 审查修复报告

基线：`7f9a9ce`。

## 修复

- 修正 Armored 的物理 HP 伤害：`ceil(physical_before * (10000 - reduction_bp) / 10000)`。
- M3 Armored 的 775 rating 映射为 3500bp；45 点物理伤害现在造成 30 点 HP 伤害。
- 元素伤害与破韧值不进入 Armored 的物理减伤分支。

## 回归覆盖

- Frenzy M1/M2/M3：伤害分别为 115%、130%、150%，Telegraph/Recovery/Cooldown 按向上取整缩放，Active 固定为 4 tick。
- Swift M1/M2/M3：移动分别为 115%、130%、145%，仅 Cooldown 缩放；Telegraph/Active/Recovery 保持 12/4/18 tick。
- Armored：45 物理 -> 30；附加 10 火焰伤害不被削减；Water Bulwark 的 10 点破韧保持不变。

## TDD 与验证

- RED：未改生产代码时，`combat.units` 仅失败精确断言 `armored_after.max_hp - armored_after.hp == 30`。
- GREEN：加载 `VsDevCmd.bat -arch=x64 -host_arch=x64` 且设置 `WindowsSDKVersion=10.0.26100.0\` 后执行：
  - `cmake --build build-release --target arpg_combat_tests`
  - `build-release\bin\arpg_combat_tests.exe`：116 cases, 0 failures
  - `ctest --test-dir build-release -R combat.units --output-on-failure`：1/1 passed
- `git diff --check` 通过。

范围：仅 Combat 命中结算与 Task 4 词缀回归测试；未修改 Task 5+ 或其它 `.superpowers` 内容。

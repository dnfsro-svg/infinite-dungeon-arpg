# Stage 9 Task 5 报告

## 范围

- 新增固定 `PlayerStatusRuntime`：一个冰寒减速槽和一个混沌腐蚀槽，并投射到 `PlayerSnapshot`。
- 所有怪物直接命中统一走 `apply_monster_direct_hit()`：近战接触、投射物和火焰爆破均先按正原始分量总和附加冰寒水伤，再结算直接伤害并刷新状态。
- 冰寒 M1/M2/M3 使用 1500/2500/3500bp 水附伤和减速，持续 60/90/120 tick；弱重施不降低现有效果，高阶重施替换并刷新。
- 腐蚀 M1/M2/M3 每 60 tick 通过 `DamageDelivery::ground_or_environment` 施加 20/30/45 混沌伤，持续 120/180/240 tick；固定槽刷新而不叠层，仍经过混沌减伤。
- 未实现 Task 6+ 的投射物词缀、死亡、奖励或视觉功能。

## TDD 证据

- RED：在 `VsDevCmd.bat -arch=x64 -host_arch=x64` 且 `WindowsSDKVersion=10.0.26100.0\` 的同一 cmd 会话中，新增冰寒测试因未产生 `max_hp - 61` 的水附伤而失败；腐蚀测试因未产生绕过闪避、受 50% 混沌减伤后的 `max_hp - 68` 而失败。
- GREEN：补齐状态槽、统一直接命中入口和环境 DoT 后，真实怪物命中及 M1→M3→M1 刷新测试全部通过。

## 验证

```text
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure

118 cases, 0 failures
```

`git diff --check` 通过。

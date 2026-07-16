# Stage 9 Task 4 报告

## 范围

- 实现强壮、狂暴、迅捷、坚甲与护盾的运行时行为。
- 保持 C++17、值类型运行时路径和 combat 与渲染/地下城/持久化的隔离。
- 未实现 Task 5+ 的状态、投射物词缀、死亡、奖励或视觉功能。

## TDD 证据

- RED：在 `VsDevCmd.bat -arch=x64` 且 `WindowsSDKVersion=10.0.26100.0\` 环境中，五项新增测试分别因横向击退、狂暴缩放、迅捷移动、坚甲减伤、护盾恢复缺失而失败。
- GREEN：新增整数 basis points 缩放与至少 1 tick 的时序缩放；护盾在实际命中后重置延迟，并在 180/150/120 个世界 tick 后一次补满。

## 验证

```text
cmake --build build-release --target arpg_combat_tests
ctest --test-dir build-release -R combat.units --output-on-failure

116 cases, 0 failures
```

`git diff --check` 通过。

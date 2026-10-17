# Stage 11-B Reserved V Fix Report

## 状态与提交

- 状态：完成。
- 分支：`codex/stage11b-settings-input`。
- Commit SHA：`8ef14386d8e2ad1f35b93365eecfd4be8154313d`。
- 范围：只修复 Task 1/2 的跨任务保留键约束；未开始 Task 4。

## 修复内容

- 设计与计划明确：`StableKey::v` 保留在稳定键词汇表中供全局截图采样，但不能作为十项玩法绑定。
- `SettingsValidationError` 新增 `reserved_key`；`validate_settings` 对任意绑定位置的 `V` 返回该错误。
- `assign_or_swap(..., StableKey::v)` 在写入前返回 `false`，完整 `SettingsData` 保持不变。
- 1,000 次 assign/swap 压力测试使用显式排除 `V` 的 44 键候选表，并逐次断言候选不是 `V`、设置仍合法且绑定唯一。
- codec 回归测试构造含 `V` 的 44 字节记录并重新计算 CRC，确认解码返回 `SettingsCodecError::invalid_settings`。
- 未修改 codec 实现、`kSettingsEncodedSize == 44U`、默认绑定、全局输入或 raylib host。

## TDD RED 证据

首次只加入测试后执行：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_settings_tests
```

按预期编译失败：`SettingsValidationError::reserved_key` 尚不存在，MSVC 报 `C2838/C2065`。

仅补枚举分类、未加入拒绝逻辑后执行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_settings_tests
ctest --test-dir out/build/windows-msvc-debug -R "^settings.units$" --output-on-failure
```

得到纯行为 RED：`35 cases, 3 failures`。

1. `reserved V is rejected in every binding` 失败；
2. `reserved V assignment leaves settings unchanged` 失败；
3. `reserved V binding with valid CRC is rejected` 失败。

## GREEN 与验证证据

最小实现后，提交前重新运行：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_settings_tests
.\out\build\windows-msvc-debug\bin\arpg_settings_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(settings.units|persistence.units|platform.units|core.units|combat.units|platform.module_boundary)$" --output-on-failure
git diff --check
```

结果：

- `arpg_settings_tests.exe`：`35 cases, 0 failures`。
- focused/boundary CTest：`6/6` 通过，`0` 失败。
- 覆盖测试：`settings.units`、`persistence.units`、`platform.units`、`core.units`、`combat.units`、`platform.module_boundary`。
- `git diff --check`：通过。
- 范围扫描：44 字节常量保留；codec 实现与 raylib host 未改。

说明：普通 PowerShell 首次直接运行 `cmake --preset windows-msvc-debug` 时，因 `WindowsSDKVersion` 为空被项目门禁拒绝；随后始终使用仓库 `scripts/Configure.ps1` 进入固定 MSVC 19.44 / Windows SDK 10.0.26100.0 环境完成 RED/GREEN 与回归。

## 审查修复：恢复 1,000 次真实冲突交换

- 审查修复 Commit SHA：`a03bf2e1478ed78983226b7154c0d9334f01c53d`。
- 独立复算旧序列：`2` 次真实冲突交换、`998` 次未占用键赋值、`0` 次自身赋值；原测试名和压力语义不成立。
- 测试质量 RED：在旧序列记录真实交换次数并要求 `1000`，`settings.units` 得到 `35 cases, 1 failure`，失败项为 `real_swap_count == 1000U`。
- 修复后的每一轮均从与 target 不同的当前 owner 获取已占用候选键，并显式断言候选不是 `StableKey::v`。
- 每轮调用 `assign_or_swap` 后断言 target 获得候选、owner 获得 target 原键、其余八项绑定逐项不变，并要求 `validate_settings == none`；共执行 1,000 次真实交换。
- 删除混入 swap stress 的 44 键未占用赋值序列；独立的 `unoccupied key assignment succeeds` 用例继续覆盖未占用键赋值。

审查修复验证命令：

```powershell
& .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_settings_tests
.\out\build\windows-msvc-debug\bin\arpg_settings_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(settings.units|persistence.units|platform.units|core.units|combat.units|platform.module_boundary)$" --output-on-failure
git diff --check
```

结果：`arpg_settings_tests.exe` 为 `35 cases, 0 failures`；focused/boundary CTest 为 `6/6` 通过、`0` 失败；`git diff --check` 通过。

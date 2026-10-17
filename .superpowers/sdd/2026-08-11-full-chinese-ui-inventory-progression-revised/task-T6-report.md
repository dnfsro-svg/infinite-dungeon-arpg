# T6 实施报告：范围守卫与聚合门禁

## 范围

只实现修订计划的 T6：

- `tests/platform/full_chinese_inventory_progression_source_test.cmake`
- `tests/platform/full_chinese_inventory_progression_source_self_test.cmake`
- `tests/platform/full_chinese_inventory_progression_gate.ps1`
- `tests/platform/full_chinese_inventory_progression_gate_self_test.ps1`
- `tests/platform/CMakeLists.txt`

未修改 `platform_test_main.cpp`；现有 selector 已严格输出 120 cases。

## RED 与 GREEN 证据

- source guard 开发中先复现了两个真实 RED：把 lambda 拒绝下沉到所有提取函数体时，授权 HUD 方法因其真实测量 lambda 被误拒；随后 lambda 扫描器也曾把数组初始化识别为 lambda。两者均以最小范围修复，并由可逆变异体锁定。
- 最后一次直接 source guard：
  `cmake "-DSOURCE_ROOT=$((Get-Location).Path)" -P tests/platform/full_chinese_inventory_progression_source_test.cmake`
  输出 `T6 full Chinese inventory/progression source guard accepted`（22.4s）。
- 最后一次纯 PowerShell gate self-test：
  `pwsh -NoProfile -ExecutionPolicy Bypass -File tests/platform/full_chinese_inventory_progression_gate_self_test.ps1 -GateScript tests/platform/full_chinese_inventory_progression_gate.ps1`
  输出 `Inventory progression gate self-test passed`。
- source self-test 覆盖活动英文、核心名称回流、默认字体、文字宽背板、非居中、透明/低对比、UTF-8 截断、旧材料名、ASCII death、注释/#if 0/常量 false/lambda 诱饵、lambda 检查移除因果；并包含数组/属性正向控制及 C++17 无空格 `return`、`throw`、`not`、`and`、`or`、`not_eq` lambda 变异。
- gate self-test 以 fake native runner 证明 selector 环境恢复、唯一精确 summary、native error、缺 test、第三 full failure、错误的已知失败、git failure、recipe 删除和顶层调用藏入条件/catch 均 fail-closed。
- 独立只读复审最终为 Ready（Critical/Important/Minor = 0/0/0）。

## 已中止/未运行的验证

用户要求“尽量不测试”后，已启动的 fresh exact
`platform.full_chinese_inventory_progression_source_self_test` 被主控中止；它**没有**通过结论。

因此以下步骤未运行，且本报告不将其宣称为通过：

- 完整 `full_chinese_inventory_progression_gate.ps1`
- `scripts/Test.ps1 -Preset windows-msvc-debug`
- full suite、formal target、Release/视觉验收

这符合用户缩减验证范围；Release 视觉交接未推进。

## 自审

- CMake 只注册两个新增 exact CTest；不改变 120/675 selector 基线。
- 聚合脚本只允许精确的已知两条 full-platform 失败，额外或交换失败均阻断。
- 本次提交只含上述五个 T6 测试/门禁文件；报告与账本位于已忽略 `.superpowers` 工作记录，不进入提交。

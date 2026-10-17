# 技能页材料跨页动作修复

## 根因

- 材料选择会跨页保留。
- 输入仲裁把 `material_selected` 作为所有页面底部动作的全局屏蔽条件。
- 状态展示又在技能页末尾用材料提示覆盖技能换装/取出提示。

因此技能页“取出”按钮会显示为启用，但左击落入背景清理层，不会提交取出请求。

## TDD

- RED 1：测试先要求页面感知的输入合同，MSVC 编译在
  `InventoryPointerPriorityInput::page` 缺失处失败。
- RED 2：加入页面仲裁后，选择器为 `120 cases, 1 failures`，精确失败于
  技能页状态被材料提示覆盖。
- GREEN：装备页仍屏蔽材料模式下的三合一；技能页放行底部动作并保持技能提示。
  聚焦选择器输出 `120 cases, 0 failures`。

## 范围

仅修改：

- `src/platform/raylib/inventory_view_math.hpp`
- `src/platform/raylib/inventory_view_math.cpp`
- `src/platform/raylib/inventory_renderer.cpp`
- `tests/platform/inventory_view_math_tests.cpp`

未运行全量 CTest、`Test.ps1`、Release 或窗口测试。

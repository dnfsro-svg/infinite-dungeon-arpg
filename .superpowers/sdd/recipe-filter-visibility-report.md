# 三合一配方筛选可见性修复

## 根因

`refresh_inventory_view_cache` 解析配方 ID 时只检查“物品存在且未装备”，
与 `InventoryFilter` 无关。筛选隐藏三件配方装备后，网格不再显示 1/3、2/3、3/3，
但底部按钮仍为启用态，点击会消耗不可见装备。

## TDD

- 在既有 TestCase 中先建立 3 件同底材普通装备，确认无筛选时 recipe ready。
- 同 generation 切换到 magic 稀有度筛选，要求
  `filtered_indices.empty()`、`resolved_recipe.count == 0`、ready false。
- RED：`120 cases, 1 failures`，精确失败于 resolved recipe 仍为 3。
- GREEN：配方索引只从 `matches_inventory_filter` 的物品建立；
  聚焦选择器输出 `120 cases, 0 failures`。

## 范围

仅修改 `inventory_view_math.cpp` 与既有
`inventory_view_math_tests.cpp`，未新增 TestCase。
未运行 CTest、`Test.ps1`、Release 或窗口。

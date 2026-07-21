# Stage 17 Task 7：五槽 HUD、技能原生表现与技能石装卸页

## 状态

完成。实现范围严格限于 Task 7：五槽主动技能 HUD、拔刀斩/暴风式原生几何表现、现有冻结式背包覆盖层内的技能石子页，以及 remove/equip/swap 原子事务接线。

未修改 J/K/L/WASD、数字键采样/路由、V8 编解码、技能伤害/冷却/施放时序或辅助技能效果。

## 暂停/背包承载结论

现有工程没有 `PauseScreen::inventory`；背包由 `I` 打开、`I/ESC` 关闭，并通过 `InventoryInputGate` 与 fixed-step 门禁冻结游戏。Task 7 文件清单也只指定 `inventory_*` 与 host 接线，不指定 `pause_menu_*`。

因此本任务没有新增 PauseScreen 枚举，而是在既有 `InventoryRenderer` 覆盖层增加两个子页按钮：

- `装备 / 材料`：默认页；每次打开背包都回到该页。既有过滤、材料、配方、装备/卸装流程未改。
- `技能石`：五个主槽、当前选中槽的五个只读辅助空位、未装备主动石库存和取出按钮。

关闭背包会清空技能页选择；装备/材料页原有选择与缓存字段未被技能页复用。

## TDD 证据

### RED

先只新增并注册：

- `tests/platform/active_skill_view_tests.cpp`
- `tests/platform/active_skill_loadout_view_tests.cpp`

首次直接执行简报命令时，普通 PowerShell 未加载 VS 开发环境，CMake 因 `WindowsSDKVersion` 为空提前失败；该次不计作功能 RED。

随后通过 VS 2022 BuildTools 开发环境（MSVC 19.44、Windows SDK 10.0.26100.0）运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
```

实际 RED：

- `active_skill_view_tests.cpp` 因 `active_skill_renderer.hpp` 不存在而失败；
- `active_skill_loadout_view_tests.cpp` 因 `ActiveSkillLoadoutView`、`ActiveSkillLoadoutLayout`、`ActiveSkillLoadoutCommand` 等纯模型/几何 API 不存在而失败。

失败原因正是 Task 7 功能尚未实现，不是测试拼写或链接错误。

### GREEN

完成最小实现后，同一平台目标构建成功，`platform.units` 首次 1/1 通过。最终直接运行平台测试程序：

```text
373 cases, 0 failures
```

## 实现摘要

### 五槽 HUD

- 公开 `ActiveSkillHudSlot` / `ActiveSkillHudModel` 使用简报规定字段和固定五槽数组。
- 槽位键号固定为 1～5；空槽显式设置 `empty=true`、空名称、零冷却。
- 名称直接投影技能目录；冷却按技能目录总 tick 计算并 clamp 到 `[0,1]`。
- 布局固定为底部居中，5 个 `58x58` 槽，槽间 `8px`。
- 冷却遮罩自底向上绘制，并显示剩余秒数；空槽绘制原生石槽轮廓。
- 新增 HUD/技能页文字全部通过已加载的 `HudFont` 使用 `DrawTextEx`；没有调用默认字体绘制新增文字，并扩充了相应中文 glyph 计划。

### 原生技能几何

- 拔刀斩只读 `CombatSnapshot`：以命中 tick 为起点保留 7 tick（约 0.12 秒）的白蓝扇形弧光，朝向读取 `player.facing`，中心读取技能快照 `locked_center`；使用 `DrawTriangleFan` 和线段。
- 暴风式只读技能快照/最后战斗事件：围绕锁定中心按固定 12 角度绘制半透明剑形，以 `strike_index` 高亮当前剑；finisher 绘制中心落剑、椭圆冲击波和 7 tick 短闪屏。
- 没有加载 DNF 或任何新增外部纹理；没有写入战斗世界、伤害、命中或施放时序。

### 技能石子页与事务

- 动作枚举只有 `select/remove/equip/swap`，不存在辅助槽编辑命令。
- 单击主槽选择；连续单击两个已装备主槽产生 swap；选库存石后点空主槽产生 equip；选已装备槽后点取出产生 remove。
- 五个辅助框只显示 `空`，点击不产生命令。
- pending save 时纯命令层拒绝全部操作、取出按钮置灰并显示 `正在保存`。
- remove/equip/swap 直接调用现有 `DungeonSession` 原子事务并由 `DungeonRuntime::service_pending_save()` 落盘。
- 选择状态只在 `commit_generation` 实际增长后更新；保存失败时 loadout 与选择均保持原值，并在技能页显示既有 `保存失败` 通知文案。

## 测试覆盖

新增 9 个平台用例覆盖：

- 固定五槽、1～5、空槽、目录名称；
- 冷却 ratio clamp；
- 1280x720、1600x900、1920x1080 下 HUD 固定尺寸/间距/居中；
- 拔刀命中窗口/朝向与暴风 12 剑、当前 strike、finisher 事件寿命；
- 五主槽、每槽五个辅助空位、未装备石库存；
- 三分辨率下所有交互区和辅助区不重叠；
- select/remove/equip/swap 命令语义；
- pending 禁用、辅助位无命令；
- 动作命令生成时不提前改变选择，支持保存失败保持 UI 选择。

## 最终验证

环境：VS 2022 BuildTools，MSVC 19.44，Windows SDK 10.0.26100.0。

```powershell
cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests arpg_stage11c_hud_stress
```

退出码 `0`；真实游戏目标、平台单元目标和 HUD stress 目标均链接成功。

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|stage11c\.hud_stress\.zero_alloc_100k)$' --output-on-failure
```

2/2 通过：

- `platform.units`：5.60 秒，373 cases / 0 failures；
- `stage11c.hud_stress.zero_alloc_100k`：0.35 秒，100k HUD 零分配通过。

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(skills\.units|combat\.units|dungeon\.units)$' --output-on-failure
```

3/3 通过：

- `skills.units`：0.01 秒；
- `combat.units`：0.94 秒；
- `dungeon.units`：196.02 秒（包含技能 loadout 原子事务回归）。

```powershell
git diff --check
```

退出码 `0`。

## 约束扫描

- 新增 active skill HUD/几何计划全部为 `std::array`、栈缓冲和值类型；没有 `std::vector`、`std::string`、`new` 或智能指针。
- 新增渲染没有 `LoadTexture`、`LoadImage` 或外部素材路径。
- 未修改 `host_input.*`、persistence、skills、combat 或 dungeon 生产实现。
- 工作树原有 `.superpowers/sdd/task-5-report.md` 修改未触碰、未暂存、未覆盖。

## 顾虑

无未解决功能顾虑。Task 7 简报要求的无窗口模型、三分辨率布局、平台单元、HUD 零分配和输入/事务回归均已验证。真实 Raylib 五张截图及持久化重启证据属于后续 Task 8 验收范围，本任务未提前扩展。

# Stage 11-A Task 10 独立审查修复报告

## 审查项

- 修复提交：`6ccedaee9e98a6f89b3bd362be3dc3f03e9b967c`（`fix: localize death recap rendering`）。
- 修复两项 Important：所有死亡来源目录 ID 的稳定中文显示；死亡覆盖层真实 CJK 字体加载、同字体测量/绘制和安全生命周期。
- 未开始 Task 11。

## RED

先扩充全部目录 ID、非法 ID 和纯字体计划测试。首次有效 RED：

```text
fatal error C1083: 无法打开包括文件: “death_overlay_font.hpp”
```

添加纯固定容量 font plan 后再次运行，证明两项行为仍未实现：

```text
[FAIL] death_overlay_view.all source ids have Chinese names
[FAIL] death_overlay_view.font plan covers overlay text
102 cases, 2 failures
```

## 修复

- 怪物 8 个、危险 7 个、怪物词缀 12 个、合法深渊规则 9 个 ID 均映射为稳定中文；unknown 和非法 monster/detail/rule ID 统一显示“未知来源”。
- 新增纯 `DeathOverlayFontPlan`：固定 4 个 Windows TTF 候选、ASCII 32..126、箭头、中文冒号和覆盖层全部中文码点；无 raylib、无动态容器。
- 字体候选按本机可靠性排序：Noto Sans SC、SimHei、等线、仿宋。未使用 raylib 6.0 `LoadFontFromMemory` 不支持扩展名的 TTC 文件。
- `DeathOverlayRenderer` 在窗口创建后只初始化一次：逐候选 `LoadFontEx`，并要求实际 glyph count 等于所需码点数；失败候选立即卸载。
- `CombatRenderer` 持有字体 renderer 资源；正常退出在 `CloseWindow()` 前显式卸载，异常退栈时析构器在窗口仍有效时卸载，接口可重复且幂等。
- 所有覆盖层文本统一通过同一个 `Font` 调用 `MeasureTextEx` / `DrawTextEx`，不再使用默认 `MeasureText` / `DrawText`。
- 所有候选失败时只记录一次明确 warning，并使用默认 ASCII 字体显示可读的死亡标题、深度/房间和 saving/continue/retry 提示，不会每帧重试加载。

## 验证

- `arpg_platform_tests` + `arpg_game` Debug 构建/链接：exit 0，无编译警告。
- `platform.units`：102/102 passed。
- focused death lifecycle：31/31 passed。
- `architecture.*` 18 项 + `platform.input_latency_source` / `platform.module_boundary` 2 项：20/20 passed，106.81 秒。
- source grep：死亡 overlay 仅存在 `MeasureTextEx` / `DrawTextEx`，无 `MeasureText` / `DrawText`。
- `git diff --cached --check`：无错误；提交后工作树干净。

## 第二轮审查修复

- 修复提交：`f4d37e2a806e86e59f783e6d063f96e4190e6c29`（`fix: complete death overlay fallback`）。
- `hazard_name` / `abyss_rule_name` 在 enum cast 前检查原始 `uint16_t`；256/257/512/513/65535 全部回退为未知。词缀已按原始值检查，怪物来源字段为 `uint8_t`，无同类宽值截断路径。
- 新增纯 `build_death_overlay_ascii_view`，无 CJK 字体时仍提供与中文视图相同的 26 行完整信息、稳定 ASCII 来源名/UNKNOWN、五类近期伤害、完整防御和元素减伤/上限、层数迁移及三种提示。
- renderer 根据字体能力选择中文或 ASCII view-model，之后走同一套布局与绘制代码，不再格式化业务数据。
- 有效 RED：宽危险 ID 256 误映射合法项；随后缺少 `build_death_overlay_ascii_view` 的编译错误。两项均在实现前观察到。
- 最终验证：`platform.units` 104/104；focused death 31/31；架构与平台边界 20/20；`arpg_game` Debug 链接成功；`git diff --check` 与 cached check 无错误。

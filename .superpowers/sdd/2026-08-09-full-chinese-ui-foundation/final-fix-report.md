# Full Chinese UI Foundation Final Fix Report

日期：2026-08-09

Fix base：`d7cdbc1`

分支：`codex/full-chinese-ui`

## 范围与结论

本轮只修复最终审查列出的基础层阻断项：动态标点字形覆盖、UTF-8 前缀截断、Design 已批准中文词表对齐，以及 Task 4 Stage12 既有失败清单。没有实现菜单或 HUD，没有修改 stable ID、枚举、玩法、存档、设置编码或版本，也没有引入 JSON、外部语言包、运行时容器或逐帧分配。

## TDD 证据

### 1. `…` 与 `、` 字形覆盖

- RED：先在现有 `shared_ui_font.complete catalog coverage` case 增加 `u8"…、"` 覆盖断言。foundation 结果为 15 cases / 1 failure，失败精确位于该断言。
- 根因：`ui_glyph_plan()` 只消费 `visit_ui_text_catalog()`；`…` 由 `ui_utf8_fit_with_ellipsis()` 生成，`、` 由多材料拾取提示拼接，两者此前都不在 visitor 语料中。
- GREEN：把真实动态样例 `设置界面…` 与 `已拾取：强化石 x1、混沌石 x1` 加入唯一 `kUiDynamicGlyphCoverage`，没有在 `shared_ui_font` 另塞语料。visitor 条目数由 268 增为 270，foundation 恢复 15/15。

### 2. `ui_utf8_copy()` 必须保持合法 UTF-8 前缀

- RED：在现有码点边界 case 中加入容量 2（可写 1 字节）、输入 `😀A` 的行为断言。旧实现输出 `A`，foundation 结果为 15 cases / 1 failure，失败位于空前缀比较。
- 根因：某个完整码点放不下时，循环只设置 `truncated=true`，随后仍允许较短码点写入。
- GREEN：第一次容量不足后锁存 `capacity_exhausted`，继续只读解码剩余输入以维护完整 `input_was_valid`，但停止所有后续写入。结果为合法前缀、写入位置 NUL 终止、`truncated=true`；非法尾字节仍令 `input_was_valid=false`。ellipsis 的完整可容纳分支继续复用 copy，截断分支保持既有完整码点前缀逻辑。
- GREEN 首跑中，新增测试曾错误要求未使用的最后一个容量字节也必须为 NUL；接口只要求写入位置 NUL 终止。该过度断言改为精确检查 `buffer[0] == '\0'` 后，foundation 为 15/15。

### 3. Design 第 7/9 节词表对齐

- RED：把现有两个 catalog case 扩展为手工字面量表，不增加 case 数。精确覆盖全部 32 个 `UiTextId`、18 个装备底材、14 个材料、8 个怪物、12 个完整怪物词缀及对应短标。当前实现得到 15 cases / 2 failures：固定文案表和稳定怪物映射表各失败一次。
- GREEN：只修正 catalog 字面量，包括 `继续游戏`、`退出游戏`、`被动星图`、`守护外衣`、`突击手套`、`嬗变石`、`定向核心`、`烈焰爆破者`、`强力`、`多重投射`、`死亡爆破` 等全部批准条目；foundation 恢复 15/15。
- Design 第 7/9 节没有逐项批准槽位、品质、24 个物品词缀、64 个被动节点、10 个设置动作和 2 个技能的正式译名；这些映射继续逐项检查非 `未知`，没有用生产表生成 expected，也没有把未批准自译伪装成 Design 字面量。

## 字形与预算

- 基线实际计划：517 个去重码点。
- 修复后通过临时测试诊断读取：517 个去重码点；诊断输出随后已移除并重新构建。
- 净变化：517 -> 517。新增 `U+2026` 与 `U+3001`，同时批准词表替换移除了不再需要的旧译码点。
- 容量：517 / 640，无 capacity overflow；正式实例常量仍为 1，单图集预算仍为 32 MiB。
- 本轮未重跑约 8 分钟且已知会被无关 HUD/性能条件阻断的完整 `stage12.material_formal`，因此没有把旧的 8 MiB GPU 图集证据当作本轮新鲜实测。新鲜验证覆盖了字形容量/预算单测和 bundled-font validator self-test。

## 最终验证

实际执行：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2
$env:ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY='1'; .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
$env:ARPG_CPLAY033_PASSIVE_CJK_ONLY='1'; .\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
ctest --preset windows-msvc-debug --output-on-failure -R '^(platform\.shared_ui_font_lifecycle_guard|platform\.shared_ui_font_lifecycle_guard_self_test|stage12\.bundled_font_validator_self_test)$'
git diff --check
```

结果：

- `arpg_platform_tests` 目标构建成功，固定 `--parallel 2`。
- foundation：15 cases / 0 failures。
- CPLAY033：15 cases / 0 failures。
- `platform.shared_ui_font_lifecycle_guard`、其 self-test、`stage12.bundled_font_validator_self_test`：3/3 passed。
- `git diff --check`：exit 0。
- 最终构建可用物理内存：6345.0 MiB -> 6303.6 MiB，下降 41.4 MiB；没有内存压力或 OOM。

## 文件

- `src/platform/raylib/ui_text_catalog.cpp`
- `src/platform/raylib/ui_utf8.cpp`
- `tests/platform/shared_ui_font_tests.cpp`
- `tests/platform/ui_text_catalog_tests.cpp`
- `tests/platform/ui_utf8_tests.cpp`
- `.superpowers/sdd/2026-08-09-full-chinese-ui-foundation/task-4-report.md`
- `.superpowers/sdd/2026-08-09-full-chinese-ui-foundation/final-fix-report.md`

## 疑虑与边界

- 无一一映射冲突或阻断项。
- 怪物短标在 Design 中只有“一至两个汉字”的约束，没有逐项批准字面量；本轮让短标沿用批准完整术语的核心字（如 `强力`、`狂热`、`多投`、`死爆`），没有改 stable ID。
- Task 4 报告只补记 Stage12 既有 `input_hole_regression=fail`，没有改生产验证逻辑。

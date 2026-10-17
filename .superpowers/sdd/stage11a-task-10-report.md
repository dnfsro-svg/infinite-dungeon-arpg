# Stage 11-A Task 10 实现报告

## 范围

- 基线：`ee139b3e12bbbfe8b43a24d00246b113361f0399`。
- 初始提交：`1b88d2203f6d871c6c4e00aa4215604a37e89944`（`feat: render death recap overlay`）。
- 首轮审查修复：`6ccedaee9e98a6f89b3bd362be3dc3f03e9b967c`（`fix: localize death recap rendering`）。
- 最终审查修复：`f4d37e2a806e86e59f783e6d063f96e4190e6c29`（`fix: complete death overlay fallback`）。
- 仅实现死亡回顾 view-model、raylib 最终覆盖层及 `death_continue` 失败的瞬态展示状态；未开始 Task 11。
- view builder 唯一输入为 `const DungeonSnapshot&`；renderer 不读取 Session、Runtime、SaveStore、stable state、pending save 或测试访问器。

## RED

先添加 `death_overlay_view_tests.cpp`、suite/CMake 注册和 Dungeon retry-failed 快照断言。普通 shell 首次构建因没有加载 Windows SDK 属无效环境失败；切换到 VS Dev 环境后得到有效 RED：

```text
fatal error C1083: 无法打开包括文件: “death_overlay_view.hpp”: No such file or directory
```

首轮 GREEN 后自审发现极端 `uint64_t` 数值可能使双值统计行超出 800x450 的紧凑列。先增加最坏值列宽测试，明确再次 RED：

```text
[FAIL] death_overlay_view.worst case values fit compact columns:
utf8_glyph_count(view.lines[index].text.data()) <= 36U
100 cases, 1 failures
```

随后才把极端数值拆为固定容量单值行，并收紧最小窗口字号/行距。

首轮独立审查先以缺失 `death_overlay_font.hpp` 得到编译 RED；加入纯字体计划后，完整目录中文名与字体覆盖仍各有一个行为 RED（102 cases, 2 failures），随后才实现稳定中文目录与一次性 CJK 字体生命周期。

第二轮独立审查新增宽来源 ID 用例，先证明 `source_detail_id=256` 会在转为 8 位 enum 后错误别名为合法危险：

```text
[FAIL] death_overlay_view.wide source ids never alias:
contains(platform::build_death_overlay_view(hazard), "未知来源")
103 cases, 1 failures
```

原始范围检查转绿后，新增完整 ASCII 回退契约，得到缺少纯 view builder 的有效编译 RED：

```text
error C2039: "build_death_overlay_ascii_view": 不是 "arpg::platform" 的成员
```

随后才实现完整纯 ASCII view-model，并让 renderer 共用统一绘制路径。

## 实现

- 新增无 raylib 依赖的固定容量 `DeathOverlayView`：完整映射死亡房深度、房号、生态、普通/深渊、致死来源、目录显示名、伤害类型、最后一击、五类 5 秒承伤、防御快照和退层目标。
- 来源显示覆盖怪物攻击、投射物、地面危险、怪物词缀和深渊环境；非法/unknown 显式回退为“未知来源”。
- 精确提示文本为“正在记录死亡”“E 继续”“保存失败，请重试”。
- 新增 1280x720 与 800x450 共用的纯布局模型；两列统计、所有固定文本 bounds 均位于面板内且不碰底部提示。最坏 64/32 位数值也拆分到紧凑列宽内。
- 新增无状态 raylib renderer：半透明全屏遮罩、居中面板、两列统计、底部提示；由 `CombatRenderer::draw()` 在世界、既有 HUD、debug 和过渡遮罩之后最后绘制。
- 新增仅驻留 Session 内存的 `death_continue_failed_`：只在 `death_continue` receipt 为 `not_committed` 时置位；retry accepted、成功发布/清理房间或进程重启时清零。未修改 V6 checkpoint 或 Persistence codec。
- MSVC 仅为 raylib 目标和 platform tests 启用 `/utf-8`，保证中文 UTF-8 字面量不污染公共编译接口。
- 危险与深渊规则在转 enum 前先检查原始 `uint16_t` 范围，256/257/512/513/65535 均不会别名合法条目；词缀本来先按原始 `uint16_t` 检查，怪物持久字段本来就是 `uint8_t`，审计未发现同类截断。
- CJK 字体全部加载失败时，使用纯 ASCII 固定容量 view-model 生成同样的 26 行完整回顾：房间、来源与稳定详情/UNKNOWN、伤害类型、最后一击、五类近期伤害、生命/护盾/护甲/闪避、四类元素减伤与上限、退层目标以及 saving/retry/continue 三种提示。renderer 不再手写残缺回退数据。

## 验证

- `arpg_platform_tests` 与 `arpg_game` Debug 构建/链接：exit 0，无编译警告。
- `platform.units`：104/104 passed（含宽 ID 不别名、完整 ASCII 字段/三提示/纯 ASCII、紧凑布局最坏值）。
- focused death lifecycle：31/31 passed（包含失败置位、retreat 不置位、retry 清零、重启清零）。
- `dungeon.units`：254/254 passed，530.02 秒。
- `architecture.*` 18 项 + `platform.input_latency_source` / `platform.module_boundary` 2 项：20/20 passed，98.27 秒。
- `git diff --cached --check`：无错误；提交后工作树干净。

## 自审结论

- 新增 view/renderer 中只有 renderer source include raylib；view-model 不含窗口依赖、动态容器或业务状态。
- 覆盖层只读当前快照，输入门禁和原 HUD 路径未改。
- `continue_failed` 是纯瞬态信息，不影响 checkpoint hash、双槽 winner、重启迁移或继续目标确定性。
- Task 11 的 host 场景、真实窗口截图和正式五路径验证未提前实现。

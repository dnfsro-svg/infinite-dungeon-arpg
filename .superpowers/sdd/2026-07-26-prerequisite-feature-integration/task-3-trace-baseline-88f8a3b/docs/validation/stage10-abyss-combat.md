# Stage 10 正式深渊房间战斗验证记录

- 验证日期：2026-07-16
- 分支：`codex/stage10-abyss-combat`
- 最终代码与测试基线：`9269811`
- 工具链：MSVC 19.44.35228.0、Windows SDK 10.0.26100.0、CMake/Ninja、raylib 6.0.0
- 范围：设计文档第 4～14 节；Task 13 简报中的“第 4～18 节”是过时编号，源规格不存在第 15～18 节，因此本记录按实际源规格完整审计到第 14 节，不虚构额外章节。

## 静态扫描

以下两条命令均以退出码 1 表示“无匹配”，没有发现占位符或核心模块越界包含 raylib：

```powershell
rg -n "TODO|FIXME|PLACEHOLDER|stage 10 later" src/abyss src/combat src/dungeon src/persistence src/platform/raylib tests/abyss tests/combat tests/dungeon tests/persistence tests/platform
rg -n '#include <raylib.h>|#include "raylib.h"' src/core src/abyss src/combat src/dungeon src/persistence
```

## CTest 清单与双配置全量结果

`ctest --preset windows-msvc-debug -N` 与配置完成后的 Release 清单均为 43 项。Stage 9 / `main` 基线有 33 项；Stage 10 增加 7 个正向项（Task 1 的 `abyss.units`，Task 12 的 fixture、validation game、formal game、stress、正向 evidence guard，以及最终审查新增的截图内容验证器），另有 3 个 `WILL_FAIL` 反向守卫，因此实际总数为 `33 + 7 + 3 = 43`。旧计划的 38 漏算了 Task 1 的 `abyss.units`，也未包含后增的 3 个负向项和截图内容验证器。证据守卫仍为四项，另有一项正向内容验证器 `stage10.formal_game.capture_content_validator`：

- `stage10.evidence.no_private_injection`
- `stage10.evidence.rejects_private_injection`
- `stage10.evidence.rejects_pre_present_capture`
- `stage10.evidence.rejects_pre_capture_with_post_dummy`

普通 PowerShell 会话不带 `WindowsSDKVersion`，直接执行第一次 `cmake --preset windows-msvc-debug` 按项目硬门禁退出 1。随后使用仓库固定脚本进入 MSVC x64 / SDK 26100 开发环境；脚本内部执行同一 `cmake --preset`，成功配置。可重跑命令如下：

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure

.\scripts\Configure.ps1 -Preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release --output-on-failure
```

| 配置 | 配置 | 构建 | 全量 CTest | 结果 |
| --- | ---: | ---: | ---: | --- |
| Debug | 成功 | 成功（目标已是最新） | 231.59s | 43/43，0 失败 |
| Release | 成功 | 成功（最终修复后重建 17 步） | 157.05s | 43/43，0 失败 |

Debug/Release 均通过仓库固定脚本重新配置、构建并运行完整清单，不是目标子集。Debug 最慢项 `dungeon.units` 为 147.79s；Release 为 78.50s。Stage 10 标签在两种配置下均为 9/9。

## 独立压力与正式 raylib 证据

### 1000 房确定性与 600 tick 零分配

精确命令：

```powershell
ctest --preset windows-msvc-debug -R '^stage10\.abyss_stress\.determinism_and_zero_alloc$' -V
```

- 退出码：0；最终 Debug 全量中的 CTest 测试时间：0.06s。
- 两个同根 session 运行 1000 房；每 37 房做 V5 encode/decode，共 27 次重载。
- 四向深渊命中：`9/11/9/7`，合计 `36/4000`；概率宽护栏为 `20..70`。
- 最终 golden hash：`0xe102b17e6423351b`。
- 极端组合：96 怪、384 弹体、96 hazard、192 ground，运行 600 tick。
- 分配增量：`allocations=0`；饱和诊断：`291/481/601`，地面饱和精确从 1 增至 601。

### 正式 raylib host

精确命令：

```powershell
ctest --preset windows-msvc-debug -R '^stage10\.formal_game\.capture_after_present$' -V
```

- 退出码：0；最终 Debug 全量中的 CTest 测试时间：10.13s。
- 真实路径：死亡 `PASS`、R 重置 `PASS`、`started` 重启失败 `PASS`、深渊与下层洞共存并下层 `PASS`。
- 截图目录：`E:\game\.worktrees\stage10-abyss-combat\out\build\windows-msvc-debug\bin\stage10-formal-game-validation`。
- 护栏：每图必须是刚生成的 1280×720 PNG；按 16 像素步长采样至少 100 个非背景样本，并满足至少 20 种颜色，或满足低色深场景的复合结构判据（至少 8 色、200 次空间转变、亮度范围至少 12000）。独立内容验证器同时证明 19 色结构图被接受、真实背景空白图以非零退出码被拒绝。

| 截图 | 内容 | 采样色 | 非背景样本 | 空间转变 | 亮度范围 |
| --- | --- | ---: | ---: | ---: | ---: |
| `01-abyss-door.png` | 四向门深渊标记 | 33 | 3504 | 882 | 234860 |
| `02-thunderstorm-warning.png` | 雷暴预警 | 21 | 3600 | 949 | 36729 |
| `03-hunting-flames-warning.png` | 追猎烈焰预警 | 26 | 3600 | 887 | 61458 |
| `04-chaos-expansion.png` | 混沌扩散区域 | 43 | 3600 | 1060 | 218434 |
| `05-reward-chest.png` | 宝箱奖励 | 38 | 3600 | 874 | 133705 |
| `06-pending-reward.png` | 待生成奖励提示 | 40 | 3600 | 893 | 150662 |
| `07-exit-confirmation.png` | 离房二次确认 | 41 | 3600 | 956 | 190049 |

洞路径落盘摘要为：`depth=2`、`is_abyss=0`、`last_transition=descent`、`resolution_valid=1`、`total=2`、`generated=2`、`claimed=0`、`abandoned=0`。

### 独立人工复核

主代理另用 Computer Use 启动 Release `arpg_game.exe`，存档隔离在 `out/manual-qa-20260716`，实际发送移动/A、J、L、F12、Esc，并确认基本输入有响应。随后人工重开正式路径产生的存档，复核最终落盘状态：

- `death-reopen.png`：HUD 为 `Depth 1`、`ABYSS NO`。
- `reset-result.png`：HUD 为 `Depth 1`、`ABYSS NO`。
- `restart-result.png`：HUD 为 `Depth 1`、`ABYSS NO`。
- `hole-result.png`：HUD 为 `Depth 2`、`ABYSS NO`。

四张截图均位于被忽略的 `out/manual-qa-20260716`，不提交。路径动作本身由自动化正式 raylib 窗口执行；人工环节是独立复核最终落盘状态和基本输入，不声称人工完整重演死亡、R、started 重启与洞下层四条动作。

真实事务 fixture 另以以下命令独立重跑；最终 Debug 全量中退出码 0、CTest 测试时间 0.07s：

```powershell
ctest --preset windows-msvc-debug -R '^stage10\.validation_fixture\.real_abyss_transactions$' -V
```

它从门预告进入实际深渊，提交 `generation 3` 的 started、`generation 4` 的 cleared，生成三件固定奖励 ID `7017342334060664774 / 5170495016027824722 / 14050745106035002130`，并验证原子领取、重载位图和离房摘要；任意顺序领取由 `dungeon_abyss_reward.abyss claims any order fail closed` 独立覆盖。

## 规格第 4～14 节对应表

| 设计节 | 规格要点 | 自动测试或证据 |
| --- | --- | --- |
| §4 稳定生成与门预告 | 四门独立 1%；预告/进入一致；初始/下层排除；旧状态迁移 | `abyss.units` 的 `frozen one percent roll`；`dungeon.units` 的 `preview supports zero through four abyss doors`、`ordinary door preview matches pending target`、`abyss door target persists selected checkpoint`、`initial room rejects a matching legacy roll`、`descent target rejects matching legacy abyss roll`；V5 codec 迁移用例 |
| §5 生命周期 | available→started→cleared/failed；死亡/R/重启一次机会；比例恢复 | `dungeon_transaction.abyss start *`、`dungeon_lifecycle.abyss fail *`、`abyss clear *`；formal 的 death/reset/restart；`combat.units` 的 `life sacrifice ratio and clear` |
| §6 危险与 9 条规则 | 深度权重边界；9 条固定目录；环境伤害与不可闪避 | `abyss.units` 的 depth 1/9/10/19/20/39/40、三危险可达和三环境完整求值；`combat.units` 的 thunderstorm/hunting/chaos 固定 tick、减伤、容量恢复用例；formal 三环境截图 |
| §7 强化遭遇 | 预算向上取整 1.5 倍；词缀下限；保留普通前缀；候选不足失败 | `abyss.units` 的 `encounter budget`、`affix minimum`；`combat.units` 的 `abyss supplement depth minimums and prefix`、`stable and satisfied bytes unchanged`、`insufficient candidates fail`、`no allocations` |
| §8 宝箱奖励 | 1/2/3 件、等级与稀有度、独立序号、自动结算 | `dungeon_abyss_reward.reward profiles levels and ordinals`、`shifted rarity danger ordering`、`stable independent ordinal streams`、`cleared starts hidden transaction`；fixture 固定三奖励 ID |
| §9 部分生成与放弃 | 地面池续发、背包满留地、原子领取、同门/同洞二次确认、LastAbyssResolution | `full pool waits and continues`、`abyss claim full oom overflow atomic`、`abyss claims any order fail closed`、`abyss door confirmation abandon`、`abyss confirmation invalidation`、`abyss hole confirmation descent`、`abyss abandon failure and overwrite`；formal 第 5～7 图及洞摘要 |
| §10 模块边界 | abyss 纯计算；dungeon 编排；combat 不读存档；persistence 仅 checkpoint；raylib 只读快照 | 18 个 `architecture.*` 加 1 个 `platform.module_boundary`，合计 19 个模块边界 CTest；`platform.input_latency_source` 另行保护输入延迟源码约束；两条静态 include 扫描；`stage10.evidence.no_private_injection` |
| §11 原子性与故障矩阵 | start/fail/异常重载/clear/claim/abandon 的失败与不确定发布 | `persistence.units` 的 `abyss start fault matrix is atomic`、`abyss fail fault matrix is atomic`、`abyss claim abandon faults are old or new`；`dungeon.units` 的 start/clear receipt mismatch、not committed、indeterminate、generation/state mismatch 用例 |
| §12 确定性、容量、性能 | 独立随机域；普通随机不漂移；1000 房重载一致；600 tick 零分配；满池降级 | 独立 stress 的 36/4000、27 次 codec、golden hash；96/384/96/192、600 tick、0 分配；`room_generation.golden seed chain is stable` 与 Stage 9 全量回归 |
| §13 测试与验收 | 双配置、长程、正式窗口、人工复核 | Debug/Release 43/43；fixture、stress、自动化 formal 正式窗口路径；4 项证据守卫与 1 项截图内容验证器；人工仅独立复核 Release 基本输入和四条路径的最终落盘 HUD，不冒充完整人工重演 |
| §14 完成边界 | 不进入首领、专属物品、召唤、光环或 Stage 11 | 静态范围扫描、分支日志和本次提交清单；最终审查修复仅补强 Stage 10 边界，不扩展 Stage 11 |

## 十项硬审计

| 硬审计 | 结果 | 直接证据 |
| --- | --- | --- |
| 四门固定 1% | PASS | `frozen one percent roll`、0～4 门预告测试；1000 房 `36/4000`，四向 `9/11/9/7` |
| 启动/失败/清场原子门禁 | PASS | start/fail/clear 的 committed、not committed、indeterminate、receipt/state/generation mismatch 测试 |
| `started` 重载变为 failed | PASS | formal `restart=PASS`，V5 保留 started 后由 session 原子失败迁移 |
| 深渊与同房下层洞共存 | PASS | `abyss door target can keep its hole`；formal `abyss_hole_descent=PASS` 且重载 depth=2 |
| 地面池满 | PASS | `full pool waits and continues`、`reload pool space wait`；压力地面饱和 `1→601` 且槽内容不变 |
| 背包满 | PASS | `abyss claim full oom overflow atomic` 用 65535 件物品验证拒绝领取、地面奖励保留、无 pending save |
| 同门二次确认 | PASS | `abyss door confirmation abandon`；formal 第 7 图；同方向第二次触发才离房 |
| 离房放弃 | PASS | `abyss door abandon excludes generated unclaimed` 与 `abyss hole confirmation descent`；只把未生成奖励计为 abandoned |
| `LastAbyssResolution` | PASS | V5 round-trip、abandon overwrite、fixture 与 formal 洞摘要均逐字段验证 |
| 普通随机不漂移 | PASS | `room_generation.golden seed chain is stable`、普通词缀前缀保持、Debug/Release 的 Stage 9 全量回归；新增随机只使用命名子域 |

## 工作树与提交卫生

完成文档后执行：

```powershell
git status --short
git diff --check
git log --oneline main..HEAD
git diff --name-only main..HEAD | rg '(^|/)(out|build-release|\.scratch)(/|$)|task3_trace_probe\.obj$|\.png$'
```

构建目录、临时存档和正式截图均位于被忽略的 `out/`，不纳入提交。最终全分支审查补强了 V5/Session 深渊来源校验、cleared 房重置防御、提交期间门视觉、共享深渊规则、深层遭遇合法性和 cleared 直接构造防御；Release 验收另推动截图内容验证器改为可证明低色深结构图与真实空白图的复合判据。修复后复审为 Critical/Important/Minor 均 0。分支停在 Stage 10，不合并 `main`，不推进 Stage 11。

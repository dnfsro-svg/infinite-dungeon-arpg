# Stage 11-D 地面物品过滤验证记录

## 范围与停止边界

Stage 11-D 在 Stage 11-C 基线上完成地面物品标签、过滤预设、自动拾取门槛、深渊奖励例外、拾取 HUD 回执，以及设置 V2 持久化闭环。本阶段不实现 macOS、主菜单、手柄、分辨率/画质设置或 Stage 12；完成后保留 `codex/stage11d-loot-filter` 分支与工作树，不合并 `main`。

## 玩家语义

暂停设置页提供三个预设：

| 预设 | 普通房显示与自动拾取 | 被过滤物品 |
| --- | --- | --- |
| 显示全部 | 普通、魔法、稀有 | 无 |
| 魔法及以上 | 魔法、稀有 | 普通仍留在生产 `DungeonSnapshot::ground_items` |
| 仅稀有 | 稀有 | 普通、魔法仍留在生产 `DungeonSnapshot::ground_items` |

过滤只控制普通房地面标签和自动拾取门槛，不删除掉落、不伪造背包转移，也不改写稳定物品状态。深渊奖励具有独立例外：低于当前预设门槛的深渊奖励仍显示并可由生产自动拾取链领取。拾取提交成功后，HUD 情境区显示 `已拾取：<品质> <物品名> · i<等级>`；保存、恢复和确认类高优先级通知仍可覆盖它。

## 设置草稿、应用与迁移

进入设置页后，地面物品过滤行修改的是 `draft.loot_filter_mode`，生产 renderer 仅在设置页使用草稿预览。取消或 `Esc` 丢弃草稿并恢复已提交标签；只有设置双槽保存成功、运行时读回成功后，“应用”才发布新的过滤与自动拾取策略。

设置编码仍为固定 44 字节，格式从 V1 升级为 V2，字节 23 存放稳定的过滤枚举。V1 记录要求该保留字节为零；成功解码时保留旧音量、窗口、VSync 与十项绑定，并将过滤预设迁移为“显示全部”。V2 的枚举越界、CRC、长度、保留字段或绑定非法仍按既有损坏恢复规则处理，不触碰角色 V6 双槽。

## 架构与热路径

- `settings` 拥有稳定枚举、V1/V2 codec、校验和双槽事务，不依赖 raylib、Dungeon、Combat 或角色 persistence。
- `dungeon` 保持掉落所有权与领取位图；普通自动拾取使用最小品质策略，深渊奖励走明确例外，不允许 presentation 直接完成领取。
- `platform/raylib` 从公开快照一次构建固定容量 `GroundLootView`；房间标签与 HUD 消费同一视图，host 不构建第二份过滤结果。
- 过滤、标签格式化与自动拾取压力路径使用固定容量，100,000 次视图构建和 1,000 次领取链由零分配/确定性测试保护。
- 架构与 evidence guards 拒绝核心模块反向依赖、私有快照注入、直接拾取完成、伪设置发布、第二渲染计划、呈现前截图和仅检查文件存在的假验证。

## 正式 raylib 证据

`stage11d.loot_formal` 启动生产 `run_raylib_host`，使用真实 `SaveStore`、`SettingsStore`、生产 `DungeonSnapshot`、物理按键映射和生产 renderer。截图只由唯一 helper 在 `EndDrawing()` 后生成。测试声明 `RUN_SERIAL`，避免同一 CTest 进程中的正式 GUI 用例竞争前台焦点。

提交证据位于 `docs/validation/evidence/stage11d/`：

| 场景 | 证据 | 必须成立的语义 |
| --- | --- | --- |
| 显示全部 | `show-all.png/.txt` | 同屏存在普通、魔法、稀有三个真实掉落 |
| 魔法及以上 | `magic-plus.png/.txt` | snapshot 仍有三件；普通隐藏且未进背包；魔法、稀有可见 |
| 仅稀有 | `rare-only.png/.txt` | snapshot 仍有三件；仅稀有标签可见 |
| 深渊例外 | `rare-abyss.png/.txt` | “仅稀有”下低品质深渊奖励可见、可领取且提交代数有效 |
| 预览后取消 | `preview-cancel.png/.txt` | 草稿可见数变化，取消后 committed/draft 与三标签恢复 |
| 自动拾取回执 | `pickup-feedback.png/.txt` | 精确物品从地面移入背包，HUD 显示中文“已拾取”回执 |

PowerShell validator 要求六张 PNG 都是最近生成的 1280×720 图像、哈希互不相同、标签/HUD 区域具有真实像素变化，且工作目录与提交目录的 PNG、摘要和 manifest 逐字节一致。摘要还验证精确物品 ID、隐藏物未进背包、深渊领取、预览回滚和 pickup commit generation。

`stage11d.loot_root_safety` 使用独立证据根，证明白名单 reset 会删除已知证据但保留未知 sentinel，并拒绝源码根、文件系统根、符号链接、Windows reparse/junction 和 canonical escape。正式 harness 不对调用者路径执行递归删除。

## 测试映射

| 要求 | 主要验证 |
| --- | --- |
| 设置 V2、V1 迁移、三个枚举、双槽事务 | `settings.units`, `stage11b.settings_stress.atomic_reload_zero_alloc` |
| 普通过滤、深渊例外、精确领取回执 | `items.units`, `dungeon.units`, `platform.units` |
| 100,000 次视图构建零分配 | `platform.units` 的 `stage11d-ground-loot` stress |
| 模块边界与 27/8 mutation | `stage11d.architecture.loot_boundaries[_self_test]` |
| renderer 单视图与 4/2 mutation | `stage11d.renderer_integration_guard[_self_test]` |
| 六场生产截图与语义 | `stage11d.loot_formal`, `stage11d.loot_evidence_validator` |
| 21 个 evidence 绕过 mutation | `stage11d.loot_evidence_guard[_self_test]` |
| 白名单清理与恶意根拒绝 | `stage11d.loot_root_safety` |

## 最终门禁

最终门禁于 2026-07-19 在 `codex/stage11d-loot-filter` 工作树串行执行，未并发启动第二个 raylib/CTest 进程：

| 门禁 | 结果 |
| --- | --- |
| Stage 8/10/11-B/11-C/11-D 聚焦回归 | 37/37 通过，491.02 秒 |
| Debug `Build.ps1 -Preset windows-msvc-debug -Fresh` | 全新 CMake 配置成功；现有对象已是最新，Ninja 无需重编 |
| Debug 完整 CTest | 79/79 通过，751.25 秒 |
| Release `Build.ps1 -Preset windows-msvc-release -Fresh` | 全新配置并完成 285/285 编译/链接步骤 |
| Release 完整 CTest | 79/79 通过，606.31 秒 |

工具链为 MSVC 19.44.35228.0、Windows SDK 10.0.26100.0、C++17。`cmake/Dependencies.cmake` 将 raylib 固定到提交 `dbc56a87da87d973a9c5baa4e7438a9d20121d28`，配置时校验版本宏为 6.0.0，并要求目标类型为 `STATIC_LIBRARY`。最终 Release 产物包含 `lib/raylib.lib`，不包含 `raylib.dll`；游戏可执行文件为 `out/build/windows-msvc-release/bin/arpg_game.exe`。

最终 Release manifest：

| 场景 | 语义哈希 | PNG SHA-256 |
| --- | ---: | --- |
| 显示全部 | `15491312844169377667` | `B8502E0B578750E873AA53AAFF320876DC95BC8AF5592BE2C18D66DF2CB2D7C4` |
| 魔法及以上 | `6946946237528180539` | `657F13CB2B26D438CEC3E0C5125816B922DF49ACB2FBAE55CACBF1BFF12B9BB3` |
| 仅稀有 | `11171795112977828828` | `C8396D14DD7014F96C4BC13BFADB49299E26509518D44A6A700AF36D739B74BB` |
| 深渊例外 | `14104438262400108594` | `BD6F0424F7B5F7D7D73E0848BEDA8665C24C79B3088CCA77832AC233522D4F63` |
| 预览后取消 | `7172763780660030846` | `96BA034C2091373C2C2A920FF1C1487C6E2E5FF743D558632ED6E652AA118217` |
| 自动拾取回执 | `4207614990933268893` | `17E8290791C000061BE2175B897C59492ED43618A74BA4CF804399E8B4378065` |

manifest 还固定普通房 `root=62, room=979, depth=1` 与深渊 `root=1, room=6`，最终结果为 `result=pass`。工作树审计确认 `out/` 无跟踪文件、无生成 DLL 或构建产物进入提交；最终提交只包含 Stage 11-D 源码、测试、README、验证文档和对应证据。Stage 12 未开始。

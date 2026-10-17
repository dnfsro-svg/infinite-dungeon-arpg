# Infinite Dungeon ARPG 项目交接

> 快照日期：2026-07-31
> 本文以当前代码为准，用于接替开发、验收和合并。历史 README、阶段计划和验证文档只代表各自提交时的状态，不能覆盖本文列出的当前差异。

## 1. 接手时先确认这四件事

| 项目 | 当前值 |
| --- | --- |
| 唯一开发工作树 | E:\game\.worktrees\whole-game-refactor-2026-07 |
| 分支 | codex/whole-game-refactor-2026-07 |
| HEAD | 30c0779d21c328f93a6231c3435573e68c08c84d |
| 相对本地 origin/main | 0 behind / 106 ahead；提交链线性 |
| 上游分支 | 未设置，远端也还没有同名分支 |
| 正式平台 | Windows x64，MSVC 19.44，Windows SDK 10.0.26100.0 |
| 游戏框架 | C++17、raylib 6.0.0、CMake 3.25+、Ninja、CTest |

不要在主控目录 E:\game 直接开发、暂存、清理、合并或重置。主控 main 自身有未提交内容，而且仓库注册了大量 worktree；任何 Git 或构建命令都应显式在上表工作树执行，并先读回分支与 HEAD。

建议每次开始前运行：

    Set-Location 'E:\game\.worktrees\whole-game-refactor-2026-07'
    git branch --show-current
    git rev-parse HEAD
    git status --short --untracked-files=all

### 必须保护的工作区内容

当前有 6 个 tracked modified 路径：

- .superpowers/sdd/2026-07-28-raylib-host-structural-refactor/progress.md
- docs/validation/evidence/stage15-audio-mix/stage15-audio-evidence.txt
- docs/validation/raylib-host-refactor-baseline.md
- src/platform/raylib/raylib_host.hpp
- tests/dungeon/CMakeLists.txt
- tests/dungeon/stage9_evidence_guard_test.cmake

其中实质内容差异集中在 progress.md 和 raylib-host-refactor-baseline.md；其余多为 stat/EOL dirt，但仍不得擅自还原。另有 3 个 tests/platform/__pycache__/*.pyc 生成物。不要 wildcard stage，不要 git clean，不要 reset；提交时只精确暂存本任务路径。

本文档 docs/project-handoff.md 是本次新增的未跟踪文件，也属于必须精确处理的工作区内容；在 Task 9 完成门禁满足前不要把它与 progress.md 或其他脏文件一起批量提交。

## 2. 当前结论

项目不是 README 所写的“止步 Stage 17”。当前代码已经包含大房间与高密度怪物、材质运行时接线、桌面启动器、HP 药水、异步双槽保存、raylib host 结构重构和 V9 房间进度存档边界。

但项目也没有完成最终收尾：

- V9 边界的聚焦 Debug/Release 验证已通过，完整 Task 9 门禁尚未闭环。
- 当前 HEAD 尚未完成 Debug 128/128、Release 128/128 的全量回归。
- Stage 10/11/11B/11C/11D/17 的正式画面证据和真实窗口动作矩阵没有在当前 HEAD 全量重跑。
- 大房间玩家跟随镜头未实现。
- 原生 240 FPS、八核帧准备池和“即将卡顿时复用/插帧”未实现。
- 强化最高 +15 的硬上限未在运行代码中执行。
- README 的技能键和存档版本说明已经漂移。
- 当前分支未推送，Codex Cloud 和 GitHub 看不到 30c0779。
- 桌面快捷方式仍指向 2026-07-26 部署的旧版，不代表当前分支。

因此，当前可描述为：主要玩法和 V9 架构改造已落地，最终回归、真实窗口验收、文档同步、推送和集成尚未完成。

## 3. 产品与玩法基线

这是单人、低资源、DNF 式 2.5D 房间动作 ARPG。启动后直接进入唯一的无限地下城，没有城镇、多人或传统迷宫。只模拟当前房间；房间之间通过保存事务切换。

### 房间循环

1. 根据根种子、房间序号、深度和入口确定房间。
2. 一次性生成全房怪物蓝图和环境蓝图。
3. 玩家附近区域的怪物进入活动池，离屏怪物保存在固定容量字段中。
4. 永久击杀初始怪物数的 25% 后开放四向门。
5. 100% 击杀才算 full clear，并结算完整房间奖励；不要把“出口开放”和“完全清房”合并。
6. 走入上、下、左、右门会分别增加火、水、电、混沌偏向，并进入同层下一房。
7. 房间有 10% 概率出现下层洞；清理条件满足后在洞附近按交互键下层，深度增加、层内房间号重置为 1、元素偏向重置。
8. 下一房门有 1% 基础概率成为深渊入口；深渊只影响该房间，不改变整层规则。

### 当前大房间参数

| 项目 | 当前实现 |
| --- | --- |
| 房间形状 | 正方形 |
| 边长 | 约 162.48 世界单位 |
| 空间网格 | 20 × 20 |
| 普通房密度 | 拥挤 50%：300–400；密集 35%：425–550；兽潮 15%：575–750 |
| 深渊密度 | 普通结果 × 1.5，最高 1125 |
| 全房怪物容量 | 1152 |
| 同时活动怪物容量 | 128 |
| 最大流式活动量 | 7 × 5 × 3 = 105 |
| 投射物 / 危险区域 | 512 / 128 |
| 环境网格 / 蓝图记录 / 运行时障碍状态 | 400 / 1200 / 400 |

全房怪物不是全部同时运行。RoomMonsterField 在网格切换时先回写活动怪物，再清空活动池并重建目标区域。修改流式逻辑时绝不能省略回写，否则离屏怪物的生命、词条和状态会丢失。

### 战斗与成长

- J：轻击/连击与空中攻击。
- K：跳跃。
- L：上挑；设计要求包含击飞并浮空约 0.5 秒。
- 五个主动技能槽；默认槽 1 为拔刀斩，槽 2 为极·鬼剑术（暴风式），槽 3–5 为空。
- 每个主动槽有五个辅助槽占位，但辅助技能 ID 集合为空；辅助石掉落、装卸和效果尚未实现。
- 拔刀斩使用 90 tick / 36 帧时间线。
- 暴风式使用 360 tick / 144 个时间线帧、12 段攻击和终结击飞，但独立素材画格只有 24 张，其余时间线帧复用映射，不能宣称有 144 张原画。
- 怪物拥有近战、远程、支援、危险区和多种词条行为。
- 死亡回退到上一层普通房，最低仍是第 1 层；深渊死亡走相同死亡流程并结算深渊失败。
- 启动读到待继续死亡状态时会自动提交继续，避免进入游戏即停在死亡界面。

### 装备、材料与防御

- 装备稀有度只有普通、魔法、稀有；没有鉴定状态。
- 三件未装备、同基底、同稀有度装备可三合一，产物取平均物品等级，强化归零。
- 材料袋包含九种制作货币、强化石和 +6/+9/+12/+15 强化券。
- 强化券可直接把低于券面等级的装备提升到券面。
- 强化成功率：+1～+3 为 100%；+4/+5/+6 为 90%/80%/70%；+7～+12 为 70%；+13 为 60%，之后每级乘 0.95，最低 0.1%。
- 失败后：+1～+6 不变；+7～+9 回 +6；+10～+11 归零；+12 及以上销毁装备。
- 武器强化底材伤害，防具强化底材护甲，饰品增加四元素减伤；词缀不随强化放大。
- 护甲与闪避共用陡峭曲线：100 → 20%，1000 → 40%，10000 → 60%，100000 → 80%，继续向 100% 逼近。
- 元素减伤默认上限 75%，词缀可提高，代码硬上限 95%。
- HP 药水有 30% 独立掉落率，回复最大生命的 25%；当前生命不高于 75% 时靠近会自动拾取并立即使用。

已知规则缺口：item_crafting 当前只防 uint32 溢出，没有阻止 +15 后继续强化，因此 +16 及以上仍可被请求。这与“最高 +15”的确认规则冲突，修复时应先补测试，再在预览和事务入口共同拒绝。

## 4. 实际操作键

以下是当前代码真相，不是旧 README 的说法：

| 按键 | 操作 | 是否可重绑 |
| --- | --- | --- |
| W / S / A / D | 上 / 下 / 左 / 右移动 | 是 |
| J | 轻击/连击、空中攻击 | 是 |
| K | 跳跃 | 是 |
| L | 上挑 | 是 |
| 当前“交互”绑定（默认 E） | 洞口交互 | 是 |
| 物理 E | 允许时死亡继续 | 否 |
| I | 背包 | 是 |
| P | 被动星盘 | 是 |
| NumPad 1–5 | 请求对应主动技能槽 | 否 |
| R | 重置当前房；仅当 R 未被其他绑定占用 | 否 |
| N | 双槽坏档恢复并开始新游戏 | 否 |
| F1 | 调试层 | 否 |
| F12 或 V | 截图 | 否 |
| Esc | 关闭覆盖层、暂停、返回或退出确认 | 否 |
| 鼠标左键 | 选择物品/材料/技能槽并操作暂停、设置和按钮 | 否 |
| 鼠标右键，或 Ctrl + 鼠标左键 | 在背包中标记/切换三合一配方选择 | 否 |
| 鼠标滚轮 | 滚动背包等可滚动列表 | 否 |

README 当前写的是普通数字键 1–5，但 host_input.cpp 实际监听 KEY_KP_1～KEY_KP_5，即小键盘。HUD 操作提示也没有清楚显示这组键。这是需要产品确认后统一代码、提示和 README 的高优先级漂移。

## 5. 架构与依赖

### 依赖方向

    app
      -> arpg_raylib
           -> dungeon
                -> combat
           -> persistence
           -> settings

    dungeon / combat / persistence
      -> checkpoint
           -> core / abyss / skills / modifiers / items / progression / passives

根 CMake 有永久依赖边界：

- checkpoint 不得依赖 combat、dungeon、persistence、settings、raylib 或 app。
- persistence 及 persistence link-isolation 探针不得链接 combat 或 dungeon。
- raylib 窗口生命周期调用被 forced-include poison 限制；InitWindow、CloseWindow 等只能由审查过的 owner 文件使用。

不要通过“临时 include 高层头”绕过这些边界。V9 重构的主要目的就是让 Persistence 在源码和链接层都与 Combat/Dungeon 解耦。

### 主要模块

| 模块 | 职责 | 关键入口 |
| --- | --- | --- |
| src/app | 参数解析和启动 host | src/app/main.cpp |
| src/core | 固定步、确定性 RNG、CRC、有界队列 | src/core/fixed_step.hpp |
| src/checkpoint | 与 wire 版本解耦的中立 checkpoint DTO、结构校验、稳定枚举 | src/checkpoint/room_checkpoint_schema.hpp |
| src/combat | 玩家、怪物池、AI、技能、碰撞、伤害、战斗快照 | src/combat/combat_world.hpp |
| src/dungeon | 房间状态机、生成、掉落、成长、深渊、死亡、转场事务 | src/dungeon/dungeon_session.hpp |
| src/items | 底材、词缀、制作、强化、材料 | src/items/item_crafting.cpp |
| src/modifiers | 属性聚合、护甲/闪避曲线、效果 | src/modifiers/player_modifier_values.cpp |
| src/passives / progression | 被动树、经验与等级 | 对应目录 |
| src/persistence | V8/V9 编解码、A/B 槽、恢复、工作线程 | src/persistence/room_progress_codec.cpp |
| src/platform/settings | 双槽设置和重绑定 | src/platform/settings/settings_types.cpp |
| src/platform/raylib | 窗口、输入、运行时、渲染、UI、音频、正式证据 | src/platform/raylib/raylib_host.cpp |
| src/launcher | 原生 Win32 启动器和部署入口 | src/launcher/launcher_main.cpp |

### 运行数据流

    物理输入
      -> PhysicalKeySnapshot / HostFrameInput
      -> 移动与普通动作缓冲
      -> 60 Hz FixedStep
      -> DungeonRuntime::fixed_tick
      -> DungeonSession::tick
      -> CombatWorld::tick

    主动技能、重置、下洞及 UI/事务命令
      -> 在呈现帧同步请求
      -> 立即改变瞬时状态，或进入 PendingSave 事务

    当前权威状态
      -> DungeonSnapshot
      -> raylib 渲染与插值

    权威状态变更
      -> PendingSave(next_state)
      -> SaveCommitWorker
      -> V9 编码、临时文件、A/B 发布、回读校验
      -> revision/generation 回执
      -> DungeonSession 发布 stable_state

装备、拾取、技能槽、转场和死亡等权威变更都必须经过保存事务。精确保存进行时会关闭权威请求，并要求物理按键全部释放后重新武装；这里是最容易引入“所有按键偶尔没反应”的位置。

### 大对象和移动陷阱

- RoomMonsterField 持有指向 CombatWorld 活动怪物池的内部指针。移动或恢复世界后必须重新绑定，不能依赖默认移动语义。
- 大型 checkpoint DTO 明确不可复制、不可移动，运行时和测试应堆分配并复用。
- V9 单份编码上限为 8 MiB；工作线程持有两个任务槽和固定编码缓冲，总驻留预算约 32 MiB。
- 战斗热路径的池/队列以及 worker 回读校验有固定容量或无分配要求，不能用无界容器临时替代。加载、状态构造和物品所有权路径允许受上限约束的堆分配。

## 6. 存档与设置

### 角色存档

当前外层格式是 V9：

- magic：ARPGSV9
- 版本：9
- rules version：1
- 带长度和 CRC 校验
- V9 包含一份规范化的 V8 durable image，并增加精确房间/战斗进度
- A/B 双槽选择、临时文件发布、回读和失败恢复
- 候选对象完整解码/校验成功后才发布，恢复保持 failure-atomic

因此“项目还是 V8”不准确。准确说法是：当前写入 V9 外层，其中嵌入兼容的 V8 持久状态图像。

默认角色存档目录：

    %LOCALAPPDATA%\InfiniteDungeon\save

可用参数：

    --seed <十进制或 0x 十六进制>
    --save-dir <目录>
    --settings-dir <目录>
    --screenshot-dir <目录>

测试和试玩必须使用隔离的 save/settings 目录，避免污染用户正式档。

### 设置

设置使用独立 settings-a.bin / settings-b.bin 双槽，当前设置格式为 V3，44 字节。支持：

- 主音量、战斗音效、音乐、环境声、UI 音量
- 窗口化/全屏
- VSync
- 地面物品过滤
- 十项玩法动作重绑定

默认 VSync 开启。设置草稿支持预览、应用和回滚；只有保存成功后输入和过滤策略才成为当前权威。

## 7. 画面、材质、字体与音频

生产构建会复制：

- assets/fonts
- assets/player
- assets/skills
- assets/stage12
- assets/stage14/audio
- assets/stage15/audio

当前材质清单已经接入玩家、八类怪物、四生态背景/道具、元素门、装备/UI 和两个主动技能。怪物、门和技能会在材质就绪时走图集；材质未就绪时可选择备用路径。因此“资源存在但完全没接线”已不是当前事实，但运行时单帧绘制失败的回退并不完整：技能材质模式不会自动切回程序特效，怪物失败路径也可能只绘制剪影。

不过不能据此宣称最终画面已经验收：

- 当前 HEAD 的全量正式 raylib 证据尚未重跑。
- 暴风式只有 24 个独立素材画格。
- 材质仍是分散目录，没有一个供 ChatGPT 直接识别的单文件整包。
- 当前只使用 GLSL 330 风格的普通材质片元管线，没有 OpenGL 4.3 Compute Shader、SSBO、GPU gameplay 或 AI/光流插帧。
- 材质纹理驻留硬上限为 256 MiB。

共享中文 UI 绘制路径使用仓库内 Noto Sans CJK Medium，以 96 px 源尺寸加载并使用双线性过滤；该路径的前景文字强制不透明纯色并带独立阴影，也有越界/遮挡审计。星盘、调试层和部分场景文字仍直接使用默认 DrawText，尚未完全统一。当前没有统一 2×/4× UI RenderTexture 超采样，只是高分辨率字形源和按视口缩放。

音频包含 14 个战斗 WAV、2 条音乐、2 条环境声和 6 个 UI 音效。GameAudio 统一管理唯一音频设备和五路混音。自动测试只能证明加载、路由和资源边界；主观音量与听感仍需真人验收。

素材许可证与代码许可证分开：

- 代码：GPL-3.0-only。
- 字体：assets/fonts/OFL.txt 和同目录 README。
- Stage 12：各项 *-source.md、README 和 build manifest。
- Stage 14/15 音频：各目录 SOURCES.md。

分发前必须保留这些来源与许可证文件。

## 8. 性能现状：不要把计划当实现

当前权威模拟固定为 60 Hz，每个呈现帧最多追赶 8 个 tick，超额时间会被丢弃。渲染使用上一/当前快照插值；默认 VSync 开启，VSync 关闭时没有已确认的 SetTargetFPS(240) 软件限速路径。

当前没有：

- 原生固定 240 FPS presentation pacer
- 1/2/4/8 lane 帧准备池
- 八核战斗或 AI 并行
- 卡顿预测
- 光流、AI 插帧或生成中间游戏状态
- OpenGL 4.3 compute shader

src 中唯一明确的后台工作线程是 SaveCommitWorker。战斗、地下城、raylib、音频、RNG 和最终绘制仍在主线程。

docs/superpowers/plans/2026-07-26-visible-set-240fps-eight-core.md 是计划，不是完成记录。该计划明确要求保持 60 Hz 权威、VSync 开启时跟随显示器、VSync 关闭时最多 240 FPS，并把总执行 lane 限制为 8；当前生产代码尚未实现这些接口。

静态审查识别出的候选热点（尚待 Release profiler 验证）：

- raylib 主循环会频繁构造完整 DungeonSnapshot。
- 快照包含大怪物/投射物/危险区/地面掉落数组。
- 高刷新率下，完整快照复制和全房投影可能消耗 CPU/内存带宽。
- run_raylib_host 仍约 1200 行，继续混合运行时协调、UI、输入、验证、截图和清理。

任何 CPU/GPU 迁移都应先用 Release 测量。不要为了“八核”并行权威 AI、RNG、存档发布或 raylib 调用，也不要引入忙等。

## 9. 已知功能缺口和文档漂移

### P1：需要优先处理

1. **玩家跟随镜头未实现。** 当前仍把约 162.48 × 162.48 的整个房间压缩到屏幕，Camera2D 主要只有战斗反馈偏移。这与“大于屏幕的房间、镜头随角色移动”的硬性要求冲突。
2. **强化 +15 未封顶。** +15 后仍能请求 +16。
3. **README 主动技能按键错误。** 实际是小键盘 NumPad 1–5，README 写普通 1–5。
4. **README 存档版本错误。** 当前是 V9 外层嵌入 V8 durable image。
5. **Task 9 未闭环。** 当前 HEAD 没有完整 128/128 双配置结果和最终真实窗口证据。
6. **材质运行时失败回退不完整。** 资源未就绪有备用路径，但材质模式中的单帧失败不能保证完整回到旧程序表现。

### P2：后续结构风险

1. run_raylib_host 仍然过重。
2. RaylibHostConfig 仍直接暴露多组 Stage 验证场景，生产配置与正式验证配置未完全分层。
3. DungeonSession 同时管理房间、经济、掉落、技能、深渊、死亡和保存事务。
4. CombatWorld 类仍聚合玩家、AI、技能、碰撞、投射物、效果和 checkpoint 适配。
5. 当前操作提示没有明确显示 NumPad 技能键。
6. 辅助技能石、主菜单、手柄、macOS、分辨率/画质设置仍未实现。

## 10. 构建、运行和部署

### 构建

仓库脚本会定位 VS 2022 Build Tools，进入 x64 DevShell，并校验 MSVC/SDK/CMake/Ninja：

    Set-Location 'E:\game\.worktrees\whole-game-refactor-2026-07'
    $env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
    .\scripts\Build.ps1 -Preset windows-msvc-debug
    .\scripts\Build.ps1 -Preset windows-msvc-release

可选 preset：

- windows-msvc-debug
- windows-msvc-release
- windows-msvc-core-debug（不构建 graphics）

raylib 通过 FetchContent 使用固定提交归档和 SHA-256，并静态链接；第一次配置可能需要下载。

### 直接运行当前分支

    .\out\build\windows-msvc-release\bin\arpg_game.exe --seed 12345 --save-dir .\out\manual-save --settings-dir .\out\manual-settings --screenshot-dir .\out\manual-shots

不要用当前桌面快捷方式判断当前 HEAD。现有快捷方式目标虽然正确指向：

    %LOCALAPPDATA%\InfiniteDungeon\app\无限地下城启动器.exe

但部署内容是 2026-07-26 旧版。

### 最终通过后部署启动器

    .\scripts\Build.ps1 -Preset windows-msvc-release
    .\scripts\DeployLauncher.ps1 -GameBuildDirectory .\out\build\windows-msvc-release\bin

DeployLauncher 只接受 Release bin，先在 LocalAppData 建 staging，再原子替换应用目录，最后创建桌面“无限地下城.lnk”。部署后应读回快捷方式目标和两个 EXE 哈希。

## 11. 测试与当前证据

Debug 和 Release 当前各注册 128 个 CTest，包含单元、架构边界、负向 mutation、自包含资产流水线、真实 raylib 和正式 evidence fixture。部分测试超时预算很长；必须串行执行，先做系统资源预检。

### 已证明

当前 V9 相关聚焦集合此前已在 Debug 和 Release 各通过 10/10：

- architecture.persistence_checkpoint_only
- architecture.checkpoint_no_runtime_headers
- checkpoint.units
- combat.units
- dungeon.units
- persistence.units
- persistence.link_isolation
- persistence.save_commit_stack_guard
- persistence.v9_wire_baseline
- platform.units

五组 V9 artifact 在 Debug/Release 字节一致，并命中冻结长度与 SHA-256。本轮会话曾报告 V9 规格/质量审查结论为 P0/P1/P2/P3 = 0，但仓库中没有可持久审计的独立审查报告；最终收尾仍需重新执行并落盘独立规格与质量审查。

注意：随后为枚举测试执行过 ctest --show-only=json-v1，构建目录中的 LastTest.log 已被刷新；不要把现有 LastTest.log 当作上述 10/10 的持久证据。接手后应重新跑聚焦集合并将控制台输出另存。

### 未证明

- 当前 HEAD 的 Debug 128/128 全量测试：最近一次运行被用户暂停，不能记为通过或失败。
- 当前 HEAD 的 Release 128/128：尚未启动。
- 当前 HEAD 的 Stage 10/11/11B/11C/11D/17 全套正式截图/summary。
- 当前 HEAD 的真实窗口移动、J/K/L、0.5 秒浮空、两技能、拾取、门、洞、死亡继续和重启存档矩阵。
- 当前 HEAD 暂停/空闲 CPU、内存和无残留进程门槛。

旧的 .superpowers/sdd/.../task-9-report.md 验证的是 ba1c93f 和 124 项测试；其中 Debug 99/124、Release 98/124 以及多项画面/资源问题都是旧快照。后续已经修复了多项 guard、fixture、栈和 V9 边界问题，所以它既不能证明当前失败，也不能证明当前通过。最终判断必须来自 30c0779 或后续提交的新结果。

### V9 聚焦复验

    Set-Location 'E:\game\.worktrees\whole-game-refactor-2026-07'
    $env:CMAKE_BUILD_PARALLEL_LEVEL = '1'
    $targets = @(
      'arpg_checkpoint_tests',
      'arpg_checkpoint_v9_wire_baseline',
      'arpg_combat_tests',
      'arpg_dungeon_tests',
      'arpg_persistence_tests',
      'arpg_persistence_link_isolation_tests',
      'arpg_platform_tests',
      'arpg_game'
    )
    $rx = '^(architecture\.persistence_checkpoint_only|architecture\.checkpoint_no_runtime_headers|checkpoint\.units|combat\.units|dungeon\.units|persistence\.units|persistence\.link_isolation|persistence\.save_commit_stack_guard|persistence\.v9_wire_baseline|platform\.units)$'

    foreach ($p in @('windows-msvc-debug', 'windows-msvc-release')) {
      .\scripts\Configure.ps1 -Preset $p
      cmake --build --preset $p --parallel 1 --target $targets
      if ($LASTEXITCODE) { throw "build failed: $p" }
      ctest --test-dir "out/build/$p" -R $rx --output-on-failure -j1 --no-tests=error
      if ($LASTEXITCODE) { throw "focused tests failed: $p" }
    }

### 完整 Task 9

先确认没有编译、测试、游戏或自动化残留，再从 Debug 重新开始。CTest 中断后不要把后半段拼成一次正式“全绿”结论。

    Set-Location 'E:\game\.worktrees\whole-game-refactor-2026-07'
    Get-Process | Where-Object ProcessName -match '^(cl|link|ninja|cmake|ctest|arpg_)'
    $env:CMAKE_BUILD_PARALLEL_LEVEL = '1'

    foreach ($p in @('windows-msvc-debug', 'windows-msvc-release')) {
      .\scripts\Configure.ps1 -Preset $p
      cmake --build --preset $p --parallel 1
      if ($LASTEXITCODE) { throw "build failed: $p" }
      ctest --preset $p -j1
      if ($LASTEXITCODE) { throw "full tests failed: $p" }
    }

不得通过提高 timeout、更新 golden、减少怪物、放宽断言或跳过正式 evidence 来让门禁变绿。失败应回到所属任务修复。

## 12. 当前提交链

V9 边界相关提交：

| 提交 | 作用 |
| --- | --- |
| 5ba484f | 设计中立 V9 checkpoint 边界 |
| d6854ac | 冻结 V9 wire manifest |
| 4a50664 | 新增中立 checkpoint schema |
| d895927 | Combat 适配中立 checkpoint |
| 20661bc | Dungeon/Persistence 切换边界 |
| f8155a0 | 完成运行时切换修复 |
| decce80 | 使 Release queue overflow 确定化 |
| 30c0779 | 跟踪 persistence link-isolation 探针 |

V9 计划文件的 31 个 checkbox 和 Task 9 progress 台账尚未按实际结果收尾。更新时只能记录真实完成项，不得为了“看起来完成”一次性全勾。计划 checkbox 可按持久证据更新；progress.md 只追加真实记录并按 V9 计划继续保持 unstaged，除非主控另行批准专门的文档提交，也不得为台账创建空提交。

当前分支没有 upstream，也未推送。只有完整门禁和真实验收满足后，才应：

1. 精确暂存本任务文档/证据。
2. 提交当前隔离分支。
3. git push -u origin codex/whole-game-refactor-2026-07。
4. 读回远端 ref 哈希。
5. 通过 PR 或新的干净 integration worktree 合并；不要在脏 main 就地 merge。

## 13. 推荐接手顺序

1. 重新跑 V9 聚焦 Debug/Release 10 项并保存日志，确认五组 wire artifact。
2. 按持久证据补齐 V9 计划 checkbox；progress.md 只追加真实记录并保持 unstaged，不得随其他文件提交。
3. 运行当前 HEAD 的 Debug 128/128；发现红项就停在首个真实失败修复。
4. 运行 Release 128/128。
5. 重跑 Stage 10/11/11B/11C/11D/17 正式 evidence。
6. 用当前 Release 工作树 EXE 做真实窗口矩阵和暂停资源采样。
7. 重新执行并落盘最终规格审查和质量审查，P0/P1/P2 必须为 0。
8. 更新 Task 9 的最终度量与验证文档，精确提交、推送，并通过 PR 或干净 integration worktree 集成。
9. 在集成后的 Release 上重新构建、部署桌面启动器，并读回快捷方式和 EXE 哈希。
10. 另开独立任务卡/工作树分别修复并验证 +15 上限、玩家跟随镜头和技能键/README 统一；不得把这些产品改动混入结构重构 Task 9 收尾。

原生 240 FPS/八核计划不应夹进当前 Task 9 收尾。先把当前结构重构和真实性能基线闭环，再单独执行 2026-07-26 计划。

## 14. 接手开发的硬规则

- 一次只做一个已批准任务卡。
- 修改代码前先确认代码真相、历史计划和当前测试。
- 先写能失败的测试，再改生产代码。
- 保持 60 Hz 权威、确定性 RNG、事件顺序和保存字节，除非新规格明确授权改变。
- 不在热路径引入无界分配、每帧线程、std::async 或 busy wait。
- raylib、OpenGL、窗口、输入、音频和最终 draw 仍由主线程拥有。
- 不直接编辑或重生 golden 来掩盖回归。
- 不改测试 timeout 来掩盖性能问题。
- 不删除文件，不重置 Git，不覆盖无关脏改动。
- 构建和测试严格串行，并在长任务前检查 CPU、内存、磁盘和残留进程。
- 真实画面结论必须来自当前工作树 Release EXE，不得用旧桌面部署代替。
- 正常退出必须先请求精确保存并等待 ready 或 faulted，再依次关闭音频、渲染资源、暂停菜单和窗口；DungeonRuntime 销毁前必须停止并 join SaveCommitWorker。禁止普通退出路径直接 CloseWindow 或提前销毁存档存储。
- Linux Codex Cloud 不能通过根 CMake 的 MSVC 硬门槛；云端适合只读规格、diff 和依赖审查，正式构建/CTest/raylib 验收必须在 Windows 本机完成。

## 15. 关键文档索引

| 文档 | 用途 |
| --- | --- |
| README.md | 用户入口，但当前有 Stage、按键和 V8/V9 漂移 |
| docs/superpowers/plans/2026-07-28-raylib-host-structural-refactor.md | 当前结构重构与 Task 9 完成门禁 |
| .superpowers/sdd/2026-07-28-raylib-host-structural-refactor/task-9-report.md | 旧 ba1c93f 阻塞快照，不是当前结论 |
| .superpowers/sdd/2026-07-28-raylib-host-structural-refactor/progress.md | 重构进度台账，当前有必须保护的未提交内容 |
| docs/superpowers/plans/2026-07-31-v9-checkpoint-boundary.md | V9 实施计划 |
| docs/superpowers/specs/2026-07-31-v9-checkpoint-boundary-design.md | V9 架构设计 |
| docs/superpowers/plans/2026-07-26-visible-set-240fps-eight-core.md | 240 FPS/八核计划，尚未实施 |
| docs/validation/raylib-host-refactor-baseline.md | 重构度量与证据，当前有未提交更新 |
| docs/validation/stage17-swordmaster-skill-stones.md | 技能石历史验收 |
| docs/validation/desktop-game-launcher.md | 启动器历史验收 |

## 16. 最后一句

继续开发时，以当前代码、当前 HEAD 的新验证结果和用户最新明确规则为准；README 与历史阶段文档只能作为线索。最危险的误判是把“计划写完”当成“生产实现完成”，或把旧桌面程序/旧 Task 9 报告当成当前分支证据。

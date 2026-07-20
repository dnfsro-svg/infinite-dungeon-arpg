# Stage 15 低资源音频混音验收

Stage 15 在不改变战斗、地下城、掉落、装备、死亡或存档正式需求基线的前提下，加入 2 条音乐、2 条环境声、6 个 UI 音效和五路音量设置。统一的 `GameAudio` 独占 raylib 音频设备并组合 Stage 14 战斗 SFX、Stage 15 流式资源与 UI 音效。

## 资源与许可

4 条 OGG 来自 OpenGameArt 上明确标为 CC0 的 Global Resonance、Trance Dungeon、Loopable Dungeon Ambience 与 Ancient Caverns；6 个 UI 源文件来自 Kenney 官方 UI Audio（CC0）。发布文件、原文件名、作者、来源链接和 SHA-256 见 [SOURCES.md](../../assets/stage15/audio/SOURCES.md)。

发布集合固定为 10 个音频文件，总计 **12,916,359 字节**，低于 **16 MiB** 上限。OGG 保留源字节；UI 音效为 44.1 kHz、16 bit、单声道 PCM WAV。`stage15.audio_sources` 校验精确集合、哈希、格式、时长和预算，`stage15.audio_sources_self_test` 证明哈希变异与多余文件会被拒绝。

## 运行逻辑

- 音乐在探索与战斗之间以 0.40 秒交叉淡化；环境声在普通房与深渊房之间以 0.25 秒交叉淡化。
- 暂停/背包/星盘将音乐和环境声降至 55%，死亡覆盖层降至 35%；战斗 SFX 与 UI 提示保持各自总线控制。
- 主音量、SFX、音乐、环境声和 UI 五路均为 0–100%、5% 步进。设置 V3 仍为 44 字节，V1/V2 迁移保留已有字段并补入新增默认值。
- UI 只在离散边沿播放导航、确认、取消、打开、关闭和奖励提示，连续持有输入不会每帧重放。
- 所有音频只从发行目录本地加载；生产源码没有下载、外部转码或子进程启动。核心、战斗、地下城和持久化层不依赖 Stage 15 资源或 raylib 音频 API。

## 自动证据

[stage15-audio-evidence.txt](evidence/stage15-audio-mix/stage15-audio-evidence.txt) 由正式 raylib 工具生成并记录：WASAPI 设备就绪、4 条 OGG 均成功进入播放状态、6 个 UI WAV 均成功解码为 Sound、回退数为 0、场景路由通过以及磁盘预算通过。

对应 CTest：

- `stage15.audio_formal`：真实 raylib 设备、解码、播放、停止和场景路由。
- `stage15.audio_evidence_validator` / `_self_test`：证据内容交叉校验及伪造结果拒绝。
- `stage15.audio_root_safety`：拒绝向工作树允许根之外写证据。
- `stage15.audio_integration_guard`：统一设备所有权、每帧 stream update、五路增益、宿主接线、无运行期下载/进程及发布复制。
- `platform.units`：349 项平台行为测试；音频场景 100,000 次更新保持零分配，设置 1,000 次原子重载压力测试通过。

## 完整验收

2026-07-20 使用 VS 2022 x64、MSVC 19.44、Windows SDK 10.0.26100.0 和 Release `-Fresh` 完成 123 个构建步骤。初次全量 CTest 暴露并修复了设置页增加 4 行后 Stage 11D `preview-cancel` 旧自动导航仍使用原行号的问题，同时恢复 Stage 11C 要求的宿主源码 seam；断言和需求基线均未放宽。

修复后的当前代码执行单次完整 CTest，结果为 **94/94 通过、0 失败，实际耗时 644.94 秒**。Stage 15 标签共 **7 项**，累计 1.66 秒。测试自动重写的历史 Stage 11D 图像和汇总在记录结果后已恢复，未混入 Stage 15 提交。

## 正常发布运行

发布 EXE 为 `out/build/windows-msvc-release/bin/arpg_game.exe`，构建后会在其旁复制 `assets/stage15/audio/`。无测试参数启动时，进程持续响应；现有外置 `arpg_keyboard_driver.exe` 成功发送 `D/W/J/J/J/L/K/A/S`。运行日志确认 WASAPI 初始化成功、Stage 14 的 14 个 WAV 与 Stage 15 的 4 个 OGG/6 个 UI WAV 全部从发布目录加载，未出现 `ERROR` 或 fallback。

自动验收确认资源可解码、可进入播放状态、路由和音量计算正确；最终响度平衡与音乐审美仍属于人工试听范围。

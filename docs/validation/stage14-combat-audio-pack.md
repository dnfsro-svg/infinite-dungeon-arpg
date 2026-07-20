# Stage 14 combat audio material pack validation

Stage 14 为战斗动作、命中、受击、落地、击杀和三类词条预警提供 14 个随发行包部署的短 PCM WAV。模拟、地下城、掉落和存档层均不读取音频资源或 raylib 音频 API。

## 范围与来源

相对验收起点 `ff4f96d`，`src/core`、`src/combat`、`src/dungeon`、`src/loot` 和 `src/persistence` 没有 Stage 14 差异。资源来自 Kenney 官方包：[RPG Audio](https://kenney.nl/media/pages/assets/rpg-audio/8e99002d76-1677590336/kenney_rpg-audio.zip) 与 [Impact Audio](https://kenney.nl/media/pages/assets/impact-sounds/87b4ddecda-1677589768/kenney_impact-sounds.zip)。原始文件及发布文件的 SHA-256 对照见 [SOURCES.md](../../assets/stage14/audio/SOURCES.md)。

| 路由 | 发布资源 | 原始资源 |
| --- | --- | --- |
| 轻击 J1 | `swing-light-1.wav` | `RPG Audio/knifeSlice.ogg` |
| 轻击 J2 | `swing-light-2.wav` | `RPG Audio/knifeSlice2.ogg` |
| J3 收招 | `swing-finisher.wav` | `RPG Audio/chop.ogg` |
| L 上挑 | `swing-launcher.wav` | `RPG Audio/drawKnife3.ogg` |
| 轻命中 | `impact-1.wav` | `Impact Audio/impactPunch_medium_000.ogg` |
| 中命中 | `impact-2.wav` | `Impact Audio/impactPunch_medium_001.ogg` |
| 重命中 | `impact-3.wav` | `Impact Audio/impactPunch_medium_002.ogg` |
| 重击摘要 | `impact-low.wav` | `Impact Audio/impactPunch_heavy_004.ogg` |
| 玩家受击 | `player-hurt.wav` | `Impact Audio/impactSoft_heavy_003.ogg` |
| 落地 | `landing.wav` | `Impact Audio/footstep_concrete_004.ogg` |
| 敌人击杀 | `enemy-defeat.wav` | `Impact Audio/impactMetal_heavy_004.ogg` |
| 闪烁词条预警 | `warning-blink.wav` | `Impact Audio/impactBell_heavy_004.ogg` |
| 连锁词条预警 | `warning-chain.wav` | `Impact Audio/impactMetal_medium_004.ogg` |
| 死亡词条预警 | `warning-death.wav` | `Impact Audio/impactGlass_heavy_004.ogg` |

## 自动证据

提交证据位于 [stage14-audio-evidence.txt](evidence/stage14-audio-material-pack/stage14-audio-evidence.txt) 与 [stage14-audio-showcase.wav](evidence/stage14-audio-material-pack/stage14-audio-showcase.wav)。`stage14-audio-evidence.txt` 记录：

- 14 个真实 `LoadWave` 解码、14 条允许播放的 cue trace；
- `impact_1` 单资源损坏时恰有 1 次回退，其他资源仍可用；
- PCM 总量为 474,532 B，低于 8,388,608 B 预算；
- showcase 为按资源表顺序拼接、资源间 250 ms 静音的 WAV，380,591 帧，SHA-256 为 `d6d9a10fc62b7891bb099aae50887ead49ed6fbda0d8b821aec4adc8dd234c40`。

`ffprobe -v error -show_entries stream=codec_name,sample_rate,channels,bits_per_sample,duration -of default=noprint_wrappers=1 docs/validation/evidence/stage14-audio-material-pack/stage14-audio-showcase.wav` 的实测输出为 `pcm_s16le`、44100 Hz、1 channel、16 bit、8.630181 s。

CTest 中的 `stage14.audio_sources` 校验来源哈希、14 个 WAV、PCM s16le/44.1 kHz/单声道/16 bit、单个资源不超过 1.5 秒及总预算；`stage14.audio_formal`、`stage14.audio_evidence_validator`、`stage14.audio_root_safety` 分别覆盖真实 raylib 解码及回退/trace、证据内容和受限输出根目录；`stage14.audio_integration_guard` 验证无运行期下载/外部转码/进程启动、核心边界及发行复制规则。

## Release 回归与发行目录

验收命令为：

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-release
```

2026-07-20 在 VS 2022 x64 Release 环境执行一次全量 `-Fresh` 验收，结果为 **87/87 通过、0 失败**，CTest 实际耗时 **614.82 秒**。其中 Stage 14 标签包含 5 项测试，累计 2.95 秒：`stage14.audio_formal`、`stage14.audio_evidence_validator`、`stage14.audio_root_safety`、`stage14.audio_sources` 与 `stage14.audio_integration_guard`。测试重新生成的 Stage 11D 历史证据已在记录结果后恢复，未混入本里程碑提交。

发布可执行文件的目标路径为 `out/build/windows-msvc-release/bin/arpg_game.exe`；构建规则会将 `assets/stage14/audio/` 复制为可执行文件旁的 `assets/stage14/audio/`。

## 真实游戏与人工边界

同一次 Release 构建产生的 `arpg_game.exe` 已在不带测试参数的情况下启动。窗口标题为 `Infinite Dungeon - Stage 3 Dungeon Rules`，进程持续响应；现有外置键盘驱动成功向该窗口发送 `D/W/J/J/J/L/K/A/S`，覆盖移动、三段轻击、L 上挑和跳跃的公开输入链路。运行日志确认 WASAPI 音频设备初始化成功，并逐一记录发行目录内 14 个 WAV 以 44100 Hz、16 bit、单声道成功加载；没有音频回退警告。

桌面观察辅助调用曾无响应约 14 分钟，已终止且未作为验收证据。自动验证与进程日志不能代替人的听觉判断，因此主观响度、音色搭配和最终混音仍留作人工试听边界。

自动验证只证明文件格式、资源哈希、加载/回退、路由和部署；主观响度、音色搭配和最终混音仍须人工试听验收。

## 边界

Stage 14 不实现音乐、环境声、UI 音效或音量分类迁移；Stage 15 尚未启动。

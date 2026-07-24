# 主动技能视觉重做验收

执行日期：2026-07-22（EDT）。本页只记录里程碑 1 的自制素材与 Raylib 演出验收，不改变技能石装配、输入、掉落或房间玩法。

## 验收内容

- `拔刀斩`：36 个图集帧；真实窗口记录蓄势帧和命中帧。
- `极·鬼剑术（暴风式）`：24 把实体剑（前 12 地面、后 12 空中）、空中无敌窗口、12 段时间线斩击、终结阶段。
- 自制暗钢古金实体剑与蓝白能量图集随 EXE 部署，并从可执行文件目录加载。
- 终结阶段固定选用图集第 17–19 帧的蓝白剑柱/金色爆点，避免映射到收势帧。

## Debug 真实 Raylib 证据

证据根目录：
`C:\tmp\arpg-full-material-pack-rebuild\out\build\windows-msvc-debug\tests\platform\stage17 skill stones evidence\stage17-run`

- `01-new-default-1280x720.png`
- `02-draw-slash-windup-1280x720.png`
- `03-draw-slash-hit-1280x720.png`
- `04-storm-ground-array-1280x720.png`
- `05-storm-aerial-array-1280x720.png`
- `06-storm-finisher-1280x720.png`
- `07-restarted-loadout-1280x720.png`

`production-summary.txt` 确认：拔刀帧峰值 35、暴风式剑数峰值 24、12 段斩击、空中无敌窗口、终结阶段、图集就绪、锁定中心与重启后的装配持久化。

## 命令与结果

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|stage17\.skill_stones\.(real_raylib|evidence_validator))$' --output-on-failure
```

结果：3/3 通过。真实窗口验收约 15.61 秒，证据校验约 0.33 秒；这两个时间是验收运行时长，不是游戏帧率基准。

## 已知限制

- 当前验收为逐帧 PNG 与状态文件；尚未加入视频编码器，因此不将短录屏作为通过条件。
- 图集与实体剑的视觉被正式截图覆盖；性能阈值应由后续专门的游戏内采样任务记录，不以本次自动化运行时长替代。

# Stage 17 剑圣技能石验收

Stage 17 增加五个可自由取出、装入和交换的主动技能石槽，以及每槽五个只读辅助槽。新档和 V1–V7 迁移默认槽 1 为 `draw_slash`、槽 2 为 `storm_swords`、槽 3–5 为 `none`，两块主动石归角色所有，25 个辅助槽均为 `none`。

## 规格自审

- 五个主槽均可通过暂停页生产 UI 取出、装入和交换；装配候选只有在真实 `SaveStore` 原子提交成功后才发布，保存失败不会半更新库存或槽位。
- 数字键 `1`–`5` 经 `PhysicalKeySnapshot`、宿主映射和 `DungeonRuntime::submit_frame_actions` 进入战斗；没有直接 Raylib 数字键旁路，也没有私有战斗注入。空槽 3–5 无副作用。
- 五个主槽各有五个只读辅助槽，合计 25 个 `none`；本阶段战斗不读取辅助数组，也不提供辅助石掉落、装卸或数值。
- V8 固定保存 owned bits、五个主技能 ID 与 25 个辅助 ID。V1–V7 迁移到默认装配；V8 往返、非法 ID、重复主动石和保留字段拒绝由持久化测试覆盖。
- 冷却、前摇、剑阵和终结为房间瞬态状态，不写检查点；重启读回装配后所有冷却为零。

## 两项技能

拔刀斩冷却 240 tick，10 tick 前摇、单次判定、14 tick 后摇；基础物理伤害 220，破防伤害 36，前方范围 5.0、末端半宽 3.2，击退速度 0.22。同一目标每次施放最多命中一次。

暴风式冷却 1800 tick，24 tick 前摇；施放时在角色前方 3.5 锁定中心，随后以 6 tick 间隔执行固定 12 段、半径 2.8、基础物理伤害 42 的剑阵。终结半径 3.8、基础物理伤害 360、击退 0.26、击飞 0.24，随后 24 tick 后摇；死亡或换房会取消未完成施放。

固定种子 `170017` 的正式场景实际观测：拔刀斩命中 2 个生产目标；暴风式普通剑段对场内目标累计记录 3 次命中，终结命中 1 次，并确认完整推进 12 段。玩家在剑阵期间移动，锁定中心保持不变。

## 真实 Raylib 场景

`stage17.skill_stones.real_raylib` 分别启动两个独立子进程。第一个在 1280×720 真实 raylib 6.0 / GLFW / OpenGL 窗口内创建真实 V8 新档，通过公开输入施放两技能，再通过背包技能石页执行 `remove1,equip5,swap2_5`；第二个进程从同一存档目录启动真实 `DungeonRuntime`，核验装配持久化且冷却为零。

Release 最近一次运行的五张绝对证据路径：

- `E:\game\.worktrees\stage16-loot-reinforcement\out\build\windows-msvc-release\tests\platform\stage17 skill stones evidence\stage17-run\01-new-default-1280x720.png`
- `E:\game\.worktrees\stage16-loot-reinforcement\out\build\windows-msvc-release\tests\platform\stage17 skill stones evidence\stage17-run\02-draw-slash-hit-1280x720.png`
- `E:\game\.worktrees\stage16-loot-reinforcement\out\build\windows-msvc-release\tests\platform\stage17 skill stones evidence\stage17-run\03-storm-array-1280x720.png`
- `E:\game\.worktrees\stage16-loot-reinforcement\out\build\windows-msvc-release\tests\platform\stage17 skill stones evidence\stage17-run\04-storm-finisher-1280x720.png`
- `E:\game\.worktrees\stage16-loot-reinforcement\out\build\windows-msvc-release\tests\platform\stage17 skill stones evidence\stage17-run\05-restarted-loadout-1280x720.png`

状态文件位于同目录 `stage17-skill-stones-state.txt`。验证器要求恰好五个固定文件名、每张 PNG 大于 4096 字节、解码尺寸恰为 1280×720、修改时间严格晚于本次 `run.marker`，并对解码后的 ARGB 像素做 SHA-256，五张图必须互不相同。它还逐字段精确检查 23 个状态字段。破坏性自测分别截断 PNG、伪造 `save_version`、回拨时间戳，并把已存在的 `MutationRoot` 替换为 junction；四种突变都必须被拒绝，junction 目标的哨兵文件必须保留。递归清理前会解析 evidence 与 mutation 的绝对/真实路径，拒绝路径链及 mutation 子树中的任何 reparse point，并再次确认 mutation 仍在受控 evidence root 内。

## 新鲜验证结果

执行日期：2026-07-21（EDT）。仓库脚本用于进入固定 MSVC/Windows SDK 环境。

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug
.\scripts\Build.ps1 -Preset windows-msvc-debug
ctest --test-dir out/build/windows-msvc-debug -L stage17 --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -LE 'graphics|formal-game' --output-on-failure

.\scripts\Configure.ps1 -Preset windows-msvc-release
.\scripts\Build.ps1 -Preset windows-msvc-release
ctest --test-dir out/build/windows-msvc-release -L stage17 --output-on-failure
ctest --test-dir out/build/windows-msvc-release -LE 'graphics|formal-game' --output-on-failure

ctest --test-dir out/build/windows-msvc-debug -R '^(stage16\.loot_reinforcement\.real_raylib|stage17\.skill_stones\.)' --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R '^architecture\.' --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R '^(stage12\.material_(formal|evidence_validator)|stage11b\.settings_(formal|evidence_validator)|stage11c\.hud_(formal|evidence_validator|evidence_validator_self_test)|stage11d\.loot_(formal|evidence_validator))$' --output-on-failure
```

- Debug Stage17（最终复验）：3/3 通过，0 失败，10.96 秒；真实 Raylib 10.14 秒、验证器 0.29 秒、含根及子树 junction 拒绝的破坏性自测 0.54 秒。
- Debug 完整 `-LE` 门禁：92/92 通过，0 失败，979.69 秒。
- Release 全量构建：213/213 构建步骤成功。
- Release Stage17（安全修复后）：3/3 通过，0 失败，7.62 秒；真实 Raylib 6.82 秒、验证器 0.27 秒、含 junction 拒绝的破坏性自测 0.51 秒。
- Release 完整 `-LE` 门禁：92/92 通过，0 失败，740.11 秒。
- Debug Stage16/17 联合真实 Raylib（修复后）：4/4 通过，0 失败，11.63 秒。
- Debug `^architecture\.`（修复后）：18/18 通过，0 失败，83.99 秒。
- Debug 既有 formal/validator 聚焦复测（修复后）：9/9 通过，0 失败，177.62 秒。

## 已知基线与实现说明

初次直接调用裸 `cmake --preset windows-msvc-debug` 时，当前 PowerShell 未带固定 Windows SDK 环境；改用仓库 `Configure.ps1` 后完成有效 RED：真实场景因尚未实现而失败，两个依赖证据测试未运行。该问题是工具链入口要求，不是产品缺陷。

真实宿主最初把 `DungeonRuntime` 和多个大型快照放在 Windows 主线程栈上，正式窗口启动时触发 `0xC00000FD`。本任务把这些既有大型对象迁到堆所有权，未扩大链接器栈、未绕过生产接口；Debug/Release 全量回归均已通过。此前记录的 Stage 9 旧图形基线不作为 Stage17 通过项；本轮要求的 Stage16/17 真实 Raylib 与两套完整门禁均以本页新鲜结果为准。

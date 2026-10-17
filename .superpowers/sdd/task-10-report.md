# Task 10：Stage 4 确定性压力、分配与容量门禁

## 范围

- 工作树：`E:\game\.worktrees\m04-validation`
- 基线：`dc290f3f0e80130ebdbc941ebb152092160a05e1`
- 仅增加 combat/dungeon 压力与容量验证；生产修复仅为池相关诊断计数的饱和递增。
- 未增加公开生产 API、动态容器、Stage 5 功能或新的运行时容量。

## RED -> GREEN 记录

1. 新增满投射物池/满危险区池测试后，第一次 core Debug 构建按预期因
   `CombatWorldTestAccess` 缺少受控测试夹具接口而失败。
2. 补齐仅测试代码的 friend 夹具后，`arpg_combat_tests.exe` 在新容量用例失败：
   `projectile_saturation_count` 从 `UINT32_MAX` 递增后回绕为零。
3. 最小修复在 `combat_world.cpp` 中加入本地 `saturating_increment`，并仅替换
   projectile/hazard 的 saturation 与 invalid-owner 四个诊断递增点。该逻辑不分配，
   不改变 AI、事件、容量或公开接口。
4. GREEN：core Debug、Debug、Release 全矩阵和 direct 可执行文件均通过。

## 新增门禁

- 10,000 个导演计划：循环覆盖 fire/water/lightning/chaos、深度 1..1000，
  每次双建计划校验字节字段等价、`encounter_plan_legal` 和波次容量；计数器前后无分配。
- 8 角色独立 combat AI 压力：12,000 固定 tick，驱动全角色、投射物和危险区生命周期，
  逐 tick 排空事件；无分配、无事件溢出、无饱和和无无效 owner。
- 各固定池容量：Monster 96、Projectile 384、Hazard 96 填满后额外创建全部拒绝；
  MonsterRuntime、ProjectileSnapshot、HazardSnapshot 所有相关槽字段保持不变；
  projectile/hazard 饱和计数保持 `UINT32_MAX` 而不回绕。
- 1,000 房压力：复用 Task8 已有的受控真实 defeat-event session 夹具，再执行真实
  wave delay、门移动、pending transition 和 save receipt 路径；每次提交显式核对
  generation、稳定房间描述、transitioning 无活动房/无 pending 状态。此处不采用
  脆弱的实际输入机器人；独立 8 角色 AI 压力覆盖真实 combat tick。
- 1,000 房测量输出：`allocation_before=0 allocation_after=0 delta=0`，dungeon、relay、
  combat、input overflow 均为 0。

## 验证

```powershell
.\scripts\Test.ps1 -Preset windows-msvc-core-debug -Fresh
# 12/12 CTest 通过

.\scripts\Test.ps1 -Preset windows-msvc-debug -Fresh
# 13/13 CTest 通过

.\scripts\Test.ps1 -Preset windows-msvc-release -Fresh
# 13/13 CTest 通过

.\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe
.\out\build\windows-msvc-debug\bin\arpg_dungeon_tests.exe
.\out\build\windows-msvc-release\bin\arpg_combat_tests.exe
.\out\build\windows-msvc-release\bin\arpg_dungeon_tests.exe
# combat 四次均：64 cases, 0 failures；dungeon 四次均：67 cases, 0 failures，并输出 1000 房 allocation delta=0 / overflow=0

git diff --check
# 无输出，退出码 0
```

## 风险

- 受控 defeat-event 的 1,000 房验证仍保留，用于 session/wave/save 生命周期压力；8 角色
  AI 另有 12,000 tick 真实 CombatWorld 压力覆盖。
- 真实 launcher-only 输入路径初次在 room38 发现反向近距离 steering/facing deadband。
  以 room38 RED 回归固定后，仅在该死区且 facing 不匹配时增加一 tick 转向 movement；未改变
  生产规则或攻击策略。修复后同 seed 10 房及 1000 committed rooms 均通过，后者输出
  `allocation-delta=0 overflow=0/0/0/0`。详见 `task-10-debug.md`。
- 分配探针统计 `operator new` 调用次数；本阶段断言稳态区间调用次数不增长，满足持续
  分配门禁。

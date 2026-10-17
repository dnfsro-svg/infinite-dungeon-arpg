# Task 11：M04 最终验收记录

工作树：`E:\game\.worktrees\m04-final-validation`  
分支：`task/m04-final-validation`  
基线/HEAD：`0d9d473aa78bb4c28f1a0322a0e03256b3f083df`  
范围审查：`f3f442d..HEAD`  
结果：**通过；桌面白屏已确认是捕获/合成层假象，不是产品渲染故障。**

## 自动化与边界

| 项目 | 结果 |
| --- | --- |
| `scripts/Test.ps1 -Preset windows-msvc-debug -Fresh` | 13/13 CTest 通过。 |
| `scripts/Test.ps1 -Preset windows-msvc-release -Fresh` | 13/13 CTest 通过。 |
| Debug direct executables | core 24、combat 64、dungeon 67、persistence 28、platform 38；全部 0 failures。Dungeon real-input 1000 room：allocation delta=0、overflows=0。 |
| Release direct executables | core 24、combat 64、dungeon 67、persistence 28、platform 38；全部 0 failures。Dungeon real-input 1000 room：allocation delta=0、overflows=0。 |
| source-boundary scan | `src/core`、`src/combat`、`src/dungeon`、`src/persistence` 中无 raylib include；combat 不 include dungeon/persistence。persistence 的 `dungeon_checkpoint.hpp` 为既定 checkpoint-only 边界例外。 |
| `git diff --check f3f442d..HEAD` | 通过，无空白错误。 |
| 工作树状态 | `git status --short` 为空；本验收仅新增工作树外的 `.superpowers/sdd` 记录。 |

## 视觉验收矩阵

Release 隔离启动：

```text
E:\game\.worktrees\m04-final-validation\out\build\windows-msvc-release\bin\arpg_game.exe
  --seed 0x8
  --save-dir E:\game\.worktrees\m04-final-validation\out\acceptance\task11-release-isolated-log-seed8
```

桌面自动化表面在 3 秒后显示纯白，但随后在同一 Debug 隔离运行中以临时的第 0 帧
Raylib framebuffer 截图复核，确认实际画面含深色房间、可读 HUD、HP 1000/1000、Wave
1/2、Targets、Active M/P/H、生态门洞与三个可见怪物。该截图为
`E:\game\.worktrees\m04-final-validation\out\build\windows-msvc-debug\bin\task11-diag-frame.png`。
它与 stderr 的 `BeginDrawing → ClearBackground → renderer.draw → EndDrawing` 连续证据一致，
故不将桌面白屏计为产品缺陷。Esc 正常关闭，确认零残留游戏进程；完整调查见
`E:\game\.superpowers\sdd\task-11-debug.md`。

八角色/生态色/预警/HP 边界/abyss 等离散映射由 platform 38 个直接测试中的纯视图用例覆盖；
真实 framebuffer 则验证了 HUD、one/two-wave 状态与场景绘制链路。
Task9 的同一 Debug 启动链已通过应用 F12 生成
`E:\game\.worktrees\m04-persistence-replay\out\build\windows-msvc-debug\stage3-dungeon-rules.png`；
本轮真实桌面控制再次确认 Esc 关闭后零残留 `arpg_game` 进程。由于当前桌面合成/捕获链持续把
OpenGL 窗口呈为纯白，F12 的当前桌面注入不作为渲染状态证据，改以 Raylib framebuffer 截图和
Task9 的实际 F12 输出交叉验证。

## 独立范围审查 finding

### Critical

无。先前桌面白屏 finding 已由临时帧级 instrumentation 和 Raylib framebuffer 截图证伪。

### Important

无额外 finding。范围内的自动化、直接可执行测试和 diff-check 均通过。

### Minor

无。

## 审查范围说明

`f3f442d..HEAD` 共涉及 Stage 4 怪物目录/固定池/AI/导演、Dungeon 波次、Raylib 渲染与相应测试（56 个文件，6323 行新增、442 行删除）。已检查边界、构建、运行测试和 diff 空白；未修改生产代码、未合并、未推送、未创建 PR。

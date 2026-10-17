### Task 3: Windows Release 确定性验收

**Files:**
- Modify: `.superpowers/sdd/issues.md`

**Interfaces:**
- Consumes: Task 2 的正式 Release 游戏与 seed 6118 确定掉落路线。
- Produces: 安装版哈希一致、吸入动画截图序列、背包数量变化和问题闭环记录。

- [ ] **Step 1: 单次构建与部署**

```powershell
$env:CMAKE_BUILD_PARALLEL_LEVEL='4'
.\scripts\Build.ps1 -Preset windows-msvc-release
.\scripts\DeployLauncher.ps1
```

读取 Build/安装版 `arpg_game.exe` SHA-256，必须一致。

- [ ] **Step 2: seed 6118 内部截图验收**

使用全新隔离 save 目录与静音 settings 启动。输入 `R`、等待 100ms、`Num1`、等待约 350ms，确认 ordinal 158 掉落 `普通 Shadow Hood · i1`；接近到 1.5 米内后以 80–120ms 间隔连续 F12，至少捕获“地面起点 / 飞行中段 / 角色腰侧到达脉冲”三态。

- [ ] **Step 3: 状态与资源读回**

正常关闭后确认：背包数量增加 1、ground item 归零、存档有效；`arpg_game`、`cl`、`cmake`、`ctest`、`ninja` 无残留。若动画无法从静态截图可靠判定，停止并保留现场，不以单元测试代替 Windows 视觉验收。

- [ ] **Step 4: 更新问题记录并提交**

在 `.superpowers/sdd/issues.md` 记录根因、实现、定向测试、Release 哈希、截图路径和资源读回，只提交本任务文件，不纳入共享工作树其他改动。

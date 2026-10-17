# Stage 12 Task 3 报告：原创环境图集与房间材质

## 交付

- 新增 `assets/stage12/environment.png`：1024 × 1024、RGBA；上部为暗色石砌房间，
  下部为火/水/雷/混沌门框和紫色深渊洞口。
- 新增 4 个地板、4 个门和 1 个洞口 `MaterialSpriteId`，并在清单中注册精确帧表和
  environment 图集映射。
- `room_renderer` 首先尝试绘制环境底图；成功时以现有
  `project_render_world`、门状态和洞口状态定位图集门/洞口。标签、箭头、锁门内衬、
  深渊标记、`Press E to descend`、危险地面、掉落和 Y 排序仍沿用既有逻辑。
- 底图、图集或任一绘制失败时，整套房间回退为既有灰盒，并继续使用矩形门和椭圆洞口。

## TDD 与验证

1. **RED**：新增 `stage12_environment_render_tests.cpp` 后，使用 VS 直接编译；预期失败为
   `arpg::platform::select_floor_sprite` 未声明。最初 CMake 预设另外被 `WindowsSDKVersion`
   为空阻断，因此用 VS Developer Command Prompt 取得了精确 RED。
2. **GREEN**：在 VS Developer Command Prompt 中执行：

   ```powershell
   cmake --build --preset windows-msvc-debug --target arpg_platform_tests
   ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
   ```

   结果：`platform.units` 通过，`1/1`，3.50 秒。首次运行发现测试主程序总数
   `291` 与实际 `292` 不符；更新该测试期望后重建并复跑通过。
3. 使用 Pillow 检查：`environment.png` 为 `RGBA (1024, 1024)`。
4. 构建 `arpg_game` 成功，发布后置步骤已复制 Stage 12 材质目录；游戏以固定种子 `979`
   和 `out/stage12-runtime-check/` 隔离存档/设置启动，窗口标题为
   `Infinite Dungeon - Stage 3 Dungeon Rules`。

## 未完成或刻意跳过的验证

- **Stage11D 回归未执行**：其正式测试会将新清单复制到
  `docs/validation/evidence/stage11d/`；该目录在任务开始前已有外部脏改。为避免覆盖，
  按简报要求仅报告此限制，未运行会写证据的 CTest。
- **实际运行截图未落盘**：游戏已启动，但 Windows 窗口截图自动化在捕获时被控制器中断，
  因而没有可交付截图。图集 PNG 本身已人工目检，门、洞口和房间帧布局可见。

## 原创来源、提示词、去键色与打包

原图无损复制到 `art_source/stage12/`。本次没有重新调用图像生成服务；提示词基线、
版权边界、完整帧表、去键色命令和打包命令均记录在
`assets/stage12/environment-source.md`。

去键色使用 imagegen 的 `remove_chroma_key.py`：门自动采样 `#f802f6`，透明
`925109/1572516` 像素；洞口采样 `#06f906`，透明 `1068555/1572864` 像素。随后运行
`tools/build_stage12_environment_atlas.py` 生成最终图集。

## 交接顾虑

`assets/stage12/source/` 是任务开始前已存在的未跟踪输入目录。根据仓库“不要删除已有文件”
约束，本任务保留了它，并且不将其暂存；所有原件也已复制到规范位置
`art_source/stage12/`。因此提交后的受版本控制运行时目录仅含环境图集与说明，但本地工作树
仍存在该未跟踪目录，需由控制器决定是否在获准后移除。

## 提交状态

提交被 Git 所有权安全检查阻断，未更改全局 `safe.directory`：

```text
fatal: detected dubious ownership in repository at 'E:/game/.worktrees/stage12-material-pack'
'.git' is owned by CodexSandboxOffline (SID ...-1007), but the current user is dnfsr (SID ...-1001).
```

因此未能暂存或创建 `feat: add stage 12 comic dungeon environment` 提交。计划暂存清单仅含
本任务资源、渲染代码、测试、`art_source/stage12/`、打包脚本和本报告；根 `CMakeLists.txt` 与
Stage11D 证据脏改均未暂存。

## 审查修复追加

### P1：原子回退

- `MaterialPack::can_draw(MaterialSpriteId)` 现在是无绘制副作用的查询：它检查精灵 ID、
  清单帧、图集可用性和纹理有效性。
- `room_renderer` 在绘制前同时预检当前地板、当前元素门框和洞口帧；门或洞口实际隐藏时
  不视为必需。任一**必需**帧不可绘制时，`should_draw_material_environment` 返回 false，
  整个房间统一使用原灰盒、矩形门和椭圆洞口，不再混用材质和回退几何。
- TDD：先新增原子回退测试并直接编译，得到预期 RED：
  `fatal error C1083: cannot open include file 'environment_render_plan.hpp'`；再实现纯预检
  接口并完成 GREEN。
- 最新定向验证：

  ```text
  cmake --build --preset windows-msvc-debug --target arpg_platform_tests
  ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
  1/1 Test #28: platform.units ... Passed (3.39 sec)
  ```

### P2：种子 979 实际启动与截图

- 启动参数：`--seed 979 --save-dir E:\game\.worktrees\stage12-material-pack\out\stage12-runtime-check\saves --settings-dir E:\game\.worktrees\stage12-material-pack\out\stage12-runtime-check\settings`。
- 截图：[绝对路径](E:\game\.worktrees\stage12-material-pack\out\stage12-runtime-check\stage12-seed979.png)，
  `1038 × 614` PNG。截取时进程窗口标题为
  `Infinite Dungeon - Stage 3 Dungeon Rules`，句柄 `69535370`。
- 观察：截图为白屏窗口；**不包含**可判读的门、洞口、掉落或 `Press E to descend` 范围，
  因而不能作为这些视觉点的通过证据。驱动步骤为：构建 `arpg_game` → 用上述隔离目录启动 →
  等待窗口句柄 → 调用截图技能的 `take_screenshot.ps1 -WindowHandle`。首次捕获误命中一个
  已有 Stage 9 验证窗口，已弃用；第二次按新进程自身 PID/句柄捕获并立即关闭测试进程。

### P2 重新采证：游戏内 Post-Present F12

桌面复制截图的白屏已排除：`raylib_host.cpp` 的 F12 分支会在 `EndDrawing()` 后调用
`LoadImageFromScreen()`，因此改用游戏内正式路径重新采证。最小复现命令为：

```powershell
out/build/windows-msvc-debug/bin/arpg_game.exe --seed 979 `
  --save-dir out/stage12-runtime-check/saves `
  --settings-dir out/stage12-runtime-check/settings
# 窗口可见并等待约 4 秒后按 F12。
```

- 截图绝对路径：`E:\game\.worktrees\stage12-material-pack\out\build\windows-msvc-debug\bin\stage8-equipment-loot.png`
- SHA-256：`AB369035B19618E343646D030D1A0BD440125044DC0D890F6E56F167286E298F`
- 分辨率：`1280 × 720` PNG。
- 截图调用时机：窗口标题为 `Infinite Dungeon - Stage 3 Dungeon Rules` 且已有窗口句柄后，
  等待 4 秒（多个 Present 帧）再发送 F12；文件由游戏内 `EndDrawing()` 之后的
  `LoadImageFromScreen()` 导出。测试进程随后关闭。
- 视觉观察：截图为**非白的真实游戏画面**，可见 Stage 12 暗色石砌房间环境背景和死亡回顾
  覆盖层。门：不可见（被死亡覆盖层遮挡）；洞口：不可见（同上）；掉落：不可见（同上）；
  `Press E to descend` 及其范围：不可见（同上）。因此该图证明了 Post-Present 渲染与环境
  背景实际输出，但不单独证明门、洞口、掉落或 E 提示的无遮挡可见性。

### P2 新存档首房采证（未形成可交付无遮挡画面）

为排除旧死亡存档，连续使用了此前不存在、且不位于 Stage8/Stage11 下的独立目录：

```powershell
out/build/windows-msvc-debug/bin/arpg_game.exe --seed 979 `
  --save-dir E:\game\.worktrees\stage12-material-pack\out\stage12-p2-fresh-979-final2\saves `
  --settings-dir E:\game\.worktrees\stage12-material-pack\out\stage12-p2-fresh-979-final2\settings
```

仅在窗口就绪后等待 2 秒，未注入攻击、移动、交互或任何战斗输入；随后聚焦该窗口并发送 F12。
导出前后比较 `stage8-equipment-loot.png` 的 UTC 修改时间，确认本次 F12 的输出确实刷新，再复制为：

- `E:\game\.worktrees\stage12-material-pack\out\stage12-p2-fresh-979-final2\evidence\seed979-fresh-f12.png`
- SHA-256：`AB369035B19618E343646D030D1A0BD440125044DC0D890F6E56F167286E298F`
- 分辨率：`1280 × 720`。

结果仍为死亡回顾覆盖层，未获得“首个可控存活房间”的无遮挡截图。环境背景可见；至少一扇门、
洞口、掉落与 `Press E to descend` 均不可见并且无法据此判定。先前的同路径首帧尝试虽为非白、
无遮挡的极暗环境底图，但也没有足够可判读的门/洞口。由于固定 seed 979 的每次新目录采证都在
允许的 2 秒窗口内未得到可控存活房间，未继续注入战斗或延长等待；掉落可见性留给 12-D。

# Task 4 Report: 单实例共享字体生命周期

## 结果

- `SharedUiFont` 是唯一 raylib 字体资源所有者；全平台源码仅该文件出现 `LoadFontEx`/`UnloadFont`。
- `HudRenderer`、`DeathOverlayRenderer`、`PauseMenuRenderer` 只借用 `Font` 值与 ready 状态；其 `shutdown()` 只清空借用。
- `CombatRenderer::initialize_resources(const SharedUiFont&)` 统一分发共享字体。
- Host 在窗口与工作目录建立后初始化共享字体；成功关闭顺序为 pause -> combat -> shared -> window。render-snapshot 早退和两个异常处理路径也先清空借用再释放 shared，窗口最后关闭。
- `kUiFontAtlasInstanceCount` 从 Task 3 阶段值 3 原子切换到 1。
- Stage12 真实运行证据：517 glyph，2048x2048 GRAY_ALPHA，`atlas_bytes=8388608`，`instance_count=1`，`total_atlas_bytes=8388608`，单图集预算 33554432。

## TDD RED

1. 测试先改为 `kUiFontAtlasInstanceCount == 1U`，并新增 renderer 禁止字体所有权调用、Host 生命周期顺序与 Stage12 单实例统计约束。
2. 首次直接构建被当前 PowerShell 缺失 MSVC `INCLUDE` 截断：`cmath: No such file or directory`，不是有效 RED。
3. 加载 VS Build Tools 环境后重跑：

```powershell
cmd /d /s /c '"...\VsDevCmd.bat" -arch=x64 -host_arch=x64 >nul && cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2'
```

结果：`shared_ui_font_tests.cpp:77` 与 `hud_font_tests.cpp:261` 均为静态断言失败，准确证明当前实例数仍为 3。

4. 单跑源守卫：

```powershell
ctest --preset windows-msvc-debug --output-on-failure -R '^stage11d\.renderer_integration_guard$'
```

结果：准确 RED：`Shared UI font guard: renderer borrower contains LoadFontEx(`。

## GREEN 与验证

### 构建与窄测试

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2`：exit 0。
- `cmake --build --preset windows-msvc-debug --target arpg_stage12_material_formal --parallel 2`：exit 0。
- foundation：`ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY=1`，最终 15 cases / 0 failures。
- `python tests/platform/ui_material_asset_pipeline_tests.py`：11 tests / OK。
- validator 与 self-test PowerShell 解析：pass。
- `git diff --check`：pass；仅 Git 行尾转换提示，无空白错误。

### Windows 清洁构建内存

命令等价于：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2 --clean-first
```

100ms 采样 `cmake,ninja,cl,link`：

- 257 build steps，exit 0。
- 聚合工作集峰值：2,567,028,736 bytes（约 2.39 GiB）。
- 单进程工作集峰值：1,978,056,704 bytes（约 1.84 GiB）。
- 未出现 OOM。

### Stage12 真实运行

```powershell
ctest --preset windows-msvc-debug --output-on-failure -R '^stage12\.material_formal$'
```

- 运行 474.70s；每个独立 raylib 初始化段各有一次 `Shared UI font loaded`，同一段内无重复加载。
- 字体字段全部通过：`bundled_font_runtime=pass`、instance 1、glyph 517、atlas/total 均 8388608、capacity 640。
- 整体测试仍 fail，原因不是字体生命周期：`hud_viewport_safe=fail`、`input_hole_regression=fail`、HUD bounds/runtime draw fail，以及 Intel UHD 上 stress average 28.405 FPS、1% low 26.505 FPS，低于 60/45 门槛。
- 因正式证据不是 pristine pass，`stage12_material_validator_self_test.ps1` 在 pristine gate 以 exit 1 停止，无法进入负向 mutation；两个脚本均已通过 PowerShell 语法解析。

### 全平台与既有守卫

- `platform.units` / 直接运行全平台：655 cases / 2 failures：
  - ecology `alpha_bounds.x` 与记录不一致；
  - case count actual 655 / expected 654。
  两项与 brief 中 Task2 已知观察一致，Task4 前后未掩盖或改 expected。
- Task4 字体/Host 源守卫已通过新增检查，随后仍在既有 `monster_affix_danger` 守卫断言失败。
- 其他既有 host/loot 守卫分别在 Task8A host-frame-gate、renderer draw、live-settings rollback 的旧基线断言失败；未改写这些 expected。

## 文件与边界

生产修改只涉及 raylib 字体所有权、renderer 注入、Host 生命周期、Stage12 状态字段与 raylib 生命周期 allowlist；测试修改同步单实例、validator 和既有关闭顺序守卫。未修改玩法、存档、设置编码、stable IDs、枚举或版本；未引入 JSON、外部语言包、运行时映射容器或逐帧分配。

brief 外追加维护的测试/构建文件：`src/platform/raylib/CMakeLists.txt`（允许 owner 查询窗口状态）、`tests/platform/ui_material_asset_pipeline_tests.py` 与既有 Host/Stage11C 顺序守卫 self-test（旧三实例/旧关闭顺序断言必须同步，未削弱约束）。

## 自审与疑虑

- 初始化失败时，所有有效但不合约的候选字体立即 `UnloadFont`；owner 保持 not-ready。
- 所有 renderer 析构/重复 shutdown 均不卸载 shared。
- Host 成功、render snapshot 早退、标准异常、未知异常均遵循借用先断开、owner 后卸载、窗口最后关闭。
- 当前没有为 `LoadFontEx` 注入失败 fake；失败清理由源审查和实现分支覆盖，真实成功生命周期由 Stage12 OpenGL 运行验证。完整 Stage12 总门仍被上述既有视觉/性能条件阻塞。

## Fix round 1/5（review 修复）

### 修复结果

- Stage16 的两条真实 capture 路径现在都在 window/`ChangeDirectory` 后接收同一个 `SharedUiFont`；局部 `CombatRenderer` 通过作用域清理先断开借用，main 随后 `shared_ui_font.shutdown()`，最后 `CloseWindow()`。
- `SharedUiFont` 显式删除 copy/move 构造与赋值；ready owner 的第二次 `initialize()` 立即幂等成功，不再 unload/reload，也不改变 owner/borrower 的 texture id、glyph 指针和 atlas metrics。
- 接受候选前同时验证 `IsFontValid`、`IsTextureValid`、实际 `texture.format == PIXELFORMAT_UNCOMPRESSED_GRAY_ALPHA`、实际格式字节数和 32 MiB 预算。所有未采用候选统一进入唯一 `release_font -> UnloadFont` 回收点。
- owner 新增 `active_instance_count()` 与 `total_atlas_bytes()` 运行时事实；Host 的 Stage12 状态只读取这两个接口，不再用实例常量或复制 single bytes 构造 total。
- Host 在 success、render-snapshot early return、`std::exception`、unknown exception 四条路径均显式锁定 pause borrower -> combat borrower -> shared owner -> window 顺序。
- 新增独立 lifecycle guard/self-test；精确限制全 raylib 源码只有 owner 的一个 `LoadFontEx` 和一个 `UnloadFont`，并负向验证重复 load/unload、非幂等、伪造 instance/total 及四条关闭分支乱序。
- Stage12 validator 增加 `-BundledFontOnly` 窄 seam；最小 pristine fixture 不再依赖当前会被 HUD/性能拦截的整套 evidence，真实执行并拒绝 instance=3 与 total=2x mutation。

### 本轮 TDD RED

1. Stage16 先单独构建：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage16_loot_reinforcement_game_validation --parallel 2
```

结果 exit 1，精确失败：

- `stage16_loot_reinforcement_game_validation.cpp(267): C2660 initialize_resources: function does not take 0 arguments`
- `stage16_loot_reinforcement_game_validation.cpp(712): C2660 initialize_resources: function does not take 0 arguments`

2. 在 `shared_ui_font_tests.cpp` 先加入不可 copy/move 静态断言，以及 texture id=0、错误格式、超预算、运行时 owner facts 测试，再构建 `arpg_platform_tests`。结果 exit 1：四个 `static_assert` 准确失败，并同时报告缺少 `ui_font_candidate_is_acceptable`、三参数实际格式字节计算、`active_instance_count`、`total_atlas_bytes`。

3. lifecycle guard self-test 的每个可逆 mutation 都要求 guard 返回非零；它实际拒绝 duplicate load、duplicate unload、去掉 ready 幂等、instance/total 伪造，以及 success/early/std/unknown 四条路径乱序。任何 mutation 若未生效或被接受，self-test 自身会失败。

4. 原 validator self-test 的真实 RED 仍是整套 pristine Stage12 evidence 先失败，无法进入字体 mutation。新增最小 seam 后，instance=3 与 total=16777216 两个 mutation 的 validator 子进程均真实返回非零，self-test 总体转为 GREEN。

### 本轮 GREEN 与验证

- Stage16 target：exit 0。
- `arpg_platform_tests` 与 `arpg_stage12_material_formal` target：exit 0。
- foundation：15 cases / 0 failures。
- 新增 CTest：`platform.shared_ui_font_lifecycle_guard`、其 self-test、`stage12.bundled_font_validator_self_test` 共 3/3 pass。
- 最小 validator self-test：pass；真实 Stage12 report 的 `-BundledFontOnly`：accepted。
- Python 契约：11 tests / OK。
- `git diff --check`：pass（只有既有 LF/CRLF 转换提示）。
- 既有 `platform.host_validation_source` 仍在 Task8A exact-surface 基线先失败；既有 `stage11d.renderer_integration_guard` 仍在 `monster_affix_danger` 基线失败。两者均未改 expected 掩盖。
- 全 platform 仍为 655 cases / 2 failures：ecology `alpha_bounds.x` 与记录不一致、case count actual 655 / expected 654；与本 Task 修改前证据相同。

### Windows parallel-2 清洁构建与内存

组合 clean-first 构建 `arpg_platform_tests`、`arpg_stage12_material_formal`、`arpg_stage16_loot_reinforcement_game_validation`：263 steps，exit 0。100ms 采样：

- 聚合工作集峰值 2,805,669,888 bytes（约 2.61 GiB）；
- 单进程工作集峰值 2,125,996,032 bytes（约 1.98 GiB）；
- 无 OOM。

### Stage12 正式运行复验

`stage12.material_formal` 运行 475.85s，整体仍因既有 `hud_viewport_safe=fail`、`input_hole_regression=fail`、HUD runtime draws 与 Intel UHD 性能（average 28.400 FPS、1% low 26.655 FPS）失败。字体与本轮幂等检查通过：

- `overlay_resources_initialized=pass`（包含第二次 initialize 后 owner/borrower texture id、glyph 指针与 metrics 不变）；
- `bundled_font_runtime=pass`；
- instance 1、glyph 517、capacity 640；
- 2048x2048 GRAY_ALPHA，atlas/total 均 8,388,608 bytes；
- 日志含 41 个独立 raylib 初始化段和 41 条 shared load，每个初始化段恰好一次；第二次幂等 initialize 未新增加载。

### Fix round 自审

- raylib 6 本地源码已核对：`IsFontValid` 明确不检查 GPU texture；`IsTextureValid` 检查 id/width/height/format/mipmaps；`UnloadFont` 对 texture id 0/空候选经 `UnloadTexture` 安全 no-op，对 default font 跳过释放。因此 rejected candidate 可统一进入一个回收点，不猜测 API 语义。
- 没有修改玩法、存档、设置编码、stable IDs、枚举或版本；没有 JSON、外部语言包、运行时映射容器或逐帧分配。
- 未删除文件、未 reset、未覆盖无关改动。

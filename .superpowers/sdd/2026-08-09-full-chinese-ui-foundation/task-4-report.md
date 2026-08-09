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

- 运行 474.70s；raylib 日志只有一次 `Shared UI font loaded`。
- 字体字段全部通过：`bundled_font_runtime=pass`、instance 1、glyph 517、atlas/total 均 8388608、capacity 640。
- 整体测试仍 fail，原因不是字体生命周期：`hud_viewport_safe=fail`、HUD bounds/runtime draw fail，以及 Intel UHD 上 stress average 28.405 FPS、1% low 26.505 FPS，低于 60/45 门槛。
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

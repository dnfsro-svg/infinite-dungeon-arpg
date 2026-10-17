# Stage 12 Task 2 报告：MaterialPack 生命周期、发布复制与程序回退

## 完成内容

- 新增 `MaterialPackState`：按图集类别记录可用性，支持未知 ID 忽略、类别回退判断、重复 `reset()` 和 `any_available()`。
- 新增 `MaterialPack`：仅在 `load()` 中依次读取 `environment.png`、`actors.png`、`effects_ui.png`；按唯一的编译期默认清单验证实际尺寸；失败类别仅禁用自身并最多输出一次警告；`draw()` 找不到可用类别、帧或纹理时返回 `false`，供后续房间/角色调用点继续现有程序绘制。
- `unload()` 只对 raylib 6.0 `IsTextureValid()` 的纹理调用 `UnloadTexture()`，随后清空状态，因此重复关闭安全。任务文字中的 `IsTextureReady()` 不存在于本仓库使用的 raylib 6.0，已使用其等价有效性 API `IsTextureValid()`。
- `CombatRenderer::initialize_resources()` 调用 `material_pack_.load()`，`shutdown_resources()` 调用 `material_pack_.unload()`；材质缺失不会影响既有 HUD 字体就绪结果或玩法。
- 默认清单修正：Task 1 的已提交代码只有清单类型、没有计划声称的默认清单。经主控确认后，在 `material_manifest.hpp` 补齐唯一 `default_material_manifest()`：环境 1024x1024、角色 2048x2048、效果/UI 1024x1024，各 RGBA，合计 24 MiB，当前帧表为空，后续 12-B/12-C/12-D 扩充。
- 发布复制命令置于 `src/app/CMakeLists.txt` 的 `arpg_game` 定义目录。CMake 禁止根目录为子目录创建的目标添加 `POST_BUILD` 命令；功能仍为复制 `assets/stage12` 至 `bin/assets/stage12`。
- 新增 `assets/stage12/README.md`，明确当前 Task 2 尚未提供三张 PNG，启动时会按类别安全程序回退；后续图集加入后会被同一发布复制命令带出。

## TDD 记录

先在 `material_asset_validation_tests.cpp` 增加以下两个测试，并把平台测试计数由 286 更新为 288：

- `material_pack_falls_back_when_atlas_is_unavailable`
- `material_pack_allows_repeated_shutdown`

首次 RED 命令为：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
```

该次在 CMake 重新生成阶段被环境阻断，未进入新测试编译：`ninja: error: WriteFile(.ninja_lock): Unable to create file. Permission denied`，同时 CMake 报 `Stage 0 requires Windows SDK 10.0.26100.0; detected ''`。因此未能观察到计划期望的 `MaterialPackState` 未定义编译错误；之后在本机 VS x64 开发环境中完成 GREEN 构建。第一次真实编译还发现 `IsTextureReady` 不存在，改用 raylib 6.0 的 `IsTextureValid` 后通过。

## 验证

在加载 `VsDevCmd.bat -arch=x64 -host_arch=x64` 并设置 `WindowsSDKVersion=10.0.26100.0` 后：

```text
cmake --build --preset windows-msvc-debug --target arpg_platform_tests  PASS
cmake --build --preset windows-msvc-debug                              PASS
ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure  PASS (1/1, 3.50s)
out/build/windows-msvc-debug/bin/arpg_platform_tests.exe               288 cases, 0 failures
```

发布布局检查：`bin/assets/stage12/README.md` 存在；`environment.png`、`actors.png`、`effects_ui.png` 当前均不存在，符合本 Task 2 的资源尚未交付约束和 README 声明。

`git diff --check` 通过。

## 提交状态与工作树边界

未修改、未暂存 `docs/validation/evidence/stage11d` 的六项外部脏改动。

尝试刷新 Git 索引以准备提交时被权限阻断，精确错误如下：

```text
error: insufficient permission for adding an object to repository database E:/game/.git/objects
error: CMakeLists.txt: failed to insert into database
fatal: Unable to process path CMakeLists.txt
```

代理因权限未自行提交，也没有设置任何全局 Git 安全目录配置；控制器后续创建了提交 `cb6b0d7`（`feat: add stage 12 material pack lifecycle`）。

## 审查修复：真实 MaterialPack 生命周期测试

审查指出此前只有 `MaterialPackState` 基础测试，未直接触发真实 `MaterialPack::load()` / `unload()`。已新增最小可注入纹理 seam `MaterialTextureApi`，只包含 `load`、`valid`、`unload` 三个函数指针；默认构造仍绑定 raylib 6.0 的 `LoadTexture`、`IsTextureValid`、`UnloadTexture`，`draw()` 仍直接调用 raylib 绘制 API，没有新增运行时依赖。

新增真实生命周期测试：

- 三张 PNG 都缺失：`load()` 返回 `false`、三个类别都不可用、未调用 fake `unload()`。
- actors 图集有效但尺寸错误：它只被 fake `unload()` 调用一次，actors 类别保持回退；之后再次 `unload()` 不会重复释放。
- 只有环境图集尺寸正确：`load()` 返回 `true`；连续两次 `unload()` 仅释放该纹理一次，且三个类别最终都不可用。

审查修复 RED 命令：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
```

RED 结果：测试编译按预期失败，`MaterialTextureApi` 尚未定义（`C2039: "MaterialTextureApi": 不是 "arpg::platform" 的成员）。

审查修复 GREEN 命令与结果：

```text
cmake --build --preset windows-msvc-debug --target arpg_platform_tests  PASS
ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure  PASS (1/1, 3.56s)
out/build/windows-msvc-debug/bin/arpg_platform_tests.exe               291 cases, 0 failures
```

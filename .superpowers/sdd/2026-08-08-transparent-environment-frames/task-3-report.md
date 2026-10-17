# CPLAY-019 Task 3 报告：Windows Release 内部 F12 视觉验收

## 状态

**PASS / 修复资源同步后已通过，并记录 CPLAY-019 闭环。**

首轮截图实际使用了构建目录中未刷新的旧 PNG，因此只能作为资源同步缺陷的 RED。修复增量资源同步、重新构建并部署后，安装版门/洞口已满足透明 surroundings、无 square floor/wall plate 的硬性验收条件；本地忽略的 issue log 已追加闭环记录。

## 构建与部署（各执行一次）

```powershell
.\scripts\Build.ps1 -Preset windows-msvc-release
```

退出：`0`；结果：配置完成，`ninja: no work to do.`。

```powershell
.\scripts\DeployLauncher.ps1 -GameBuildDirectory E:\game\out\build\windows-msvc-release\bin
```

退出：`0`；结果：安装到 `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\app`。

## 首轮旧资源 SHA-256 发布读回

| 文件 | Build SHA-256 | Install SHA-256 | 一致 |
| --- | --- | --- | --- |
| `arpg_game.exe` | `c0c5ce468a2cbae2f1379829ff1207f249e04eef4e062d5374520e6cbc45951a` | `c0c5ce468a2cbae2f1379829ff1207f249e04eef4e062d5374520e6cbc45951a` | 是 |
| `assets/stage12/element_doors.png` | `82cff26b7aa710677bebd15df3c718f83a9b2cdb7afb4b05e4b31b3d44cfa393` | `82cff26b7aa710677bebd15df3c718f83a9b2cdb7afb4b05e4b31b3d44cfa393` | 是 |
| `assets/stage12/element_doors_material.png` | `22302afee11b134674d4452f27b1946ed260ba6d171f59fd4402390adc32e0a9` | `22302afee11b134674d4452f27b1946ed260ba6d171f59fd4402390adc32e0a9` | 是 |
| `assets/stage12/lightning_environment.png` | `8810e230a5b20e874de0dff6e7a09e4cf6be73ab26479702e2fa65178ebef20b` | `8810e230a5b20e874de0dff6e7a09e4cf6be73ab26479702e2fa65178ebef20b` | 是 |
| `assets/stage12/lightning_environment_material.png` | `eb12f7614bfe7dc56d328626acfd9558a107aba969145754c6a1f990f8880c77` | `eb12f7614bfe7dc56d328626acfd9558a107aba969145754c6a1f990f8880c77` | 是 |
| `assets/stage12/chaos_environment.png` | `ea91696e85870b41b3a826b68168be6dc82fac008c4df1c1f5213f79e102fbed` | `ea91696e85870b41b3a826b68168be6dc82fac008c4df1c1f5213f79e102fbed` | 是 |
| `assets/stage12/chaos_environment_material.png` | `54d73ab8b5bc4fe5e357da76622214f7b80cc77fcd4ade53b482473cbc7dad54` | `54d73ab8b5bc4fe5e357da76622214f7b80cc77fcd4ade53b482473cbc7dad54` | 是 |

## 首轮内部 F12 验收（旧安装资源，RED）

所有游戏均使用静音设置：`E:\game\out\play-cycle5-20260808-171507\settings`；画面证据仅为游戏运行时由 F12 生成的 PNG，未用 `CopyFromScreen` 作画面结论。输入由 `E:\game\out\keyboard-driver-tdd\Release\arpg_keyboard_driver.exe --pid <pid> --key F12 --hold-ms 120` 发送。

| 图片 | 游戏/驱动退出 | 判断 |
| --- | --- | --- |
| `E:\game\out\cplay019-transparent-environment-20260808-193555\screenshots\stage8-equipment-loot.png`（SHA-256 `f542b2912a9a1bd3f0552f2e7caa5279d6ee136fae2dd87d2cfc3439ca7f61dc`） | 复制 `play-cycle5` 存档；F12 driver `0`，`CloseMainWindow=True`，game `0` | 水生态基线；不是本任务受影响的闪电/混沌结论。 |
| `E:\game\out\cplay019-transparent-environment-20260808-193555\screenshots\lightning\stage8-equipment-loot.png`（SHA-256 `b187a3494fb0994ab7f44761472477cf27ad5feb162658651a07b16652a651ad`） | `--seed 34`（离线按仓库确定性算法得出：电生态、hole sample 305）；F12 driver `0`，`CloseMainWindow=True`，game `0` | **FAIL**：HUD 显示“生态：电”，洞口主体可见；中部及右侧洞口下方/后方存在矩形地板或墙面板。未满足透明 surroundings、无方形底板；不能判定为修复成功。 |
| `E:\game\out\cplay019-transparent-environment-20260808-193555\screenshots\chaos\stage8-equipment-loot.png`（SHA-256 `c653d1abf0dbc2e3faf53af48c4642ed6608eab27dbf6873ed17a4c856903109`） | `--seed 31`（离线按仓库确定性算法得出：混沌生态、hole sample 901）；F12 driver `0`，`CloseMainWindow=True`，game `0` | **FAIL**：HUD 显示“生态：混沌”，洞口/门相关主体可见；右上、右下两处主体仍含明显矩形底板。未满足透明 surroundings、无方形底板。 |

闪电与混沌目标运行均为独立 `save-dir`；基线运行使用从 `play-cycle5` 复制出的 `run_a.sav`/`run_b.sav`。每次截图后均立即使用窗口关闭消息正常退出，未同时运行多个游戏。

## 进程清理

初始检查无 `arpg_game/cl/cmake/ctest/ninja`。最终复查也为 `NO_RELEVANT_PROCESSES`。

首次基线启动误将 raylib stdout/stderr 重定向到未并行消费的管道，导致游戏 PID 28900 无响应，未生成截图；已先两次 `CloseMainWindow=True`，等待 5 秒后按恢复流程执行 `Stop-Process -Id 28900 -Force`，并结束两个挂起的外置驱动。后续三次验收取消日志重定向，均响应正常、F12 成功、`CloseMainWindow=True` 且游戏 exit `0`。该异常轮不作为验收画面证据。

## 首轮 Concern / 资源同步 RED 夹具

发布文件与安装文件彼此相同，但它们与工作区已重建的 PNG 哈希不同；这条 RED 最终定位到 Release 构建目录没有刷新资源，而不是 Task 2 生成资产或运行时 selector 失效。首轮未修改 C++、Python、PNG、JSON、玩法或 issue log。

## 修复轮：Release 资源同步与最终 PASS

- 根因：`arpg_game POST_BUILD` 只在链接时复制资源，纯 PNG 更新不会触发；首轮 Build 为 `ninja: no work to do`，所以构建/安装均为旧 atlas。
- 修复提交：隔离分支 `4e6aaa23`，main `7127830`。定向 `app.runtime_asset_sync` 为 `1/1 passed`，覆盖无变化、修改、新增和删除。
- 最终 Build：`scripts/Build.ps1 -Preset windows-msvc-release` exit `0`，输出包含 `Synchronizing runtime assets for arpg_game`；Deploy exit `0`。
- 8 个门/生态 PNG 的源码、Release Build、Install SHA-256 全部一致；`arpg_game.exe` Build/Install SHA-256 均为 `10DB1A1544C518D1734406A3C1D4426E4BE4A634FB42A72A06BD91F9C1B54138`。
- 闪电证据：`E:\game\out\cplay019-transparent-environment-fixed-20260808-202813\lightning\screenshots\01-lightning-hole-and-door.png`；门为干净拱门，多个洞口为独立椭圆轮廓。
- 混沌证据：`E:\game\out\cplay019-transparent-environment-fixed-20260808-202813\chaos\screenshots\01-chaos-hole.png` 与 `02-chaos-door.png`；洞口和门均无方形墙地底板、裁切或 atlas bleed。
- 仍可见的矩形已对象级归属为 `capacitor_bank/grounding_rod` 重叠和 `chaos_anomaly_condenser` 完整帧，不是门、洞口或 wall；不混入本任务。
- 两个安装版实例均使用全 0 静音设置与隔离存档；正常关闭后相关游戏/构建进程为 0。

## 2026-08-08 资源同步根因 RED 补充

后续逐文件比对确认：Task 3 上述 Build/Install 一致只证明“旧 Build 资源被正确部署”，不证明新源资源进入 Build。分支源资源与 `E:\game\out\build\windows-msvc-release\bin` 的 8 个目标文件 SHA-256 全部不同：

| 文件 | 分支源 SHA-256 | 旧 Build SHA-256 |
| --- | --- | --- |
| `element_doors.png` | `6e6fcbdfb30bf169edfb44ca907a9914563bf2ea11f01f79154d2cfce0371cd4` | `82cff26b7aa710677bebd15df3c718f83a9b2cdb7afb4b05e4b31b3d44cfa393` |
| `element_doors_material.png` | `807e8de8241f9297574766dd28554c93ebb97bbc5d1b3b51ad269407a5ca4217` | `22302afee11b134674d4452f27b1946ed260ba6d171f59fd4402390adc32e0a9` |
| `water_environment.png` | `a08bf7a0aacde2e0d6ae9dcfde36fee070c5f775d845b2a9da0182e6998d9581` | `2b57fa6da16a6a43c7846ff5e1b67d756bc3451307be013ecc7e108ca17ff9ca` |
| `water_environment_material.png` | `71a1b09b0e8351ac70a369955a6218b102bf2ac3a53d7f49a07127f325b6a304` | `06e5f43e82cedab9c1e4759346f9e8713021f07ef8ee57e98bd1f0d8fb77f31a` |
| `lightning_environment.png` | `75d83cddc542b13e97c04703104bc945cc0e8eaa4913aa7f9ac7e79a488a64ec` | `8810e230a5b20e874de0dff6e7a09e4cf6be73ab26479702e2fa65178ebef20b` |
| `lightning_environment_material.png` | `579a4da9f8225e7d0c1e9aeeeb0fff80d5f22ef0bf09e5cf5c0ced626542cd12` | `eb12f7614bfe7dc56d328626acfd9558a107aba969145754c6a1f990f8880c77` |
| `chaos_environment.png` | `b6cad8f31fd967616b9956bcb757746473573638bf3d8a5d7804fb1e490f3f15` | `ea91696e85870b41b3a826b68168be6dc82fac008c4df1c1f5213f79e102fbed` |
| `chaos_environment_material.png` | `310b8067d7562ae3173441de152c99e9923195ebcce3914b52f6d8e7f20a6ae6` | `54d73ab8b5bc4fe5e357da76622214f7b80cc77fcd4ade53b482473cbc7dad54` |

`src/app/CMakeLists.txt` 当时只用 `add_custom_command(TARGET arpg_game POST_BUILD ...)` 拷贝资源；纯 PNG 变化不会让 `arpg_game` 重链接，因而 Ninja 报 `no work to do` 并跳过 POST_BUILD。这是可重现的根因 RED。

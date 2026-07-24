# 四生态原生房间背景正式验证

验证日期：2026-07-23

渲染器：raylib 6.0，生产 `RoomBackgroundRenderPlan` / `MaterialPack` 路径

状态：`native-background-verified`

## 结论

- 火、水、电、混沌均使用独立的 3840×2160 master、2560×1440 runtime 和 2560×1440 material 图。
- 1280×720 使用 `1/2` 缩放，1920×1080 使用 `3/4` 缩放；两档均不放大源图。
- 正式矩阵包含 4 个生态 × 2 种视图 × 2 个分辨率，共 16 张真实 raylib 截图。
- `background-only` 只调用生产背景绘制函数；不会绘制深渊、危险区、掉落、房间物件、门、洞、角色、HUD、被动树、背包、暂停页或材质画廊。
- `gameplay` 使用正常生产渲染链。HUD 与背景共用同一份展示生态快照，运行时遥测要求火、水、电、混沌逐图一致。验证器同时要求背景与玩法图有足量像素差异，并拒绝缺图、错误尺寸、错误缩放、伪造 SHA、重复生态图和背景图冒充玩法图。
- 生产 manifest：`full_pack_bytes=302170112`，`resident_peak_bytes=163708928`，低于 256 MiB 硬上限。

## 资产绑定

| 生态 | master SHA-256 | runtime SHA-256 | material SHA-256 |
|---|---|---|---|
| 火 | `cc452a958c597165319d0e4472fcd1eea57469c2a259e7ad510ffde63096e4be` | `92fdd54b6add6e478a6169d38d37cbc1909d97eb988784747e64d0e906f4e934` | `562cf35a0f219166379bb76878bea693d73536883d124fccd9b978afdf943d8e` |
| 水 | `c88d21b8f07ae2bf6228310c2650460323c0e64b71c2eb3e311d2516cdda8291` | `9bc6fe17d10197b3cbebfe89f8f089f9a34caee48785a14ef4e36bb8db31ad3c` | `504dac11af3110668566eaaee8d397c86ce76743572513ef94a8577d4a563c8c` |
| 电 | `770176291443c1d7b39391aeadb94df938a37027af5241aa9a18341f3db05192` | `bc1554dcb340c80fc4a2fa956d14c9347f72b28355503964c010f90f7b663995` | `c327d9f53d55075c405fcfd10f872fc3618ab57319d27a84515b11c49de800c6` |
| 混沌 | `6bdd13be736900558b923f6d40e07e8ba91a2972e0ec7c2ef7d9869c57fd5838` | `004f5aa835d40dc648c34b21fb9359c9a36bbe5ff873543e12cd27b198f870e8` | `4d2851b1ac604f49425e7c3cc64ed76355a2194df8c4960e4fdc9f08140d3b02` |

这些哈希同时由三处绑定：正式报告、工作树中的实际文件、`assets/stage12/room-background-build.json` 的 `output_sha256`。验证器要求三者完全一致，并要求三类资产各自在四生态间均不重复。

## 16 张截图 SHA-256

| 生态 | 视图 | 1280×720 | 1920×1080 |
|---|---|---|---|
| 火 | background-only | `fc3f8f7ef66919c36d1d7a8036816606fb14f68c2f0b5ee5838ce43b7a34a5b5` | `8b5496ad926d1c3eec04bf578c6806963b592b6e62dbb32bd963a698e84a90eb` |
| 火 | gameplay | `39b53331ea13d90cf9f3d9312119f54cdf6e3989cbbfec2d466965beb7ad499a` | `d4043b21682a8a0d532d122384aee44fd00e91c783fd30823ab1c66e05352907` |
| 水 | background-only | `2aeb0da93fa10db071824d2831241aac81f26aedb3c160597453f3b83d59702c` | `33935bd5fff4f650fe9922727b6c9d97838c0838be63dd88f351e2dba455f9ec` |
| 水 | gameplay | `6035b4a8151bf10b3b61a17cc2101d5c6471bd9fe9f69639c5d0c002b74aaa06` | `39647e23f21a69a4a824d8b7cb2df1b8df749972202be1bb7f35854cb0b05af4` |
| 电 | background-only | `51c74a6f3a0ec3e269dfd41ab78686c39c7d9c0fc159f54df7dab1084ef63f0c` | `df25494b22da8208fa8211dae039a9a6e8c2d02ded40512d348b689a270b66b3` |
| 电 | gameplay | `5cb10393b3397b4ca4bafe38cad38ea7d26218de50cb69084e630940921c4785` | `a6e13cf92d6bb368e87820455b21b40851ca9ec0caddfc1c059791d139e7ef47` |
| 混沌 | background-only | `26ae61082a4f8af7394114dcf9f43795767cbce60f8bdad281ae3790d3f86e23` | `c8ca09a6a9963cdab27850d4dd1a50b554d6ae34044f53d8f7cbe6a0cc4f9230` |
| 混沌 | gameplay | `7d9f3ae4e367d5ee88496ad22b63c7bdb4b508e62d740f55be73758e6cdff30a` | `c02c3507e38e6b29c411620e71224c761c09cf0ad520ee76cf7da8676463c96a` |

包内截图位于 `previews/`。本地原始证据位于构建目录的 `stage12 material evidence/stage12-run/`。

## 房间语义记录

- 四门：正常 1920×1080 生产帧 `game-1920x1080.png` 展示上、下、左、右四个门位；SHA-256 为 `a6833a6a827d2a35c2d2197fdd921249a5b76e21349ab57913e3fac2b16d0e67`。
- 洞口：16 张背景矩阵使用战斗/展示房间，不伪称其中可见洞口。洞口与输入回归由 `input-hole-summary.txt` 独立证明，记录 `depth=2`、`last_transition=1`、`resolution_valid=1`；SHA-256 为 `ba07ce942c901e85ca3299db841b1da33b5574155721873f2878661922f4ee70`。
- 出生区与路径：背景中心和近景保持连续可行走地面；玩法图中的玩家、怪物和掉落均落在该连续区域内。
- 轮廓：四生态 background-only 图在两档分辨率下各自 SHA 唯一；玩法图相对对应纯背景图还必须通过采样像素差门槛。

## 运行与边界

Debug 正式验证器会嵌套完整生产渲染链。其专用 MSVC 测试 EXE 栈保留为 2 MiB，避免 Debug 大栈帧与 UCRT 叠加造成误崩；该设置只作用于 `arpg_stage12_material_formal`。PE 头回读确认 `arpg_game.exe` 仍为默认 1 MiB，游戏 runtime 未改变。

旧交接包未被覆盖：

- v1：`B61C54BE14E99FC083D4EA5D665EC24D899E565C835E077926CE46CD86D0F2A4`
- v2：`3C1C47DE0E20FC3C4E97806CD2719FF44357563ABF3D8C5D9B4BEBFE301E6D1C`

新包名称：`arpg-material-pack-v3-native-backgrounds.zip`。

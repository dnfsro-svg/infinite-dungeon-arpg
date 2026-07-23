# Task 2：水、雷、混沌原生房间背景报告

日期：2026-07-23
范围：只新增/重建水、雷、混沌的离线背景资产与来源管线；未接入运行时渲染，未改动玩法、输入、门洞或数值。

## RED 到 GREEN

- RED：任务开始时仅有火生态的原生背景管线；水、雷、混沌没有清单来源和三件套输出，四生态构建/来源隔离合同失败。
- GREEN：`python -m unittest tests.platform.native_room_background_asset_pipeline_tests -v` 通过，4 项测试均为 `OK`。
- 该测试覆盖四生态（含既有 fire）：精确 master/runtime/material 尺寸、单次 LANCZOS 下采样、清单来源隔离、clean-index `git archive` 后的可重建性、每个清单来源均被 placement 实际消费、零放大矩形交叉校验、ROI 连通、来源/输出 SHA、生态输出唯一性、边带非空白、硬横/竖面板边、连续区重复裁切与生态色比例。

## 最终构建设计

- 母版：3840×2160 RGBA；运行时颜色图与材质图：2560×1440 RGBA。
- 水、雷、混沌中央连续战斗 ROI 固定为 `(864,486)-(2976,1674)`；主场景、右延展、近景四个连续 placement 在上层覆盖该 ROI。
- 外围只使用 4 个 periphery 大裁切和 4 个 wall/floor 大裁切，均为原生 1:1；外围先进入独立层，`GaussianBlur(6)`、对比度 `0.78`、alpha `100` 后合成。平滑暗角遮罩在中央 55% 为零，故不降低主战斗区清晰度。
- 混沌额外把主/近景左移，右延展羽化设为 75，保证右侧地面连续；最终以 1.07 的轻度色彩饱和度保留克制的紫红/酸绿识别。

## ImageGen 素材记录

所有来源均为 2026-07-23 以内置图像生成器原创生成，未使用或放大旧 runtime 裁片。通用负约束：`no text, UI, characters, doors, holes, props, obstacles, logo, watermark`；画面为可用作 2.5D ARPG 房间背景的原生环境材质。

| 生态 / 角色 | 最终生成提示词摘要 | 原始尺寸 | SHA-256 |
|---|---|---:|---|
| water / wall | `original wet charcoal stone dungeon wall, cold cyan moisture glow, sparse antique gold trim, 2.5D ARPG background, no gameplay object` | 1254×1254 | `fde2c89da44e12481b510a3b1d121142df15b7f8d47536c1e15369a226fb3bbd` |
| water / floor | `original wet charcoal flagstone floor, shallow cyan reflections, sparse antique gold inlay, open 2.5D combat floor` | 1254×1254 | `ac92ec5cb181442015ccbc21e270aa8924c3dd42ee85073af4b2fb2996b47748` |
| water / periphery | `wide wet charcoal and cyan flooded-stone periphery, distant damp architecture, antique-gold accents, open center` | 1536×1024 | `4dfd6d888621ec1da3f4d13ecaf170b029ba12cfca6d65f5adb9c5f5668f500b` |
| water / open room | `wide original open wet-stone dungeon combat room, charcoal, restrained cyan water reflections, small antique-gold details, unobstructed floor` | 1536×1024 | `5183e59dbf4c277ccd4be82fcf972400f1948cc554167b81ac7e363b0602361b` |
| water / near | `wide foreground continuation of open wet charcoal flagstone floor, cold cyan reflections, no obstacle` | 1774×887 | `f33a7756394533782f0918fc18dd1eaad0ac404787c4add27b010a75ae222c75` |
| water / right | `edit corresponding open-room-v5 into a matching tall wet-charcoal right continuation, cyan moisture light, open traversable floor` | 1024×1536 | `256d783508a5105bc6763a24df59a3c56479b49dc3e34f0ba7187fdbc3f7b1e9` |
| lightning / wall | `original dark steel dungeon wall, restrained copper trim and blue-white electric fissures, 2.5D background` | 1254×1254 | `f07e038258e1d7bb664944b784bc5cb55b07f060335736af171a02d77badee4d` |
| lightning / floor | `original dark steel flagstone combat floor, copper lines and restrained blue-white electric cracks, open` | 1254×1254 | `681dee4b870a992d03bfaf7204e950a04e5428a4575531c2c513418d5fc2cce6` |
| lightning / open room | `wide open dark steel and copper dungeon combat room, blue-white electrical fissures only in distant context, unobstructed floor` | 1744×902 | `c846e27ca751b1b955972fdaed20d1175d6de0be77b05ed84fcd4ffda50f2735` |
| lightning / near | `wide foreground continuation of dark steel open combat floor, restrained copper seams and blue-white crack light` | 1746×901 | `19082f0852409ccaa8b64cd5b4c4020b36205766bf0e06c87e393a4add069091` |
| lightning / periphery | `wide dark steel electrical dungeon periphery, copper architecture and distant blue-white fissure light, no central obstruction` | 1693×929 | `53a96892cfaa3bca73e009e705ec911ddf136bb5cf3af154fbe939150ac970a4` |
| lightning / right | `edit corresponding open-room-v5 into a matching tall dark-steel right continuation, blue-white far-wall fissures, no obstacle` | 1024×1536 | `342342a336ba132aa48e9e06e4a89212b6a06d9a2484ec204e5d129a6b9755b3` |
| chaos / wall | `original obsidian dungeon wall, restrained magenta fissures and small acid-green mineral glints, 2.5D background` | 1254×1254 | `08a326670d6cec8cba6c395164ce055ed70277433fee52f7e901ae9dc6b875f3` |
| chaos / floor | `original open obsidian flagstone combat floor, subdued magenta cracks and sparse acid-green mineral light` | 1254×1254 | `4d16b8267bd4e1b0ee09e89a640f8832f7ce2f68f5b0af7063e81e372e96ce77` |
| chaos / open room | `wide unobstructed obsidian chaos dungeon combat room, magenta fissures, restrained acid-green edge glow, no crystal in center` | 1774×887 | `d4c29c429e6f83f19a03f400ec88dcdb402b51897689952589d4ea01bcb0efbd` |
| chaos / near | `wide foreground continuation of open obsidian floor, subtle magenta cracks, sparse acid-green mineral accents, no obstacle` | 1536×1024 | `da08599308df25eda34631af9b81035e963df4f7a0f6c05ea7bf85fe58adabc0` |
| chaos / periphery | `wide obsidian chaos periphery, magenta and restrained acid-green distant crystals at edges only, clear central floor` | 1536×1024 | `06bfc526699aa43f118db40ebddf33fa14fb6a5ae7ebca42c66c49bac2afe7c4` |
| chaos / right | `edit corresponding open-room-v5 into a matching tall obsidian right continuation, magenta fissures, edge-only acid-green light, no obstacle` | 1024×1536 | `b8a7d86481852855fd173240e56cc7f6b24e13c0cccaf659bf40da17195bf1d0` |

## 输出与 placement 证明

| 生态 | placements / 连续 ROI | scale 范围 | master SHA-256 | runtime SHA-256 | material SHA-256 |
|---|---:|---:|---|---|---|
| fire | 51 / 4 | 0.7397–1.0 | `cc452a958c597165319d0e4472fcd1eea57469c2a259e7ad510ffde63096e4be` | `92fdd54b6add6e478a6169d38d37cbc1909d97eb988784747e64d0e906f4e934` | `562cf35a0f219166379bb76878bea693d73536883d124fccd9b978afdf943d8e` |
| water | 12 / 4 | 1.0 | `c88d21b8f07ae2bf6228310c2650460323c0e64b71c2eb3e311d2516cdda8291` | `9bc6fe17d10197b3cbebfe89f8f089f9a34caee48785a14ef4e36bb8db31ad3c` | `504dac11af3110668566eaaee8d397c86ce76743572513ef94a8577d4a563c8c` |
| lightning | 12 / 4 | 1.0 | `770176291443c1d7b39391aeadb94df938a37027af5241aa9a18341f3db05192` | `bc1554dcb340c80fc4a2fa956d14c9347f72b28355503964c010f90f7b663995` | `c327d9f53d55075c405fcfd10f872fc3618ab57319d27a84515b11c49de800c6` |
| chaos | 12 / 4 | 1.0 | `6bdd13be736900558b923f6d40e07e8ba91a2972e0ec7c2ef7d9869c57fd5838` | `004f5aa835d40dc648c34b21fb9359c9a36bbe5ff873543e12cd27b198f870e8` | `4d2851b1ac604f49425e7c3cc64ed76355a2194df8c4960e4fdc9f08140d3b02` |

每个 Task 2 placement 的 `scale_x` 与 `scale_y` 均为 1.0；输出颜色图由对应母版仅一次 LANCZOS 下采样生成。`assets/stage12/room-background-build.json` 保存完整 source SHA、源/目标矩形和连续 ROI 覆盖证明。

## 人工视觉验收

- water：PASS。湿炭黑、冷青反射和古金细节可辨；中央地面连通、无障碍；外围低对比真纹理无规则面板或硬缝。
- lightning：PASS。暗钢/铜/蓝白识别明确；电光留在远景语境，不切断战斗地面；中央 55% 连通清晰。
- chaos：PASS。黑曜、紫红裂隙与克制酸绿可辨；右侧连续地面已补足；晶体只在远景边缘，中央无障碍。

## 哈希稳定性

在 GREEN 后对四生态依次重建两轮，并比较每个 master/runtime/material 的 SHA-256；两轮结果完全一致。随后运行 clean-index archive rebuild 测试，归档工作树同样可完整重建四生态。

最终验证记录：

- 工作树完整模块：4/4 PASS，30.693 秒。
- `git write-tree` + `git archive` 解压后的真实无 `.git` 目录：完整模块 4 项中 3 项 PASS，1 项 Git-index 专属检查按设计明确 SKIP；可移植构建合同全部执行并通过，14.025 秒。
- 无 Git 归档目录连续两轮重建四生态的 12 个 master/runtime/material 输出：`STABILITY_MISMATCHES=0`。
- 暂存树 `git diff --cached --check`：PASS。

## 复审修复

- 归档可移植性：`git archive ddccbd6` 的无 `.git` 目录先稳定复现 RED：旧索引测试报 `fatal: not a git repository`。测试已拆分为可选的 Git 索引来源检查与必跑的临时无 Git 目录构建 helper；该 helper 只复制构建器、正式 source manifest、manifest 所声明的正式 PNG 和必要的输出目录，不复制整个 ROOT、无关未跟踪图或 deliverables。所有 build/output 断言仅在该最小临时闭包目录运行，故不会覆写共享工作树产物。无 Git 归档运行整个模块时，索引检查会以清晰原因 skip，其余完整构建合同继续执行。
- 输出唯一性：master、runtime、material 三类 SHA 均单独要求四生态唯一。
- 候选治理：审查指出的 11 张未消费候选已 `git mv` 到 `art_source/stage12/backgrounds/candidates/{water,lightning,chaos}/`；为防止 v6 替换后遗留，3 张 v5 右延展也一并移入，现共 14 张。`candidates/candidate-manifest.json` 对每张记录 `status`、replacement、ImageGen prompt、日期、尺寸与 SHA-256；README 明确禁止正式清单、构建器、runtime 和 package 消费该目录。
- 右延展 RED：旧 v5 的 main/right 全重叠端点亮度漂移为 water `7.55`、lightning `27.23`，在 `< 5.0` 的连续性门槛下可靠失败。以对应 `open-room-v5` 编辑生成 portrait `open-right-v6` 后，三生态变为 water `1.62`、lightning `0.90`、chaos `3.16`；起点与结束梯度均 `< 3.0`。雷图 overlap 内的 `7.93` 内部峰值被定位为合法蓝白裂隙纹理，结构峰值阈值为 `< 9.0`，不是放宽端点门槛。
- 最终人工复核：water、lightning、chaos 的 v6 右延展均 PASS；完整 16:9 中中央地面连通，无门、洞、柱、能量束、瀑布、障碍、文字或 UI。
- 第二轮复审的产物绑定 RED：在 `git archive 5b1f427` 的无 Git 副本中，用已提交的 lightning runtime 覆盖 water runtime；旧测试仍错误返回 `OK (skipped=1)`，证明临时构建的自校验不能保护正式提交产物。
- 第二轮复审的产物绑定 GREEN：临时最小闭包重建后，逐项比较四生态共 12 个 master/runtime/material SHA-256 与测试根目录中的已提交版本，并比较规范化后的完整 `room-background-build.json`；同一故障注入现在会以 `water rebuilt runtime no longer matches its committed output` 明确失败，共享工作树仍只读。
- 候选双向闭包：候选数量固定为 14，manifest path 必须唯一，且 `candidates/**/*.png` 文件集合必须与 manifest path 集合完全相等；未登记候选或悬空记录都会失败。
- 第二轮修复最终验证：工作树完整模块 4/4 PASS（30.673 秒）；最终暂存树经 `git write-tree` + `git archive` 解压后确认无 `.git`，完整模块 3 PASS、1 个 Git-index 专属检查按设计 SKIP（14.092 秒）；`git diff --cached --check` PASS。

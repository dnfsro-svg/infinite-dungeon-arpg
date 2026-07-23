# Task 2：水、雷、混沌原生房间背景报告

日期：2026-07-23
范围：只新增/重建水、雷、混沌的离线背景资产与来源管线；未接入运行时渲染，未改动玩法、输入、门洞或数值。

## RED 到 GREEN

- RED：任务开始时仅有火生态的原生背景管线；水、雷、混沌没有清单来源和三件套输出，四生态构建/来源隔离合同失败。
- GREEN：`python -m unittest tests.platform.native_room_background_asset_pipeline_tests -v` 通过，3 项测试均为 `OK`。
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
| water / right | `tall right-side continuation of wet charcoal dungeon room, cyan moisture light, open traversable floor` | 1024×1536 | `7f028f348ca65f9d63213eaa91f8253c9a6a2546237f1e4ce8b84bd88ba8437d` |
| lightning / wall | `original dark steel dungeon wall, restrained copper trim and blue-white electric fissures, 2.5D background` | 1254×1254 | `f07e038258e1d7bb664944b784bc5cb55b07f060335736af171a02d77badee4d` |
| lightning / floor | `original dark steel flagstone combat floor, copper lines and restrained blue-white electric cracks, open` | 1254×1254 | `681dee4b870a992d03bfaf7204e950a04e5428a4575531c2c513418d5fc2cce6` |
| lightning / open room | `wide open dark steel and copper dungeon combat room, blue-white electrical fissures only in distant context, unobstructed floor` | 1744×902 | `c846e27ca751b1b955972fdaed20d1175d6de0be77b05ed84fcd4ffda50f2735` |
| lightning / near | `wide foreground continuation of dark steel open combat floor, restrained copper seams and blue-white crack light` | 1746×901 | `19082f0852409ccaa8b64cd5b4c4020b36205766bf0e06c87e393a4add069091` |
| lightning / periphery | `wide dark steel electrical dungeon periphery, copper architecture and distant blue-white fissure light, no central obstruction` | 1693×929 | `53a96892cfaa3bca73e009e705ec911ddf136bb5cf3af154fbe939150ac970a4` |
| lightning / right | `tall right-side continuation of open steel dungeon floor, blue-white lightning far-wall glow, no obstacle` | 1024×1536 | `abfc34591470133fc1589b5c2efc9c6ef45556737e6af3ab25d6ceff77366806` |
| chaos / wall | `original obsidian dungeon wall, restrained magenta fissures and small acid-green mineral glints, 2.5D background` | 1254×1254 | `08a326670d6cec8cba6c395164ce055ed70277433fee52f7e901ae9dc6b875f3` |
| chaos / floor | `original open obsidian flagstone combat floor, subdued magenta cracks and sparse acid-green mineral light` | 1254×1254 | `4d16b8267bd4e1b0ee09e89a640f8832f7ce2f68f5b0af7063e81e372e96ce77` |
| chaos / open room | `wide unobstructed obsidian chaos dungeon combat room, magenta fissures, restrained acid-green edge glow, no crystal in center` | 1774×887 | `d4c29c429e6f83f19a03f400ec88dcdb402b51897689952589d4ea01bcb0efbd` |
| chaos / near | `wide foreground continuation of open obsidian floor, subtle magenta cracks, sparse acid-green mineral accents, no obstacle` | 1536×1024 | `da08599308df25eda34631af9b81035e963df4f7a0f6c05ea7bf85fe58adabc0` |
| chaos / periphery | `wide obsidian chaos periphery, magenta and restrained acid-green distant crystals at edges only, clear central floor` | 1536×1024 | `06bfc526699aa43f118db40ebddf33fa14fb6a5ae7ebca42c66c49bac2afe7c4` |
| chaos / right | `tall right-side continuation of open obsidian chaos room, magenta fissured floor, edge-only acid-green light, no obstacle` | 1024×1536 | `67ae8fcc0e3eb274240e2e435d7b26279a2ebcba6b8ba3403a637024413f863f` |

## 输出与 placement 证明

| 生态 | placements / 连续 ROI | scale 范围 | master SHA-256 | runtime SHA-256 | material SHA-256 |
|---|---:|---:|---|---|---|
| fire | 51 / 4 | 0.7397–1.0 | `cc452a958c597165319d0e4472fcd1eea57469c2a259e7ad510ffde63096e4be` | `92fdd54b6add6e478a6169d38d37cbc1909d97eb988784747e64d0e906f4e934` | `562cf35a0f219166379bb76878bea693d73536883d124fccd9b978afdf943d8e` |
| water | 12 / 4 | 1.0 | `c86b6f5fcb0fcd696a076af3d9c43c04513cc0c1d0f61ab59decf4603ab3e282` | `0a7dd7ab93e09593314783694ded5db2a2ee80e966e642eacdf33d6131e30155` | `5809534dfca69af6378f095460c6e60813003805ab8a174e5f04bd1a0f3d9d21` |
| lightning | 12 / 4 | 1.0 | `2ae52870f21e8d3b21665337f7ea9f88c16425392ea520279206a25ee3480558` | `eb02114b3d352c03454b53c1d843b388c02aea8bf6aeeae1d63c8277f364cab7` | `ae29a71f8f5c97af31699f89c3d402a5c9f9497705373ea02ee76d56d8f59f2f` |
| chaos | 12 / 4 | 1.0 | `2957f08659de3d173b5d1e7d2d7b8148b943ebdd65ad167c5b5e385d26bfc578` | `b702a3dfa62cfe8e3b864499f8a230e4e27caa2cb18c5175f6e2fab7609dbed0` | `e3768c285bf72849e64197d9f4b14250c3ad303f09bcec6b10afef7ef3c59adc` |

每个 Task 2 placement 的 `scale_x` 与 `scale_y` 均为 1.0；输出颜色图由对应母版仅一次 LANCZOS 下采样生成。`assets/stage12/room-background-build.json` 保存完整 source SHA、源/目标矩形和连续 ROI 覆盖证明。

## 人工视觉验收

- water：PASS。湿炭黑、冷青反射和古金细节可辨；中央地面连通、无障碍；外围低对比真纹理无规则面板或硬缝。
- lightning：PASS。暗钢/铜/蓝白识别明确；电光留在远景语境，不切断战斗地面；中央 55% 连通清晰。
- chaos：PASS。黑曜、紫红裂隙与克制酸绿可辨；右侧连续地面已补足；晶体只在远景边缘，中央无障碍。

## 哈希稳定性

在 GREEN 后对四生态依次重建两轮，并比较每个 master/runtime/material 的 SHA-256；两轮结果完全一致。随后运行 clean-index archive rebuild 测试，归档工作树同样可完整重建四生态。

# Stage 12 环境图集来源与打包记录

## 来源与许可边界

本图集只使用本任务提供的原创概念源图，原件无损保存在
`art_source/stage12/`：

- `environment-concept-v1.png`（1254 × 1254）：暗色石砌房间；
- `door-concept-v1.png`（1254 × 1254）：石砌门，背景键色 `#ff00ff`；
- `abyss-hole-concept-v1.png`（1536 × 1024）：紫色深渊洞口，背景键色 `#00ff00`。

本次未重新调用图像生成服务。以下为随源图交接的可复现提示词基线（不是对原始生成历史的断言）：

```text
Use case: stylized-concept
Asset type: comic dungeon game environment atlas
Primary request: dark hand-painted stone dungeon chamber, frontal perspective,
empty floor for combat, readable silhouettes, no character, no text, no watermark.
Door variant: a single stone arch doorway on a perfectly flat #ff00ff chroma-key
background, centered with generous padding, no shadow and no text.
Abyss variant: a single oval stone-rimmed abyss hole with violet runes on a perfectly
flat #00ff00 chroma-key background, centered with generous padding, no shadow and no text.
Constraints: original artwork only; preserve floor perspective; avoid UI and logos.
```

## 去键色与打包步骤

1. 用 imagegen 内置辅助脚本分别去除门与洞口的边框键色：

```powershell
python "$env:CODEX_HOME/skills/.system/imagegen/scripts/remove_chroma_key.py" `
  --input art_source/stage12/door-concept-v1.png `
  --out out/stage12-art-temp/door-alpha.png `
  --auto-key border --soft-matte --transparent-threshold 12 `
  --opaque-threshold 220 --despill

python "$env:CODEX_HOME/skills/.system/imagegen/scripts/remove_chroma_key.py" `
  --input art_source/stage12/abyss-hole-concept-v1.png `
  --out out/stage12-art-temp/hole-alpha.png `
  --auto-key border --soft-matte --transparent-threshold 12 `
  --opaque-threshold 220 --despill
```

2. 运行 `python tools/build_stage12_environment_atlas.py`。脚本只读取原件与上述临时 alpha 图，写入 `assets/stage12/environment.png`。
3. 用 Pillow 确认最终文件是 **1024 × 1024 RGBA**；清单的 RGBA 预算为
   `4 × 1024 × 1024 = 4,194,304` 字节。

## 帧表

| 帧族 | 像素矩形 | 锚点 | 用途 |
| --- | --- | --- | --- |
| `environment_floor_{fire,water,lightning,chaos}` | `0,0,1024,704` | `512,704` | 透视房间底图 |
| `environment_door_fire` | `0,704,112,112` | `56,112` | 火元素门框 |
| `environment_door_water` | `112,704,112,112` | `56,112` | 水元素门框 |
| `environment_door_lightning` | `224,704,112,112` | `56,112` | 雷元素门框 |
| `environment_door_chaos` | `336,704,112,112` | `56,112` | 混沌元素门框 |
| `environment_hole` | `448,704,192,160` | `96,80` | 深渊洞口主体 |

下方未使用的透明区域留给后续道具帧；不被运行时读取。

## 公共元素门与独立生态道具

`tools/build_environment_props.py` 首次从火环境图集的既有火门帧以及水、雷、混沌旧图集
`(512,0,768,256)` 门格生成 `element_doors.png`，并在替换任何生态输出前一次性读入四个
来源。后续仅在 `environment-props-build.json` 的 RGBA hash 验证通过时复用公共门图集，避免
重排后的 wall 格被递归当成门。每帧取最大前景连通域、保留透明边距并落脚到 y=244。生态
道具使用对应 `backgrounds/<ecology>/<ecology>-wall-tile-v1.png` 的完整 wall 原图和既有概念
裁剪区域，生成记录写入 `environment-props-build.json`。

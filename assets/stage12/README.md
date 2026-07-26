# Stage 12 材质包资源

运行时材质包固定查找本目录下的基础图集与元素生态独立图集：

- `environment.png`：1024 × 1024，Stage 12-B 房间、元素门框与深渊洞口
- `actors.png`：2048 × 2048
- `effects_ui.png`：1024 × 1024
- `element_doors.png`：1024 × 256，火、水、雷、混沌四个公共门帧（各 256 × 256）
- `water_environment.png`、`water_bulwark.png`、`water_support.png`
- `lightning_environment.png`、`lightning_shooter.png`、`lightning_dasher.png`
- `chaos_environment.png`、`chaos_chaser.png`、`chaos_hazard.png`

每张颜色图集都有同名 `_material.png` 配对图。运行时只常驻当前房间生态的
环境与两种怪物图集，火、水、雷、混沌图集不会同时全部常驻。

环境道具由 `python tools/build_environment_props.py --root .` 可复现生成。
它保留每个生态的 512 × 512 房间区，并把 wall、surface prop、hole、light、solid
prop 隔离到固定的五个 256 × 256 单元；输出坐标、alpha 边界和足部锚点记录在
`environment-props-build.json`。颜色与材质图的 alpha 通道必须逐字节一致。
首次发布在替换生态图集前从旧图集的门格捕获输入；后续发布使用 JSON 哈希验证过的
`element_doors.png` 作为稳定 canonical 输入，避免把已重排的 wall 格误当成门。

图集缺失、尺寸不匹配或任一帧不能绘制时，房间或角色会安全回退为既有几何绘制。发布后置步骤会将整个 `assets/stage12` 目录复制到可执行文件同级的 `bin/assets/stage12`，不需要运行时解析清单或在绘制热路径读取文件。

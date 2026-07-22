# Stage 12 材质包资源

运行时材质包固定查找本目录下的基础图集与元素生态独立图集：

- `environment.png`：1024 × 1024，Stage 12-B 房间、元素门框与深渊洞口
- `actors.png`：2048 × 2048
- `effects_ui.png`：1024 × 1024
- `water_environment.png`、`water_bulwark.png`、`water_support.png`
- `lightning_environment.png`、`lightning_shooter.png`、`lightning_dasher.png`

每张颜色图集都有同名 `_material.png` 配对图。运行时只常驻当前房间生态的
环境与两种怪物图集，火、水、雷图集不会同时全部常驻。

图集缺失、尺寸不匹配或任一帧不能绘制时，房间或角色会安全回退为既有几何绘制。发布后置步骤会将整个 `assets/stage12` 目录复制到可执行文件同级的 `bin/assets/stage12`，不需要运行时解析清单或在绘制热路径读取文件。

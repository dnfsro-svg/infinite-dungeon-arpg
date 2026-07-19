# Stage 12 材质包资源

运行时材质包固定查找本目录下的三个内置图集：

- `environment.png`：1024 × 1024，Stage 12-B 房间、元素门框与深渊洞口
- `actors.png`：2048 × 2048
- `effects_ui.png`：1024 × 1024

本任务只交付 `environment.png`。`actors.png` 与 `effects_ui.png` 仍可缺失；游戏会对每个缺失类别只警告一次并安全回退。环境图集缺失、尺寸不匹配或任一帧不能绘制时，房间会整体回退为原有灰盒、矩形门和椭圆洞口。发布后置步骤会将整个 `assets/stage12` 目录复制到可执行文件同级的 `bin/assets/stage12`，不需要运行时解析清单或在绘制热路径读取文件。

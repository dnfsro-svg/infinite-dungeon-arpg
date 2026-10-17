# Release 最小实机画面抽查

- 日期：2026-08-11（America/New_York）
- 源码 HEAD：`04ceb0acf272`
- 已部署目录：`C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\app`
- 隔离运行目录：`C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\runtime`
- 启动参数：独立 `--save-dir`、`--settings-dir`、`--screenshot-dir`

## 抽查范围

按用户“尽量不测试”的要求，只启动一次 Release 游戏并查看三个代表页面：

1. 暂停根页：标题和“继续游戏 / 设置 / 退出游戏”均为中文；三行文字在整行选项背板中居中；选中态与未选中态有清晰对比。
2. 显示设置页：标题和“窗口模式 / 垂直同步 / 拾取过滤 / 返回”均为中文；动态值位于同一居中行内；文字与深色背板对比清晰。
3. 背包装备/材料页：页签、装备栏、背包、物品详情和材料袋均为中文；标题、页签、筛选项与材料条目在各自容器中布置规整。`I / ESC` 属物理按键标识，按键含义以中文呈现。

系统窗口捕获无法读取 OpenGL 内容，只得到白色客户区；因此改用游戏内 F12 正式渲染截图。原文件每次会覆盖，已立即复制为唯一证据：

- `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\runtime\screenshots\review-01-pause-root.png`（159748 bytes）
- `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\runtime\screenshots\review-02-display-settings.png`（193420 bytes）
- `C:\Users\dnfsr\AppData\Local\InfiniteDungeon\full-cn-review-20260811-0754\runtime\screenshots\review-03-inventory.png`（387807 bytes）

## 退出与边界

- 先关闭背包，再使用 `Alt+F4` 走宿主干净退出路径。
- 退出后 `arpg_game` 进程数为 0。
- 隔离存档目录生成 `run_a.sav`、`run_b.sav` 和 `run.lock`；未使用默认用户存档/设置目录。
- 工作树仍 clean。

未执行完整自动化、三分辨率矩阵、正式验证程序、被动树实机页、死亡页、音频或输入手感验收；本报告只支持上述三个页面的最小人工结论。

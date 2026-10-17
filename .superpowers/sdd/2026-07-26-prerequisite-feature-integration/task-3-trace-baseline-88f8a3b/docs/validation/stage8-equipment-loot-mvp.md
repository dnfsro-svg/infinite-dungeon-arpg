# Stage 8 装备与掉落 MVP 验证记录

- 验证日期：2026-07-15
- 分支：`codex/stage8-equipment-loot-mvp`
- 构建基线：`4f888a9` 加本次 Task 10 工作树变更
- raylib：6.0.0
- 窗口验证程序：`build-release/bin/arpg_stage8_validation_game.exe`
- 验证存档：`build-release/stage8-window-validation/run_a.sav`
- 固定验证种子：237；首个稳定怪物序号保证触发 1% 掉落

## 自动化验证

| 构建 | 结果 | 总耗时 | 地下城压力测试 |
| --- | --- | ---: | ---: |
| Debug | 29/29 通过 | 212.72 秒 | 163.83 秒 |
| Release | 29/29 通过 | 185.14 秒 | 138.47 秒 |

`dungeon.units` 共 119 个用例，包含两个同根种子的 1,000 房固定 trace。每房逐怪处理掉落和拾取，按固定顺序换装、三合一，并每 37 房编码、解码和重建一个 session。测试逐步比较房间种子、generation、物品字节、装备 ID、领取位、物品序号、完整角色 build 与地面掉落；故意交换拾取顺序时能报告首个不一致。

压力测试同时暴露并修正了 MSVC Debug 下大状态按值复制造成的额外分配。最终 transition/save 路径改为移动稳定状态，并只保留上一房间描述用于事件；Release 保持零额外 vector proxy 分配，Debug 只容许标准库自身的单个调试代理分配。

## 窗口验收

| 项目 | 结果 | 证据 |
| --- | --- | --- |
| 初始房间与固定验证装备 | 通过 | `evidence/stage8/01-initial-room.png` |
| `I` 打开暂停式三栏背包；暂停期间画面状态不推进 | 通过 | `evidence/stage8/02-inventory-three-columns.png` |
| 单击查看完整词缀、局部/全局标签和整套属性差异 | 通过 | `evidence/stage8/03-item-detail.png` |
| 双击武器后装备槽和实时战斗属性立即改变 | 通过 | `evidence/stage8/04-equipped-live-stats.png` |
| 热换生命/护盾装备只截断当前值，不回复也不补满 | 通过 | `evidence/stage8/05-hot-swap-no-heal.png` |
| 选择三件同部位、同稀有度、未装备鞋子 | 通过 | `evidence/stage8/06-recipe-selected.png` |
| 三件材料原子消失并生成一件新鞋，选择清空 | 通过 | `evidence/stage8/07-recipe-result.png` |
| 固定首掉落在近战击杀位置生成并因处于 1.5 距离内自动拾取，背包数 2 增至 3 | 通过 | `evidence/stage8/08-auto-pickup.png` |
| 退出前背包、装备和合成产物状态 | 通过 | `evidence/stage8/09-before-restart.png` |
| 重启后装备 ID、三件背包物品、合成产物和角色 build 保持 | 通过 | `evidence/stage8/10-restart-persistence.png` |
| 超过 1.5 距离不拾取、未拾取掉落离房销毁 | 通过（确定性单元/事务测试） | `dungeon.units` 的掉落、拾取和离房清理用例 |

近战击杀发生在拾取半径内时，地面物品在同一固定步进入拾取事务，因此窗口证据直接表现为背包数增加；地面池的可见生命周期、1.5 边界和离房销毁由同一构建内的确定性测试逐状态验证。

## 输入与截图验证说明

Windows 桌面自动化向 OpenGL/raylib 窗口发送键盘输入时，raylib 原输入队列在该环境下会漏收消息。主机现在保留 raylib 输入并增加 Windows 异步键状态兼容路径；窗口验收使用只随测试构建生成的 `arpg_stage8_validation_input.exe` 发送键盘消息。鼠标查看、双击换装、右键选择三合一和按钮点击均通过真实窗口执行。

截图由 `LoadImageFromScreen` 加绝对路径 `ExportImage` 生成，避免 raylib `TakeScreenshot` 对启动工作目录的隐式依赖。普通 `arpg_game` 不启用连续验证截图。

## 结论

Stage 8 装备闭环满足设计第 15 节：装备生成、1% 独立掉落、靠近拾取、三栏背包、即时换装、无回复热换、三合一、V4 持久化和 1,000 房确定性均通过。没有开始史诗装备、鉴定、工艺、腐化、怪物词条、物品过滤器或 Stage 9 内容。

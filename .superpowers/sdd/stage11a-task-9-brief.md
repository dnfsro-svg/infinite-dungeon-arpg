# Stage 11-A Task 9 实现简报

## 目标

仅实现计划 Task 9：DungeonRuntime 的固定 tick 保存顺序、`request_death_continue()` 转发、纯死亡输入门禁，以及 raylib host 接入。不要绘制死亡覆盖层或实现 Task 10+。

基线：`4b616af`。

## Runtime 必须满足

- 提供一个可测试、由 host 唯一调用的 fixed-tick 入口（命名可依现有风格），内部顺序固定为：`session.tick(movement)` → `service_pending_save()` → host 再读取 snapshot。不要让 host 继续手写两步从而绕过顺序。
- 死亡在该 tick 产生后，真实 SaveStore receipt 必须在同一 fixed tick 服务；committed 后 snapshot 为 `death_pending`，not_committed 仍为同一可继续 death screen，indeterminate/fault 同步 Runtime 状态。
- Runtime `request_death_continue()` 只做 running/session 门禁并转发 Session 公开 API，不复制事务逻辑。
- 同一 fixed tick 死亡后，Runtime 的拾取/装备/配方等公开玩法请求必须由新状态拒绝，不能在保存后夹入另一 pending。
- V5 load 仍先迁移并提交再构造 Session；合法 V6 pending death（含深渊 failed resolution）初始化不得进入 legacy started 修复、不得增加 generation/重骰 target。

## 纯输入门禁

- 在 `raylib_input.hpp/.cpp` 实现计划中的 `FrameKeyState`、`DeathInputGate` 和 `death_input_gate(death_saving, death_pending, keys)`，函数不得依赖 raylib 全局状态，测试使用纯值。
- `death_saving` 优先：F12/V、F1、Esc 可用；E 禁止；`forward_gameplay=false`。
- `death_pending`：E、F12/V、F1、Esc 可用；WASD/J/K/L/R/I/P、门/洞、重置和所有鼠标 gameplay 禁止；`forward_gameplay=false`。
- 非死亡：`forward_gameplay=true`，既有 inventory/passive gate 继续决定细分转发；截图/debug/Esc 语义不得退化。
- saving/pending 同时为 true 时 saving 语义占优，E 仍禁止。

## Host 接入

- 每帧一次采样相关按键，死亡 gate 必须先于 inventory/passive 开关、鼠标处理、R、攻击、移动和 E descent。
- 死亡状态出现时关闭/禁止 inventory 与 passive overlay；Esc 直接退出，F1 仍切 debug，F12/V 仍只设置既有单一截图路径。
- pending death 的 E 仅调用 `runtime.request_death_continue()`；不得落入 `session.request_descent()`。
- saving/pending 时 fixed-step 继续运行以推进 receipt/transition，但 movement 必须为空且不提交 actions。
- 截图最终仍只由 `present_frame_and_maybe_capture()` 执行一次，不新增第二个 capture 路径。

## TDD、证据和验证

- 先写纯输入矩阵与 Runtime RED，真实构建并记录缺失函数/API；再改生产。
- Runtime 测试使用真实临时 SaveStore 和生产 Combat 致死路径；不得注入 private pending/stable death/death snapshot。Host 行为尽量由纯 gate + Runtime fixed tick 证明，不靠无法执行的代码文本断言。
- 覆盖 committed/not_committed/indeterminate fixed tick、同 tick后玩法拒绝、continue 转发、V5 migration、V6 normal/deep abyss pending death generation/target 不变。
- 保持 Task 6-8、inventory/passive、截图、Stage10 host validation 不退化。
- 至少运行 Platform 全套、focused death/Persistence、Stage10 fixture、Architecture、`git diff --check`、无 `/STACK`。自审后更新 `.superpowers/sdd/stage11a-task-9-report.md` 并提交 `feat: gate input during death flow`。
- 长任务每约一分钟回报“没有卡住”。

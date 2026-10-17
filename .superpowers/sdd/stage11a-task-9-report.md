# Stage 11-A Task 9 实现报告

## 范围

- 基线：`4b616af386d12fbcaae6ae15f74342aaeb52d8f8`。
- 仅实现 Runtime fixed-tick 顺序、死亡继续窄转发、纯死亡输入 gate 和 raylib host 接入；未实现 Task 10 死亡 overlay。
- Runtime 死亡测试使用真实临时 SaveStore、公开 Session API 和生产 Combat 伤害入口；没有注入 private pending、stable death 或 death snapshot。

## RED

先新增纯 gate 4 组矩阵和 Runtime committed / not_committed / indeterminate / continue / V5 / V6 pending death 测试。第一次普通 shell 构建因未加载 Windows SDK 属无效环境失败，未据此修改生产；切换到 VS Dev 环境后得到有效 RED：

```text
error C2039: "FrameKeyState": 不是 "arpg::platform" 的成员
error C2039: "death_input_gate": 不是 "arpg::platform" 的成员
error C2039: "fixed_tick": 不是 "arpg::platform::DungeonRuntime" 的成员
error C2039: "request_death_continue": 不是 "arpg::platform::DungeonRuntime" 的成员
```

生产最小实现后的首轮测试又暴露 not_committed 后 snapshot 丢失死亡回顾：

```text
[FAIL] dungeon_runtime.fixed tick not committed keeps same death retryable:
after.death.has_value()
```

随后才补只读 deterministic retry death projection。

## 实现

- `DungeonRuntime::fixed_tick()` 是 host 唯一 fixed-step 入口，固定执行 `session.tick(movement)` 后 `service_pending_save()`；host 只在返回后读取 snapshot。
- Runtime `request_death_continue()` 只检查 running/session 并转发 Session 公开 API，不复制 next-state 或 receipt 逻辑。
- 纯 `death_input_gate()` 不读 raylib 全局状态：saving 优先且禁 E；pending 允许 E；两者都仅保留 F12/V、F1、Esc 并关闭全部 gameplay 转发；非死亡继续交给既有 inventory/passive gates。
- host 每帧一次采样 E/F12/V/F1/Esc、WASD、J/K/L、R/I/P 和 gameplay mouse；死亡 gate 在 inventory/passive toggle、mouse、R、action、movement 和 descent 前执行。
- death saving/pending 关闭 overlay 但不清 fixed-step accumulator；每个死亡 step 使用空 movement，动作不入队。多-step frame 的 E continue 只在循环外提交一次。
- pending E 只调用 Runtime continue，非死亡 E 才能进入 descent。accepted/faulted continue 后立即刷新 current/previous，零-step frame 也立刻显示 saving/fault，而不会保留旧 can-continue snapshot。
- 截图仍只设置既有 capture path，并由唯一 `present_frame_and_maybe_capture()` 在 `EndDrawing()` 后执行；running 分支实际使用 gate 的 screenshot 输出。
- V5 仍先真实迁移 commit 后构造 Session；V6 普通/深渊 pending death 初始化前后磁盘完整 state 相同，不触发 legacy started 修复。
- `dungeon_snapshot.cpp` 额外补了设计要求的 not_committed“正在记录死亡”可见性：只从 frozen Combat snapshot、stable state 和命名 deterministic generator 纯重建 target/checkpoint；不修改 stable、不保存 cache、不消费可变 RNG。测试逐字段比较失败前、失败后和 retry commit 后 target descriptor 相同。

## 测试覆盖

- saving / pending / both / nondeath 四组纯输入矩阵；all-keys 场景覆盖 movement、attack、reset、inventory、passives 和 mouse gameplay 不泄漏。
- fixed tick 同步 committed、not_committed 和 indeterminate；真实 receipt kind echo、generation、phase、death snapshot 和 Runtime fault 状态。
- 同 tick死亡提交后 pickup/equip/unequip/recipe、action 和 descent 均拒绝。
- continue request 立即进入 saving snapshot，下一 fixed tick真实落盘，随后构造已保存 target room。
- 合法 V5 文件迁移后 generation +1、death canonical none；V6 普通和真实 abyss-start→Combat死亡 checkpoint 重载不增加 generation、不修改完整磁盘 state/target/failed resolution。

## 验证

- Platform Debug：96/96 passed。
- Focused death lifecycle：31/31 passed。
- Dungeon Debug 全套：254/254 passed。
- Persistence Debug 全套：83/83 passed。
- Stage10 real abyss fixture：passed。
- Architecture：18/18 passed，98.16 秒。
- `platform.input_latency_source` 与 `platform.module_boundary`：2/2 passed。
- `arpg_game` Debug 链接成功。
- `git diff --check`：clean。
- `/STACK`：无匹配。

## 自审结论与疑虑

- host 不再直接组合 `session.tick()` 与 `service_pending_save()`；grep 仅发现 `runtime.fixed_tick()`。
- continue 的零-step frame 已即时刷新 snapshot；多-step frame 不重复提交 continue，死亡出现后的后续 step movement 始终为空。
- V6 abyss 测试先提交合法 available，再由 Session 自动生成 abyss_start pending，经真实 SaveStore receipt 进入 started，最后通过生产 Combat 致死；没有手工伪造 started death checkpoint。
- Task 10 overlay 未提前实现。当前死亡 snapshot 已提供 saving/pending 数据，但绘制仍留给下一任务。

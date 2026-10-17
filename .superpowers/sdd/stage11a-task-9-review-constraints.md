# Stage 11-A Task 9 独立复审约束

复审 `4b616af..Task9 HEAD`，读取设计、计划 Task9、brief、report 和 full diff。不要改代码。

重点检查：

1. host 是否只通过 Runtime fixed-tick 入口执行 `session.tick -> save -> snapshot`，没有保留可漂移的重复顺序。
2. Runtime 同 tick死亡 committed/not_committed/indeterminate 状态、receipt kind echo和公开玩法请求拒绝是否真实落盘验证。
3. `request_death_continue` 是否只是窄转发；pending E 不会调用 descent。
4. death saving/pending/both/nondeath 的纯输入矩阵是否完整；WASD/J/K/L/R/I/P/mouse/door/hole 全禁，F12/V/F1/Esc 保留，saving 禁 E。
5. gate 是否先于 inventory/passive/mouse/reset/action/movement/descent；死亡时 overlays 不会暂停 fixed step。
6. fixed step 在死亡期间仍推进且 movement/action 为空；多 step frame 不会重复提交 continue 或 gameplay。
7. 截图是否仍走唯一 `present_frame_and_maybe_capture`，F12/V 不产生双截图。
8. V5 迁移顺序不退化；V6 普通/深渊 pending death load 不增加 generation、不改 target/resolution。
9. 测试不注入 private pending/stable death/death snapshot；Host 关键分支有纯函数/Runtime 可执行证据，不以弱静态字符串检查代替。
10. Task10 UI 未提前；Platform/Stage10/Architecture/Task8 focused 无退化，无 `/STACK`、padding `memcmp`，工作树与报告一致。

结论必须为 Approved 或 Needs fixes，并按 Critical / Important / Minor 给出文件行号。

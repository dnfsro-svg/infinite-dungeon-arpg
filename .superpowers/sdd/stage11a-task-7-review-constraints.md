# Stage 11-A Task 7 独立复审约束

复审 `763ff5e..Task7 HEAD`，读取设计、计划、Task 7 brief、实现报告和完整 diff。不要改代码。

重点检查：

1. 正式深渊死亡是否只提交一次 `death_retreat`，不再走旧 `prepare_abyss_failure()` 双事务/同房 reset。
2. next 是否在本地一次写全 death、target、failed abyss lifecycle 和完整 failed resolution；stable 在 verified commit 前不变。
3. `death_was_abyss` 与死亡房 anchor 是否来自冻结死亡时的 started abyss；发布后的 `current_room.is_abyss=false` 是否仍能通过合法性检查。
4. failed resolution 的 seed/rule/total/generated/claimed/abandoned 是否严格规范，且不物化奖励、不保留 ground chest/pending reward。
5. not_committed 是否保留同一个 Combat death snapshot 并逐字段相同重试；committed 才清 transient。
6. cache/receipt 是否覆盖完整 exact next，started abyss 或 death/abyss 字段被篡改时 fail closed。
7. 合法 pending death 重启是否不会被遗留 started-save 修复二次改变 generation/target/resolution。
8. Task 6 的事件容量预留、ordinary death、Stage 10 手动失败/放弃/成功奖励路径是否无退化。
9. 测试证据是否完全走生产 API；不得直接注入 death snapshot、stable death、pending save，fixture 更新不得弱化原有不变量。
10. 无 `/STACK`、无 padding `memcmp`、工作树干净、报告与实际一致。

结论必须为 Approved 或 Needs fixes，并按 Critical / Important / Minor 列出问题和文件行号。

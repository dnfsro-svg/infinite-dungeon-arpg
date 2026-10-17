# Stage 10 Task 12 实现报告

## 结果

- 新增固定 root 的两条 1000 房深渊轨迹：一条不中断，另一条每 37 房执行真实 V5 `encode_checkpoint` / `decode_checkpoint`，共 27 次；逐房结果完全一致。
- 每个深渊房均通过真实 `DungeonSession` 奖励物化、公开拾取、退出 warning、neutral release、同向二次触发生成并持久化 `LastAbyssResolution`，codec 往返后逐字段一致。
- 逐字段比较四门预告、实际房间、规则/危险、完整遭遇计划、奖励槽和合法 resolution 摘要。
- 固定 golden：四向命中 `9/11/9/7`，总计 `36/4000`；宽护栏 `20..70`；最终 hash `0xe102b17e6423351b`。
- 新增真实 `DungeonSession` 中 96 怪、每怪 3 个 M3 词缀、Chaos Expansion、满 384 projectile / 96 hazard / 192 ground pool 的 600 tick 压测；AllocationProbe 结果为 0，地面饱和诊断精确增加 600（`1 -> 601`），无 invalid owner/event overflow；探针前后对全部 192 个真实 ground 槽的 active/ordinal/source/abyss ordinal/position 和 ItemInstance 全字段逐槽比较相等。
- 新增真实 SaveStore + DungeonSession 夹具：公开移动/J、公开 pickup/exit API，输出门 seed、started/clear generation、环境伤害、三件 reward id、reload masks 和 abandon summary。
- 放弃夹具明确验证 warning event、armed、neutral release、同向二次触发，并从保存前 masks、完整 ground snapshot 与 inventory 推导 resolution 后逐项断言，同时要求摘要 room seed 等于目标深渊 seed、规则为 Life Sacrifice。
- 正式 raylib host 验证只注入 MovementInput/Action/R/E 等公开输入脚本；未调用测试私有访问器、未改 phase/masks、未注入 defeated。
- 证据守卫覆盖全部正式 evidence 源（含 host header）；`EndDrawing`、`LoadImageFromScreen`、`ExportImage` 仅允许各出现一次并共同封装在唯一 `present_frame_and_maybe_capture` helper，recovery/main 都只能经该 helper 呈现。新增私有注入、呈现前截图、前置真实截图加后置 dummy 三个 `WILL_FAIL` 反向 CTest。
- 正式生成七张 EndDrawing 后截图：门标记、雷暴预警、追猎烈焰预警、混沌扩散、宝箱、待生成提示、二次确认。
- 正式跑通死亡、R、started 重启、深渊+洞四路径。洞路径使用纯生成筛选的 hunting-flames+hole+全 melee root，真实保留 2 件奖励，观察第一次 E warning，第二次 E 原子 resolution+descent；重载为 `depth=2`、`last_transition=descent`、`LastResolution(valid,total=2,claimed=0)`。

## 验证

- 指定四 targets fresh build：通过。
- `ctest --preset windows-msvc-debug -R "^stage10\\." --output-on-failure`：8/8，通过，13.07 秒，0 失败。
- `dungeon.units`：199/199，通过，148.00 秒。
- persistence/platform/architecture 相关回归：22/22，通过，70.38 秒，0 失败。
- `git diff --check`：通过。
- 截图像素统一护栏：尺寸 1280x720、freshness、采样颜色数 >=20、非背景样本 >=100。
- 人工检查发现首版 `04-chaos-expansion.png` 被转场黑幕覆盖（1 色）；改为目标出现后累计 16 个已呈现帧再截取。最终七图颜色数为 `31/21/25/44/40/42/44`，04 为 44 色真实 HUD/房间/环境区域。
- 验证输出和临时存档仅位于 target 可执行文件目录下；正式程序每次只清理自己的 `stage10-*validation` 专用目录，未触碰用户默认存档。
- 完成后无 `ctest`、验证游戏或 raylib 窗口进程残留。

## 疑虑

- 无未解决 Critical/Important；正式像素阈值保留在 CTest 中，防止黑帧回归。

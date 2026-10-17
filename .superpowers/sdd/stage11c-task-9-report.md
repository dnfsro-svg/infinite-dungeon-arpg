# Stage 11-C Task 9 正式 HUD 验收报告

## 结论

- 6 个场景均由独立进程启动真实 `run_raylib_host`，经物理按键采样、映射、暂停门控、`Session` 提交和固定帧推进到达目标状态。
- 截图仅在 `EndDrawing` 后通过唯一生产呈现捕获点生成。
- 正式验证、证据验证、反注入守卫、自测守卫与 237 项平台单元测试全部通过。
- 结果：PASS。

## TDD 证据

初始 RED：`stage11c.hud_formal` 因正式可执行文件不存在失败；validator 依赖未执行；guard/self-test 脚本不存在，4 项为 0/4。

level-up 视觉闭环的聚焦 RED：新增 `PlayerPanelPlan.progression_text` 单测后编译失败，明确报告该字段不存在。随后在生产 HUD 计划层格式化等级与未分配点，renderer 只消费计划文本；不存在截图专用分支。

## 最终命令与结果

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build out/build/windows-msvc-debug --target arpg_platform_tests arpg_stage11c_hud_formal arpg_game
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|stage11c\.hud_formal|stage11c\.hud_evidence_validator|stage11c\.hud_evidence_guard|stage11c\.hud_evidence_guard_self_test)$' --output-on-failure
```

结果：5/5 通过；`platform.units` 为 237 cases；正式场景 6/6。

## 正式证据

证据目录：`E:\game\.worktrees\stage11c-complete-hud\out\build\windows-msvc-debug\tests\platform\stage11c hud evidence`

| 场景 | PNG | 生产快照哈希 | 人工核对 |
|---|---|---:|---|
| 正常战斗 | `combat.png` | `13190197248370712797` | HP/XP、目标、房间导航及怪物战斗态可见 |
| 低生命状态 | `low-health.png` | `3176236133384143545` | HP 225/1000、低生命条和状态标签可见 |
| 清房出口 | `cleared.png` | `11591981634548195452` | 出口开放目标和清房奖励通知可见 |
| 深渊放弃警告 | `abyss-warning.png` | `12392544927936643533` | 紫色深渊边框、同门二次确认与放弃全部剩余奖励警告可见 |
| 升级与点数 | `level-up.png` | `11591981634548195452` | 底部 `LV 2 · 未分配点 1`、升级与奖励通知可见 |
| F1 调试层 | `debug.png` | `13190197248370712797` | 完整 F1 诊断层与 HUD 同帧可见 |

`cleared` 与 `level-up` 使用同一真实清房生产快照，但各自独立进程、场景语义和捕获帧不同；最终 PNG 哈希也不同。未伪造或直接修改快照。

## 守卫覆盖

反注入自测逐项验证以下突变都会被拒绝：直接构造 ViewModel、TestAccess、逻辑动作排队、呈现前截图、伪造字体就绪、伪造 PASS、绕过 Session 推进。

正式场景同时验证 1280x720 PNG、文件新鲜度、布局矩形、玩家值、状态标签、目标、通知、导航、CJK 字体、F1 状态和生产快照哈希。

## 生产路径修复

- `CombatRenderer::initialize_resources` 现在返回死亡字体与 HUD CJK 字体的真实联合就绪状态，正式摘要不再把字体状态写死。
- HUD notice state 以最后实际呈现的观察值补足多固定帧下的深渊确认与升级/点数边沿，保留原固定 tick 边沿语义。
- 玩家面板生产渲染补充等级与未分配点，并由 `PlayerPanelPlan` 聚焦单测锁定。

提交：`f01c4ee` (`test: validate stage 11c hud in raylib`)。

## 最终复审修复（独立后续提交）

### RED

- `platform.units`：生产中文语料断言在旧版 `Save failed` 处失败；新增后实际 240 cases，旧固定计数仍为 237。
- validator self-test：旧 validator 接受全部 5 个命名突变：`removed_chinese`、`missing_glyph_boxes`、`cleared_chinese_panel`、`tampered_snapshot_hash`、`zero_sized_rect`。
- evidence guard self-test：旧 guard 接受真实 host 捕获块内的 `host_direct_model_overwrite` 原位替换。

### 修复

- 所有 `HudNoticeState` 玩家可见通知改为中文优先，保留动态按键以及 HP/XP 约定缩写；shared death/HUD font plan 覆盖实际生产通知文本。
- 增加深渊确认、升级、被动点发生于两个呈现帧之间的边沿回归，并验证重复 observe 不重置通知寿命、过期后不重新入队。
- 正式摘要新增 `notice_texts`；每个正式 HUD rect 均记录正面积槽位；aggregate snapshot hash 与逐场景 summary 精确交叉。
- validator 对六场景执行独立强语义校验；关键中文 objective/notice 区域验证深色面板覆盖率、明亮字形像素和重复空心缺字方框。
- validator 自测对真实 PNG/summary/report 进行命名 mutation，并按预期原因拒绝。
- evidence guard 绑定 Stage11C 状态/物理驱动/hash/summary 定义区以及 runtime state 初始化到最终 summary 写出的完整捕获区；model/hash 赋值必须各恰好一次，summary gate 必须真实，禁止绕过 Session 直接写 progression。
- host guard 自测使用真实生产位置原位替换，覆盖 direct model overwrite、fake snapshot hash、fake summary state、bypassed Session progression。

### 最终 GREEN

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|stage11c\.hud_(formal|evidence))' --output-on-failure
```

结果：6/6 通过，0 failures；`platform.units` 240 cases。

关键中文通知区特征：

- abyss-warning：bright 3093，dark ratio 0.920，hollow boxes 0。
- cleared：bright 2814，dark ratio 0.930，hollow boxes 0。
- level-up：bright 2814，dark ratio 0.930，hollow boxes 0。

验收阈值为 bright >= 80、dark ratio >= 0.45、hollow boxes < 3，和真实图像特征保留数量级余量。六张截图已人工目检，深渊放弃与升级/奖励通知均为中文且无缺字方框。

复审修复提交：`f6d0a76` (`fix: harden stage 11c hud evidence`)。

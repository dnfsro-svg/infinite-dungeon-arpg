# Task 7-10 评审修复报告

- 提交：`50dc8d7`（`fix: preserve v9 drop compatibility and hud diagnostics`）
- 范围：Task 7/8 的 V9 次级掉落兼容与最大合法载荷往返；Task 10 的同帧有界可见集 HUD 诊断。
- 工作树：`E:\game\.worktrees\finish-unfinished-branches-20260801`

## 1. P1：保留旧 Task 5 V9 次级掉落序号兼容

### 根因

旧 Task 5 V9 对普通房材料使用 `checkpoint_material_ordinal(m)` 写线：普通材料和优惠券均落在偶数序号 `2*m`。优惠券/血瓶共享的旧持久 claim 还可能同时留下 `2*m` 与 `m` 两个 room claim。后续实现把普通房次级掉落改为 canonical 序号：普通材料为偶数、优惠券和血瓶为奇数，但 V9 外层没有能区分两种含义的标记，因此直接读取旧文件会把相同线值解释成不同对象。

### 设计与实现

- 保持 V9 主格式版本不变，在现有 V9 尾扩展的 abyss lifecycle 字节之后增加 canonical secondary ordinal 标记 `0xC9`。
- 新编码器始终写入该标记；解码器看到标记时按 canonical 语义读取。
- 无标记的 V9 明确按旧 Task 5 语义迁移，并把 `migrated` 置为 `true`：
  - 从 durable `material_claimed_drop_bits` 精确重建旧 Task 5 应有的 room secondary claim 集；不接受无法对应 durable 状态的任意旧 claim。
  - 普通材料记录把旧偶数 wire ordinal 除以 2，结合 `source` 校验普通材料/优惠券奇偶语义。
  - abyss reserve ordinal 保持原映射，不做普通房除 2。
  - 折叠旧优惠券/血瓶共享产生的重复 claim，生成 canonical claim 集。
  - 迁移后按 canonical ordinal 排序，拒绝碰撞，再运行现有完整 structural validator。
- `kV9MaximumEncodedBytes` 相应增加 1 字节；V9 readback verifier 要求新编码结果同时包含 lifecycle 与 canonical 标记。

### RED / GREEN

- 一次直接调用未进入 VS 开发环境的 `cl` 构建因标准库 `cmath` 不可用而失败；这是环境错误，不计为 TDD RED。随后统一通过 `scripts/Configure.ps1 -Preset windows-msvc-debug` 进入 MSVC 环境，并保持 `-j1` 串行构建。
- 有效 RED：旧 Task 5 wire fixture 把优惠券记录改回偶数 wire ordinal/source，并移除新标记、重算 CRC/长度。现有解码器在 `decode_checkpoint_v9_into(...) == CodecError::none` 断言处失败；新增用例还触发 suite case count 门禁。
- GREEN：实现迁移后 `persistence.units` 110/110 通过，78.70 秒。测试确认旧 common wire `8 -> canonical 4`、旧 coupon wire `14/source 1 -> canonical 7/source 1`，旧 claim 20/26 折叠为 canonical 10/13，并通过 structural validator。

### 已知兼容风险

提交 `5efb1e2` 曾短暂生成“无标记但已使用 canonical ordinal”的 V9。它与公开的旧 Task 5 无标记 wire 在字节层不可判别。本修复优先保证已存在的 Task 5 文件，因此所有无标记 V9 都按旧语义迁移；若有人实际保存过 `5efb1e2` 的中间评审构建文件，该文件可能被误迁移。该歧义无法在不增加外部版本/标记的情况下可靠消除。

## 2. P2：真实最大合法 V9 负载往返

### 合法上限

- V9 物理 secondary 存储容量是 2320，但普通房在 `generated_monsters = 1152` 时可同时合法存在的次级 ground records 上限是 2304。
- abyss 房普通怪物上限更低，reserve records 又受真实奖励数量约束，因此 2320 条记录不是可同时构造的合法游戏状态。
- 最大 equipment ground records 为 1152。

### 测试内容

新增真实编码/解码往返测试，覆盖两组极值：

1. 1152 条 equipment ground records，加 2304 条按序的 secondary records（普通材料偶数、优惠券/血瓶奇数）。验证实际编码小于 8 MiB 且不超过 `kV9MaximumEncodedBytes`，然后逐条核对 equipment ordinal/source/reward/position/item 核心字段与 secondary tag/ordinal/source/position/material。
2. 清空 ground records，填满全部合法 equipment claim 位 1152 和普通 secondary claim 位 2304，再做真实编码/解码，验证两个 bitset 完全相等且 ground count 均为 0。

### RED / GREEN

- 新测试主体在现有 codec 上首次即可完成往返；有效失败仅为精确用例数门禁 `actual 111 != expected 110`。这证明评审指出的是测试覆盖缺口，而不是该极值路径上的现存 codec 行为缺陷。
- 更新门禁后 GREEN：`persistence.units` 111/111 通过，80.17 秒。
- 最终复验：`persistence.units` 111/111 通过，80.59 秒。

## 3. P2：Task 10 同帧有界可见集 HUD 诊断

### 根因

生产宿主原顺序是先调用 `renderer.observe_presented_hud_frame(...)` 构建 HUD，随后才计算 camera/world query 并调用 `write_render_snapshot(...)`。因此 HUD 不可能展示当前 presented frame 的有界可见集，只能缺失或使用旧帧数据。

### 设计与实现

- `HudViewModelProjector::build`、`CombatRenderer::observe_hud` 与 `observe_presented_hud_frame` 增加可选的 presented `DungeonRenderSnapshot*`，旧调用保持兼容。
- `RoomHudModel` 新增固定容量 `HudVisibleSetModel`，记录：
  - `camera_version`
  - resident monster 可见数
  - environment 可见数与 `candidates_examined`
  - equipment/material/health potion 合计可见掉落数与 `drop_candidates_examined`
- 文本直接使用同一有界快照和生产常量生成，无全房扫描、无动态分配：
  - resident 上限 `combat::room_spatial::maximum_streaming_monsters = 105`
  - environment candidate 上限 `kEnvironmentQueryCandidateCapacity = 105`
  - drop candidate 上限 `kRoomDropQueryCandidateCapacity = 331`
  - 示例：`可见：怪 105/105 环境 73/105 掉落 53/331`
- 宿主先完成 camera/query、`write_render_snapshot` 和可选 showcase world 处理，再把同一个 `render_world` 指针传给 HUD observe，随后才进入 `BeginDrawing`。原 HUD validation observer 仍紧随 renderer HUD observe。
- HUD 目标面板增加独立诊断行，不覆盖既有目标、经验、移动或三行控制提示：逻辑高度从 142 调整为 164；战斗排除区与 debug panel 起点调整到 logical Y=186。
- 三个支持分辨率 1024x576、1280x720、1920x1080 均验证七行文本在目标面板内、彼此不重叠；诊断使用现有 CJK HUD 渲染和不透明 secondary 对比色。
- 宿主源码测试进一步锁定 `write_render_snapshot -> showcase world -> observe_presented_hud_frame(render_world) -> BeginDrawing` 顺序。

### RED / GREEN

- 第一版测试直接调用尚不存在的五参数接口并访问尚不存在的字段，产生编译错误；按评审要求，该结果不计有效行为 RED。
- 测试随后改为可编译兼容检测 seam：旧四参数投影器正常编译运行；缺少 presented world 投影能力时，以 `ARPG_REQUIRE(false)` 产生真实行为失败。有效 RED：`hud_view_model.same-frame visible set diagnostics` 在 `hud_view_model_tests.cpp:219` 失败，同时精确用例数门禁报告 `actual 564 != expected 563`。
- 实现生产链路后，首轮平台测试仅暴露三个布局安全失败：164px 面板与旧 combat exclusion/debug Y 边界重叠，以及旧 `height <= 142` 断言。调整布局契约并把测试扩成七行后 GREEN：`platform.units` 564/564 通过，17.81 秒。
- 实现过程中还修正了一个局部编译错误：`render_world` 已是裸指针，不能调用 `.get()`；改为直接传递该指针。
- 加入最终宿主同帧顺序断言后复验：`platform.units` 564/564 通过，18.10 秒。
- 同帧测试验证第二帧完全覆盖第一帧：camera version 701、resident 105、environment 73/检查 105、drop 19+23+11=53/检查 331，文本不截断，二次投影分配计数不变。

## 4. 最终验证证据

实际执行并取得的最终结果：

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests -- -j1`：通过。
- `ctest --test-dir out/build/windows-msvc-debug -R '^persistence\.v9_wire_baseline$' --output-on-failure`：1/1 通过，0.20 秒。
- `ctest --test-dir out/build/windows-msvc-debug -R '^persistence\.units$' --output-on-failure`：111/111 通过，80.59 秒。
- `ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure`：564/564 通过，18.10 秒。
- `stage11c.architecture.hud_boundaries`：通过，59.85 秒。
- `platform.host_input_source`：通过，546.23 秒。
- `git diff --check`：通过；仅出现工作树 LF 将来可能转 CRLF 的 Git 提示，无 whitespace error。
- 禁止产物检查：三个 `task9*.log` 与 `tests/platform/__pycache__` 未进入本次修改或提交。

### 附加门禁中的基线既有失败

附加五项宿主/HUD 源码守卫共 2 项通过、3 项失败。三项失败均锚定提交 `5efb1e2` 已存在且本次未修改的基线语句，因此未越权扩展 Task 7-10 修复范围：

1. `platform.host_validation_source`：Task 8C 报 `Host explicit close: expected 1, found 2: window.close();`。对照 `git show 5efb1e2`，基线与当前均有 2 个 `window.close();`，分别位于 bounded render snapshot 发布失败路径和正常收尾路径。
2. `platform.host_validation_sequence`：报 real renderer draw return direct-exact count 为 0。基线与当前均只有同一条 `return renderer.draw(`，该 lambda return 语句未被本次修改。
3. `stage11c.hud_evidence_guard`：报 facade summary binding/order。基线与当前均有 2 个 `audio.shutdown();` 和 1 个 `validation_runtime->write_summaries(...)`，且 bounded render snapshot 失败路径中的第一个 audio shutdown 本来就早于正常收尾 summary；本次没有修改这些语句或其错误路径所有权。

这三项作为基线测试债记录，不影响本次新增回归用例和相关单元/线协议基线的通过结论，但若要求整个仓库所有架构守卫全绿，需要另立任务修复守卫对错误退出分支的计数/顺序建模。

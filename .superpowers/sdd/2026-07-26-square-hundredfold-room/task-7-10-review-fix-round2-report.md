# Task 7/10 第二轮复审修复报告

- 基线：`50dc8d7`（`fix: preserve v9 drop compatibility and hud diagnostics`）
- 工作树：`E:\game\.worktrees\finish-unfinished-branches-20260801`
- 范围：仅处理第二轮复审 P1a（无 marker V9 次级掉落语义消歧）和 P1b（HUD 可见集诊断字体覆盖）。

## 1. P1a：无 marker V9 次级掉落按整文件语义消歧

### 根因

旧 Task 5 的普通材料 wire ordinal 是 canonical ordinal 的两倍：source 0 为 `0 mod 4`，source 1 为 `2 mod 4`。短暂提交 `5efb1e2` 已直接写 canonical ordinal，但尚未写后来的 `0xC9` marker。于是无 claims 的 source 0 / ordinal 4 同时可以表示旧 Task 5 的 spawn 1，也可以表示临时 canonical 的 spawn 2；原迁移器固定除以 2，会把后者静默误读成 ordinal 2。

### 实现

- 从 durable `material_claimed_drop_bits` 同时构造 Task 5 expected claims 和 canonical expected claims：仅匹配一方时形成全文件语义证据，两方都不匹配立即拒绝。
- 普通材料 ground 按 source/奇偶形成直接证据：
  - source 0 / `2 mod 4`、source 1 / 奇数只可能是 canonical；
  - source 1 / `2 mod 4` 只可能是 Task 5；
  - source 0 / `0 mod 4` 可能同时成立，需要进一步判别。
- 对 source 0 歧义记录比较 old candidate `spawn = packed / 4` 与 canonical candidate `spawn = packed / 2`：优先匹配同一 checkpoint 中的 monster ordinal/position、同 spawn equipment、health potion 或 coupon 位置；没有唯一位置证据时再用 defeat bit 的单边可达性。
- 所有 claims、直接 ground 证据和歧义位置证据必须收敛为同一种文件语义；冲突、混合或仍无法唯一判别的语义敏感文件返回 `CodecError::invalid_state`。
- 只有判定为 Task 5 时才统一除以 2、重排、拒绝碰撞并迁移 claims；canonical 文件保持 ordinal 不变。没有任何语义差异的数据（空次级状态、健康药水、reserve、ordinal 0）可安全按 canonical 重写 marker。
- 正常含 `0xC9` marker 的 V9 路径和 wire schema 未改变。

### RED / GREEN

- 一次未进入 VS 开发环境的直接构建因 `<cmath>` 不可用失败，不计有效 RED；后续均通过 `scripts/Configure.ps1` 初始化 MSVC 19.44 / Windows SDK 26100，并以 `-j1` 构建。
- 第一条有效 RED：临时 canonical source 0 / ordinal 4 / spawn 2 位置 fixture 解码后得到 ordinal 2，断言要求保持 4；`persistence.units` 失败。
- 完整矩阵有效 RED：114 cases 中 3 failures，分别是临时 canonical 被误迁移、歧义 coincident 位置被接受、canonical/Task 5 混合语义被接受。
- GREEN 后 114/114 通过；自审又补了零 claims、位置只匹配 old spawn 1 的正式 Task 5 ordinal 4 -> canonical 2 独立兼容用例。最终 `persistence.units` 115/115 通过。
- 旧 coupon `14/source 1 -> 7/source 1` 与 durable/room claims 折叠用例继续通过。
- 所有无 marker fixture 都先由生产编码器生成真实 V9，再删除 marker或改写目标 record，并重算 payload length 与 CRC。

## 2. P1b：HUD 可见集诊断共享字体覆盖

### 根因与实现

生产 HUD 会生成 `可见：怪 105/105 环境 73/105 掉落 53/331`，但共享字体计划没有请求 `见`、`掉`、`落` 三个 codepoint，required-text audit 也未包含这条诊断文案。

- 新测试构造真实 `DungeonRenderSnapshot`，通过 `HudViewModelProjector` 生成 `model.room.visible_set.text`，再直接要求 `death_overlay_font_covers_text(plan.shared, generated_text)`。
- `hud_font.cpp` 只新增 `见`（U+89C1）、`掉`（U+6389）、`落`（U+843D）；`环境` 已由共享 death-overlay 字体提供，没有重复列入新增 codepoints。
- required-text audit 加入完整诊断示例，仍由固定容量、去重的共享 codepoint plan 承载。

### RED / GREEN

- 第一版测试误用五参数自由函数而编译失败，不计有效 RED；改用真实 `HudViewModelProjector::build` 后成功编译。
- 有效 RED：`platform.units` 565 cases 中仅 `hud_font.visible-set ViewModel shared font coverage` 失败，失败点为生成文本不被共享字体覆盖。
- GREEN：新增三个缺失 codepoints 和 required text 后，`platform.units` 565/565 通过；既有唯一性和固定容量测试同时通过。

## 3. V9 wire golden 修复

最终门禁发现 wire golden 仍冻结在加入 canonical marker 之前：五个固定 fixture 的实际长度全部恰好多 1 字节。逐文件读取确认尾字节均为 `0xC9`，再更新五组长度/SHA256；未修改编码器。更新后 `persistence.v9_wire_baseline` 通过。

## 4. 最终验证

实际执行：

- `. ./scripts/Configure.ps1 -Preset windows-msvc-debug`，随后 `cmake --build --preset windows-msvc-debug --target arpg_checkpoint_v9_wire_baseline arpg_persistence_tests arpg_platform_tests -- -j1`：通过。
- `ctest --test-dir out/build/windows-msvc-debug -R '^(persistence\.units|persistence\.v9_wire_baseline|platform\.units)$' --output-on-failure`：3/3 tests 通过，0 failures；`persistence.units` 81.08 秒，wire baseline 0.21 秒，`platform.units` 17.89 秒，总计 99.27 秒。
- `git diff --check`：见提交前最终检查。

## 5. 自审与剩余风险

- 测试预期使用手写 ordinal、source、位置候选和完整诊断文本，没有调用迁移器内部 helper 推导期望。
- marker 正常路径不进入本次消歧；Task 5 迁移只在整个文件语义一致时发生。
- 有意 fail closed：无 marker、source 0、非零 `0 mod 4` 且 claims/位置/defeat/同 spawn 掉落都无法唯一判别的中间存档现在返回 `invalid_state`。这避免静默错领掉落，但不可验证的历史中间存档需要外部恢复策略。
- 本轮没有运行人工窗口画面验收；改动不改变 HUD 布局，只扩充现有共享字体 glyph plan，自动化 platform 字体/布局/渲染相关单元门禁已覆盖。
- 未修改或提交 task 9 日志与任何 `__pycache__`/`.pyc`。

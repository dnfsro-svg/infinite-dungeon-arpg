# Stage 11-A Task 4 实现报告

## 范围

- 基线：`8832c7fa43c7d0871403cfb120943af8069d8b2f`
- 实现提交：`72e5379ca32753c47326c66ce9c6b0cafd9216e9`
- 补充测试提交：`822d10690be4184871343763cff588850158b204`
- 生产代码仅修改 `src/persistence/checkpoint_codec.hpp/.cpp`。
- 测试修改仅位于 `tests/persistence/`。
- 未修改 Dungeon、Combat、Session 或 raylib 生产代码。

## TDD RED

先新增 `death_checkpoint_codec_tests.cpp` 并注册 suite，未修改生产代码即运行 Debug 构建。构建按预期失败：

```text
error C2039: "kV6DeathPayloadSize": is not a member of arpg::persistence
error C2039: "kV6BasePayloadSize": is not a member of arpg::persistence
error C2039: "kV6BaseEncodedCheckpointSize": is not a member of arpg::persistence
```

这证明旧 V5 codec 不具备 V6 格式和死亡块。

## GREEN / 实现

- encoder 只写 format 6 和 8-byte magic `ARPGSV6\0`。
- decoder 接受 V1-V6；`migrated = format != 6`。
- 保留 V5 常量和原 magic；新增独立 `kFifthCheckpointFormatVersion`。
- 精确实现 224-byte death block：base payload 428 bytes、base encoded size 460 bytes。
- 所有整数沿用 little-endian helper；有符号字段使用明确的 two's-complement 解码。
- reserved bytes 编码为零，解码非零返回 `invalid_state`。
- V4/V5/V6 由 `ItemLayout` 选择布局，item record 共享同一解码循环。
- V5 item offset 236，V6 item offset 460；40-byte record 未改变。
- encoder/decoder 只调用 header-only `valid_death_checkpoint_structural()` 处理死亡稳定结构。

## 迁移和错误矩阵

- V6 canonical none、完整 pending_continue 逐字段 round-trip；none 状态也保留非零历史 `death_sequence`。
- V5 fixture 精确恢复 V5 magic/version/payload/CRC；验证 item、equipment、claimed bits、next sequence 与旧稳定字段不变，同时 death sequence 为 0、death 为 canonical none。
- V1-V4 全部验证 migrated、death sequence 0、canonical none。
- 覆盖 enum、boolean、reserved、final sum、source ID、生命/护盾、防御、recent overflow 和 target 结构错误。
- 覆盖 death block 的每个截断大小 236..459、尾随字节、payload length、CRC、magic/version mismatch。
- 原最大 65535 item、长度算术、allocation failure 测试继续通过。

## 验证

```text
arpg_persistence_tests.exe: 76 cases, 0 failures
persistence.units: 1/1 passed
architecture.persistence_no_raylib: passed
architecture.persistence_checkpoint_only: passed
git diff --check: clean
```

## 关注项

`src/persistence/save_slot.cpp` 的稳定状态相等比较还未包含 death 字段，但它不在 Task 4 允许修改范围；后续死亡事务/receipt 接入任务必须在启用 pending death 的 SaveStore 路径前扩展该比较。Codec 本身没有已知未解决问题。

## 独立审查 Important 修复

- 审查修复提交：`a0b624fa8f8a7a90613278b47536bc4f76dd49df`

### RED

- 在 detached `8832c7f` 临时工作树中运行真实 V5 encoder，生成含完整 abyss/resolution/ownership/三件装备记录的 356-byte fixture。
- 将 fixture 冻结进测试且不再调用当前 V6 encoder。首次故意保留 CRC 的单字节差异，构建成功后得到 `76 cases, 1 failure`，唯一失败为 `v5 fixture migrates to canonical none` 的 decode 非 `none`；校正为基线原始 CRC 后进入 GREEN。这证明 fixture 测试独立于当前 encoder 且能捕获字节漂移。

### 修复内容

- 删除运行时 `as_v5_fixture(V6)` 方案，改为 `v5_golden_fixture.hpp` 中来源固定为基线 `8832c7f` 的 356-byte V5 golden。
- V5 migration 逐字段核对 generation、biases、完整 room、transition、progression、passive、abyss、last resolution、三件 item 的全部 record、equipment、claimed bits、next sequence，以及 canonical death none。
- V6 pending fixture 改用具区分度多字节值；对绝对 offsets 245..451 的每个标量、数组边界、little-endian 和负数 two's-complement 加入独立 golden-byte 断言，保留全字段 round-trip。
- reserved offsets 249、254、255、452..459 共 11 bytes 各自 mutation、重算 CRC，并逐项断言 `invalid_state`。
- 本轮未修改生产代码。

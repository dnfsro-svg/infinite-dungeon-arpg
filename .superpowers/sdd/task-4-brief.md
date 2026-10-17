# Stage 11-A Task 4 实现任务：V6 checkpoint 与 V5 迁移

## 边界

- 基线 HEAD：`8832c7fa43c7d0871403cfb120943af8069d8b2f`。
- 只修改 `src/persistence/checkpoint_codec.hpp/.cpp` 与 `tests/persistence/` 中必要测试/注册。
- 不修改 Dungeon/Combat/Session/raylib 生产代码，不提前实现 Task 5+。
- Persistence 只依赖稳定 `dungeon_checkpoint.hpp` 与 header-only `valid_death_checkpoint_structural()`；禁止包含/链接 DungeonRules、room generation、Combat 目录或 raylib。

## 固定格式

- 新 magic：8 bytes `ARPGSV6\0`，format `6`；保留 V1-V5 magic/decoder。
- encoder 只写 V6；decoder 接受 V1-V6；`migrated = format != 6`。
- V5 204-byte base payload 后追加 224-byte death block。
- `kV6DeathPayloadSize=224`、`kV6BasePayloadSize=428`、`kV6BaseEncodedCheckpointSize=460`。
- V6 item record 仍为 40 bytes，从绝对 offset 460 开始；V5 item record 仍从 236 开始。
- 既有 V5 payload/encoded 常量必须保留其 V5 语义；新增明确 V6 常量，避免把历史 fixture 的常量偷换语义。

## Death block 绝对偏移

| offset | size | field |
|---:|---:|---|
| 236 | 8 | `death_sequence` |
| 244 | 1 | lifecycle |
| 245 | 1 | data version |
| 246 | 1 | source kind |
| 247 | 1 | damage type |
| 248 | 1 | monster id |
| 249 | 1 | reserved zero |
| 250 | 2 | detail id |
| 252 | 1 | was abyss |
| 253 | 1 | death ecology |
| 254 | 2 | reserved zero |
| 256 | 8 | death depth |
| 264 | 8 | death floor-room index |
| 272 | 8 | raw damage |
| 280 | 8 | barrier loss |
| 288 | 8 | health loss |
| 296 | 8 | final damage |
| 304 | 40 | recent five-type damage |
| 344 | 16 | hp/max hp/barrier/max barrier as i32 |
| 360 | 16 | armor/evasion as i64 |
| 376 | 8 | armor reduction/evasion rate as i32 |
| 384 | 16 | four elemental reductions as i32 |
| 400 | 16 | four elemental caps as i32 |
| 416 | 32 | target room index/seed/depth/floor index |
| 448 | 4 | target entry/ecology/hole/abyss |
| 452 | 8 | reserved zero |

所有整数沿用现有 codec 的 little-endian helpers。所有 reserved bytes 编码为 0，解码非零必须拒绝。

## TDD 必测

1. 先写 RED，再改生产代码；记录能证明旧 V5 实现失败的断言。
2. V6 canonical none 与完整 pending_continue round-trip，逐字段比较 `death_sequence` 和 DeathCheckpoint 全字段。
3. 真实 V5 fixture 解码：`migrated=true`、`death_sequence=0`、canonical none；原 V5 run/room/summary/abyss/passive/items/equipment/claimed bits/next sequence 语义不变。
4. V1-V4 回归继续通过，迁移 death 状态同样为 sequence 0 + canonical none。
5. 精确错误矩阵：
   - invalid lifecycle/source/damage/ecology/entry enum -> `invalid_enum`；
   - invalid boolean -> `invalid_boolean`；
   - nonzero reserved、结构关系错误（含 final sum、生命/护盾、防御、target）-> `invalid_state`；
   - CRC/magic/format/rules/payload length 使用既有对应错误。
6. 每个 death block 截断边界、尾随字节、错误 payload length、CRC、magic/version mismatch 都拒绝；测试必须重算 CRC 时明确区分结构错误与坏 CRC。
7. item count 最大值 65535、乘法/加法溢出、allocation failure 防御继续有效；V5/V6 item decoder 共享一套逻辑，以布局常量选择 count/ownership/item offsets，不复制两套 decoder。
8. encoder 对无效 DeathCheckpoint 返回 `nullopt`；decoder 只调用 `valid_death_checkpoint_structural()` 做稳定结构校验。

## 验证与交付

- 构建 `arpg_persistence_tests`。
- 运行测试可执行文件与 `ctest --preset windows-msvc-debug -R '^persistence\.units$' --output-on-failure`。
- 运行相关 architecture tests，证明 persistence 未引入 Dungeon/Combat/raylib 实现依赖。
- `git diff --check`；审计 `git diff 8832c7f..HEAD` 范围。
- 报告写 `.superpowers/sdd/task-4-report.md`，包含 RED/GREEN 证据、布局、迁移、错误矩阵、测试结果、疑虑。
- 提交生产和测试变更，建议消息 `feat: persist death checkpoints in save v6`。


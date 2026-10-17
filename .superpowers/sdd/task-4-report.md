# Stage 8 Task 4 实现报告

## 状态

- 完成：装备防御、独立闪避 RNG、direct/ground 交付分类、热换 build、武器物理与局部攻速已接入 combat。
- combat 未依赖 items；未修改 dungeon、persistence、raylib、passives、items 或 UI。

## TDD RED / GREEN

1. 防御包、RNG、交付分类
   - RED：加载 MSVC 环境后，编译按预期因 `DamageDelivery`、`evasion_seed`、新快照字段和 optional resolver 未定义而失败。
   - GREEN：84 项通过；覆盖五分量单次闪避、ground/零包不消费、reset 重播 seed、护甲、四元素、-6000/7500/9500 cap、ceil、invalid 与合法零。
2. 热换 build
   - RED：编译按预期因 `apply_player_build`、`weapon_physical`、`local_attack_speed_bp` 未定义而失败。
   - GREEN：87 项通过；升限不回复、降限只 clamp，位置/速度/攻击/hurt/invulnerability 保留，无效构造/热换原子拒绝。
3. 武器物理与局部攻速
   - RED：编译按预期因 `build_player_hit_packet` 仍是不可失败返回而失败。
   - GREEN：92 项通过；helper 改为 optional，冻结顺序 checked 求值，真实命中增伤，局部×全局攻速只缩 startup/recovery，active 保持 3 ticks。
4. 完整 build 有效性与缺失 RNG 覆盖
   - RED：负 `weapon_physical` 的 build 被 resolver 错误接受。
   - GREEN：95 项通过；resolver 复用完整 build 验证，并补 0 evasion 不推进 RNG、同 seed/trace 等价、真实 hazard 不闪避。
5. 自审补测
   - RED：极低攻速使 recovery 超出 `uint16_t` 并被热换接受。
   - GREEN：build 预验证拒绝不可表示 phase ticks；95 项通过。

## 最终回归

- `cmake --build build-release --target arpg_combat_tests`：成功，`ninja: no work to do.`
- `ctest --test-dir build-release -R combat.units --output-on-failure`：1/1 suite 通过，95/95 cases，0 failures。
- `git diff --check`：通过；仅有仓库既有 LF→CRLF 提示，无 whitespace error。
- `rg` 审计：`src/combat` 与 `tests/combat` 无 `items` 依赖、无旧 `resistance` 名称。

## DamageDelivery 调用点清单

- direct：`src/combat/monster_ai.cpp` 的接触命中解析；覆盖普通近战及 charger/dasher 的 contact 路径。
- direct：`src/combat/monster_ai_special.cpp` 的 bomber 直接爆炸命中。
- direct：`src/combat/combat_world.cpp` 的投射物碰撞命中。
- direct：`src/combat/combat_world.cpp` 的既有 int 兼容重载；用于旧测试/直接物理伤害入口。
- ground_or_environment：`src/combat/combat_world.cpp` 的 hazard 固定区域周期伤害。
- `monster_ai_melee.cpp`、`monster_ai_special.cpp` 的 charge/dash 均经 `resolve_monster_contact_attack` 汇入上述 direct 接触点；`monster_ai_ranged.cpp` 只生成投射物，交付分类在碰撞解析点完成。

## 变更文件

- 生产：`combat_types.hpp`、`combat_world.hpp/.cpp`、`combat_snapshot.cpp`、`player_simulation.cpp`、`hit_resolution.cpp`、`monster_ai.cpp`、`monster_ai_special.cpp`。
- 测试：`player_build_tests.cpp`、新建 `player_defense_tests.cpp`、`combat_test_support.hpp`、`combat_test_main.cpp`、`CMakeLists.txt`。

## 自审

- `DamagePacket` 保持 int ABI；两个可能失败的 helper 用 optional 区分 invalid 与合法零。
- 五分量 reduction/ceil/求和/final damage_taken 均 checked；invalid 对 hp/barrier/hurt/事件无部分副作用。
- build 构造、reset 使用已接受 build；热换失败不改 config/runtime，成功只更新派生值并向下 clamp。
- 未改攻击 hitbox、active ticks、命中次数、AI 行为或 UI；武器伤害走所有既有近战 AttackId 的共同命中路径。

## 疑虑

- 无已知功能疑虑。双目标构建存在基线已有的 `allocation_probe.cpp` C4530 警告，不影响测试通过。

## 送审前跨模块消费者迁移

- 原因：Task 4 将公开快照成员 `PlayerSnapshot::resistance` 改名为 `damage_reduction`；dungeon 被动树测试仍有五处旧 API 消费，导致 `arpg_dungeon_tests` 无法编译。
- 范围：仅机械修改 `tests/dungeon/dungeon_passive_tree_tests.cpp` 五处成员访问；未修改 dungeon 生产代码、断言值或行为。
- 构建：加载 MSVC/Windows SDK 环境后执行 `cmake --build build-release --target arpg_combat_tests arpg_dungeon_tests`，两个目标成功；dungeon 目标 25 个 build steps 完成。
- 测试：执行 `ctest --test-dir build-release -R "combat.units|dungeon.units" --output-on-failure`，`combat.units` 0.67 秒通过，`dungeon.units` 71.15 秒通过，2/2 tests、0 failures。
- 格式：`git diff --check` 通过，仅有 LF→CRLF 提示。
- 旧名审计：`rg -n "\\.resistance|fire_resistance|water_resistance|lightning_resistance|chaos_resistance" src tests` 无匹配。

## Task 4 审查 Important 修复

### RED

- 新增 invalid-direct 双世界 trace：世界 A 先接收一个五分量 `INT_MAX`、必定 resolve 为 nullopt 的 direct 包，世界 B 不接收；随后两次合法 direct 应保持相同伤害与 RNG 序列，并断言 invalid 不改完整 player snapshot/tick、不发事件。
- 新增完整 build 字段一致性测试：`max_health=-1` 同时要求 `build_player_hit_packet`、`resolve_player_damage` 返回 nullopt，CombatWorld 构造回退默认，热换原子拒绝。
- RED 结果：`combat.units` 97 cases 中 2 failures；分别命中 builder 接受非法 `max_health`，以及 invalid direct 推进 RNG 后双世界 hp 分叉。

### GREEN

- 在 `combat_world.cpp` 抽取纯 `validate_player_build_fields`：统一验证 `values.valid`、bridge 非负字段、生命/防御/移动/攻速等非负字段、各 damage increased 与 cap bonus；builder、resolver、derive/apply 共用，无 helper 递归。
- `apply_player_damage` 先完成不带 RNG 的 checked resolve；nullopt 立即返回。仅对有效解析且原始包有正分量的 direct 包执行原有一次 `next_bounded(10000)`，未改概率、direct/ground 分类或正常伤害顺序。
- GREEN 结果：`arpg_combat_tests` 构建成功；`combat.units` 97/97，0 failures。

### 文件与自审

- 修改：`src/combat/combat_world.cpp`、`tests/combat/player_defense_tests.cpp`、`tests/combat/player_build_tests.cpp`、`tests/combat/combat_test_main.cpp`。
- 自审：调用图为 builder→纯字段验证、derive→纯字段验证→builder、resolver→纯字段验证→derive；不存在 builder/derive/resolver 互相递归。
- 自审：monster contact、bomber、projectile 仍为 direct，hazard 仍为 ground_or_environment；active ticks、AI、UI 与 damage arithmetic 未改。
- Minor：基线 `allocation_probe.cpp` C4530 warning 保留，本修复未扩大范围。

## Task 4 第二轮复审 Important 修复

### RED

- 新增 `max_health=INT64_MAX` 四路径一致性：公开 builder 与 resolver 必须 nullopt，CombatWorld 构造必须回退默认，热换必须原子拒绝。
- 新增 `attack_speed=1` 四路径一致性：该 build 的已知 recovery phase 无法表示为 `uint16_t`，四条公开路径同样必须拒绝。
- RED 结果：`combat.units` 99 cases 中 2 failures；两项都精确命中公开 `build_player_hit_packet` 错误接受完整无效 build，其他三条路径已经拒绝。

### GREEN

- 抽取内部 `build_player_hit_packet_checked_unvalidated`，仅负责 base 与 packet 的 checked 算术，不调用完整 build validation。
- `derive_player_build` 在完成字段、max、phase 检查后，用内部 helper 预演所有已知攻击 base packet。
- 公开 `build_player_hit_packet` 先调用完整 `valid_player_build`，通过后才调用内部 helper；`resolve_player_damage` 同样先调用完整 valid。
- GREEN 结果：`arpg_combat_tests` 构建成功；`combat.units` 99/99，0 failures；`git diff --check` 通过。

### 文件与调用图自审

- 修改：`src/combat/combat_world.cpp`、`tests/combat/player_build_tests.cpp`、`tests/combat/combat_test_main.cpp`。
- 调用图：公开 builder→valid→derive→内部 checked builder；resolver→valid→derive→内部 checked builder；constructor/apply→derive→内部 checked builder。
- 内部 checked builder 不调用 valid/derive/公开 builder，因此调用图无递归；正常 packet 算术、direct/ground、RNG 与伤害顺序未改。
- Minor：C4530 warning 未处理，保持既有记录。

# 可维护性重构准备报告（2026-07-23）

## 目标

本报告基于当前 `work` 分支通读 `src/`、`tests/`、`docs/superpowers/specs/` 与既有重构结果后整理，目的不是立刻改玩法，而是为下一轮行为保持型重构建立工程化边界：

1. 让代码更容易维护：职责清晰、依赖单向、行为由测试锁定。
2. 让每个模块更容易增加、删除或修改内容：新增内容进入明确目录，删除内容可通过边界测试和调用方证明安全。

## 当前模块地图

| 模块 | 当前职责 | 对外扩展点 | 维护风险 |
| --- | --- | --- | --- |
| `core` | 固定步、确定性随机、CRC、固定容量容器 | 只放跨两个以上领域复用且无游戏语义的基础设施 | 一旦混入玩法语义，会让所有上层模块隐式耦合。 |
| `abyss` | 深渊规则与奖励 | 深渊配置、奖励规则 | 已被 `combat`、`dungeon`、`persistence` 使用，接口需保持轻量。 |
| `progression` | 等级、经验与进度规则 | 等级曲线、奖励点数 | 适合保持纯规则模块，避免反向依赖角色或 UI。 |
| `modifiers` | EffectSet、Modifier 计算、玩家属性汇总 | 新增词缀维度、新增属性 | 是战斗、物品、被动共同语言，字段命名和默认值必须稳定。 |
| `skills` | 主动技能目录与技能装配 | 新技能、技能槽规则 | 适合目录驱动；运行时行为应继续由 `combat` 消费。 |
| `items` | 物品目录、生成、制作、材料、词缀 | 新物品、新配方、新材料 | 目录数据和算法已混在实现文件中，后续可先分离数据表与规则函数。 |
| `passives` | 被动星盘目录与规则 | 新节点、新连接、新奖励 | 与 `modifiers`/`progression` 强相关，适合保持纯规则和目录分层。 |
| `combat` | 战斗世界、攻击、AI、碰撞、技能运行、快照 | 新怪物、新攻击模式、新词缀运行逻辑 | 最大热点之一；应继续把怪物 AI、命中结算、快照输出拆成可替换小单元。 |
| `dungeon` | 房间生成、会话、转场、奖励、材料掉落、检查点 | 新房间规则、新遭遇、新奖励流 | `dungeon_session.cpp` 与 `dungeon_transition.cpp` 行数最大，生命周期和持久化提交边界需优先收敛。 |
| `persistence` | 存档路径、槽位、事务、恢复、checkpoint 编码 | 新存档字段、新迁移 | 存档字节兼容性高风险；任何改动都应先补黄金值或迁移测试。 |
| `platform/settings` | 窗口与输入设置 | 新设置项 | 应保持平台层配置，不反向泄漏进核心规则。 |
| `platform/raylib` | 输入、音频、渲染、HUD、运行时桥接 | 新 UI 面板、新音频路由、新表现效果 | 表现层文件多且易产生横向耦合；所有核心模块必须继续不包含 raylib。 |
| `app` | 可执行入口 | 启动参数、资源复制 | 只做组装，不承载业务规则。 |

## 依赖方向原则

下一轮重构建议坚持如下方向，新增模块或文件必须能放入其中一个层级：

```text
app
  -> platform/raylib, platform/settings
      -> dungeon, persistence
          -> combat, items, passives, skills, progression, abyss
              -> modifiers
                  -> core
```

允许例外必须写入对应设计文档并用架构测试保护。特别是：

- `core`、`combat`、`dungeon`、`items`、`passives`、`persistence` 不应包含 raylib 头。
- `persistence` 可以知道快照格式和迁移规则，但不应调用运行时战斗推进。
- `items`、`passives`、`skills` 应尽量输出规则数据，由 `combat` 或 `dungeon` 在运行时解释。
- `platform/raylib` 可以组合所有领域模块，但领域模块不能回调 UI 或音频。

## 新增或删除内容的模块规则

### 新增内容

| 需求类型 | 推荐落点 | 必须同步检查 |
| --- | --- | --- |
| 新怪物或 AI 分支 | `src/combat/monster_*`，必要时补目录文件 | 确定性测试、快照测试、对象池容量。 |
| 新攻击或技能运行逻辑 | `src/combat/attack_catalog.*`、`active_skill_runtime.*`、`src/skills/*` | 输入缓存、命中帧、冷却和回放测试。 |
| 新房间或遭遇规则 | `src/dungeon/room_*`、`encounter_*` | 同种子生成结果、转场事务、奖励结算测试。 |
| 新物品、材料或配方 | `src/items/*catalog*`、`item_generation.*`、`item_crafting.*` | 稀有度边界、配方失败路径、掉落与拾取集成。 |
| 新被动节点 | `src/passives/passive_tree_catalog.*` 与规则测试 | 花费、连接、重复分配和属性汇总。 |
| 新存档字段 | `src/persistence/checkpoint_codec.*` 与 checkpoint 类型 | 版本迁移、黄金 fixture、故障恢复。 |
| 新 HUD/音频/表现 | `src/platform/raylib/*_view.*`、`*_renderer.*`、`audio_*` | 平台测试、架构 guard、必要时截图验收。 |

### 删除或修改内容

删除前需要满足三类证据：

1. 引用证据：`rg` 确认没有生产调用方，或所有调用方已在同一提交迁移。
2. 行为证据：相关模块测试、黄金值或回放测试通过。
3. 边界证据：CMake 链接依赖和架构 guard 没有新增反向依赖。

不建议删除以下内容，除非单独开迁移任务：存档旧版本兼容、公开 checkpoint 字段、输入按键语义、固定容量热路径、`DummySnapshot` 这类仍由兼容计划约束的别名。

## 优先重构切片

### 1. Dungeon 会话生命周期切片

`dungeon_session.cpp` 和 `dungeon_transition.cpp` 是当前最大两个生产文件。建议先把它们拆成“输入意图处理、房间推进、战斗事件桥接、奖励提交、转场事务”五个内部文件或内部 helper，并保持 `DungeonSession` 作为唯一公开入口。

验收重点：相同种子房间、洞口/出口提交、死亡 checkpoint、奖励结算、材料掉落。

### 2. Checkpoint 编码切片

`checkpoint_codec.cpp` 当前承载大量版本读写细节。建议按“字段写入、字段读取、版本迁移、校验/错误映射”分区，先加局部 helper，不改变字节布局。

验收重点：现有黄金 fixture、v8 技能装配、死亡 checkpoint、旧存档迁移。

### 3. Items 数据与规则切片

`item_crafting.cpp`、`item_catalog.cpp`、`item_modifiers.cpp` 同时承担目录数据和规则逻辑。建议先把纯数据构造收拢到 catalog，规则函数保持无副作用，避免 UI 或 dungeon 直接依赖具体构造细节。

验收重点：配方成功/失败、词缀边界、生成权重、材料目录完整性。

### 4. Combat 运行时扩展切片

`combat_world.cpp` 仍是核心编排入口。新增战斗内容时优先扩展目录、AI 小文件、命中结算 helper，而不是在 `CombatWorld::tick` 风格入口继续追加条件分支。

验收重点：固定 tick、输入缓存、命中帧、怪物 affix、技能石、零分配路径。

## 建议的目录约定

- `*_types.hpp`：只放跨文件共享的值类型、枚举和轻量配置，不放大型算法。
- `*_catalog.*`：只负责稳定目录和查找，不直接推进运行时状态。
- `*_runtime.*` / `*_simulation.*`：负责有状态推进。
- `*_view.*`：生成与平台无关的视图模型或渲染计划。
- `*_renderer.*`：只做 raylib 绘制，不持有领域状态。
- `*_boundary_test.cmake` / `*_architecture_guard_test.cmake`：锁定依赖方向和禁止包含。

## 最小安全流程

每个重构 PR 保持一个主要切片，流程如下：

1. 记录待改文件行数、调用方和 CMake 依赖。
2. 先补或确认覆盖高风险行为的测试。
3. 迁移一个职责边界，避免跨模块同时重写。
4. 运行模块测试和平台架构 guard。
5. 用 `git diff --stat` 与 `rg` 检查是否混入玩法、输入、存档版本或依赖变化。
6. PR 描述中列出“不改变的行为”和“新增/删除内容的落点”。

## 本轮未改动生产行为

本报告仅新增重构准备文档，不修改 `src/` 生产代码、CMake 链接关系、资源或测试期望。后续可按上面的切片逐个提交行为保持型重构。

# Stage 11-B 暂停、设置与按键重绑定设计

## 1. 目标

Stage 11-B 在已验收的 Stage 11-A `918f2e9` 基线上，实现不改变战斗、地下城随机或角色存档语义的暂停与本机设置闭环。玩家仍然启动后直接进入唯一地下城，不增加主菜单；正常游戏中按 `Esc` 打开暂停层，并可调整音量、窗口模式、垂直同步和玩法按键。设置跨重启保存，HUD 按键提示始终来自当前绑定。

本阶段是完整 HUD 的前置任务卡。输入绑定先成为唯一权威来源，后续 HUD 重制不再依赖硬编码的 `WASD/J/K/L/E/I/P` 文本。

## 2. 范围

### 2.1 本阶段实现

- 正常游戏中的暂停、继续、设置和退出确认。
- 暂停时冻结固定步模拟，不累计恢复后的补帧。
- 主音效音量 `0%`～`100%`，步长 `5%`，默认 `100%`。
- 窗口化/全屏切换，默认窗口化。
- VSync 开关，默认开启。
- 十个玩法动作的键盘重绑定：上、下、左、右、轻击、跳跃、上挑、交互、背包、星盘。
- 冲突按键交换、恢复默认设置、应用与取消。
- 设置独立双槽持久化、版本和校验和、损坏恢复默认。
- 当前 HUD 操作提示从绑定生成，不进行完整 HUD 重制。
- Debug/Release 自动测试以及真实 raylib 暂停、设置、重绑和重启证据。

### 2.2 本阶段不实现

- 主菜单、角色选择、难度选择或安全营地。
- 手柄、鼠标侧键、组合键、按住/切换行为设置。
- 分辨率列表、渲染比例、画质档位、色盲模式或语言切换。
- 背景音乐；当前只有音效总音量。
- 地面物品过滤器、自动拾取规则或背包过滤扩展。
- 完整 HUD 重制、技能栏、小地图、任务栏或新美术。
- macOS 构建、全游戏性能重构或 Stage 11-C 以后内容。
- 战斗数值、攻击 tick、随机流、V6 角色存档布局或 Dungeon 状态机修改。

## 3. 方案选择

采用“设置与输入先行”方案，而不是先重制 HUD 或先做物品过滤器。原因是 HUD 必须展示实时绑定；如果先重制 HUD，接入重绑定时会再次修改所有按键提示。设置只属于平台层，不需要改 Combat、Dungeon、Items、Progression 或角色存档。

设置数据、编解码和存储放在无 raylib 的平台子模块；raylib 适配层只负责把稳定键标识映射为 `KeyboardKey`、采样设备、应用窗口状态和绘制界面。这样文件恢复与冲突规则可以无窗口测试，且核心模块继续禁止依赖 raylib。

## 4. 设置数据模型

### 4.1 动作

稳定动作枚举固定为：

1. `move_up`
2. `move_down`
3. `move_left`
4. `move_right`
5. `light_attack`
6. `jump`
7. `launcher`
8. `interact`
9. `inventory`
10. `passive_tree`

默认绑定依次为 `W/S/A/D/J/K/L/E/I/P`。动作 ID 和顺序属于设置文件格式，不使用 Combat 的瞬时输入枚举代替。

`Esc`、`Enter`、`F1`、`F12`、`V` 和窗口关闭不属于可重绑定动作：

- `Esc` 必须始终可取消捕获、返回、关闭覆盖层或打开暂停。
- `Enter` 保留给设置界面确认。
- `F1`、`F12`、`V` 保持调试和截图安全入口。

### 4.2 可绑定键

稳定键枚举只接受：

- `A`～`Z`；
- `0`～`9`；
- 上、下、左、右方向键；
- `Space`；
- 左/右 `Shift`；
- 左/右 `Ctrl`。

不保存 raylib 的整数常量。平台适配层维护稳定键与 raylib `KeyboardKey` 的完整双向表及显示名称。未知键、保留键、重复动作、重复键或枚举越界均使整个设置记录无效。

### 4.3 其他字段

`SettingsData` 包含：

- `std::uint8_t master_sfx_percent`，范围 `0..100` 且必须为 `5` 的倍数；
- `WindowMode window_mode`：`windowed` 或 `fullscreen`；
- `bool vsync_enabled`；
- 十项 `StableKey` 绑定；
- `std::uint64_t revision`，每次成功应用加一，溢出时拒绝保存并显示错误。

默认值为 `100%`、窗口化、VSync 开启、`W/S/A/D/J/K/L/E/I/P` 和 revision `0`。

## 5. 重绑定规则

进入按键捕获后，只处理下一次允许键的按下沿：

- 新键未被占用：直接替换当前动作的键。
- 新键已被另一个动作占用：交换两个动作的键。
- 保留键或不支持的键：保持捕获状态并显示原因。
- `Esc`：取消本次捕获，不改变草稿。
- 窗口失焦：取消捕获，防止恢复焦点时误绑定。

任何有效设置始终满足十个动作和十个键一一对应，不允许“未绑定”。恢复默认只修改设置草稿，用户仍需应用。

## 6. 暂停状态机与输入优先级

平台 UI 状态为：

```text
gameplay
  -> pause_root
      -> settings
          -> capture_binding
      -> quit_confirm
```

输入优先级固定为：

1. 恢复错误界面；
2. Stage 11-A 死亡保存/等待继续；
3. 已打开的背包或星盘；
4. 暂停/设置界面；
5. 正常玩法输入。

具体规则：

- 背包或星盘打开时按 `Esc` 只关闭该覆盖层，不同时打开暂停。
- 死亡和恢复界面保持 Stage 11-A 的 `Esc` 退出行为，不打开暂停。
- 正常玩法且没有 pending save 时，`Esc` 打开 `pause_root`。
- 存在 pending save 时不打开暂停；当前提交继续完成，玩家需要再次按 `Esc`。
- `pause_root` 中 `Esc` 或“继续”恢复游戏。
- `settings` 中 `Esc` 等价于取消并返回暂停根层。
- `capture_binding` 中 `Esc` 只取消捕获并返回设置列表。
- `quit_confirm` 中 `Esc` 返回暂停根层；确认后正常关闭 host。

暂停打开的首帧清除固定步累积器。暂停期间不调用 Session tick、不提交玩法动作、不采样玩法移动，也不让固定步累积；恢复后从新的真实帧时间开始，因此不会补跑暂停期间的 tick。渲染、窗口事件、截图和设置 UI 继续更新。

## 7. 运行时设置应用

设置界面维护 `committed` 与 `draft` 两份值：

- 音量、全屏和 VSync 在修改草稿时立即预览。
- 按键修改只影响草稿；应用成功后的下一帧成为输入和 HUD 权威。
- “取消”或 `Esc` 恢复进入设置前的 committed 音量、窗口模式和 VSync，并丢弃草稿。
- “应用”先校验草稿、生成 revision，再持久化；只有持久化成功才发布为 committed。
- 保存失败时恢复 committed 运行时状态，保留草稿和错误提示，允许重试。

全屏切换使用 raylib 6.0 的桌面全屏能力；退出全屏恢复 1280×720 的默认窗口尺寸和居中位置。本阶段不记忆任意窗口尺寸。VSync 使用 raylib 窗口状态 API 应用，应用后以平台状态读回结果；读回不一致视为应用失败并回滚 committed 状态。

音量只在设置发布或启动加载时调用 raylib 音量 API，渲染和固定 tick 热路径不重复设置。

## 8. 设置持久化

设置与角色 V6 存档完全分离。设置目录沿用 host 解析后的本机数据目录；测试传入独立 save directory 时，设置也写入该隔离目录。

使用两个固定槽 `settings-a.bin` 和 `settings-b.bin`：

- magic：8 字节 `ARPGSET1`；
- format：`1`；
- 固定 payload 长度；
- generation；
- `SettingsData` 字段；
- reserved zero；
- CRC32。

加载时解码两个槽并选择 generation 较新的合法记录；相同 generation 但内容不同视为冲突，回退默认。保存写入旧槽或无效槽对应的临时文件，刷新并原子替换该槽，随后读回验证。不能覆盖角色存档文件，也不能修改 V6 codec。

错误策略：

- 两槽都缺失：使用默认值，不显示错误。
- 一槽合法：使用合法槽，并在下次成功应用时修复冗余。
- 两槽损坏、版本未知、CRC/长度/reserved/枚举/绑定校验失败：使用默认值，暂停设置页显示一次“设置已恢复默认值”；游戏直接可玩，不进入角色存档恢复界面。
- 写入失败：现有合法槽保持可加载，当前 Apply 不发布。

设置 I/O 只发生在启动和 Apply，不进入 Combat/Dungeon/Render 热路径。

## 9. 输入桥与 HUD

现有硬编码采样改为：

```text
SettingsData.bindings
  -> raylib stable-key adapter
  -> per-frame physical key snapshot
  -> logical FrameInput
  -> death/inventory/passive/pause gates
  -> MovementInput and Combat actions
```

同一物理键每个渲染帧只采样一次。移动使用 down 状态，动作使用 pressed 边沿；固定步仍消费同一帧生成的逻辑输入，不新增一帧队列。设置层不得直接调用 Combat 或 Dungeon。

HUD 操作提示由纯函数生成固定容量文本，只有绑定 revision 变化时刷新缓存。提示至少覆盖移动、轻击、跳跃、上挑、交互、背包和星盘，不再包含硬编码 `WASD/J/K/L/E/I/P`。F1/F12/Esc 仍可显示固定安全键。

## 10. 模块边界

- 新的设置类型、验证、编解码和双槽 store 不依赖 raylib、Combat、Dungeon、Items 或角色 Persistence codec。
- raylib 键适配、窗口应用和设置绘制仅依赖设置平台模块及现有只读 DungeonSnapshot。
- Combat、Dungeon、Items、Progression、Passives 和角色 Persistence 不得包含设置、暂停或 raylib 类型。
- 设置文件不得影响任何命名随机流、room seed、death sequence、item sequence 或 commit generation。

## 11. 错误与边界

- revision 最大值时 Apply 拒绝，不回绕。
- 失焦、最小化和窗口关闭时不得留下捕获状态。
- 暂停层打开、关闭或切换设置时不得向玩法转发同帧按键。
- 同帧 `Esc` 与攻击/移动并发时暂停优先，玩法动作全部丢弃。
- 同帧设置 Apply 与窗口关闭时先完成或拒绝 Apply，再按最终 committed 状态退出。
- 全屏/VSync 应用失败不得写入设置槽。
- 设置损坏不得归档或覆盖角色存档。
- 所有 UI 在 1024×576、1280×720、1920×1080 内完整可见；窗口更小时使用 1024×576 的逻辑布局并裁剪世界，不裁剪设置关键按钮。

## 12. 测试与验收

### 12.1 无窗口测试

- 默认数据和十个稳定动作/键黄金值。
- 支持键完整映射、显示名、保留键拒绝和交换冲突。
- 设置验证覆盖音量步长、枚举、重复键、revision 和 reserved。
- format 1 固定字节布局、大小、CRC、截断、尾随和未知版本拒绝。
- 双槽选择、generation、单槽恢复、写入故障、读回故障和冲突回默认。
- 暂停状态机每条边、覆盖层优先级、同帧输入丢弃和失焦取消。
- 固定步冻结 600 个渲染帧后 tick/Combat/Dungeon snapshot 不变，恢复首帧无补帧。
- 输入绑定每帧单次采样、动作边沿、移动 down 状态和无新增延迟。
- HUD 提示与绑定 revision 一致且固定容量生成不分配。
- 1000 次设置 swap/apply/reload 后配置确定、角色 V6 存档 hash 不变。

### 12.2 真实 raylib 正式路径

正式验证必须启动生产 `run_raylib_host` 并生成 fresh 1280×720 证据：

1. 战斗中打开暂停，连续 120 presented frames 的逻辑 tick 与玩家/怪物状态保持不变。
2. 设置页显示 committed 值和十项绑定。
3. 将轻击从 `J` 改为一个未占用允许键，Apply 后旧键不攻击、新键通过生产输入链攻击，HUD 显示新键。
4. 将轻击绑定到已占用键，验证交换而非产生重复或未绑定。
5. 关闭并重启 host，设置、HUD 和实际输入保持一致。
6. 构造单槽损坏和双槽损坏，分别验证合法槽恢复与默认恢复，不影响同目录角色存档。

截图只能在 `EndDrawing()` 后走现有唯一捕获 helper。summary 必须记录 tick freeze、旧/新键动作计数、swap、revision、重启、slot recovery 和角色存档 hash。证据 guard 必须拒绝直接注入逻辑动作、直接改 committed settings、Present 前截图、绕过暂停 gate 继续 tick，以及通过测试专用 setter 修改生产状态。

### 12.3 完成门禁

- Debug 和 Release fresh configure、clean-first build、完整 CTest 全部通过。
- 新设置压力、正式 raylib 路径和 evidence mutation self-test 包含在全量 CTest 中。
- 两轮独立完整差异审查无 Critical、Important 或 Minor。
- 工作树不跟踪 build、截图、设置槽、角色存档或日志。
- README 和 Stage 11-B 验证报告准确记录按键、暂停、设置位置、文件恢复和未实现范围。

## 13. 停止边界

完成本设计后停止在 `codex/stage11b-settings-input` 工作树。不得开始完整 HUD、地面物品过滤、macOS、主菜单、手柄、分辨率/画质设置或其他 Stage 11 子任务，也不得合并 `main`，等待里程碑验收。

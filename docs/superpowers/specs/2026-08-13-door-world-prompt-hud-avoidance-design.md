# 门世界提示避让 HUD 设计

**日期：** 2026-08-13
**状态：** 已确认（依据用户“无需确认，采取推荐方案”的持续授权）

## 背景与根因

下方房门固定在房间最远端。玩家位于房间中部时，该门投影远在屏幕下方；现有 `world_prompt_layout` 会把任意提示背板钳回 viewport，因此离屏门的“水流”标签被钳到屏幕底部中央。主动技能 HUD 随后绘制，但空槽没有名称背板，于是“水流”从第三个空槽名称区域露出，看起来像技能名。

这不是主动技能 ViewModel 生成错误，也不是简单的绘制顺序错误。根因是门标签缺少“来源是否可见”的门禁，并且通用世界提示布局不知道主动技能 HUD 的保留区域。

## 目标

- 离屏房门不得通过 viewport clamp 产生屏幕边缘标签。
- 可见房门的标签必须保留；若其背板会与主动技能 HUD 相交，则整体移动到 HUD 上方。
- 800×450、1280×720、1920×1080 均不得发生门标签与主动技能 HUD 相交。
- 保持门主题文案、箭头、输入、地图、存档、技能栏位置和绘制顺序不变。
- 洞口状态、洞口交互提示和深渊标题继续使用现有通用世界提示路径，不应用门专属剔除。
- 布局为纯计算、固定大小、`noexcept`，逐帧零动态分配。

## 非目标

- 不重排整个 HUD。
- 不把房门标签改成小地图或边缘指示器。
- 不隐藏处于可见范围内的房门提示。
- 不修改“水流”等中文术语。
- 不扩展到怪物标签、地面掉落、洞口提示或调试覆盖层。

## 方案比较

### 方案 A：只剔除离屏门

当投影房门矩形与 viewport 无正面积交集时不画标签。它能修复当前截图，但门刚进入屏幕、标签仍位于底部时，标签仍可能与技能栏相交。

### 方案 B：只增加底部安全线

给门标签统一设置 `safe_bottom`。实现较短，但会移动本来位于技能栏横向范围之外的侧门提示，也不能准确表达真实 HUD 形状。

### 方案 C：来源可见性门 + 矩形 blocker（采用）

先要求投影门矩形与 viewport 有正面积交集，再对门标签的通用布局结果与主动技能 HUD 完整视觉包围盒做二维相交检查。只有实际相交时才把标签垂直移动到 blocker 上方。该方案同时消除离屏假标签和近门碰撞，且不影响其它世界提示。

## 数据与接口

### 主动技能 HUD 完整视觉边界

`ActiveSkillHudLayout` 新增 `visual_bounds`，它是五个槽位各自 `text_regions[index].visual_bounds` 的并集，包含：

- 键帽区；
- 槽位图形；
- 冷却文字区；
- 技能名称区。

布局无效时 `visual_bounds` 保持空矩形。现有 `bounds` 仍只表示五个槽位本体，不改变原语义。

### 门专属提示布局

在 `dungeon_view_math.hpp/.cpp` 增加纯函数：

```cpp
[[nodiscard]] WorldPromptLayout door_world_prompt_layout(
    Vector2 anchor,
    Vector2 measured,
    Rectangle door_bounds,
    Rectangle hud_blocker,
    Vector2 screen_offset,
    float screen_width,
    float screen_height) noexcept;
```

契约：

1. `anchor`、`measured`、`door_bounds`、`screen_offset` 或 viewport 含非有限值，viewport 非正，或门矩形无面积时返回不可见布局。
2. `hud_blocker` 的唯一“无 blocker”表示是所有分量有限且宽/高非正的空矩形；含 NaN/Inf 的 blocker 必须 fail-closed。
3. 将 `door_bounds` 和 `anchor` 加上本帧 `screen_offset` 后再判断最终屏幕空间。最终门矩形与 viewport 无正面积交集时返回不可见布局。
4. 对最终屏幕 anchor 调用既有 `world_prompt_layout`，保留其字号缩放、背板尺寸、文字居中和屏内钳制规则。
5. 有限空 blocker 时退化为只带来源可见性门的通用布局；有效 blocker 与结果背板不相交时原样保留该最终屏幕布局。
6. blocker 在四边扩展安全间距后参与碰撞。安全间距为 `12 * ui_viewport_scale` 像素，覆盖当前最大 9 像素战斗镜头震动并保留至少 3 像素余量。
7. 相交时只调整最终屏幕 Y：将提示背板底边放到扩展 blocker 的上边缘，保持 X、字号、背板尺寸和文字居中不变。
8. 上移后若不能完整留在 viewport、仍与扩展 blocker 相交或数值无效，则 fail-closed 为不可见。
9. 返回前从背板和文字位置减去 `screen_offset`，使返回值仍是 `BeginMode2D` 内的绘制坐标；raylib 应用同一个 offset 后，最终屏幕矩形与上述验证完全一致。

### 渲染接线

`room_renderer.cpp` 在 `draw_doors` 每次调用时：

1. 用当前 viewport 计算一次 `ActiveSkillHudLayout`；
2. `CombatRenderer` 把本帧真实 `camera_offset` 作为 `Vector2` 沿 `draw_room -> draw_doors` 传入；该参数只供门提示做最终屏幕几何计算，不重复应用到实际绘制；
3. 保存门材质绘制的布尔结果。本地纯 helper 从 `default_material_manifest()` 查找 `visual.sprite`，使用与 `MaterialPack::draw` 相同的 source 宽高、foot anchor、flip 和 `0.72 * projected.scale` 算出真实 destination；材质成功绘制时以该 destination 为 `door_bounds`，fallback 时以 82×70 的既有 `frame` 为 `door_bounds`；
4. `draw_world_prompt` 在收到门矩形、HUD blocker 和 screen offset 时选择 `door_world_prompt_layout`，未收到时继续选择通用 `world_prompt_layout`；
5. 门调用传入本帧实际绘制边界与 `active_skill_layout.visual_bounds`；
6. 洞口、洞口交互、深渊标题不传门参数，因此行为不变。

使用完整材质 destination 是保守且与真实 draw 同源的可见性边界：即使 destination 内含透明边缘，也只会让标签略早出现，不会在门材质已经可见时错误隐藏标签。helper 找不到 manifest frame 或算出无效 destination 时 fail-closed 使用 fallback `frame`，不猜测常量。

重复计算同一个纯 `active_skill_hud_layout(width, height)` 是有意的窄接线：世界层和稍后的 HUD 层使用同一函数与同一 viewport，几何不会分叉，也无需把 HUD 状态沿 `CombatRenderer -> draw_room -> draw_doors` 扩散。

## 绘制与安全顺序

- 门的图形、箭头和装饰照常绘制。
- 仅标签在字体有效、文本有效且门专属布局可见时绘制。
- 背板仍先于文字绘制，颜色和共享 CJK 字体路径不变。
- 通用 `world_prompt_layout` 不修改，避免洞口与深渊提示回归。
- 现有 room → actors → HUD 绘制阶段不修改。

## 错误处理

- NaN、Inf、负尺寸、空 viewport、无面积来源矩形：不可见。
- 有限空 blocker：仍执行来源可见性门；有效的门提示退化为现有通用布局。含 NaN/Inf 的 blocker：不可见。
- 主动技能布局无效时传入有限空 blocker，不访问无效 HUD 几何。
- 字体未就绪：沿用现有 renderer fail-closed，不加载或回退第二份字体。
- 没有足够空间移到 blocker 上方：不可见，而不是覆盖 HUD 或写出 viewport。

## TDD 与最小验证

不新增平台 `TestCase`，在既有布局 case 内追加断言：

1. **RED：离屏门** — 800/1280/1920 中，远在屏下的门矩形返回不可见；旧代码缺少 API/会把标签钳到底边。
2. **RED：真实材质可见边界** — 构造 256×256、foot anchor 128×244、draw scale `0.72 * projected.scale` 的门 destination：材质上缘已进入 viewport、82×70 fallback frame 仍完全离屏时，标签必须可见；另覆盖材质绘制失败时只采用 fallback frame。
3. **RED：可见门避让** — 构造与主动技能 `visual_bounds` 相交的门提示，要求结果可见、屏内、文字仍居中、与 HUD blocker 保持安全间距。
4. **最终屏幕震动矩阵** — 对每个支持分辨率和 `screen_offset` 四个极值 `{±9, ±9}`，把返回 backing/text position 加回 offset，断言最终矩形位于 viewport、与未扩展真实 HUD blocker 不相交且文字仍居中；另覆盖预震动门矩形离屏但加 offset 后入屏的可见性边界。
5. **可见且不碰撞** — 上方/侧方提示的最终屏幕布局与通用布局完全一致。
6. **完整 HUD 包围盒** — `visual_bounds` 精确覆盖五槽 key/slot/cooldown/name 区且位于 viewport 内。
7. **极端输入与 10,000 次探针** — anchor/measured/door/offset/viewport 或 blocker 的 NaN/Inf fail-closed；有限空 blocker 正常退化；纯布局循环分配计数不变。
8. **真实接线守卫** — 扩展现有房间提示 source guard，要求门路径实际消费真实材质/fallback 边界、`camera_offset`、`door_world_prompt_layout` 和主动技能 `visual_bounds`，同时证明洞口两条调用仍走通用路径；加入可逆 mutation self-test。

遵守“尽量不测试”：实现阶段只运行平台测试目标的必要增量构建、单个相关 selector（优先 CPLAY030；若该入口不覆盖修改的现有 case，则使用 MENU/HUD 183）以及房间提示 direct guard/self-test。不运行 full、formal、`scripts/Test.ps1` 或 Release 窗口。

## 验收标准

- 三个支持分辨率下，离屏下门不再出现“水流”底边标签。
- 门材质 destination 进入屏幕后标签仍可见；其最终屏幕背板与主动技能完整视觉边界无交叠，并通过 `camera_offset` 四个 ±9 像素极值。
- 空槽仍是 key-only；技能文字背板功能无改动。
- 洞口状态、动态交互键提示与深渊标题布局无变化。
- 所有新增纯计算路径零动态分配，工作树只含本问题范围内文件。

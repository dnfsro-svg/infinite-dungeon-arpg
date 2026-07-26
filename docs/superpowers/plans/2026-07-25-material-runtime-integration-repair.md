# 材质运行时完整接入修复 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 让已部署的怪物、四元素门、两个主动技能、环境物件和 UI 材质真正进入正式 raylib 游戏路径，同时修正 NumPad 技能输入，并保持玩法规则不变。

**Architecture:** 先以固定容量位掩码把 `MaterialPack` 从“公共 + 单一生态”升级为按不可变 `DungeonSnapshot` 选择的动态 residency；随后让门、怪物、技能、环境和死亡界面都通过同一颜色图/材质图绘制边界消费资源。所有图像处理在确定性离线脚本完成，运行时只做请求差异同步、帧选择和绘制；每个对象保留局部程序回退，不让单张坏图拖垮整个房间。

**Tech Stack:** C++17、raylib 6.0.0、CMake 3.25、MSVC 19.44、Windows SDK 10.0.26100.0、CTest、Python 3 + Pillow。

## Global Constraints

- 设计基线是 `docs/superpowers/specs/2026-07-25-material-runtime-integration-repair-design.md`；本计划不得改写其中已确认的范围。
- 不改变伤害、AI、碰撞、掉落、房间生成、技能石装卸、冷却、存档或 J/K/L/WASD 语义。
- 攻击预警线、危险区精确边界、动态数字和全屏暗幕继续程序绘制；只有其字体或承载面板进入统一 UI 路径。
- 不删除旧资源；资源失败时只回退对应对象，并只记录一次包含 atlas ID 与两条路径的警告。
- 峰值纹理显存不得超过 `256 * 1024 * 1024 = 268435456` 字节；预算按颜色图和材质图两份计算，也要校验切换时 `resident | requested` 的过渡峰值。
- residency 请求、动画帧表、门几何和环境布局使用固定容量结构；绘制热路径不得新增堆分配、磁盘查询或图像处理。
- 请求内容未变化时不得调用纹理 load/unload；请求变化时先加载新增 atlas，完成尝试后才卸载移除 atlas。
- 拔刀斩 90 tick 使用现有全部 36 帧；暴风式 360 tick 使用现有全部 24 帧，但不得宣称已满足 96 张独立角色帧。
- 火怪只接入当前原创图集中真实存在的姿势；不得用重复或插值帧虚报 12/16/20/8/16 完整动作板。
- 五个主动技能槽只绑定 `KEY_KP_1`～`KEY_KP_5` 和 Win32 `VK_NUMPAD1`～`VK_NUMPAD5`；主键盘 `1`～`5` 不触发技能。
- 真实窗口验收覆盖 800×450、1280×720、1920×1080；30 怪物加暴风式目标平均 60 FPS、1% low ≥45 FPS。
- 编译和 CTest 使用 `--parallel 1` / `-j 1`；真实窗口、截图和性能采样不与编译并发，不为每个小改动重复全量 109 项测试。
- 所有任务遵循 RED → GREEN → 定向回归 → 独立提交；保留并绕开主控 `E:\game` 的未提交文件。

---

## File responsibility map

| Area | Files | Responsibility |
|---|---|---|
| Residency 请求 | `src/platform/raylib/material_residency.hpp/.cpp` | 用 64 位固定掩码把公共资源、当前房间、可绘制怪物和已装备技能投影成请求；计算请求与过渡显存。 |
| 纹理生命周期 | `src/platform/raylib/material_pack.hpp/.cpp` | 仅在请求变化时同步差异，先加载后卸载，隔离单 atlas 失败并暴露运行时诊断。 |
| Manifest/校验 | `material_asset_types.hpp`, `material_manifest.hpp`, `material_asset_validation.*` | 注册公共门和技能 atlas、独立环境帧、预算和颜色/材质配对约束。 |
| 门 | `dungeon_view_math.*`, `combat_renderer.*`, `room_renderer.cpp` | 按方向元素选门；绘制暗化关闭态、小锁、矢量箭头和高清标签。 |
| 怪物 | `material_animation.*`, `monster_material_presenter.*`, `actor_renderer.cpp` | 异生态仍驻留；六种完整动画和两种火怪显式关键姿势都走材质帧。 |
| 技能 | `active_skill_assets.*`, `active_skill_view.*`, `active_skill_renderer.*`, `actor_renderer.cpp`, `combat_renderer.*` | 技能颜色/材质配对、完整现有帧映射、玩家/效果中心分离、基础玩家抑制和专属图标。 |
| 环境 | `*_room_material_slice.*`, `environment_prop_layout.*`, `room_renderer.cpp` | 从独立透明单元读取 bbox/锚点/比例并在三种分辨率安全布局。 |
| UI/文字 | `death_overlay_view.*`, `death_overlay_renderer.*`, `actor_renderer.cpp`, `hud_font.*` | 死亡九宫格与标题板；伤害数字和 `DEFEATED` 统一走高清 HUD 字体。 |
| 输入 | `host_input.cpp`, `raylib_input.*` | raylib 小键盘采样及 Win32 虚拟键映射。 |
| 离线资源 | `tools/build_environment_props.py`, `tools/build_active_skill_material_maps.py`, `assets/stage12/*`, `assets/skills/*` | 确定性生成独立门/环境物件和技能材质图，运行时不加工图片。 |
| 验收/打包 | `tests/platform/*`, `tools/package_material_pack_v2.py`, `docs/validation/material-runtime-integration-repair.md` | 单元、资产、真实窗口、显存、帧覆盖、性能和部署一致性证据。 |

---

### Task 1: 动态材质常驻请求与差异同步

**Files:**
- Create: `src/platform/raylib/material_residency.hpp`
- Create: `src/platform/raylib/material_residency.cpp`
- Modify: `src/platform/raylib/material_pack.hpp`
- Modify: `src/platform/raylib/material_pack.cpp`
- Modify: `src/platform/raylib/material_asset_validation.hpp`
- Modify: `src/platform/raylib/material_asset_validation.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/material_asset_validation_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces `MaterialAtlasMask`, `MaterialResidencyRequest`, `base_material_residency_request()`, `make_material_residency_request(const dungeon::DungeonSnapshot&)`, `material_residency_bytes(...)` and `MaterialPack::synchronize_residency(...)`.
- Task 2 adds the public door atlas to the base request; Task 5 adds equipped skill atlases and atomically removes the direct skill texture loader.

- [ ] **Step 1: Write the RED tests for request composition and fixed capacity.**

Add cases to `material_asset_validation_tests.cpp` with the following assertions:

```cpp
static_assert(static_cast<std::size_t>(MaterialAtlasId::count) <= 64U);

dungeon::DungeonSnapshot snapshot{};
snapshot.has_active_room = true;
snapshot.ecology = dungeon::DungeonElement::fire;
snapshot.combat.emplace();
snapshot.combat->monster_count = 1U;
snapshot.combat->monsters[0].active = true;
snapshot.combat->monsters[0].id = combat::MonsterId::chaos_chaser;
snapshot.combat->monsters[0].hp = 1;

const auto request = make_material_residency_request(snapshot);
ARPG_REQUIRE(request.contains(MaterialAtlasId::fire_environment));
ARPG_REQUIRE(request.contains(MaterialAtlasId::fire_room_background));
ARPG_REQUIRE(request.contains(MaterialAtlasId::chaos_chaser));
ARPG_REQUIRE(!request.contains(MaterialAtlasId::chaos_environment));
ARPG_REQUIRE(material_residency_bytes(default_material_manifest(), request)
    <= 256U * 1024U * 1024U);
```

Also assert an active monster with `hp == 0` remains requested while its snapshot is drawable, and an inactive slot is not requested.

- [ ] **Step 2: Write the RED tests for synchronization semantics.**

Expand the fake texture API so texture handles and call history are separate fixed arrays; call history capacity is `MaterialAtlasId::count * 8U`. Add cases that assert:

```cpp
ARPG_REQUIRE(pack.synchronize_residency(request));
const auto loads = fake.load_count;
const auto unloads = fake.unload_count;
ARPG_REQUIRE(pack.synchronize_residency(request));
ARPG_REQUIRE(fake.load_count == loads);
ARPG_REQUIRE(fake.unload_count == unloads);
```

Inject a failure for the chaos-chaser material map and require the fire background remains available while only `chaos_chaser` is unavailable. Then request a different room and require every new load operation appears before the first unload operation. Finally request every atlas and require an over-budget transition fails without any load/unload call.

After one successful synchronization, run 10,000 identical `synchronize_residency(request)` calls inside the existing allocation probe and require zero heap allocations as well as unchanged load/unload counters.

- [ ] **Step 3: Run the RED build.**

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
```

Expected: compilation fails only because `material_residency.hpp` and `synchronize_residency` do not exist.

- [ ] **Step 4: Implement the fixed-capacity request.**

Use this public shape in `material_residency.hpp`:

```cpp
using MaterialAtlasMask = std::uint64_t;
static_assert(static_cast<std::size_t>(MaterialAtlasId::count) <= 64U);

struct MaterialResidencyRequest final {
    MaterialAtlasMask atlases{};
    void require(MaterialAtlasId id) noexcept;
    [[nodiscard]] bool contains(MaterialAtlasId id) const noexcept;
    friend bool operator==(MaterialResidencyRequest lhs,
        MaterialResidencyRequest rhs) noexcept {
        return lhs.atlases == rhs.atlases;
    }
    friend bool operator!=(MaterialResidencyRequest lhs,
        MaterialResidencyRequest rhs) noexcept { return !(lhs == rhs); }
};

[[nodiscard]] MaterialResidencyRequest
base_material_residency_request() noexcept;
[[nodiscard]] MaterialResidencyRequest make_material_residency_request(
    const dungeon::DungeonSnapshot& snapshot) noexcept;
[[nodiscard]] std::size_t material_residency_bytes(
    const MaterialManifestDefinition& manifest,
    MaterialResidencyRequest request) noexcept;
```

The base mask contains `environment`, `actors`, `effects_ui`, all five player atlases, `items_ui`, and `ui_material`. Map room ecology to exactly one environment and one room-background atlas; map each active monster ID to exactly its monster atlas. Use saturating addition and count `rgba_bytes * 2U` per atlas.

- [ ] **Step 5: Implement transactional residency synchronization.**

Add to `MaterialPack`:

```cpp
[[nodiscard]] bool synchronize_residency(
    MaterialResidencyRequest request) noexcept;
[[nodiscard]] bool residency_satisfied(
    MaterialResidencyRequest request) const noexcept;
[[nodiscard]] MaterialResidencyRequest requested_residency() const noexcept;
[[nodiscard]] MaterialAtlasMask resident_atlases() const noexcept;
[[nodiscard]] std::size_t resident_bytes() const noexcept;
```

Return before manifest validation when `request == requested_residency_`, returning `residency_satisfied(request)`; an unchanged previously failed request therefore returns `false` without retrying I/O. Reject a target or `resident | requested` transition above `memory_budget_bytes`. Initialize the shader once, load each missing color/material pair into local handles, commit successful pairs, then unload removed pairs. A failed half-pair is immediately cleaned; other successful pairs remain resident. Emit at most one warning per atlas and include its numeric ID, color path and material path. Store the failed request so the next identical frame performs no I/O. Keep `load(MaterialEcology)` only as a compatibility wrapper that constructs base + every manifest atlas tagged with that ecology; `ecology_ready()` retains the same compatibility meaning but production rendering decisions use `residency_satisfied` or per-atlas availability. Production `CombatRenderer::draw()` calls:

```cpp
static_cast<void>(material_pack_.synchronize_residency(
    make_material_residency_request(current)));
```

`initialize_resources()` synchronizes only `base_material_residency_request()`. `draw_room_background_only()` constructs base + the requested room environment/background rather than mutating a single `current_ecology_` state.

- [ ] **Step 6: Update validation and run GREEN tests.**

Replace the old “common + maximum one ecology” peak calculation with `material_residency_bytes`. The legal worst request is base + all eight monster atlases + the largest room environment/background; Tasks 2 and 5 extend the same case with doors and two equipped skills.

Register exactly four new C++ cases from Steps 1–2 and change the platform expected case count from 443 to 447.

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: request composition, no-op synchronization, failure isolation, load-before-unload, budget rejection and existing platform cases all pass.

- [ ] **Step 7: Commit.**

```powershell
git add src/platform/raylib/material_residency.* src/platform/raylib/material_pack.* src/platform/raylib/material_asset_validation.* src/platform/raylib/combat_renderer.cpp src/platform/raylib/CMakeLists.txt tests/platform/material_asset_validation_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: add selective material residency"
```

### Task 2: 生成公共四元素门与独立环境物件

**Files:**
- Create: `tools/build_environment_props.py`
- Create: `tests/platform/environment_prop_asset_pipeline_tests.py`
- Create: `assets/stage12/element_doors.png`
- Create: `assets/stage12/element_doors_material.png`
- Create: `assets/stage12/environment-props-build.json`
- Modify: `assets/stage12/water_environment.png`
- Modify: `assets/stage12/water_environment_material.png`
- Modify: `assets/stage12/lightning_environment.png`
- Modify: `assets/stage12/lightning_environment_material.png`
- Modify: `assets/stage12/chaos_environment.png`
- Modify: `assets/stage12/chaos_environment_material.png`
- Modify: `tools/build_water_material_slice.py`
- Modify: `tools/build_lightning_material_slice.py`
- Modify: `tools/build_chaos_material_slice.py`
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/material_residency.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/ecology_material_coverage_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `assets/stage12/README.md`
- Modify: `assets/stage12/environment-source.md`
- Modify: `assets/stage12/water-source.md`
- Modify: `assets/stage12/lightning-source.md`
- Modify: `assets/stage12/chaos-source.md`

**Interfaces:**
- Consumes `MaterialResidencyRequest::require` from Task 1.
- Produces `MaterialAtlasId::element_doors`; existing `environment_door_fire/water/lightning/chaos` sprites move to it without changing sprite IDs.
- Produces deterministic `build_element_doors(root)`, `build_ecology_environment(ecology, root)` and `validate_outputs(root)` functions.

- [ ] **Step 1: Write the RED Python asset tests.**

Register `stage12.environment_prop_asset_pipeline` and assert:

```python
self.assertEqual(Image.open(color).size, Image.open(material).size)
self.assertEqual(Image.open(element_doors).size, (1024, 256))
self.assertEqual(color.getchannel("A").tobytes(),
                 material.getchannel("A").tobytes())
for cell in range(4):
    tile = element_doors.crop((cell * 256, 0, (cell + 1) * 256, 256))
    bbox = tile.getchannel("A").getbbox()
    self.assertIsNotNone(bbox)
    self.assertGreaterEqual(min(bbox[0], bbox[1], 256 - bbox[2], 256 - bbox[3]), 12)
self.assertEqual(len({cell.tobytes() for cell in door_cells}), 4)
```

For each water/lightning/chaos object cell, require a nonempty alpha bbox, at least 8 transparent pixels on every edge, matching material alpha, one dominant connected component, and the manifest source rectangle/anchor from `environment-props-build.json`.

- [ ] **Step 2: Run the RED asset test.**

```powershell
python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
```

Expected: failure because the shared builder, public door atlas and build manifest do not exist.

- [ ] **Step 3: Implement the deterministic exporter.**

`build_element_doors()` takes the authored fire door from `(768, 0, 1024, 256)` of `fire_environment.png` and the water/lightning/chaos doors from `(512, 0, 768, 256)` of their current atlases. For every source, retain the largest connected foreground component, crop its alpha bbox, add 12 transparent pixels, aspect-fit it into a 256×256 cell, align its foot to y=244, and preserve alpha.

`build_ecology_environment()` keeps the 512×512 room field at `(0,0)` and repacks exactly five isolated cells:

```python
LAYOUT = {
    "wall": (512, 0),
    "surface_prop": (512, 256),
    "hole": (0, 512),
    "light": (256, 512),
    "solid_prop": (512, 512),
}
```

Use the authored `art_source/stage12/backgrounds/<ecology>/<ecology>-wall-tile-v1.png` for `wall`; use the existing per-ecology concept crop coordinates already present in the three old builders for the remaining four objects. Convert border-connected background to transparency with a fixed RGB distance threshold of 28, keep the largest subject component plus components at least 2% of its area, feather alpha by 2 pixels, crop, pad by 8 pixels and aspect-fit without stretching. If the output subject still touches the safe margin or the second unrelated component exceeds 2%, raise `RuntimeError` and do not overwrite output files.

Generate material maps with channels `R=roughness`, `G=emissive`, `B=metalness`, `A=color alpha`; write all outputs to temporary siblings, validate them, then atomically replace the committed PNGs and JSON. Make the three existing ecology scripts call this shared builder for their environment stage so they cannot overwrite it with the old collage path.

- [ ] **Step 4: Register manifest and residency metadata.**

Append `MaterialAtlasId::element_doors` immediately before `count`, register it as common `1024×256` with `1'048'576U` bytes per RGBA image, move all four door frames to x offsets `0,256,512,768`, source size `256×256`, foot anchor `{128,244}`, and add the atlas to `base_material_residency_request()`.

Update water/lightning/chaos frame records to the five-cell layout emitted in JSON. Use each object’s emitted foot anchor and alpha bbox; do not reuse `{128,246}` for every object. Extend the budget test to include `element_doors`.

Add exactly two C++ cases to `ecology_material_coverage_tests.cpp`: one verifies four unique common-door frames, the other cross-checks all emitted prop records against manifest bounds. Change the platform expected case count from 447 to 449.

- [ ] **Step 5: Generate, validate and run platform metadata tests.**

```powershell
python tools/build_environment_props.py --root .
python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-debug -R '^stage12\.environment_prop_asset_pipeline$' --output-on-failure -j 1
```

Expected: byte-for-byte repeat generation, four unique door cells, isolated props, manifest bounds and the updated residency budget pass.

- [ ] **Step 6: Commit.**

```powershell
git add tools/build_environment_props.py tools/build_water_material_slice.py tools/build_lightning_material_slice.py tools/build_chaos_material_slice.py tests/platform/environment_prop_asset_pipeline_tests.py tests/platform/CMakeLists.txt tests/platform/ecology_material_coverage_tests.cpp tests/platform/platform_test_main.cpp src/platform/raylib/material_asset_types.hpp src/platform/raylib/material_manifest.hpp src/platform/raylib/material_residency.cpp assets/stage12/element_doors.png assets/stage12/element_doors_material.png assets/stage12/environment-props-build.json assets/stage12/water_environment.png assets/stage12/water_environment_material.png assets/stage12/lightning_environment.png assets/stage12/lightning_environment_material.png assets/stage12/chaos_environment.png assets/stage12/chaos_environment_material.png assets/stage12/README.md assets/stage12/environment-source.md assets/stage12/water-source.md assets/stage12/lightning-source.md assets/stage12/chaos-source.md
git commit -m "feat: export isolated environment materials"
```

### Task 3: 四方向门运行时绘制

**Files:**
- Modify: `src/platform/raylib/dungeon_view_math.hpp`
- Modify: `src/platform/raylib/dungeon_view_math.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `tests/platform/dungeon_view_math_tests.cpp`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes the four door sprite IDs now backed by `MaterialAtlasId::element_doors`.
- Produces `DoorRenderDecision` with sprite/tint/lock state and `door_arrow_geometry(...)` without any text glyph arrow.

- [ ] **Step 1: Write RED tests for direction, state and arrow geometry.**

Use this contract:

```cpp
struct DoorRenderDecision final {
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    const char* label{};
    Rgba8 body_tint{};
    Rgba8 text{};
    bool draw_lock_marker{};
};

struct DoorArrowGeometry final {
    Vector2 tail{};
    Vector2 tip{};
    Vector2 head_left{};
    Vector2 head_right{};
    float thickness{};
};

[[nodiscard]] DoorArrowGeometry door_arrow_geometry(
    dungeon::ExitDirection direction, float center_x, float center_y,
    float scale) noexcept;
```

Assert up/down/left/right return four distinct door sprites matching `door_theme(direction).element`; closed tint alpha remains 255 with RGB dimmed rather than replaced; only closed mode draws a lock marker. For each direction, assert the arrow tip points outward and every point stays inside a 48×48 scaled box.

- [ ] **Step 2: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
```

Expected: compile failure because `DoorRenderDecision::sprite` and `door_arrow_geometry` are absent.

- [ ] **Step 3: Implement the pure door plan.**

`door_render_decision(mode, direction)` must select `select_door_sprite(door_theme(direction).element)`. Use closed tint `{150,150,150,255}`, open tint `{255,255,255,255}`, and text colors already defined by the door theme. Remove `arrow`, `locked_interior`, and `draw_locked_interior` fields. Generate a shaft plus two head segments in `door_arrow_geometry`; no UTF-8 arrow string enters the draw path.

- [ ] **Step 4: Replace the red overlay and glyph path.**

Remove the aggregate `draw_material_environment` argument from `draw_doors`. For each direction, attempt `visual.sprite` through `MaterialPack`; on failure draw only that door’s existing outline fallback. Tint closed doors through `body_tint`, draw a small 12×14 scaled dark-steel lock body plus 8-pixel arc at the upper-right of the door, and draw the three arrow segments with `DrawLineEx`. Open doors add only two low-alpha element-colored edge lines. Keep labels on `hud_font` via `MeasureTextEx` and `draw_crisp_ui_text`.

Remove door readiness from `can_draw_room_environment()` so a missing common door atlas cannot disable a healthy room background and prop set. The tests require all four directional sprites to be drawable when `element_doors` is healthy and require the background decision to remain material-backed when that atlas is unavailable.

Register exactly three new cases from Step 1 and change the platform expected case count from 449 to 452.

- [ ] **Step 5: Run GREEN tests.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: four-way selection, arrow geometry, closed/open plans and renderer source guards pass; no `DrawRectangleRec` locked interior and no `MeasureText(visual.arrow` remain.

- [ ] **Step 6: Commit.**

```powershell
git add src/platform/raylib/dungeon_view_math.* src/platform/raylib/combat_renderer.* src/platform/raylib/room_renderer.cpp tests/platform/dungeon_view_math_tests.cpp tests/platform/stage12_environment_render_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: render directional element doors"
```

### Task 4: 异生态怪物与火怪显式姿势

**Files:**
- Modify: `src/platform/raylib/material_animation.hpp`
- Modify: `src/platform/raylib/material_animation.cpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/monster_material_presenter.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `tests/platform/material_animation_tests.cpp`
- Modify: `tests/platform/stage12_actor_render_tests.cpp`
- Modify: `tests/platform/fire_room_material_slice_tests.cpp`
- Modify: `tests/platform/ecology_material_coverage_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes dynamic monster-atlas residency from Task 1.
- Produces phase-aware `MonsterAnimationState` and optional explicit frame sequences for nonuniform fire atlases.

- [ ] **Step 1: Write RED coverage for every monster and fire pose.**

Expand `MonsterAnimationState` test expectations to `idle`, `move`, `telegraph`, `active`, `recovery`, `cooldown`, `hurt`, `death`. For all eight monster IDs, require a non-null clip and in-bounds frame for each drawable phase. Assert fire-bomber frame cells `{0,1,2,3,4,5,6,7,8,9,10,11}` are reachable and fire-charger cells `{0,1,2,3,4,5,6,8,9,10,11}` are reachable; charger cell 7 is transparent and must never be returned.

Assert an active chaos chaser in a fire snapshot produces `use_material_frame == true` when its atlas is available and does not reach `draw_monster_silhouette` on the healthy path.

- [ ] **Step 2: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: fire clips are absent because `has_complete_material_animation` excludes both fire monsters.

- [ ] **Step 3: Add an explicit-frame clip representation.**

Extend the clip definition without changing the existing 96×96 ecology sheets:

```cpp
struct ExplicitMonsterAnimationFrame final {
    Rectangle source{};
    Vector2 foot_anchor{};
    std::uint8_t duration_ticks{1U};
    std::uint8_t key_pose_index{};
};

struct MonsterAnimationClipDefinition final {
    combat::MonsterId monster{};
    MonsterAnimationState state{};
    MaterialAtlasId atlas{};
    std::uint16_t first_cell{};
    std::uint16_t frame_count{};
    std::uint8_t frames_per_second{18U};
    std::uint8_t key_pose_count{4U};
    const ExplicitMonsterAnimationFrame* explicit_frames{};
};
```

When `explicit_frames != nullptr`, `monster_animation_frame()` uses the exact rectangle, anchor and nonzero duration; `monster_animation_frame_index()` walks the fixed frame array’s cumulative tick durations and either loops or clamps. Otherwise both functions retain the 96-pixel/9-column path.

- [ ] **Step 4: Register exact fire sequences and phase mapping.**

Use 256×256 source cells and per-frame foot anchors derived from the committed alpha bounds. The phase sequences are fixed as follows:

```text
fire_bomber (cell@duration_ticks):
idle [0@6]; move [1@4]; telegraph [2@6,7@6];
active [8@3,3@3,9@4]; recovery [4@4,10@6];
cooldown [5@6]; hurt [4@5]; death [6@6,11@12]

fire_charger (cell@duration_ticks):
idle [0@6]; move [1@4]; telegraph [2@5,8@5];
active [9@3,3@3,10@4]; recovery [4@6];
cooldown [5@6]; hurt [4@5]; death [6@6,11@12]
```

For bomber cells 0..11 use foot anchors `{125,234}`, `{111,235}`, `{141,235}`, `{112,228}`, `{121,221}`, `{110,222}`, `{130,225}`, `{113,229}`, `{121,222}`, `{109,224}`, `{128,220}`, `{116,220}`. For charger cells 0..6 and 8..11 use `{125,250}`, `{119,249}`, `{132,253}`, `{123,250}`, `{132,249}`, `{155,252}`, `{114,249}`, `{144,227}`, `{128,231}`, `{128,229}`, `{109,229}`.

Map AI telegraph/active/recovery/cooldown to their namesake animation states so each transition resets its clip clock. Water/lightning/chaos may point all four phase states to their authored 20-frame special range. Remove the fire exclusion from `MonsterMaterialPresenter`; keep its fixed slot array and generation reset behavior.

Remove the five generic `kDefaultAnimationClips` monster rows that claim unavailable 12/16/20/8/16 fire frames. `material_animation_clip(AnimationClipId::monster_*)` must return `nullptr`; all monster presentation and validation use the phase-aware monster clip table above.

Register exactly three new C++ cases from Step 1 and change the platform expected case count from 452 to 455.

- [ ] **Step 5: Run GREEN and verify no false completeness claim.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: every existing fire pose is reachable, all six off-ecology full sequences still pass, and tests assert actual fire counts 12/11 rather than the unavailable 12/16/20/8/16 action totals.

- [ ] **Step 6: Commit.**

```powershell
git add src/platform/raylib/material_animation.* src/platform/raylib/material_manifest.hpp src/platform/raylib/monster_material_presenter.cpp src/platform/raylib/actor_renderer.cpp tests/platform/material_animation_tests.cpp tests/platform/stage12_actor_render_tests.cpp tests/platform/fire_room_material_slice_tests.cpp tests/platform/ecology_material_coverage_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: present all available monster material poses"
```

### Task 5: 技能颜色/材质图配对与按装备常驻

**Files:**
- Create: `tools/build_active_skill_material_maps.py`
- Create: `tests/platform/active_skill_material_asset_pipeline_tests.py`
- Create: `assets/skills/draw_slash_atlas_material.png`
- Create: `assets/skills/storm_swords_atlas_material.png`
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/material_residency.cpp`
- Modify: `src/platform/raylib/active_skill_assets.hpp`
- Modify: `src/platform/raylib/active_skill_assets.cpp`
- Modify: `src/platform/raylib/active_skill_renderer.hpp`
- Modify: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `tests/platform/active_skill_asset_tests.cpp`
- Modify: `tests/platform/material_asset_validation_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes `MaterialPack::draw_frame` and the request from Task 1.
- Produces `MaterialAtlasId::skill_draw_slash`, `MaterialAtlasId::skill_storm_swords`, `active_skill_material_atlas(...)`, and metadata-only `active_skill_atlas_frame(...)`.

- [ ] **Step 1: Write RED asset and metadata tests.**

The Python test requires exact dimensions and alpha parity:

```python
CASES = {
    "draw_slash": ((1254, 1254), 36),
    "storm_swords": ((1024, 1536), 24),
}
self.assertEqual(color.size, expected_size)
self.assertEqual(material.size, expected_size)
self.assertEqual(color.getchannel("A").tobytes(),
                 material.getchannel("A").tobytes())
```

Run the generator twice into temporary directories and require equal SHA-256 hashes. In C++, require the two atlas definitions, all 36/24 source cells, and that a loadout containing only draw slash requests only `skill_draw_slash`; both equipped skills request both atlases and remain under 256 MB.

- [ ] **Step 2: Run RED.**

```powershell
python -m unittest tests.platform.active_skill_material_asset_pipeline_tests -v
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
```

Expected: the material PNGs and material atlas IDs do not exist.

- [ ] **Step 3: Implement deterministic material-map generation.**

For each nontransparent pixel, compute luminance `Y=(54R+183G+19B)/256`, energy `max(B-R, G-R, 0)`, ancient-gold mask where `R>G> B` and `R-B>=24`, and dark-metal mask where `Y<118` and channel spread `<42`. Emit:

```text
R roughness = clamp(220 - Y/2 - 70*dark_metal, 40, 235)
G emissive  = clamp(energy*3 + (Y>210 ? 48 : 0), 0, 255)
B metalness = clamp(32 + 180*dark_metal + 145*ancient_gold, 0, 255)
A           = source alpha
```

The script writes temporary PNGs, reopens them, verifies dimensions/alpha, then replaces the output. Runtime code never invokes Pillow or parses pixels.

- [ ] **Step 4: Move skill atlases into the material manifest.**

Append both atlas IDs before `count`, with dimensions `1254×1254` and `1024×1536`; register `rgba_bytes = width * height * 4U`. Keep the grid definitions:

```cpp
draw_slash: columns=6, rows=6, frame_count=36, cell=209x209
storm_swords: columns=4, rows=6, frame_count=24, cell=256x256
```

Replace `ActiveSkillAssets` with metadata-only lookup and expose:

```cpp
struct ActiveSkillAtlasFrame final {
    MaterialAtlasId atlas{MaterialAtlasId::count};
    Rectangle source{};
    Vector2 foot_anchor{};
    Vector2 weapon_anchor{};
};

[[nodiscard]] MaterialAtlasId active_skill_material_atlas(
    skills::ActiveSkillId id) noexcept;
[[nodiscard]] std::optional<ActiveSkillAtlasFrame>
active_skill_atlas_frame(skills::ActiveSkillId id,
    std::size_t frame_index) noexcept;
```

Add an equipped non-`none` skill’s atlas to `make_material_residency_request`. Do not mark skill atlases common.

Atomically adapt `ActiveSkillRenderer::draw_world` to accept `const MaterialPack&` and draw its current selected frame through `MaterialPack::draw_frame`; remove the class-owned `Texture2D` array and every direct `LoadTexture`, `UnloadTexture` and `DrawTexturePro` call. Remove the corresponding initialize/shutdown calls and startup warning from `CombatRenderer`. Keep `active_skill_assets_ready()` as a compatibility query for manifest/frame-metadata validity; actual per-skill GPU residency is queried with `material_atlas_available(active_skill_material_atlas(id))` after the first synchronized draw and is captured that way in Task 10. Task 6 changes frame selection and player suppression, not texture ownership.

Register exactly three new C++ cases from Step 1 and change the platform expected case count from 455 to 458.

Register the Python test as CTest `stage17.active_skill_material_asset_pipeline` with labels `headless;platform;assets;stage17` and timeout 120 seconds.

- [ ] **Step 5: Run GREEN tests.**

```powershell
python tools/build_active_skill_material_maps.py --root .
python -m unittest tests.platform.active_skill_material_asset_pipeline_tests -v
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: deterministic maps, alpha parity, frame bounds, loadout-driven residency and final legal peak budget pass.

- [ ] **Step 6: Commit.**

```powershell
git add tools/build_active_skill_material_maps.py tests/platform/active_skill_material_asset_pipeline_tests.py tests/platform/active_skill_asset_tests.cpp tests/platform/material_asset_validation_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp assets/skills/draw_slash_atlas_material.png assets/skills/storm_swords_atlas_material.png src/platform/raylib/material_asset_types.hpp src/platform/raylib/material_manifest.hpp src/platform/raylib/material_residency.cpp src/platform/raylib/active_skill_assets.* src/platform/raylib/active_skill_renderer.* src/platform/raylib/combat_renderer.*
git commit -m "feat: add material-backed active skill atlases"
```

### Task 6: 完整技能帧、玩家抑制与专属图标

**Files:**
- Modify: `src/platform/raylib/active_skill_view.hpp`
- Modify: `src/platform/raylib/active_skill_view.cpp`
- Modify: `src/platform/raylib/active_skill_renderer.hpp`
- Modify: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/material_asset_types.hpp`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `tests/platform/active_skill_view_tests.cpp`
- Modify: `tests/platform/active_skill_asset_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes material-backed frame metadata from Task 5.
- Produces `make_active_skill_effect_plan(const combat::CombatSnapshot&, const combat::CombatEvent*, bool)` once per rendered snapshot; both actor and world passes consume the same `ActiveSkillEffectPlan`.

- [ ] **Step 1: Write RED tests for full frame reachability and positions.**

Define and test:

```cpp
enum class ActiveSkillVisualMode : std::uint8_t {
    none, material, procedural_fallback,
};

[[nodiscard]] std::size_t active_skill_visual_frame_index(
    skills::ActiveSkillId id, std::uint16_t elapsed_ticks) noexcept;
```

For draw slash ticks 0..89, collect returned indices and require exactly 0..35. For storm ticks 0..359, require exactly 0..23 including 16 and 20..23. Assert `player_position == snapshot.player.position`, `effect_center == snapshot.active_skill.locked_center`, and changing only the lock center never moves `player_position`.

- [ ] **Step 2: Write RED tests for healthy/fallback exclusivity and HUD icons.**

Use this plan contract:

```cpp
struct ActiveSkillEffectPlan final {
    ActiveSkillVisualMode mode{ActiveSkillVisualMode::none};
    combat::Vec3 player_position{};
    combat::Vec3 effect_center{};
    MaterialAtlasId atlas{MaterialAtlasId::count};
    std::size_t atlas_frame{};
    bool suppress_base_player{};
    std::size_t procedural_main_visual_count{};
    DrawSlashVisualPlan draw_slash{};
    StormSwordsVisualPlan storm_swords{};
    float screen_flash_alpha{};
};

[[nodiscard]] ActiveSkillEffectPlan make_active_skill_effect_plan(
    const combat::CombatSnapshot& snapshot,
    const combat::CombatEvent* last_event,
    bool material_ready) noexcept;
```

Healthy material requires `mode == material`, `suppress_base_player`, and `procedural_main_visual_count == 0`. Missing material requires `procedural_fallback`, no suppression, and a nonzero fallback main-visual count. Add `skill_icon_draw_slash` and `skill_icon_storm_swords` tests: occupied slots have distinct non-missing icons; empty slots use `missing`.

After constructing the snapshot once, call `make_active_skill_effect_plan` 10,000 times inside the existing allocation probe and require zero heap allocations.

- [ ] **Step 3: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: current draw slash exposes only the hit window and storm omits frames.

- [ ] **Step 4: Implement exact visual frame mapping.**

Use integer math:

```cpp
// draw slash: tick 0..89
return (std::min<std::size_t>)(35U,
    static_cast<std::size_t>(elapsed_ticks) * 36U / 90U);

// storm swords
if (elapsed_ticks < 72U)  return elapsed_ticks * 5U / 72U;
if (elapsed_ticks < 324U) return 5U + (elapsed_ticks - 72U) * 15U / 252U;
if (elapsed_ticks < 342U) return 20U + (elapsed_ticks - 324U) * 2U / 18U;
return 22U + (elapsed_ticks - 342U) * 2U / 18U;
```

Clamp storm to 23. Keep combat timeline `kStormSwordsClip.frame_count` unchanged; visual atlas frames are deliberately decoupled from its logical event frame count.

- [ ] **Step 5: Draw material skills once and suppress duplicate player.**

Build the plan once in `CombatRenderer::draw` after residency synchronization. `material_ready` is true only when the currently active skill ID is not `none` and `material_pack_.available(active_skill_material_atlas(id))`; another equipped skill’s healthy atlas cannot mask this skill’s failure. Pass the plan to `draw_actors` and `ActiveSkillRenderer::draw_world(const ActiveSkillEffectPlan&, const MaterialPack&, float, float)`. The actor pass omits the base player only when `plan.suppress_base_player`; the skill renderer calls:

```cpp
const ScreenProjection player = project_combat_position(
    plan.player_position, width, height);
material_pack.draw_frame(frame.atlas, frame.source, frame.foot_anchor,
    {player.x, player.ground_y}, flip_x, scale, WHITE);
```

Use `effect_center` only for damage-area flashes, pull lines and the 24 sword placements. Material mode does not draw the fan, triangle-sword or finisher ellipse as the main visual; fallback mode retains all three. Resource failure never hides the base player.

- [ ] **Step 6: Add skill-specific material icons.**

Register representative frames as sprites: draw slash frame 18 source `{0,627,209,209}`, storm frame 20 source `{0,1280,256,256}`. Add:

```cpp
[[nodiscard]] MaterialSpriteId active_skill_icon_sprite(
    skills::ActiveSkillId id) noexcept;
```

Add `MaterialSpriteId icon` to `ActiveSkillHudSlot`; draw it inside the slot’s inset bounds through `material_pack.draw_to`, then draw the cooldown mask and text. Retain the generic slot frame, not the generic blue-ball main icon.

Register exactly four new C++ cases from Steps 1–2 and change the platform expected case count from 458 to 462.

- [ ] **Step 7: Run GREEN tests.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: 36/24 index coverage, position separation, healthy/fallback exclusivity, no duplicate player, and distinct HUD icons pass.

- [ ] **Step 8: Commit.**

```powershell
git add src/platform/raylib/active_skill_view.* src/platform/raylib/active_skill_renderer.* src/platform/raylib/combat_renderer.* src/platform/raylib/actor_renderer.cpp src/platform/raylib/material_asset_types.hpp src/platform/raylib/material_manifest.hpp tests/platform/active_skill_view_tests.cpp tests/platform/active_skill_asset_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: render complete active skill material timelines"
```

### Task 7: 分辨率安全的环境物件布局

**Files:**
- Create: `src/platform/raylib/environment_prop_layout.hpp`
- Create: `src/platform/raylib/environment_prop_layout.cpp`
- Modify: `src/platform/raylib/water_room_material_slice.hpp`
- Modify: `src/platform/raylib/water_room_material_slice.cpp`
- Modify: `src/platform/raylib/lightning_room_material_slice.hpp`
- Modify: `src/platform/raylib/lightning_room_material_slice.cpp`
- Modify: `src/platform/raylib/chaos_room_material_slice.hpp`
- Modify: `src/platform/raylib/chaos_room_material_slice.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/stage12_environment_render_tests.cpp`
- Modify: `tests/platform/ecology_material_coverage_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes the isolated frame metadata generated by Task 2.
- Produces a pure normalized layout with at most nine placements and a projected-bounds validator.

- [ ] **Step 1: Write RED layout tests for all target resolutions.**

Add the following types:

```cpp
struct EnvironmentPropDefinition final {
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    MaterialLayer layer{MaterialLayer::body};
    Rectangle alpha_bounds{};
    Vector2 foot_anchor{};
    float recommended_scale{1.0F};
};

struct EnvironmentPropPlacement final {
    MaterialSpriteId sprite{MaterialSpriteId::missing};
    Vector2 normalized_foot_position{};
    bool flip_x{};
    float scale{1.0F};
};

struct EnvironmentPropLayout final {
    std::array<EnvironmentPropPlacement, 9> props{};
    std::size_t count{};
};

[[nodiscard]] EnvironmentPropLayout environment_prop_layout(
    dungeon::DungeonElement ecology, float width, float height) noexcept;
[[nodiscard]] const EnvironmentPropDefinition*
environment_prop_definition(MaterialSpriteId sprite) noexcept;
[[nodiscard]] Rectangle project_environment_prop_bounds(
    const EnvironmentPropDefinition& definition,
    const EnvironmentPropPlacement& placement,
    float width, float height) noexcept;
```

For water/lightning/chaos at 800×450, 1280×720 and 1920×1080, project every alpha bbox and require x within `[8,width-8]`, y within `[8,height-max(72,0.12*height)-8]`, `count == 5`, and no pair with intersection area above 15% of the smaller bbox. Require the hole sprite is not included in decorative props because it has its own gameplay location.

- [ ] **Step 2: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
```

Expected: compile failure because the pure layout API does not exist.

- [ ] **Step 3: Implement one shared layout instead of three draw loops.**

Use these normalized foot positions for all three ecologies: `{0.12,0.34}`, `{0.88,0.34}`, `{0.18,0.78}`, `{0.82,0.78}`, `{0.50,0.82}`. Resolve ecology-specific sprites from the existing slice objects, take scale from each `EnvironmentPropDefinition`, and apply a viewport clamp computed from the real alpha bbox rather than the 256×256 cell. Reserve the bottom `max(72, 0.12*height)` pixels for HUD.

Copy the generated bbox, anchor and scale numbers into `constexpr EnvironmentPropDefinition` arrays in the three slice files; JSON is test/provenance input only and is never opened by the game runtime.

Replace `draw_water_room_props`, `draw_lightning_room_props`, and `draw_chaos_room_props` with one loop over `environment_prop_layout`. Split the remaining aggregate gate: room-background availability controls only background-vs-grid fallback; props and the gameplay hole always attempt their own sprite and fall back independently. If one prop sprite cannot draw, draw only a low-alpha dark-steel outline matching that prop’s projected alpha bbox; do not flip the whole room to the gray-box renderer.

Register exactly three resolution-layout cases (one per ecology, each iterating all three resolutions) and change the platform expected case count from 462 to 465.

- [ ] **Step 4: Run GREEN tests.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-debug -R '^stage12\.environment_prop_asset_pipeline$' --output-on-failure -j 1
```

Expected: isolated sprite metadata and all three target viewport layouts pass without overflow/HUD collision.

- [ ] **Step 5: Commit.**

```powershell
git add src/platform/raylib/environment_prop_layout.* src/platform/raylib/water_room_material_slice.* src/platform/raylib/lightning_room_material_slice.* src/platform/raylib/chaos_room_material_slice.* src/platform/raylib/room_renderer.cpp src/platform/raylib/CMakeLists.txt tests/platform/stage12_environment_render_tests.cpp tests/platform/ecology_material_coverage_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: lay out isolated room props safely"
```

### Task 8: 死亡材质面板与高清战斗文字

**Files:**
- Modify: `src/platform/raylib/death_overlay_view.hpp`
- Modify: `src/platform/raylib/death_overlay_view.cpp`
- Modify: `src/platform/raylib/death_overlay_renderer.hpp`
- Modify: `src/platform/raylib/death_overlay_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/hud_font.hpp`
- Modify: `src/platform/raylib/hud_font.cpp`
- Modify: `tests/platform/death_overlay_view_tests.cpp`
- Modify: `tests/platform/ui_material_slice_tests.cpp`
- Modify: `tests/platform/hud_font_tests.cpp`
- Modify: `tests/platform/stage12_actor_render_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes common `ui_warning_modal`, `ui_label_plate` and the already loaded HUD font.
- Produces `DeathOverlayMaterialPlan` and `CombatTextStyle` pure plans.

- [ ] **Step 1: Write RED plan and source-path tests.**

```cpp
struct DeathOverlayMaterialPlan final {
    bool visible{};
    MaterialSpriteId panel{MaterialSpriteId::ui_warning_modal};
    MaterialSpriteId title_plate{MaterialSpriteId::ui_label_plate};
    float panel_border_pixels{32.0F};
};

[[nodiscard]] DeathOverlayMaterialPlan death_overlay_material_plan(
    bool visible) noexcept;

struct CombatTextStyle final {
    float damage_font_size{};
    float defeated_font_size{};
    float spacing{1.0F};
};

[[nodiscard]] CombatTextStyle combat_text_style(
    int screen_width, int screen_height) noexcept;
```

Require visible death mode selects both UI sprites and hidden mode performs zero material draws. Require font coverage for ASCII digits, `-+%`, and `DEFEATED`; require sizes increase monotonically from 800×450 to 1920×1080. Add source guards requiring `MeasureTextEx`/`draw_crisp_ui_text` in damage and defeated paths and forbidding their old direct `MeasureText`/`DrawText` calls.

- [ ] **Step 2: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: death renderer does not accept `MaterialPack` and actor text uses the default font.

- [ ] **Step 3: Integrate existing UI materials.**

Change the renderer signature to:

```cpp
void DeathOverlayRenderer::draw(
    const dungeon::DungeonSnapshot& snapshot,
    const MaterialPack& material_pack) const noexcept;
```

Draw the full-screen dark overlay programmatically, the central panel with `draw_nine_slice(plan.panel, bounds, 32.0F)`, and the title plate with `draw_region_fit`. Keep dynamic title/body, separator and continue/retreat text programmatic. Call it as `death_overlay_.draw(current, material_pack_)`.

If either material draw returns `false`, draw the existing dark panel/plate geometry for only that missing element; the full-screen dark overlay and text remain visible in both paths.

- [ ] **Step 4: Route combat text through the shared HUD font.**

Extend `draw_effects` to receive `Font hud_font, bool hud_font_ready`. Use `combat_text_style(width,height)` based on `ui_viewport_scale`, `MeasureTextEx`, and `draw_crisp_ui_text`; fallback to `GetFontDefault()` only when the loaded font is invalid. Keep the text color pure/opaque at the center (`alpha=255`) and use only a dark one-pixel shadow for contrast; do not introduce gradient text.

Register exactly four new C++ cases from Step 1 and change the platform expected case count from 465 to 469.

- [ ] **Step 5: Run GREEN tests.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: material plan, nine-slice source guard, font coverage/scaling and actor text route pass.

- [ ] **Step 6: Commit.**

```powershell
git add src/platform/raylib/death_overlay_view.* src/platform/raylib/death_overlay_renderer.* src/platform/raylib/combat_renderer.cpp src/platform/raylib/actor_renderer.cpp src/platform/raylib/hud_font.* tests/platform/death_overlay_view_tests.cpp tests/platform/ui_material_slice_tests.cpp tests/platform/hud_font_tests.cpp tests/platform/stage12_actor_render_tests.cpp tests/platform/platform_test_main.cpp
git commit -m "fix: integrate death and combat text materials"
```

### Task 9: NumPad 1～5 双输入路径

**Files:**
- Modify: `src/platform/raylib/host_input.cpp`
- Modify: `src/platform/raylib/raylib_input.hpp`
- Modify: `src/platform/raylib/raylib_input.cpp`
- Modify: `tests/platform/active_skill_input_tests.cpp`
- Create: `tests/platform/raylib_input_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Produces `win32_virtual_key_for_raylib(int)` as a pure mapping used by both `platform_key_pressed` and `platform_key_down`.
- Preserves `map_host_frame_input` and `submit_frame_actions` signatures.

- [ ] **Step 1: Write RED tests for raylib sampling and Win32 mapping.**

Require each `KEY_KP_1..KEY_KP_5` pressed edge sets exactly slot 0..4; `KEY_ONE..KEY_FIVE` set none. A key that is down but not pressed must not repeatedly cast. Require:

```cpp
ARPG_REQUIRE(win32_virtual_key_for_raylib(KEY_KP_1) == 0x61);
ARPG_REQUIRE(win32_virtual_key_for_raylib(KEY_KP_2) == 0x62);
ARPG_REQUIRE(win32_virtual_key_for_raylib(KEY_KP_3) == 0x63);
ARPG_REQUIRE(win32_virtual_key_for_raylib(KEY_KP_4) == 0x64);
ARPG_REQUIRE(win32_virtual_key_for_raylib(KEY_KP_5) == 0x65);
```

- [ ] **Step 2: Run RED.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
```

Expected: current `KEY_ONE + index` assertions fail and the pure Win32 mapper is absent.

- [ ] **Step 3: Implement explicit key arrays and shared mapping.**

```cpp
constexpr std::array<int, skills::kActiveSkillSlotCount>
    kActiveSkillKeys{{KEY_KP_1, KEY_KP_2, KEY_KP_3, KEY_KP_4, KEY_KP_5}};
```

Sample each explicit key through the injected `pressed` function. In `win32_virtual_key_for_raylib`, map only `KEY_KP_1..5` to `VK_NUMPAD1..5` (`0x61..0x65`) in addition to existing supported keys; both asynchronous pressed/down functions call this mapper. Do not infer continuity from raylib enum values.

- [ ] **Step 4: Run GREEN and input architecture guard.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.input_latency_source|platform\.host_input_source)$' --output-on-failure -j 1
```

Expected: raylib and Win32 paths map NumPad correctly, main numbers do nothing, and centralized input-source guards remain green.

- [ ] **Step 5: Recount the platform suite and commit.**

Register exactly four cases: NumPad-to-slot mapping, main-number rejection/pressed-edge behavior, Win32 NumPad virtual-key mapping, and unchanged submit semantics. Change the platform expected case count from 469 to 473.

```powershell
git add src/platform/raylib/host_input.cpp src/platform/raylib/raylib_input.* tests/platform/active_skill_input_tests.cpp tests/platform/raylib_input_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git commit -m "fix: bind active skills to numpad keys"
```

### Task 10: 正式 raylib 验收、性能与部署闭环

**Files:**
- Modify: `tests/platform/stage12_material_formal_game_validation.cpp`
- Modify: `tests/platform/stage12_material_validator.ps1`
- Modify: `tests/platform/stage17_skill_stones_game_validation.cpp`
- Modify: `tests/platform/stage17_skill_stones_validator.ps1`
- Modify: `tests/platform/package_material_pack_v2_tests.py`
- Modify: `tools/package_material_pack_v2.py`
- Create: `docs/validation/material-runtime-integration-repair.md`
- Create: `docs/validation/evidence/material-runtime-integration/.gitkeep`

**Interfaces:**
- Consumes every production path from Tasks 1–9.
- Produces machine-readable screenshots/metrics and a deployable package manifest; no gameplay code is introduced here.

- [ ] **Step 1: Extend formal fixtures before changing validators.**

Add deterministic fixture modes for:

```text
cross-ecology: fire room + chaos chaser
doors-closed / doors-open: all four directions visible
draw-slash: ticks 0, 45, 46, 66, 89
storm-swords: ticks 0, 71, 72, 180, 323, 324, 342, 359
environment: water/lightning/chaos at 800x450, 1280x720, 1920x1080
death: warning modal + title plate
stress: 30 monsters + storm swords for 1800 presented frames
```

Run every non-stress mode at `{800,450}`, `{1280,720}` and `{1920,1080}`. Run the stress mode at 1920×1080 after a 300-frame warmup.

Record per frame: requested/resident mask, resident bytes, load/unload totals, material draw status for each monster/door/skill, base-player suppression, procedural-main-visual count, average FPS and sorted frame-time p99 (used as 1% low: `1000 / p99_ms`).

- [ ] **Step 2: Run validators RED against the existing runtime.**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage12_material_formal arpg_stage17_skill_stones_game_validation --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^(stage12\.material_formal|stage17\.skill_stones\.real_raylib)$' --output-on-failure -j 1
```

Expected: new evidence assertions fail on old output fields or missing captures; a graphics-context/environment failure is reported separately from a rendering assertion failure.

- [ ] **Step 3: Implement evidence checks with numeric and pixel gates.**

Require:

- cross-ecology screenshot reports both fire background and chaos-chaser atlas resident and drawn;
- four door sprite IDs are distinct, no default-font `?` glyph is emitted, and no opaque red rectangle covers more than 10% of any door bbox;
- captured draw-slash atlas frames include 0 and 35 and the union is 0..35; storm union is 0..23;
- healthy skill frames report base-player suppression and zero procedural main visuals; an injected missing-map fixture reports the fallback inverse;
- each environment prop alpha bbox is inside the viewport and above the HUD reserve in all nine captures;
- death capture records nonzero `ui_warning_modal` nine-slice and `ui_label_plate` draws;
- stress `resident_bytes <= 268435456`, no load/unload occurs after the warmup request stabilizes, average FPS ≥60 and `1000/p99_ms >=45`.

The validator must reject showcase-only captures as proof for formal room integration.

- [ ] **Step 4: Update packaging and deployed-file checks.**

Add the public door pair, both skill material maps, environment build JSON, this plan, `docs/superpowers/specs/2026-07-25-material-runtime-integration-repair-design.md`, and the new source/validator files to `package_material_pack_v2.py`. Replace the historic constants with the reviewed manifest totals `FULL_PACK_BYTES = 329430304`, `RESIDENT_PEAK_BYTES = 226800928`, and `TRANSITION_PEAK_BYTES = 261010720`; the package test requires the formal evidence values and packaged metadata to match all three numbers and verifies SHA-256 hashes against source files.

Add these exact package references:

```python
PLAN_FILES += (
    "docs/superpowers/specs/2026-07-25-material-runtime-integration-repair-design.md",
    "docs/superpowers/plans/2026-07-25-material-runtime-integration-repair.md",
    "docs/validation/material-runtime-integration-repair.md",
)
INTEGRATION_FILES += (
    "src/platform/raylib/material_residency.hpp",
    "src/platform/raylib/material_residency.cpp",
    "src/platform/raylib/material_pack.hpp",
    "src/platform/raylib/material_pack.cpp",
    "src/platform/raylib/material_asset_types.hpp",
    "src/platform/raylib/material_animation.hpp",
    "src/platform/raylib/material_animation.cpp",
    "src/platform/raylib/monster_material_presenter.hpp",
    "src/platform/raylib/monster_material_presenter.cpp",
    "src/platform/raylib/environment_prop_layout.hpp",
    "src/platform/raylib/environment_prop_layout.cpp",
    "src/platform/raylib/active_skill_assets.hpp",
    "src/platform/raylib/active_skill_assets.cpp",
    "src/platform/raylib/host_input.cpp",
    "src/platform/raylib/raylib_input.hpp",
    "src/platform/raylib/raylib_input.cpp",
    "tools/build_environment_props.py",
    "tools/build_active_skill_material_maps.py",
    "tests/platform/environment_prop_asset_pipeline_tests.py",
    "tests/platform/active_skill_material_asset_pipeline_tests.py",
)
```

- [ ] **Step 5: Run the bounded final verification sequence.**

Run each command serially and do not keep the game open between build steps:

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_stage12_material_formal arpg_stage17_skill_stones_game_validation arpg_game --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|combat\.units|skills\.units|dungeon\.units)$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-debug -R '^(stage12\.(environment_prop_asset_pipeline|material_formal|material_evidence_validator)|stage17\.(active_skill_material_asset_pipeline|skill_stones\.(real_raylib|evidence_validator))|platform\.(input_latency_source|host_input_source))$' --output-on-failure -j 1

.\scripts\Configure.ps1 -Preset windows-msvc-release
cmake --build --preset windows-msvc-release --target arpg_game arpg_stage12_material_formal arpg_stage17_skill_stones_game_validation --parallel 1
ctest --test-dir out/build/windows-msvc-release -R '^(stage12\.material_formal|stage12\.material_evidence_validator|stage17\.skill_stones\.real_raylib|stage17\.skill_stones\.evidence_validator)$' --output-on-failure -j 1
```

Expected: targeted unit/asset/input suites, Debug/Release targets, all required viewport captures, residency budget and performance gates pass. Do not run unrelated full 109-test CTest unless a targeted failure indicates cross-module risk.

- [ ] **Step 6: Inspect screenshots and system resource evidence.**

Open the 800×450, 1280×720 and 1920×1080 captures at original resolution and record a pass/fail row for cross-ecology monster, four doors, both skills’ phases, three ecology prop sets and death panel. Record compiler/game process lists before and after validation and confirm no `arpg_game`, compiler, CTest or helper remains running.

- [ ] **Step 7: Write validation handoff and commit.**

`docs/validation/material-runtime-integration-repair.md` records commit, exact commands/exit codes, screenshot paths, atlas bytes, load/unload counts, average FPS, 1% low, remaining art gaps (fire full action boards and storm 96 independent character frames), and any graphics-environment limitation without relabeling it as a gameplay failure.

```powershell
git add tests/platform/stage12_material_formal_game_validation.cpp tests/platform/stage12_material_validator.ps1 tests/platform/stage17_skill_stones_game_validation.cpp tests/platform/stage17_skill_stones_validator.ps1 tests/platform/package_material_pack_v2_tests.py tools/package_material_pack_v2.py docs/validation/material-runtime-integration-repair.md docs/validation/evidence/material-runtime-integration
git commit -m "test: validate material runtime integration"
```

---

## Completion gate

The branch is complete only when all ten task commits exist, the worktree is clean, targeted Debug/Release verification is green, the three-resolution screenshots have been inspected at original size, residency stays under 256 MB, the stress capture meets 60 FPS average and 45 FPS 1% low, and no background build/game process remains. If real raylib cannot create a graphics context, preserve logs and stop before claiming visual completion; headless unit success alone is insufficient.

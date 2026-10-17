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


### Task 3: 制作环境图集并替换房间、门和洞口表现

**Files:**
- Create: `assets/stage12/environment.png`
- Create: `assets/stage12/environment-source.md`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `tests/platform/material_animation_tests.cpp`
- Create: `tests/platform/stage12_environment_render_tests.cpp`

**Interfaces:**
- Consumes: `MaterialPack::draw`、`DungeonSnapshot`、`DoorRenderDecision`、`HoleVisualMode`。
- Produces: 以现有投影位置绘制的环境材质；任何环境帧不可用时返回现有灰盒、矩形门和椭圆洞口。

- [ ] **Step 1: 写出环境资源选择失败测试**

```cpp
ARPG_TEST(environment_maps_each_dungeon_element_to_a_floor_sprite) {
    ARPG_REQUIRE(select_floor_sprite(dungeon::DungeonElement::fire) != MaterialSpriteId::missing);
    ARPG_REQUIRE(select_floor_sprite(dungeon::DungeonElement::chaos) != MaterialSpriteId::missing);
}
```

- [ ] **Step 2: 编译测试，确认环境资源选择器未实现而失败**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests`

Expected: 编译失败，错误指向 `select_floor_sprite`。

- [ ] **Step 3: 生成并清理环境图集，接入 `room_renderer.cpp`**

创建原创环境图集，包含暗色地板、墙体、四元素重点色门框、锁门内衬、深渊标记、洞口和少量装饰。以现有 `project_render_world` 计算的位置、缩放和门/洞口状态绘制纹理；保留现有标签、箭头、`Press E to descend` 提示和所有交互判断。`environment-source.md` 记录生成来源、清理步骤、图集尺寸、RGBA 内存和每个帧族。

- [ ] **Step 4: 运行房间回归与实际截图检查**

Run: `ctest --test-dir out/build/windows-msvc-debug -R "platform.units|stage11d.loot_formal" --output-on-failure`

Expected: PASS；截图中四向门、元素提示、深渊标识、掉落和洞口均可见，洞口 E 范围不变。

- [ ] **Step 5: 提交 12-B**

```powershell
git add assets/stage12/environment.png assets/stage12/environment-source.md src/platform/raylib/material_manifest.hpp src/platform/raylib/room_renderer.cpp tests/platform/material_animation_tests.cpp tests/platform/stage12_environment_render_tests.cpp
git commit -m "feat: add stage 12 comic dungeon environment"
```

## 12-C：角色与怪物包


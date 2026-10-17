### Task 2: 实现 `MaterialPack` 生命周期、发布复制和程序回退

**Files:**
- Create: `src/platform/raylib/material_pack.hpp`
- Create: `src/platform/raylib/material_pack.cpp`
- Create: `assets/stage12/README.md`
- Modify: `src/platform/raylib/combat_renderer.hpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `CMakeLists.txt`
- Modify: `tests/platform/material_asset_validation_tests.cpp`

**Interfaces:**
- Consumes: `material_manifest.hpp` 的图集定义与 `raylib::LoadTexture`。
- Produces: `bool MaterialPack::load() noexcept`、`void MaterialPack::unload() noexcept`、`bool MaterialPack::available(MaterialAtlasId) const noexcept` 与 `void MaterialPack::draw(MaterialSpriteId, Vector2, bool) const noexcept`。

- [ ] **Step 1: 写出回退和重复释放测试**

```cpp
ARPG_TEST(material_pack_falls_back_when_atlas_is_unavailable) {
    MaterialPackState state{};
    state.set_available(MaterialAtlasId::actors, false);
    ARPG_REQUIRE(!state.can_draw(MaterialSpriteId::player_idle));
}

ARPG_TEST(material_pack_allows_repeated_shutdown) {
    MaterialPackState state{};
    state.reset();
    state.reset();
    ARPG_REQUIRE(!state.any_available());
}
```

- [ ] **Step 2: 运行测试，确认新增状态类型尚未定义而失败**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests`

Expected: 编译失败，错误指出 `MaterialPackState` 未定义。

- [ ] **Step 3: 实现加载、卸载、绘制和发布复制**

`MaterialPack::load` 必须依次加载三张内置 PNG，验证实际尺寸与编译期清单一致；单张失败只禁用对应类别并记录一次警告。`draw` 在类别可用时用 `DrawTexturePro` 按脚底锚点绘制；不可用时返回 `false`，由现有调用点继续程序绘制。`unload` 只对 `IsTextureReady` 的纹理调用 `UnloadTexture`，然后清空状态。CMake 使用 `add_custom_command(TARGET arpg_game POST_BUILD ...)` 把 `assets/stage12` 复制到 `bin/assets/stage12`。

- [ ] **Step 4: 运行资源生命周期和发布布局验证**

Run: `cmake --build --preset windows-msvc-debug; ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure`

Expected: PASS；可执行目录包含三张图集和来源说明；重复关闭不会崩溃。

- [ ] **Step 5: 提交完整 12-A**

```powershell
git add assets/stage12 src/platform/raylib/material_pack.* src/platform/raylib/combat_renderer.* src/platform/raylib/CMakeLists.txt CMakeLists.txt tests/platform/material_asset_validation_tests.cpp
git commit -m "feat: add stage 12 material pack lifecycle"
```

## 12-B：地下城环境包


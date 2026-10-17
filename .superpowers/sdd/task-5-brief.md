### Task 5: 制作效果图集、掉落图标和正式验证

**Files:**
- Create: `assets/stage12/effects_ui.png`
- Create: `assets/stage12/effects-ui-source.md`
- Modify: `src/platform/raylib/material_manifest.hpp`
- Modify: `src/platform/raylib/actor_renderer.cpp`
- Modify: `src/platform/raylib/room_renderer.cpp`
- Modify: `src/platform/raylib/material_asset_validation.cpp`
- Create: `tests/platform/stage12_material_formal_game_validation.cpp`
- Create: `tests/platform/stage12_material_validator.ps1`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `docs/validation/stage12-comic-material-pack.md`

**Interfaces:**
- Consumes: `CombatFeedback`、`GroundLootView`、`MaterialPack` 和现有截图基础设施。
- Produces: 一套可自动验证的实际 raylib 证据，证明 1280×720 与 1920×1080 下的环境、角色、效果和回退。

- [ ] **Step 1: 写出效果层级、掉落稀有度和预算失败测试**

```cpp
ARPG_TEST(effect_manifest_stays_within_texture_budget) {
    ARPG_REQUIRE(validate_material_manifest(default_material_manifest()).valid);
}

ARPG_TEST(loot_rarity_uses_distinct_material_icons) {
    ARPG_REQUIRE(select_loot_sprite(items::ItemRarity::normal)
        != select_loot_sprite(items::ItemRarity::rare));
}
```

- [ ] **Step 2: 构建并确认未实现的效果选择器导致失败**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests`

Expected: 编译失败，错误指出 `select_loot_sprite` 未定义。

- [ ] **Step 3: 接入效果、物品图标和正式验证器**

制作火、水、电、混沌、命中火花、上挑轨迹、落地尘土、词缀光环与白/蓝/黄掉落图标。效果仍按现有渲染层级和事件时序绘制；物品标签与自动拾取阈值不改变。正式验证器必须生成两个分辨率的截图、清单校验报告、三图集尺寸和内存报告、资源损坏回退记录及输入/洞口回归记录。PowerShell 验证器拒绝缺失文件、错误分辨率、超过预算、未触发回退或未包含八种怪物证据的目录。

- [ ] **Step 4: 运行完整构建、测试和正式视觉验证**

Run: `./scripts/Test.ps1 -Preset windows-msvc-debug; ./scripts/Test.ps1 -Preset windows-msvc-release; ctest --test-dir out/build/windows-msvc-release -R stage12 --output-on-failure`

Expected: 全部 PASS；正式证据包含 1280×720 与 1920×1080 的真实 raylib 截图，资源回退被验证且游戏保持可运行。

- [ ] **Step 5: 写入验证记录并提交 12-D**

```powershell
git add assets/stage12/effects_ui.png assets/stage12/effects-ui-source.md src/platform/raylib tests/platform docs/validation/stage12-comic-material-pack.md
git commit -m "feat: complete stage 12 comic material pack"
```

## Plan Self-Review

- 规格覆盖：12-A 的清单/加载/回退，12-B 的环境，12-C 的玩家和八种怪物，12-D 的特效、掉落、正式验证均有独立任务。
- 占位符扫描：计划不含占位标记、未定设计或“类似此前任务”的引用；每项代码工作给出路径、接口、失败测试、验证命令和提交范围。
- 类型一致性：所有任务使用同一套 `MaterialAtlasId`、`MaterialSpriteId`、`MaterialPack` 和 `MaterialFrameDefinition` 名称；后续任务只消费前序明确产出的接口。

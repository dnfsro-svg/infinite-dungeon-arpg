# Stage 12 Task 4 报告：角色图集与绘制

## 完成内容

- 将指定原创概念图复制为 `art_source/stage12/actors-concept-v1.png`，去除 `#00ff00` 键色及绿边，生成透明 `assets/stage12/actors.png`。
- 图集为 2048×2048 RGBA，透明四角；采用 224×224、左侧 8×9 有效格布局，前 68 格有内容。
- 新增固定玩家的 idle、move、J1/J2/J3、L 上挑、jump rise/fall、air J、landing、hurt、dead 帧映射。
- 扩展八种现有怪物的全部 idle/move/telegraph/active/recovery/cooldown/defeated 精灵 ID 和显式 AI 阶段映射。
- `actor_renderer.cpp` 通过 `MaterialPack::draw` 绘制图集；左向只用 `flip_x` 镜像右向图。图集/帧不可绘制时，玩家回到既有几何绘制、怪物回到既有剪影绘制。
- 未更改插值、阴影遍历、Y 排序、预警、血条、词缀轮廓、调试体积或输入/战斗逻辑；图集路径仍保留这些叠加层。

## 审查修复

- 重建玩家 12 格：idle、move、J1/J2/J3、L 上挑、跳升/降、空中 J、落地、受击和死亡不再只靠亮度或位移复用。构建期合成使用旋转、腾空、落地压缩、受击倾斜、倒地和剑弧；L 格含清晰向上的蓝白挥剑弧。
- 图集绘制按裁切主体而非完整 224px 格计算：玩家 `projected.scale × 82/172`、怪物 `projected.scale × 78/176`，在投影基准下为 82px 与 78px，匹配原有 82px 玩家和 70–86px 怪物几何范围。
- 扩展阶段测试为每种怪物 7 个 AI 阶段两两不同，且每个所选精灵必须存在于 manifest 并通过 frame 边界校验。
- 成功的材质怪物路径在 `target_flash_seconds > 0` 时以 additive 白色精灵叠层重绘，恢复原剪影路径的受击白闪效果；不可绘制时仍使用原回退路径。
- 文档明确 2048px 画布可容纳 9×9 槽位，但 manifest 与导出实际使用左侧 8×9 有效格，消除布局歧义。
- 最终将生成器与 manifest 统一为 8 列顺序，并新增图集内容门禁：全部 68 个 actors manifest 帧都必须至少包含一个非透明像素；八种怪物的 7 个 AI 阶段都必须有两两不同的导出像素 hash。构建期 SHA-256 自检显示 `empty_indices` 为空、68 帧无重复 hash；chaos_hazard 的 61–67 帧均非透明且 hash 全不同。
- `assets/stage12/README.md` 已说明 actors 图集已交付，且缺失/不可绘制时仍按原语义安全回退。

## TDD 记录

先新增 `stage12_actor_render_tests.cpp` 并接入平台测试。实现前运行平台测试得到预期红灯：

```text
295 cases, 2 failures
[FAIL] stage12_actor_render.maps every monster phase to a distinct material frame
```

实现映射与清单后第一次绿灯候选发现真实缺陷：256px 的 8×9 布局越出 2048px 图集，manifest 校验失败并导致 3 个 MaterialPack 测试失败。已改为 224px 的 8×9 有效格布局并重新生成图集。

## 实际验证

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
.\out\build\windows-msvc-debug\bin\arpg_platform_tests.exe
# 298 cases, 0 failures

ctest --test-dir out/build/windows-msvc-debug -R "combat.units|platform.units|stage11b.settings_formal" --output-on-failure
# 3/3 passed; 0 failed; 9.40 sec

# Earlier full baseline before the final critical atlas re-export:
# 79/79 passed; 0 failed; 765.90 sec
```

资源自检：`actors.png=2048x2048`，四个角 alpha 均为 `0`；12 个玩家格均有非透明像素边界；L 格边界为 `47,0-223,220`，确认上挥剑弧触达格顶；最终 68 格 SHA-256 检查无空格、无重复 hash；`git diff --check` 通过。

## 范围说明

未提交。工作树已有的 `CMakeLists.txt`、Stage11D evidence 和 `assets/stage12/source/` 外部脏改未修改或纳入本任务。

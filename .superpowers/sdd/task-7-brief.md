### Task 7: 增加五槽 HUD、技能特效和暂停页装卸界面

**Files:**
- Create: `src/platform/raylib/active_skill_view.hpp`
- Create: `src/platform/raylib/active_skill_view.cpp`
- Create: `src/platform/raylib/active_skill_renderer.hpp`
- Create: `src/platform/raylib/active_skill_renderer.cpp`
- Modify: `src/platform/raylib/combat_renderer.cpp`
- Modify: `src/platform/raylib/inventory_view_math.hpp`
- Modify: `src/platform/raylib/inventory_view_math.cpp`
- Modify: `src/platform/raylib/inventory_renderer.hpp`
- Modify: `src/platform/raylib/inventory_renderer.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `tests/platform/active_skill_view_tests.cpp`
- Create: `tests/platform/active_skill_loadout_view_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`

- [ ] **Step 1: 写纯 view/model RED 测试**

  在无窗口测试中断言：HUD 恒为五槽；显示 1～5；空槽模型明确；冷却比例 clamp 到 `[0,1]`；拔刀/暴风名称与目录一致；暂停技能页显示五个主槽、当前选中槽的五个辅助空位、未装备石库存；点击命中区域在 1280×720、1600×900、1920×1080 均不重叠；动作命令只有 select/remove/equip/swap，不产生辅助编辑命令。

- [ ] **Step 2: 运行 RED**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_platform_tests
  ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure
  ```
  Expected: 新 view/model 类型和几何函数不存在。

- [ ] **Step 3: 实现只读 HUD 模型和布局**

  公开模型固定为：
  ```cpp
  struct ActiveSkillHudSlot final {
      std::uint8_t key_number{};
      skills::ActiveSkillId id{skills::ActiveSkillId::none};
      std::array<char, 48> name{};
      float cooldown_ratio{};
      bool empty{};
  };
  struct ActiveSkillHudModel final {
      std::array<ActiveSkillHudSlot,
          skills::kActiveSkillSlotCount> slots{};
  };
  ```
  HUD 锚定屏幕底部中央，五个 58×58 槽，8 px 间距；冷却用自下而上的半透明深色遮罩和剩余秒数；空槽画石槽轮廓。所有文字走现有 `HudFont`，不得用 raylib 默认字体。

- [ ] **Step 4: 实现技能原生几何表现**

  拔刀斩：按 snapshot facing 和命中 tick 绘制 0.12 秒白蓝扇形弧光，使用 `DrawTriangleFan`/线段，不加载外部纹理。暴风式：围绕 locked center 以固定 12 个角度绘制半透明剑形；当前 strike 高亮；finisher 绘制中心落剑、圆形冲击波和短屏闪。表现只消费快照/事件，不能改变伤害或触发时机。

- [ ] **Step 5: 实现暂停页主动槽操作**

  在现有暂停→背包页面增加“技能石”子页按钮，不改变装备/材料页默认行为。交互规则固定为：单击主槽选中；选中已装备槽后点“取出”发 remove；选中一个库存石后点击空主槽发 equip；依次点击两个已装备主槽发 swap；保存 pending 时禁用全部装配按钮并显示“正在保存”；失败显示现有保存错误通知且保持原模型。五个辅助框只画“空”，不响应点击。

- [ ] **Step 6: 运行 GREEN、字体和三分辨率布局回归**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_platform_tests
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|stage11c\.hud_stress\.zero_alloc_100k)$' --output-on-failure
  ```
  Expected: 五槽/暂停页模型通过，现有 HUD 字体、背包命中和 100k 零分配测试不回归。

- [ ] **Step 7: 提交界面任务卡**

  ```powershell
  git add src/platform/raylib tests/platform
  git commit -m "feat: add active skill hud and loadout screen"
  ```

---


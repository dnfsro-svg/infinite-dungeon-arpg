### Task 8: 完成真实 Raylib 验收、文档和整体验证

**Files:**
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `tests/platform/stage17_skill_stones_game_validation.cpp`
- Create: `tests/platform/stage17_skill_stones_validator.ps1`
- Create: `tests/platform/stage17_skill_stones_validator_self_test.ps1`
- Create: `docs/validation/stage17-swordmaster-skill-stones.md`
- Modify: `README.md`

- [ ] **Step 1: 先注册 Stage 17 正向和负向证据测试**

  注册：
  - `stage17.skill_stones.real_raylib`
  - `stage17.skill_stones.evidence_validator`
  - `stage17.skill_stones.evidence_validator_self_test`

  第一个运行真实 raylib 6.0/OpenGL、真实 `SaveStore` 和生产 `DungeonRuntime`；第二个校验新鲜文件；第三个复制证据并分别破坏 PNG、状态文件字段和时间戳，断言 validator 拒绝。

- [ ] **Step 2: 运行 RED，确认因验证场景尚未实现而失败**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_stage17_skill_stones_game_validation
  ctest --test-dir out/build/windows-msvc-debug -L stage17 --output-on-failure
  ```
  Expected: 缺少正式场景或证据文件，不能假通过。

- [ ] **Step 3: 实现生产路径正式场景**

  场景固定 1280×720，并通过公开输入/事务路径依次完成：
  1. 新档截图五槽默认状态；
  2. 用数字键 1 施放拔刀斩并在命中帧截图；
  3. 用数字键 2 施放暴风式并在剑阵与终结各截图；
  4. 暂停页取出槽 1、把拔刀斩装入槽 5、交换槽 2/5；
  5. 关闭并重启真实 runtime，截图并核验交换后装配仍在；
  6. 输出 UTF-8 状态文件，包含 V8、五槽 ID、owned bits、25 个辅助 none、两技能命中计数、暴风式中心不随玩家移动、重启后装配。

- [ ] **Step 4: 实现证据验证器**

  必须检查 5 张非空 PNG 均为 1280×720、修改时间晚于本次 run marker、像素内容互不相同；状态文件所有字段精确匹配；不得仅检查文件存在。所有清理限定在测试提供的 `stage17-run` 子目录，先解析绝对路径并验证仍位于 evidence root 内。

- [ ] **Step 5: 运行 Stage 17 专项 GREEN**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_stage17_skill_stones_game_validation arpg_platform_tests arpg_dungeon_tests arpg_combat_tests arpg_persistence_tests arpg_skills_tests
  ctest --test-dir out/build/windows-msvc-debug -L stage17 --output-on-failure
  ```
  Expected: 正向真实 Raylib、validator 和 validator self-test 全通过。

- [ ] **Step 6: 运行 Debug 非图形完整回归和相关图形套件**

  ```powershell
  ctest --test-dir out/build/windows-msvc-debug -LE 'graphics|formal-game' --output-on-failure
  ctest --test-dir out/build/windows-msvc-debug -R '^(stage16\.loot_reinforcement\.real_raylib|stage17\.skill_stones\.)' --output-on-failure
  ```
  Expected: 所有非图形测试及 Stage 16/17 真实 Raylib 通过；若旧 Stage 9 图形基线仍失败，仅按已有记录列出，不能算作 Stage 17 通过项。

- [ ] **Step 7: 运行 Release 构建和 Stage 17 测试**

  ```powershell
  cmake --preset windows-msvc-release
  cmake --build --preset windows-msvc-release
  ctest --test-dir out/build/windows-msvc-release -L stage17 --output-on-failure
  ctest --test-dir out/build/windows-msvc-release -LE 'graphics|formal-game' --output-on-failure
  ```
  Expected: Release 编译成功，Stage 17 与非图形完整回归通过。

- [ ] **Step 8: 更新验证记录并做规格自审**

  `docs/validation/stage17-swordmaster-skill-stones.md` 记录准确命令、通过数量、耗时、五张绝对证据路径、V8 迁移、两技能数值和已知基线异常。README 把当前阶段改为 Stage 17。逐条对照设计文档，确认：5 主槽、每槽 5 空辅助位、全槽可取出/替换/交换、默认 1/2、空 3～5、原子保存、冷却不持久化、两技能战斗行为都已覆盖。

- [ ] **Step 9: 扫描占位实现、非法依赖和测试后门**

  ```powershell
  rg -n "TODO|TBD|placeholder|not implemented|test injection|private injection" src tests docs/validation/stage17-swordmaster-skill-stones.md
  rg -n "IsKeyPressed|IsKeyDown|GetKeyPressed" src/platform/raylib -g '!host_input.cpp' -g '!raylib_input.cpp' -g '!stable_key_raylib.cpp'
  ctest --test-dir out/build/windows-msvc-debug -R '^architecture\.' --output-on-failure
  git diff --check
  ```
  Expected: 新增文件无占位行为、数字键没有旁路采样、模块边界通过、无空白错误。

- [ ] **Step 10: 提交最终验收任务卡**

  ```powershell
  git add README.md src/platform/raylib/raylib_host.hpp src/platform/raylib/raylib_host.cpp tests/platform docs/validation/stage17-swordmaster-skill-stones.md
  git commit -m "test: validate stage17 skill stones in raylib"
  ```

---

## Final completion checklist

- [ ] `git status --short` 为空，没有混入用户既有或无关改动。
- [ ] `1` 施放拔刀斩，`2` 施放暴风式，`3`～`5` 空槽无副作用。
- [ ] 五个主动槽均可取出、装入、交换，保存失败不半更新。
- [ ] 每个主槽恰好显示五个只读空辅助槽，战斗不读取辅助数组。
- [ ] 拔刀斩每目标一次、明确击退；暴风式锁定中心、12 段、终结击飞并可被死亡/换房取消。
- [ ] V8 往返、V1～V7 迁移、非法 ID/重复石拒绝全部有测试。
- [ ] Debug/Release 的 Stage 17、新增单元和非图形完整回归均有新鲜结果。
- [ ] 真实 1280×720 Raylib 五张截图和状态证据通过内容验证及破坏性自测。

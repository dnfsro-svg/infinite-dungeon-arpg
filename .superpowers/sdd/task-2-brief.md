### Task 2: 将装配写入 V8 检查点并迁移 V1～V7

**Files:**
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/CMakeLists.txt`
- Modify: `tests/persistence/CMakeLists.txt`
- Create: `tests/persistence/checkpoint_v8_skill_loadout_tests.cpp`
- Modify: `tests/persistence/persistence_test_main.cpp`

- [ ] **Step 1: 写 V8 往返、迁移和拒绝测试**

  覆盖：任意五槽排列往返；空槽；owned 位往返；V7 字节流迁移为默认装配；V1 固定样本同样迁移；主动 ID `2`、辅助 ID `0`、重复主动石、槽中非 owned 主动石均返回 `CodecError::invalid_state` 或 `invalid_enum`；V7 既有 golden 字节不改变。

- [ ] **Step 2: 运行 RED**

  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_persistence_tests
  ctest --test-dir out/build/windows-msvc-core-debug -R '^persistence\.units$' --output-on-failure
  ```
  Expected: 新 V8 测试因版本仍为 7 或状态未编码而失败。

- [ ] **Step 3: 扩展检查点状态和固定布局**

  在 `DungeonRunState` 末尾增加：
  ```cpp
  skills::SkillLoadoutState skill_loadout{
      skills::default_skill_loadout()};
  ```
  版本常量改为：
  ```cpp
  inline constexpr std::uint32_t kSeventhCheckpointFormatVersion = 7U;
  inline constexpr std::uint32_t kCheckpointFormatVersion = 8U;
  inline constexpr std::size_t kV8SkillLoadoutPayloadSize = 40U;
  inline constexpr std::size_t kV8BasePayloadSize = 756U;
  inline constexpr std::size_t kV8BaseEncodedCheckpointSize = 788U;
  ```
  40 字节按顺序为：`owned_active_bits` 8 字节 little-endian、5 个主动 ID、25 个辅助 ID、2 个保留零字节。物品可变记录仍紧随 base payload；不要改变 V7 的 716 字节解析偏移。

- [ ] **Step 4: 实现 V8 编解码与旧档默认迁移**

  编码前调用 `validate_skill_loadout`；解码逐个验证 enum，再验证整体不变量和两个保留字节为零。V1～V7 的成功路径在完成各自原有迁移后统一赋值 `default_skill_loadout()` 并保持 `migrated=true`。把 persistence 公开链接到 `arpg_skills`，不要让 skills 反向依赖 persistence。

- [ ] **Step 5: 运行 GREEN、V7 golden 和边界回归**

  ```powershell
  cmake --build --preset windows-msvc-core-debug --target arpg_persistence_tests
  ctest --test-dir out/build/windows-msvc-core-debug -R '^(persistence\.units|architecture\.persistence_checkpoint_only)$' --output-on-failure
  ```
  Expected: V8 新测试、全部旧版本迁移和 persistence 边界测试通过。

- [ ] **Step 6: 提交存档任务卡**

  ```powershell
  git add src/dungeon/dungeon_checkpoint.hpp src/persistence tests/persistence
  git commit -m "feat: persist skill stone loadout in checkpoint v8"
  ```

---


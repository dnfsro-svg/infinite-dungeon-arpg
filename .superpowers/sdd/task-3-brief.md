## Task 3: 固定四向门预告并排除初始房/下层房

**Files:**

- Modify: `src/dungeon/room_generation.hpp`
- Modify: `src/dungeon/room_generation.cpp`
- Modify: `src/dungeon/dungeon_types.hpp`
- Modify: `src/dungeon/dungeon_snapshot.cpp`
- Modify: `src/dungeon/dungeon_transition.cpp`
- Modify: `tests/dungeon/room_generation_tests.cpp`
- Modify: `tests/dungeon/dungeon_navigation_tests.cpp`
- Modify: `tests/dungeon/dungeon_test_main.cpp`

**Produces:** `AbyssDoorPreview` 四向快照，门预告与实际目标完全一致。

**Consumes:** `derive_door_room_seed` 和 Task 1 的 `is_abyss_roll`；不消费任何现有顺序 RNG。

- [ ] 增加 10 个测试，覆盖四方向 seed、0～4 扇深渊门、预告/提交一致、初始房 false、下层目标 false、洞与深渊共存、bias 变化不改变深渊门、门预告不泄露规则。

新增纯函数和快照字段：

```cpp
[[nodiscard]] std::array<bool, 4> preview_abyss_doors(
    const checkpoint::RoomDescriptor& current) noexcept;

struct DungeonSnapshot final {
    std::array<bool, 4> abyss_doors{};
};
```

`preview_abyss_doors` 对每个方向使用 `derive_door_room_seed(current.seed, current.index + 1, direction)`，随后调用 `is_abyss_roll`。目标房生成不再从 `generate_room_descriptor` 的旧 `samples.abyss` 决定资格：门路径显式传入预告结果；初始和 descent 路径显式写 `false`。门 transition 的 next state 若命中深渊，还必须用目标 room seed + depth 写入 available、rule、danger 和 rules version，但快照的门预告只暴露布尔图标。

- [ ] 先运行并观察预告字段/初始房断言失败：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests
& .\out\build\windows-msvc-debug\bin\arpg_dungeon_tests.exe
```

- [ ] 实现后运行 dungeon 与 persistence 回归：

```powershell
ctest --preset windows-msvc-debug -R "dungeon.units|persistence.units" --output-on-failure
```

预期：两组通过；旧 ecology/hole golden samples 不变。

- [ ] 提交：

```powershell
git add src/dungeon tests/dungeon
git commit -m "feat: preview deterministic abyss door targets"
```

---


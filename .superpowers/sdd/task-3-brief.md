## Task 3：实现纯事件路由、轮换和每 tick 预算

**Files:**
- Create: `src/platform/raylib/audio_routing.hpp`
- Create: `src/platform/raylib/audio_routing.cpp`
- Create: `tests/platform/audio_routing_tests.cpp`
- Modify: `src/platform/raylib/combat_audio.hpp`
- Modify: `src/platform/raylib/combat_audio.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/combat_feedback_tests.cpp`
- Modify: `tests/platform/monster_view_tests.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
enum class AudioCue : std::uint16_t {
    swing_light, swing_finisher, swing_launcher, impact, impact_low,
    player_hurt, landing, enemy_defeat,
    warning_blink, warning_chain, warning_death
};

struct AudioPlan final { AudioCueMask cues{}; };

[[nodiscard]] AudioPlan route_audio_plan(
    const combat::CombatEvent& event) noexcept;

class AudioSelectionState final {
public:
    [[nodiscard]] AudioAssetId select(AudioCue cue) noexcept;
    void reset() noexcept;
};

class AudioPlaybackBudget final {
public:
    [[nodiscard]] bool allow(AudioCue cue, std::uint64_t tick) noexcept;
    void reset() noexcept;
};
```

- [ ] **Step 1: 添加 8 个失败用例**

覆盖：J1/J2/air J 到轻挥组；J3；launcher；hit/heavy/player_hit/landing/defeated；三类 warning；2 路与 3 路轮换；reset；每类同 tick 上限；tick 回绕；100,000 次路由/预算更新零分配。

Run: `cmake --build --preset windows-msvc-release --target arpg_platform_tests`

Expected before implementation: `audio_routing.hpp` 或新接口缺失。

- [ ] **Step 2: 移出并扩展现有路由逻辑**

将 `AudioCue`、`route_audio_cues` 和 `WarningAudioThrottle` 从 `combat_audio.*` 迁移到纯逻辑模块。兼容测试直接改为包含 `audio_routing.hpp`，不保留第二份路由实现。

- [ ] **Step 3: 实现确定性选择和预算**

上限固定为：swing 1、impact 3、impact_low 1、player_hurt 1、landing 1、enemy_defeat 2、三种 warning 各 1。warning 还需满足独立 12 tick 限流。`reset`、`room_destroyed`、`room_reset` 只清计数状态。

**现有事件边界说明：** `reset` 是 `CombatEventKind`；`room_destroyed` 和 `room_reset` 是 `DungeonEventKind`，宿主已在这两类事件上调用 `CombatAudio::stop_all()`。本任务不得让纯路由模块依赖 dungeon；应让 `CombatEventKind::reset` 以及 `CombatAudio::stop_all()` 都复位 `AudioSelectionState` 和 `AudioPlaybackBudget`，从而覆盖三条现有生产路径。不要新增重复事件或核心层依赖。

- [ ] **Step 4: 注册并运行 324 个平台用例**

更新 `platform_test_main.cpp` 的期望总数为 324。

Run: `ctest --test-dir out/build/windows-msvc-release -R platform.units --output-on-failure`

Expected: 324 个用例通过，旧 Stage 13 音频 cue 断言改用新接口后保持原语义。

- [ ] **Step 5: 精确提交 Task 3**

```powershell
git add src/platform/raylib/audio_routing.hpp src/platform/raylib/audio_routing.cpp src/platform/raylib/combat_audio.hpp src/platform/raylib/combat_audio.cpp src/platform/raylib/CMakeLists.txt tests/platform/audio_routing_tests.cpp tests/platform/combat_feedback_tests.cpp tests/platform/monster_view_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git commit -m "refactor: route deterministic combat audio"
```

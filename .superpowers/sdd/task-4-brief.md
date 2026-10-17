## Task 4：实现逐资源加载和程序化回退

**Files:**
- Create: `src/platform/raylib/procedural_audio.hpp`
- Create: `src/platform/raylib/procedural_audio.cpp`
- Create: `src/platform/raylib/audio_pack.hpp`
- Create: `src/platform/raylib/audio_pack.cpp`
- Create: `tests/platform/audio_pack_tests.cpp`
- Modify: `src/platform/raylib/combat_audio.hpp`
- Modify: `src/platform/raylib/combat_audio.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
struct AudioSoundApi final {
    Wave (*load_wave)(const char* path){};
    bool (*wave_valid)(Wave wave){};
    Sound (*load_sound_from_wave)(Wave wave){};
    bool (*sound_valid)(Sound sound){};
    void (*unload_wave)(Wave wave){};
    void (*unload_sound)(Sound sound){};
    void (*play_sound)(Sound sound){};
    void (*stop_sound)(Sound sound){};
};

class AudioPack final {
public:
    AudioPack() noexcept;
    explicit AudioPack(AudioSoundApi api) noexcept;
    [[nodiscard]] bool load() noexcept;
    void play(AudioAssetId id) noexcept;
    void stop_all() noexcept;
    void unload() noexcept;
    [[nodiscard]] bool available(AudioAssetId id) const noexcept;
    [[nodiscard]] bool using_fallback(AudioAssetId id) const noexcept;
};
```

- [ ] **Step 1: 添加 8 个假 API 失败用例**

覆盖：14 个成功加载；一个缺失只回退一个；格式错误只回退一个；外部 Sound 创建失败后回退；全部缺失仍 ready；不完整 API 安全失败；只卸载有效对象；重复 `unload()` 安全且不重复卸载。

Run: `cmake --build --preset windows-msvc-release --target arpg_platform_tests`

Expected before implementation: 新 `AudioPack` 接口缺失。

- [ ] **Step 2: 把现有合成波形抽成 14 个语义回退**

继续使用固定样本数组、固定种子噪声和 22,050 Hz/16-bit/mono Wave 生成 Sound；不同 ID 可以共享合成器形状，但每个 ID 必须独立创建、独立记录 fallback 状态，不使用堆容器。

- [ ] **Step 3: 实现逐资源加载、验证和回退**

每项按 `LoadWave -> 元数据/峰值校验 -> LoadSoundFromWave -> UnloadWave` 执行。失败项立即转程序化 Wave；任何临时 Wave 都只卸载一次。清单级失败时 14 项全部回退，但音频设备可用时 `AudioPack::load()` 仍返回 true。

- [ ] **Step 4: 用 AudioPack 重写 CombatAudio 内部实现**

保留 `initialize()`、`consume_event()`、`stop_all()`、`shutdown()`、`ready()` 公共签名。`consume_event()` 只调用纯路由、预算、选择和 `AudioPack::play()`；房间重置事件先停止全部 Sound，再复位选择和预算。

- [ ] **Step 5: 注册并运行 332 个平台用例**

更新 `platform_test_main.cpp` 的期望总数为 332。

Run: `ctest --test-dir out/build/windows-msvc-release -R platform.units --output-on-failure`

Expected: 332 个用例通过；假 API 证明单资源回退和关闭幂等。

- [ ] **Step 6: 精确提交 Task 4**

```powershell
git add src/platform/raylib/procedural_audio.hpp src/platform/raylib/procedural_audio.cpp src/platform/raylib/audio_pack.hpp src/platform/raylib/audio_pack.cpp src/platform/raylib/combat_audio.hpp src/platform/raylib/combat_audio.cpp src/platform/raylib/CMakeLists.txt tests/platform/audio_pack_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git commit -m "feat: load combat audio with per-asset fallback"
```


## Task 2：建立稳定清单和纯资源校验

**Files:**
- Create: `src/platform/raylib/audio_asset_types.hpp`
- Create: `src/platform/raylib/audio_manifest.hpp`
- Create: `src/platform/raylib/audio_asset_validation.hpp`
- Create: `src/platform/raylib/audio_asset_validation.cpp`
- Create: `tests/platform/audio_asset_validation_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**

```cpp
enum class AudioAssetId : std::uint8_t {
    swing_light_1, swing_light_2, swing_finisher, swing_launcher,
    impact_1, impact_2, impact_3, impact_low, player_hurt, landing,
    enemy_defeat, warning_blink, warning_chain, warning_death, count
};

struct AudioDecodedMetadata final {
    unsigned int frame_count{};
    unsigned int sample_rate{};
    unsigned int sample_size{};
    unsigned int channels{};
    float peak{};
};

[[nodiscard]] AudioValidationResult validate_audio_manifest(
    const AudioManifestDefinition& manifest) noexcept;
[[nodiscard]] AudioValidationResult validate_audio_metadata(
    const AudioManifestEntry& entry,
    const AudioDecodedMetadata& metadata) noexcept;
```

- [ ] **Step 1: 添加 8 个失败用例**

覆盖：恰好 14 个且 ID 唯一；重复 ID；绝对路径；`..` 逃逸；采样率/位深/声道错误；零时长或超过 1.5 秒；静音/削波；总 PCM 超过 8 MiB。

Run: `cmake --build --preset windows-msvc-release --target arpg_platform_tests`

Expected before implementation: 新头文件或符号缺失导致构建失败。

- [ ] **Step 2: 实现固定清单和无 raylib 校验**

清单路径统一以 `assets/stage14/audio/` 开头；路径检查拒绝盘符、根路径、反斜杠逃逸和空段；PCM 字节按 `frame_count * channels * sample_size / 8` 使用溢出安全计算。峰值必须 `> 0.0F && < 1.0F`。

- [ ] **Step 3: 注册实现和测试套件**

将新源文件加入 `arpg_raylib`，将测试源加入 `arpg_platform_tests`，在 `platform_test_main.cpp` 注册 8 个测试并把期望用例数从 308 更新为 316。

- [ ] **Step 4: 构建并运行平台测试**

Run: `cmake --build --preset windows-msvc-release --target arpg_platform_tests`

Run: `ctest --test-dir out/build/windows-msvc-release -R "platform.units|stage14.audio_sources" --output-on-failure`

Expected: 平台 316 个用例通过，资源源验证通过。

- [ ] **Step 5: 精确提交 Task 2**

```powershell
git add src/platform/raylib/audio_asset_types.hpp src/platform/raylib/audio_manifest.hpp src/platform/raylib/audio_asset_validation.hpp src/platform/raylib/audio_asset_validation.cpp src/platform/raylib/CMakeLists.txt tests/platform/audio_asset_validation_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git commit -m "feat: validate stage 14 audio manifest"
```


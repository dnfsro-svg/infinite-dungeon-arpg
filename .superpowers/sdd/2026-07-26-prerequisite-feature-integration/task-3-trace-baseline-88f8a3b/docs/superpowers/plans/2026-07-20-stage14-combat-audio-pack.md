# Stage 14 低资源战斗音效材质包 Implementation Plan

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** 在不改变战斗、地下城、随机、掉落、死亡和存档规则的前提下，用 14 个随游戏发布的 CC0 短音效建立可区分、可回退、可验证的战斗声音层。

**Architecture:** 资源通过固定脚本下载、校验并离线标准化；编译期清单和纯 C++ 校验层定义稳定 ID 与格式预算；纯事件路由把既有 `CombatEvent` 转换为 cue；`AudioPack` 通过可注入 raylib 函数表逐资源加载，失败项独立使用程序化回退；`CombatAudio` 只负责设备生命周期、确定性轮换、每 tick 播放预算和停止/卸载。所有资源路径、raylib 类型和播放状态留在 `src/platform/raylib`，不进入核心模拟。

**Tech Stack:** C++17、raylib 6.0、CMake、Ninja/MSVC、CTest、PowerShell 7/Windows PowerShell、FFmpeg（只用于资源准备，不属于普通构建或运行时依赖）。

## Global Constraints

- 严格实现设计规格中的 14 个稳定 ID，不增加背景音乐、环境循环、UI、拾取、开门或洞口音效。
- 发布文件固定为 44,100 Hz、16-bit、单声道 PCM WAV；每段 `0 < duration <= 1.5s`；解码 PCM 总量不超过 8 MiB。
- `src/core`、`src/combat`、`src/dungeon` 不得出现 Stage 14 资源路径或 raylib 音频依赖。
- 运行时不联网、不调用 FFmpeg、不读压缩包；全部 Sound 只在启动时创建，在关闭时卸载。
- 单资源失败只回退该 ID；音频设备失败时静默继续游戏，不影响输入、模拟或渲染。
- 轮换、限流和 reset 不读取任何游戏随机流；音频装饰的丢弃不得丢弃战斗事件或奖励。
- 热路径使用固定容量数组和整数计数器，不分配堆内存、不加载文件、不创建 Wave/Sound。
- 保留当前工作树的历史证据和未提交用户改动；不删除文件、不重置 Git、不批量暂存无关文件。
- 每项任务遵守红灯、绿灯、重构顺序；每个提交只包含该任务明确列出的文件。

---

## File Structure

- `scripts/PrepareStage14Audio.ps1`: 下载两个 Kenney 官方 ZIP、校验原文件哈希、提取 14 个源文件并用固定 FFmpeg 参数生成发布 WAV。
- `assets/stage14/audio/*.wav`: 14 个运行时短音效。
- `assets/stage14/audio/SOURCES.md`: 官方来源、许可、原文件名、原文件 SHA-256、发布文件 SHA-256 和转换命令。
- `src/platform/raylib/audio_asset_types.hpp`: 稳定 ID、清单条目、解码元数据、错误码和预算常量。
- `src/platform/raylib/audio_manifest.hpp`: 14 条编译期清单及相对路径。
- `src/platform/raylib/audio_asset_validation.hpp/.cpp`: 不依赖设备的清单和解码元数据校验。
- `src/platform/raylib/audio_routing.hpp/.cpp`: 纯 `CombatEvent -> AudioPlan` 路由、确定性选择和每 tick 预算。
- `src/platform/raylib/procedural_audio.hpp/.cpp`: 14 个语义的确定性程序化回退 Wave。
- `src/platform/raylib/audio_pack.hpp/.cpp`: 可注入 raylib API 的逐资源加载、回退、停止和卸载。
- `src/platform/raylib/combat_audio.hpp/.cpp`: 设备生命周期与事件消费门面，保留现有公开接口。
- `src/app/CMakeLists.txt`: 将 `assets/stage14/audio` 复制到 `arpg_game.exe` 同级发布目录。
- `src/platform/raylib/CMakeLists.txt`: 注册 Stage 14 平台层实现文件。
- `tests/platform/audio_asset_validation_tests.cpp`: 清单、路径、格式和预算测试。
- `tests/platform/audio_routing_tests.cpp`: 路由、轮换、限流、reset 和零分配测试。
- `tests/platform/audio_pack_tests.cpp`: 假 raylib API 下的部分失败、回退和生命周期测试。
- `tests/platform/stage14_audio_formal_game_validation.cpp`: 真实 raylib 解码、损坏副本回退、事件 trace 和试听 WAV 生成。
- `tests/platform/stage14_audio_validator.ps1`: 验证正式证据、资源哈希和试听文件。
- `tests/platform/stage14_audio_source_validator.ps1`: 验证已提交的 14 个发布资源与 `SOURCES.md`。
- `tests/platform/stage14_audio_integration_guard.cmake`: 防止资源路径越界、热路径加载和核心层反向依赖。
- `docs/validation/stage14-combat-audio-pack.md`: 最终命令、测试结果、资源表、回退结果、真实启动和限制。
- `docs/validation/evidence/stage14-audio-material-pack/`: 正式 evidence text 与 showcase WAV。

## Task 1：固定来源并生成 14 个发布资源

**Files:**
- Create: `scripts/PrepareStage14Audio.ps1`
- Create: `assets/stage14/audio/SOURCES.md`
- Create: `assets/stage14/audio/swing-light-1.wav`
- Create: `assets/stage14/audio/swing-light-2.wav`
- Create: `assets/stage14/audio/swing-finisher.wav`
- Create: `assets/stage14/audio/swing-launcher.wav`
- Create: `assets/stage14/audio/impact-1.wav`
- Create: `assets/stage14/audio/impact-2.wav`
- Create: `assets/stage14/audio/impact-3.wav`
- Create: `assets/stage14/audio/impact-low.wav`
- Create: `assets/stage14/audio/player-hurt.wav`
- Create: `assets/stage14/audio/landing.wav`
- Create: `assets/stage14/audio/enemy-defeat.wav`
- Create: `assets/stage14/audio/warning-blink.wav`
- Create: `assets/stage14/audio/warning-chain.wav`
- Create: `assets/stage14/audio/warning-death.wav`
- Create: `tests/platform/stage14_audio_source_validator.ps1`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Input: 两个固定官方 ZIP URL、14 个原文件路径和预期原文件 SHA-256。
- Output: 确定内容的 14 个 PCM WAV、可审计来源表、非零退出码验证器。

- [ ] **Step 1: 先写资源验证器并确认红灯**

验证器要求：目录仅含 14 个预期 WAV 和 `SOURCES.md`；每个文件非空；RIFF/WAVE、PCM format 1、44,100 Hz、16-bit、mono；持续时间不超过 1.5 秒；总 PCM 不超过 8 MiB；来源表包含 14 个原文件 SHA 和 14 个发布 SHA。

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/platform/stage14_audio_source_validator.ps1 -AssetDirectory assets/stage14/audio`

Expected before assets: 失败并明确报告目录或首个缺失资源。

- [ ] **Step 2: 实现幂等且路径安全的资源准备脚本**

脚本固定下载：

```text
https://kenney.nl/media/pages/assets/rpg-audio/8e99002d76-1677590336/kenney_rpg-audio.zip
https://kenney.nl/media/pages/assets/impact-sounds/87b4ddecda-1677589768/kenney_impact-sounds.zip
```

固定映射：

```text
swing-light-1.wav  <- RPG Audio/knifeSlice.ogg
swing-light-2.wav  <- RPG Audio/knifeSlice2.ogg
swing-finisher.wav <- RPG Audio/chop.ogg
swing-launcher.wav <- RPG Audio/drawKnife3.ogg
impact-1.wav       <- Impact Audio/impactPunch_medium_000.ogg
impact-2.wav       <- Impact Audio/impactPunch_medium_001.ogg
impact-3.wav       <- Impact Audio/impactPunch_medium_002.ogg
impact-low.wav     <- Impact Audio/impactPunch_heavy_004.ogg
player-hurt.wav    <- Impact Audio/impactSoft_heavy_003.ogg
landing.wav        <- Impact Audio/footstep_concrete_004.ogg
enemy-defeat.wav   <- Impact Audio/impactMetal_heavy_004.ogg
warning-blink.wav  <- Impact Audio/impactBell_heavy_004.ogg
warning-chain.wav  <- Impact Audio/impactMetal_medium_004.ogg
warning-death.wav  <- Impact Audio/impactGlass_heavy_004.ogg
```

每个源文件先比对设计规格调研记录中的 SHA-256，再运行：

`ffmpeg -nostdin -y -i <source> -ac 1 -ar 44100 -c:a pcm_s16le <output>`

临时目录必须位于脚本创建的唯一子目录；清理前验证解析后的绝对路径仍位于该子目录，不能递归删除调用者目录。

- [ ] **Step 3: 生成资源与来源表并通过验证**

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/PrepareStage14Audio.ps1`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/platform/stage14_audio_source_validator.ps1 -AssetDirectory assets/stage14/audio`

Expected: `14/14 PASS`，格式和总预算合格，`SOURCES.md` 中发布哈希与文件一致。

- [ ] **Step 4: 将资源验证注册为 CTest**

测试名固定为 `stage14.audio_sources`，标签 `headless;platform;stage14`，普通测试不触发下载或 FFmpeg。

- [ ] **Step 5: 精确提交 Task 1**

```powershell
git add scripts/PrepareStage14Audio.ps1 assets/stage14/audio tests/platform/stage14_audio_source_validator.ps1 tests/platform/CMakeLists.txt
git commit -m "assets: add stage 14 combat audio sources"
```

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

- [ ] **Step 4: 注册并运行 324 个平台用例**

更新 `platform_test_main.cpp` 的期望总数为 324。

Run: `ctest --test-dir out/build/windows-msvc-release -R platform.units --output-on-failure`

Expected: 324 个用例通过，旧 Stage 13 音频 cue 断言改用新接口后保持原语义。

- [ ] **Step 5: 精确提交 Task 3**

```powershell
git add src/platform/raylib/audio_routing.hpp src/platform/raylib/audio_routing.cpp src/platform/raylib/combat_audio.hpp src/platform/raylib/combat_audio.cpp src/platform/raylib/CMakeLists.txt tests/platform/audio_routing_tests.cpp tests/platform/combat_feedback_tests.cpp tests/platform/monster_view_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git commit -m "refactor: route deterministic combat audio"
```

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

## Task 5：接入发布目录并锁定架构边界

**Files:**
- Modify: `src/app/CMakeLists.txt`
- Create: `tests/platform/stage14_audio_integration_guard.cmake`
- Modify: `tests/platform/CMakeLists.txt`

**Interfaces:**
- Production lookup: `assets/stage14/audio/<name>.wav`，与现有 Stage 12 相对发布目录策略一致。
- Guard output: 发现核心层资源路径、热路径加载、运行时 FFmpeg/联网或缺少发布复制命令时失败。

- [ ] **Step 1: 先写集成守卫并确认缺少发布复制时红灯**

守卫扫描：`src/core`、`src/combat`、`src/dungeon` 不得包含 `assets/stage14`、`AudioAssetId` 或 raylib 音频 API；`consume_event` 不得调用 `LoadWave`/`LoadSound`；普通源文件不得出现下载 URL、`ffmpeg` 或进程启动；`src/app/CMakeLists.txt` 必须复制 Stage 14 目录。

Run: `cmake -P tests/platform/stage14_audio_integration_guard.cmake`

Expected before CMake packaging change: 失败并报告缺少 Stage 14 copy rule。

- [ ] **Step 2: 将资产复制到游戏发布目录**

在现有 Stage 12 `POST_BUILD` 后增加独立 Stage 14 `copy_directory`，源为 `${PROJECT_SOURCE_DIR}/assets/stage14/audio`，目标为 `$<TARGET_FILE_DIR:arpg_game>/assets/stage14/audio`。

- [ ] **Step 3: 注册并运行架构守卫**

CTest 名固定为 `stage14.audio_integration_guard`，标签 `headless;platform;architecture;stage14`。

Run: `ctest --test-dir out/build/windows-msvc-release -R "stage14.audio_sources|stage14.audio_integration_guard" --output-on-failure`

Expected: 两项通过。

- [ ] **Step 4: 构建游戏并核对发布文件**

Run: `cmake --build --preset windows-msvc-release --target arpg_game`

Run: `powershell.exe -NoProfile -Command "(Get-ChildItem out/build/windows-msvc-release/src/app/assets/stage14/audio -Filter *.wav).Count"`

Expected: 输出 `14`。

- [ ] **Step 5: 精确提交 Task 5**

```powershell
git add src/app/CMakeLists.txt tests/platform/stage14_audio_integration_guard.cmake tests/platform/CMakeLists.txt
git commit -m "build: package stage 14 combat audio"
```

## Task 6：生成真实 raylib 正式证据和试听文件

**Files:**
- Create: `tests/platform/stage14_audio_formal_game_validation.cpp`
- Create: `tests/platform/stage14_audio_validator.ps1`
- Modify: `tests/platform/CMakeLists.txt`
- Create: `docs/validation/evidence/stage14-audio-material-pack/stage14-audio-evidence.txt`
- Create: `docs/validation/evidence/stage14-audio-material-pack/stage14-audio-showcase.wav`

**Interfaces:**
- Formal executable args: `<scratch-evidence-root> <committed-evidence-root>`；另支持 `--root-safety-self-test <candidate-root>`。
- Evidence text: 14 项路径、SHA-256、格式、帧数、时长、峰值、PCM 字节、fallback 结果、事件 trace 和总预算。
- Showcase order: 轻击 A/B、终结、上挑、命中 A/B/C、低频、受击、落地、击杀、blink、chain、death；段间插入固定 250ms 静音。

- [ ] **Step 1: 写证据验证器并确认无证据时红灯**

验证器拒绝：缺文件、条目少于或多于 14、哈希不匹配、格式错误、静音/削波、PCM 超预算、缺少 `single_resource_fallback=PASS`、缺少全部 cue trace、showcase 不是合法 PCM WAV。

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/platform/stage14_audio_validator.ps1 -EvidenceDirectory docs/validation/evidence/stage14-audio-material-pack`

Expected before evidence: 失败并报告首个缺失文件。

- [ ] **Step 2: 实现真实 raylib 解码和 evidence 生成器**

使用 `LoadWave` 解码与游戏相同的 14 个文件，扫描 PCM 计算峰值/静音和总字节；复制一个文件到受控临时目录后破坏 RIFF 头，证明只有对应 ID 使用 fallback。通过注入 play 函数记录公开 `CombatEvent` 覆盖全部 cue 的 trace。生成 showcase 时只拼接解码 PCM 与固定静音，不修改战斗私有状态。

- [ ] **Step 3: 注册三项正式测试**

CTest：

```text
stage14.audio_formal               FIXTURES_SETUP stage14_audio_evidence
stage14.audio_evidence_validator   FIXTURES_REQUIRED stage14_audio_evidence
stage14.audio_root_safety
```

正式测试 `RUN_SERIAL TRUE`，临时证据写入构建目录，确认通过后才把相同内容复制到 committed evidence 目录。

- [ ] **Step 4: 运行并验证正式证据**

Run: `cmake --build --preset windows-msvc-release --target arpg_stage14_audio_formal`

Run: `ctest --test-dir out/build/windows-msvc-release -R "stage14.audio_(formal|evidence_validator|root_safety)" --output-on-failure`

Run: `powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/platform/stage14_audio_validator.ps1 -EvidenceDirectory docs/validation/evidence/stage14-audio-material-pack`

Expected: 三项 CTest 和 committed evidence 验证全部通过；showcase 可被 `ffprobe` 识别为 44.1 kHz、16-bit、mono WAV。

- [ ] **Step 5: 精确提交 Task 6**

```powershell
git add tests/platform/stage14_audio_formal_game_validation.cpp tests/platform/stage14_audio_validator.ps1 tests/platform/CMakeLists.txt docs/validation/evidence/stage14-audio-material-pack
git commit -m "test: add stage 14 audio evidence"
```

## Task 7：完整回归、真实游戏启动和里程碑报告

**Files:**
- Create: `docs/validation/stage14-combat-audio-pack.md`
- Modify: `README.md`
- Preserve: `docs/validation/evidence/stage10-formal-game-validation/`
- Preserve: `docs/validation/evidence/stage11d/`
- Preserve: `docs/validation/evidence/stage12-material-pack/`

**Interfaces:**
- Input: Release 全量构建、全部 CTest、发布目录 `arpg_game.exe`、Stage 14 evidence/showcase。
- Output: 可重复验收报告与仅包含 Stage 14 文档的最终提交。

- [ ] **Step 1: 审计正式范围和工作树清洁度**

Run: `git status --short`

Run: `git diff -- src/core src/combat src/dungeon src/loot src/persistence`

Expected: Stage 14 未修改模拟、地下城、掉落或存档；除有意 Stage 14 文件外无新增脏改动。

- [ ] **Step 2: 运行 Release 全量构建和全部测试**

Run: `cmake --preset windows-msvc-release`

Run: `cmake --build --preset windows-msvc-release`

Run: `ctest --test-dir out/build/windows-msvc-release --output-on-failure`

Expected: 全部测试通过；`platform.units` 明确报告 332 个用例；Stage 10、11D、12、13 既有测试不退化。

- [ ] **Step 3: 启动普通发布游戏验证正常装载路径**

从 `out/build/windows-msvc-release/src/app/arpg_game.exe` 启动，不传测试专用参数。确认游戏窗口可进入、输入响应正常、音频设备 ready、日志没有 14 条 fallback 警告。使用现有公开键位触发轻击、J3、L 上挑、命中、受击、落地和击杀；三类词条声音以正式 trace/showcase 验证，不伪造房间状态。

- [ ] **Step 4: 验证试听文件可播放并记录人工边界**

Run: `ffprobe -v error -show_entries stream=codec_name,sample_rate,channels,bits_per_sample,duration -of default=noprint_wrappers=1 docs/validation/evidence/stage14-audio-material-pack/stage14-audio-showcase.wav`

Expected: PCM s16le、44100、1 channel、16 bits。报告明确区分“文件与路由自动验证通过”和“主观响度/音色仍需用户听感验收”。

- [ ] **Step 5: 写验收报告并更新 README 当前里程碑**

报告记录：提交范围、官方来源链接、14 项资源表、所有命令和实际测试数、PCM 总预算、单资源损坏回退、发布目录、真实游戏启动结果、showcase 路径、未实现项和 Stage 15 未启动声明。

- [ ] **Step 6: 最终差异复核并提交**

Run: `git diff --check`

Run: `git status --short`

```powershell
git add README.md docs/validation/stage14-combat-audio-pack.md
git commit -m "docs: validate stage 14 combat audio pack"
```

- [ ] **Step 7: 停在 Stage 14 验收边界**

不开始音乐、环境声、UI 音效、音量分类迁移或 Stage 15。向用户交付游戏 EXE、验收报告、evidence text 和 showcase WAV 的绝对路径。


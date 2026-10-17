# Stage 14 Task 2 报告：稳定音效清单与纯资源校验

## 交付范围

- 建立固定 14 ID `AudioAssetId` 契约和 `assets/stage14/audio/` 清单。
- 建立不引用 raylib 类型的 `AudioDecodedMetadata`、路径校验、PCM 格式/时长/峰值校验和总 PCM 预算校验。
- 未修改 `src/core`、`src/combat` 或 `src/dungeon`；未实现 `AudioPack`、播放、设备访问或运行时加载。

## 变更文件

- `src/platform/raylib/audio_asset_types.hpp`
- `src/platform/raylib/audio_manifest.hpp`
- `src/platform/raylib/audio_asset_validation.hpp`
- `src/platform/raylib/audio_asset_validation.cpp`
- `src/platform/raylib/CMakeLists.txt`
- `tests/platform/audio_asset_validation_tests.cpp`
- `tests/platform/CMakeLists.txt`
- `tests/platform/platform_test_main.cpp`

## TDD 证据

### RED

先只加入 `audio_asset_validation_tests.cpp` 的 8 个用例及测试注册，尚未创建任何生产头文件或实现。测试覆盖：

1. 恰好 14 个唯一 ID；
2. 重复 ID；
3. 绝对路径；
4. `..`、反斜杠和空路径段逃逸；
5. 采样率、位深、声道错误；
6. 零时长或超过 1.5 秒；
7. 静音或削波；
8. 总 PCM 超过 8 MiB。

使用项目的 VS 2022 x64 / SDK 26100 / MSVC 19.44 开发环境后执行：

```powershell
cmake -S . -B .task2-build -G Ninja -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Release
cmake --build .task2-build --target arpg_platform_tests
```

结果：构建按预期失败，真实编译器错误为：

```text
audio_asset_validation_tests.cpp(3): fatal error C1083:
无法打开包括文件: “audio_asset_validation.hpp”: No such file or directory
```

这证明失败来自尚不存在的需求接口，而不是测试拼写或运行环境问题。

### GREEN

随后仅添加音频类型、静态清单、无 raylib 校验实现和 `arpg_raylib` 源文件注册。校验规则为：

- 清单必须恰好有 14 项，所有 ID 已知且唯一；
- 路径必须以 `assets/stage14/audio/` 开头，并拒绝盘符/根路径、`..`、反斜杠、`.` 和空段；
- 格式必须是 44,100 Hz、16-bit、单声道；
- `0 < duration <= 1.5s`，峰值必须满足 `0.0F < peak < 1.0F`；
- PCM 字节以溢出安全的 `frame_count * channels * sample_size / 8` 计算，总量上限为 8 MiB。

隔离 Release 验证命令：

```powershell
cmake --build .task2-build --target arpg_platform_tests
ctest --test-dir .task2-build -R 'platform.units|stage14.audio_sources' --output-on-failure
```

结果：`platform.units` 和 `stage14.audio_sources` 均通过（2/2）。临时目录随后移动至已忽略的 `build-stage14-task2`，未进入提交范围。

## 最终验证

按任务简报的正式预设执行：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-release
cmake --build --preset windows-msvc-release --target arpg_platform_tests
ctest --test-dir out/build/windows-msvc-release -R 'platform.units|stage14.audio_sources' --output-on-failure
.\out\build\windows-msvc-release\bin\arpg_platform_tests.exe
```

结果：

```text
100% tests passed, 0 tests failed out of 2
316 cases, 0 failures
```

额外自检：

```powershell
git diff --check
rg -n '#include <raylib|Texture2D|AudioStream|\bSound\b|\bWave\b' `
  src/platform/raylib/audio_asset_types.hpp `
  src/platform/raylib/audio_manifest.hpp `
  src/platform/raylib/audio_asset_validation.hpp `
  src/platform/raylib/audio_asset_validation.cpp
```

结果：`git diff --check` 无空白错误；第二条命令零匹配，确认本任务的音频清单和校验没有 raylib 运行时类型或头文件依赖。

## 顾虑

无功能顾虑。平台测试运行中出现的既有 Stage 12 材质回退日志不属于本任务，且不影响 316/316 通过结果。

## 精确提交

- SHA：`d7c47c1ca3200dee5d90c0eac50de4321d8bd19d`
- 主题：`feat: validate stage 14 audio manifest`
- 提交内容：仅本报告“变更文件”列出的 8 个功能文件；本报告未加入提交。

## 审查修复：路径截断与独立 PCM 预算

### RED

在不新增测试用例数量（仍为 8）的前提下，扩展既有路径用例，加入短路径 `"a"`、根路径 `/assets/stage14/audio/...` 和 `.` 段；扩展既有预算用例，改为直接调用计划中的独立纯接口 `validate_audio_pcm_budget`，使用 63 段合法最大时长数据验证预算内，64 段验证超过 8 MiB。每段均为 66,150 frames、44.1 kHz、16-bit、单声道，因此预算测试不再依赖单段超过 1.5 秒的错误优先级。

先于生产修改执行：

```powershell
cmake --build build-stage14-task2 --target arpg_platform_tests
```

结果：按预期 RED，编译器报告新增接口不存在：

```text
error C2039: "validate_audio_pcm_budget": 不是 "arpg::platform" 的成员
error C3861: “validate_audio_pcm_budget”: 找不到标识符
```

短字符串 `"a"` 的新断言也在同一测试先行改动中；旧实现虽尚未显式检查 NUL，但会在第一个 NUL 与前缀字符不等时返回 false，因此它没有独立的可观察行为红灯。修复仍加入了先检查 `character == '\0'` 的防御性分支，避免继续把截断 C 字符串当作前缀比较。

### GREEN

实现新增：

- `validate_audio_pcm_budget(const AudioDecodedMetadata*, std::size_t)`：独立、无 raylib、溢出安全地累计 PCM 字节并施加 8 MiB 上限；
- `validate_audio_manifest` 先调用每项 `validate_audio_metadata`（路径、格式、时长、峰值），全部通过后才调用聚合预算接口；
- `has_audio_path_prefix` 在每次前缀比较前显式拒绝 NUL 截断。

使用正式项目环境执行：

```powershell
. .\scripts\Configure.ps1 -Preset windows-msvc-release
cmake --build --preset windows-msvc-release --target arpg_platform_tests
ctest --test-dir out/build/windows-msvc-release -R 'platform.units|stage14.audio_sources' --output-on-failure
.\out\build\windows-msvc-release\bin\arpg_platform_tests.exe
git diff --check
```

结果：构建成功；`platform.units`、`stage14.audio_sources` 为 2/2 通过；平台二进制输出 `316 cases, 0 failures`；`git diff --check` 无空白错误。

### 审查修复提交

- SHA：`7ceef4f386b76c8e299cdf4ef3a63ed13d4c1954`
- 主题：`fix: harden stage 14 audio validation`
- 提交范围：仅 `audio_asset_validation.cpp`、`audio_asset_validation.hpp` 和 `audio_asset_validation_tests.cpp`；本报告未加入提交。

# Stage 14 Task 1 报告：发布音频资源

## 状态

完成。14 个发布 WAV、固定来源准备脚本、可审计来源表、资源验证器与 CTest 注册均已交付；未修改 core、combat 或 dungeon 规则。

## TDD 证据

### RED 1：资源尚不存在

先创建 `tests/platform/stage14_audio_source_validator.ps1`，再执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/platform/stage14_audio_source_validator.ps1 -AssetDirectory assets/stage14/audio
```

结果：预期非零失败，明确报告：

```text
Stage 14 audio validation FAILED: Stage 14 audio directory is missing: E:\game\.worktrees\stage14-audio-material-pack\assets\stage14\audio
```

### RED 2：注册 CTest 的兼容性门槛

注册 `stage14.audio_sources` 后，在 Visual Studio x64 开发环境运行：

```powershell
ctest --test-dir out/build/windows-msvc-release -R "^stage14\.audio_sources$" --output-on-failure
```

结果：预期失败，验证器调用的环境不提供 `Get-FileHash`。这证明 CTest 实际执行了新验证器，而不是仅检查 CMake 文本。

### GREEN

将 SHA-256 计算改为 .NET `System.Security.Cryptography.SHA256`，避免依赖 PowerShell cmdlet。固定官方 Kenney URL 下载、逐项原文件 SHA-256 校验和 FFmpeg 转码完成后，执行：

```powershell
powershell.exe -NoProfile -ExecutionPolicy Bypass -File scripts/PrepareStage14Audio.ps1
powershell.exe -NoProfile -ExecutionPolicy Bypass -File tests/platform/stage14_audio_source_validator.ps1 -AssetDirectory assets/stage14/audio
```

结果：准备脚本第二次幂等执行成功；验证器输出：

```text
14/14 PASS - Stage 14 audio sources are valid; total PCM: 474532 bytes.
```

已重新配置 Release CMake 树，并在 Visual Studio x64 开发环境中执行最终 CTest：

```powershell
ctest --test-dir out/build/windows-msvc-release -R "^stage14\.audio_sources$" --output-on-failure
```

结果：1/1 通过，标签为 `headless;platform;stage14`；测试没有下载网络资源或调用 FFmpeg。

## 实现与自审

- `PrepareStage14Audio.ps1` 只使用两条固定 Kenney 官方 ZIP URL；每个原始 OGG 均在转码前比对任务指定 SHA-256。
- ZIP 内部目录经实际审计为 `Audio/<文件名>`；`SOURCES.md` 保留需求规定的包限定来源路径（`RPG Audio/...`、`Impact Audio/...`）及原始和发布 SHA-256。
- 每个输出由 `ffmpeg -nostdin -y -i <source> -ac 1 -ar 44100 -c:a pcm_s16le <output>` 生成。
- 临时路径位于 GUID 唯一子目录；所有写入与清理路径均先验证绝对路径未逃逸；清理仅递归删除脚本创建的临时根目录。
- 准备脚本拒绝覆盖含有意外条目的发布目录；已存在的预期 14 个 WAV 和 `SOURCES.md` 可安全更新，满足幂等重跑。
- 验证器拒绝额外条目、空文件、非 RIFF/WAVE、非 PCM/44.1 kHz/16-bit/mono、单个时长超过 1.5 秒、总 PCM 超过 8 MiB，以及来源表哈希缺失。

## 文件

- `scripts/PrepareStage14Audio.ps1`
- `assets/stage14/audio/SOURCES.md`
- `assets/stage14/audio/*.wav`（14 个）
- `tests/platform/stage14_audio_source_validator.ps1`
- `tests/platform/CMakeLists.txt`
- `.superpowers/sdd/task-1-report.md`

## 提交

主题：`assets: add stage 14 combat audio sources`

## 顾虑

- 无已知功能顾虑。
- 初始非开发环境缺少 `WindowsSDKVersion`，且既有 Release 构建树在沙箱内无法写入；最终 CMake/CTest 已在 Visual Studio x64 开发环境中成功执行。
- 资源准备需联网下载任务指定的官方 Kenney ZIP；正常 CTest 只校验已提交资源，不联网也不需要 FFmpeg。

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


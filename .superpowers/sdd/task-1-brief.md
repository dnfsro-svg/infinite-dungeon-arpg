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

**固定原文件 SHA-256（必须逐项验证）：**

```text
RPG Audio/knifeSlice.ogg                         4CD96DC630BED9840C15F1DD2306DA2CC56A4DA26A5D3F1A03C5A7265AC5E54F
RPG Audio/knifeSlice2.ogg                        6C2064D0EF988D1EC3D56868E823EA8823A5CAC00F2742560052633529407DEF
RPG Audio/chop.ogg                               D00C2B3C9FFF07E376145C8C8C45C90E5084EC192F6CE0387DB233F7B86F1486
RPG Audio/drawKnife3.ogg                         A11AE62FB1A628425769D11A9DE394980AD8909C31F4C9A4316F226963E21CAF
Impact Audio/impactPunch_medium_000.ogg           486988AA2D6440FFC4C62A0E8CCF3C23673BA84424BD4723378D451B7255EB5C
Impact Audio/impactPunch_medium_001.ogg           E71D23ABCC3F10D9D6BC9615B2A431FFF70B12333A5FCDD9964457564ACE4349
Impact Audio/impactPunch_medium_002.ogg           6492F9CCBC8D24DCEF607BB1B48790B7870961474561F7FE5601E1C73A3018EF
Impact Audio/impactPunch_heavy_004.ogg            F4C0C3EB8AB6517583B8218ED03F28923D1B687379AAB4C1D5A6D4C10CF8E500
Impact Audio/impactSoft_heavy_003.ogg             D00286A2DC62EE5CB4D42BF56120E7050A855F69E655FC1294410AADD337EBA3
Impact Audio/footstep_concrete_004.ogg            24DD8DB2413E5B81AD181E1205C12A1C7801DA5F85715D471B92A4B2F632E45E
Impact Audio/impactMetal_heavy_004.ogg            6D65B463C0555DD5BE16B8DB6D2CBE23A94E07A4637779B8AD17D0DB3E500A87
Impact Audio/impactBell_heavy_004.ogg             49FE4FAFA2001BD0D312976796824571CA8429851F285997E1327D17BB34FD00
Impact Audio/impactMetal_medium_004.ogg           03E10F800199C7166208E41C11CF4ACFAC6CC265467F02357DA71972BC967295
Impact Audio/impactGlass_heavy_004.ogg            C7742B0E33C1733DDAF902AA68464A4F5364E4D4CB6FF11B76A7AA4151A138DD
```

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

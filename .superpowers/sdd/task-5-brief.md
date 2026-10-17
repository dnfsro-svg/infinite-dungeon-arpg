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

当前项目把所有可执行文件统一输出到 `out/build/windows-msvc-release/bin/`，因此实际发布目录由 `$<TARGET_FILE_DIR:arpg_game>` 解析为该 `bin` 目录。

Run: `powershell.exe -NoProfile -Command "(Get-ChildItem out/build/windows-msvc-release/bin/assets/stage14/audio -Filter *.wav).Count"`

Expected: 输出 `14`。

- [ ] **Step 5: 精确提交 Task 5**

```powershell
git add src/app/CMakeLists.txt tests/platform/stage14_audio_integration_guard.cmake tests/platform/CMakeLists.txt
git commit -m "build: package stage 14 combat audio"
```

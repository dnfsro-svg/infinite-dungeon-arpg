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

当前项目统一把可执行文件输出到 `out/build/windows-msvc-release/bin/`。从 `out/build/windows-msvc-release/bin/arpg_game.exe` 启动，不传测试专用参数。确认游戏窗口可进入、输入响应正常、音频设备 ready、日志没有 14 条 fallback 警告。使用现有公开键位触发轻击、J3、L 上挑、命中、受击、落地和击杀；三类词条声音以正式 trace/showcase 验证，不伪造房间状态。

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

本 Task 只收口 Stage 14，不在同一提交内开始音乐、环境声、UI 音效或音量分类迁移。Stage 14 验收提交完成后，由控制器另行进入已获用户授权的 Stage 15 推荐范围。向用户交付游戏 EXE、验收报告、evidence text 和 showcase WAV 的绝对路径。

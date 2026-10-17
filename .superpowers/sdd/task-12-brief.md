## Task 12: 1000 房确定性、600 tick 零分配与正式验证夹具

**Files:**

- Create: `tests/dungeon/dungeon_abyss_stress_tests.cpp`
- Create: `tests/dungeon/stage10_validation_fixture.cpp`
- Create: `tests/dungeon/stage10_validation_game.cpp`
- Create: `tests/dungeon/stage10_formal_game_validation.cpp`
- Create: `tests/dungeon/stage10_validation_capture_test.ps1`
- Create: `tests/dungeon/stage10_formal_game_capture_test.ps1`
- Create: `tests/dungeon/stage10_evidence_guard_test.cmake`
- Modify: `tests/dungeon/CMakeLists.txt`
- Modify: `tests/dungeon/dungeon_test_main.cpp`
- Modify: `src/platform/raylib/raylib_host.hpp`
- Modify: `src/platform/raylib/raylib_host.cpp`

**Produces:** 可重复的 headless 轨迹、真实窗口 present 后截图、证据防注入守卫。

**Consumes:** 现有 Stage 8/9 validation capture 机制；不得伪造 combat defeat trace 或直接写最终截图。

- [ ] 压力测试遍历固定 root seed 的 1000 个房间；每 37 房执行真实 `encode_checkpoint`/`decode_checkpoint`，并验证门预告、实际 room descriptor、危险/规则、遭遇、奖励和摘要逐字段相同。

- [ ] 在 1000 房样本中统计四向门总判定次数和命中数，只验证固定 golden 数值与约 1% 的宽松统计护栏；概率正确性的严格证明仍由阈值边界单测负责。

- [ ] 构造最极端 96 怪、3 词缀、高危环境规则、满 projectile/hazard/ground pool 场景，启动 `AllocationProbe` 后运行 600 tick；预期 `allocations == 0`，无 fixed-pool 越界且 saturation 只增加诊断计数。

- [ ] validation fixture 必须走真实 session、SaveStore 和 pickup/abandon API，输出稳定文本证据：门预告 seed、started commit generation、rule/danger、环境伤害、clear commit、三件 reward id、reload masks、abandon summary。

- [ ] formal game validation 使用 raylib 窗口真实运行，先用纯生成 API 搜索固定会出现目标深渊规则的 root seeds，再只通过正常 MovementInput/Action/SaveStore 驱动窗口；至少捕获七张已 present 帧：深渊门图标、雷暴预警、追猎烈焰预警、混沌扩散区域、宝箱落地、待生成提示、离房二次确认。沿用 `validation_exit_after_presented_frames`，截图必须发生在 `EndDrawing` 后。

- [ ] 用临时存档分别跑死亡、R、started 状态重启和“深渊+洞口”四条正式路径；前三条重载后必须只剩同 seed 普通房，洞口路径必须在清场后完成奖励确认并成功下层。

- [ ] evidence guard 扫描夹具与 host，禁止调用测试私有访问器、直接注入 defeated event、直接改 phase/masks、用静态图片替代窗口截图。

- [ ] 先运行夹具并确认新 target 不存在，再接入 CMake；完成后运行：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_stage10_validation_fixture arpg_stage10_validation_game arpg_stage10_formal_game_validation
ctest --preset windows-msvc-debug -R "stage10" --output-on-failure
```

预期：Stage 10 headless fixture、evidence guard、present capture、formal game capture 全部通过并生成非空 PNG。

- [ ] 提交：

```powershell
git add tests/dungeon src/platform/raylib/raylib_host.*
git commit -m "test: add formal stage 10 abyss validation"
```

---


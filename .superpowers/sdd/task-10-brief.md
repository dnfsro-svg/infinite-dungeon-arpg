## Task 10: 1000 房确定性、全量回归与窗口验收

**Files:**
- Create: `tests/dungeon/dungeon_equipment_stress_tests.cpp`
- Modify: `tests/dungeon/CMakeLists.txt`
- Create: `docs/validation/stage8-equipment-loot-mvp.md`

- [ ] **Step 1: 写 1000 房双 session 压力测试**

两个 session 使用同一 root seed 和固定输入 trace。每房逐只触发击杀，靠近拾取；有合法物品时按固定序列换装和三合一；每 37 房编码、解码并重建其中一个 session。每步比较：room seed、generation、物品记录逐字节、equipment IDs、claimed bits、sequence、完整 build 和 ground drop ordinal/content。

- [ ] **Step 2: 运行 RED 并确认测试能发现扰动**

先在测试 trace 的副本中人为改变一个 room 的 pickup 顺序，断言 comparison helper 报告不等；恢复相同 trace 后，生产实现若仍有随机流耦合则测试失败并给出首个房间/字段。

```powershell
cmake --build build-release --target arpg_dungeon_tests
ctest --test-dir build-release -R dungeon.units --output-on-failure
```

- [ ] **Step 3: 修正仅由测试暴露的确定性缺口**

只允许调整随机域隔离、稳定排序或显式序号；不得通过放宽比较、固定结果常量或跳过重启来使测试通过。修改的生产文件须加入本任务提交并在验证记录列明原因。

- [ ] **Step 4: 完整 Debug 验证**

```powershell
cmake -S . -B build-debug -G Ninja -DCMAKE_BUILD_TYPE=Debug -DARPG_FETCH_RAYLIB=OFF -DCMAKE_PREFIX_PATH=E:/game/.deps/raylib-6.0
cmake --build build-debug
ctest --test-dir build-debug --output-on-failure
```

Expected: 全部测试通过。

- [ ] **Step 5: 完整 Release 验证**

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DARPG_FETCH_RAYLIB=OFF -DCMAKE_PREFIX_PATH=E:/game/.deps/raylib-6.0
cmake --build build-release
ctest --test-dir build-release --output-on-failure
git diff --check
git status --short
```

Expected: 全部测试通过；仅本任务预期文件处于未提交状态。

- [ ] **Step 6: 执行窗口验收并记录证据**

启动 `build-release/src/app/arpg_game.exe`，按设计规格第 15.2 节逐项验证：可见掉落、1.5 距离拾取、I 暂停、三栏、查看/双击换装、实战属性变化、生命护盾只截断、三合一、未拾取离房消失、重启持久化。将日期、构建提交、存档路径、每项 pass/fail 和截图路径写入 `docs/validation/stage8-equipment-loot-mvp.md`；任何 fail 都先修复并重跑受影响测试与窗口步骤。

- [ ] **Step 7: 提交最终验证**

```powershell
git add tests/dungeon docs/validation
git add -u
git commit -m "test: validate stage 8 equipment loot milestone"
```

- [ ] **Step 8: 最终审查边界**

```powershell
git log --oneline 352cc13..HEAD
git diff --stat 352cc13..HEAD
rg -n "epic|identify|craft|corrupt|monster affix|item filter" src tests
ctest --test-dir build-release --output-on-failure
git status --short
```

逐项确认没有实现明确排除功能，工作树干净，并停在 `codex/stage8-equipment-loot-mvp` 等待用户验收。

---

## Spec Coverage Checklist

- [ ] 6 固定基底、24 词缀、T1–T8 门槛和值由 Task 2 golden 测试锁定。
- [ ] 稀有度、词缀数、tier 权重、需求等级、局部武器语义由 Task 3 锁定。
- [ ] 闪避、护甲、元素减伤、75/95% cap 与不回复热换由 Task 1/4 锁定。
- [ ] 1% 独立掉落、192 ordinal/容量、1.5 拾取、离房销毁由 Task 7 锁定。
- [ ] 无限背包查询、即时换装、三合一和提交故障语义由 Task 6/8 锁定。
- [ ] I/P 互斥、战斗暂停、零补跑、三栏虚拟化和比较由 Task 9 锁定。
- [ ] V4 精确布局、65,535 边界、V1–V3 迁移和双槽恢复由 Task 5 锁定。
- [ ] 双 session 1000 房、Debug/Release、窗口验收与停止边界由 Task 10 锁定。

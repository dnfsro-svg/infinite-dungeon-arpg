## Task 13: 全量验证、规格审计与里程碑封口

**Files:**

- Modify: `README.md`
- Create: `docs/validation/stage10-abyss-combat.md`
- Modify only if evidence finds a defect: Stage 10 files named in Tasks 1～12

**Produces:** Debug/Release 双配置证据、33 个既有 CTest 加 5 个 Stage 10 新测试入口的完整回归、规格逐条对应表。

- [ ] 运行格式/占位符/边界扫描：

```powershell
rg -n "TODO|FIXME|PLACEHOLDER|stage 10 later" src/abyss src/combat src/dungeon src/persistence src/platform/raylib tests/abyss tests/combat tests/dungeon tests/persistence tests/platform
rg -n "#include <raylib.h>|#include \"raylib.h\"" src/core src/abyss src/combat src/dungeon src/persistence
```

预期：第一条无输出；第二条无输出。

- [ ] 运行 Debug 全量：

```powershell
cmake --preset windows-msvc-debug
cmake --build --preset windows-msvc-debug
ctest --preset windows-msvc-debug --output-on-failure
```

预期：构建 0 error，`38/38` CTest 通过。

- [ ] 运行 Release 全量：

```powershell
cmake --preset windows-msvc-release
cmake --build --preset windows-msvc-release
ctest --preset windows-msvc-release --output-on-failure
```

预期：构建 0 error，`38/38` CTest 通过。

- [ ] 单独重跑压力与正式 raylib 证据，记录命令、退出码、耗时、截图路径、1000 房 golden 摘要和 600 tick 分配计数到 `docs/validation/stage10-abyss-combat.md`。

- [ ] 按设计文档第 4～18 节逐条审计并在验证文档中建立“规格条目 → 测试名/证据”对应表，特别复核：四门 1%、启动/失败/清场原子门禁、started 重载失败、洞共存、地面池满、背包满、同门二次确认、离房放弃、LastAbyssResolution、普通随机不漂移。

- [ ] 检查工作树只包含 Stage 10：

```powershell
git status --short
git diff --check
git log --oneline main..HEAD
```

预期：`git diff --check` 无输出；没有 `.scratch/`、`task3_trace_probe.obj`、构建目录或截图误入提交。

- [ ] 更新 README 的当前里程碑和操作说明，然后提交：

```powershell
git add README.md docs/validation/stage10-abyss-combat.md
git commit -m "docs: record stage 10 abyss validation"
```

- [ ] 最终确认：

```powershell
git status --short
```

预期：Stage 10 工作树干净。停在 `codex/stage10-abyss-combat`，不自动合并 main，不推进 Stage 11。

---

## 实施时的固定检查点

每完成一个任务，执行者必须报告：

1. 新增/修改的稳定接口。
2. Red 测试具体失败原因。
3. Green 后局部测试命令与结果。
4. 是否影响随机域、存档格式或零分配约束。
5. 提交哈希。

遇到存档 receipt、固定容量、规则语义或普通随机 golden 的冲突时，停止该任务并回到设计规格，不得通过放宽断言完成测试。

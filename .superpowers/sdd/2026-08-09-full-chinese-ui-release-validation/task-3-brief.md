### Task 3: 全量自动化与资源审计

**Files:**
- Create: `docs/validation/full-chinese-ui.md`
- No production source changes in this task; a failing suite returns work to the owning RED→GREEN task before this gate is rerun.

**Interfaces:**
- Consumes: 三份实现计划的全部结果。
- Produces: Debug 全量测试、Release 构建、字体资源与持久化回归证据。

- [ ] **Step 1: 记录提交前状态和资源基线**

```powershell
git status --short --untracked-files=all
Get-Process cl,ninja,cmake,ctest,arpg_game -ErrorAction SilentlyContinue |
  Select-Object Id,ProcessName,WorkingSet64
```

确认无其他构建/游戏进程后再开始。记录 Noto 字体 SHA256、现有 Release exe SHA256 和可用内存。

- [ ] **Step 2: 运行完整 Windows Debug 自动化**

```powershell
& '.\scripts\Test.ps1' -Preset windows-msvc-debug
```

Expected: exit 0，所有 CTest 通过。若失败，只修复与本分支相关的真实问题；修复后重跑对应定向测试，再完整重跑 `Test.ps1`。

- [ ] **Step 3: 运行核心兼容回归**

```powershell
cmake --build --preset windows-msvc-debug --target arpg_persistence_tests arpg_settings_tests arpg_combat_tests --parallel 2
& '.\out\build\windows-msvc-debug\bin\arpg_persistence_tests.exe'
& '.\out\build\windows-msvc-debug\bin\arpg_settings_tests.exe'
& '.\out\build\windows-msvc-debug\bin\arpg_combat_tests.exe'
```

Expected: 三个套件全部 0 failure；checkpoint wire baseline、设置 codec、战斗数值均未改变。

- [ ] **Step 4: 构建 Release 并记录真实字体资源**

```powershell
& '.\scripts\Build.ps1' -Preset windows-msvc-release
```

Expected: exit 0。正式 Stage 12 证据必须显示：`bundled_font_instance_count=1`、字形数不超过 640、单/总图集均不超过 32 MiB、CJK ready。

- [ ] **Step 5: 写入验证文档并提交自动化结果所需修复**

`docs/validation/full-chinese-ui.md` 记录完整命令、时间、exit code、suite 结果、字形数、图集尺寸和核心兼容结果；不写“预计通过”。

```powershell
git add docs/validation/full-chinese-ui.md
git diff --cached --check
git commit -m "docs: record full Chinese UI verification"
```

---


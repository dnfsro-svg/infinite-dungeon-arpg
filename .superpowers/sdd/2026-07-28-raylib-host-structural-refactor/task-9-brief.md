### Task 9: 子项目 A 全量门禁、独立审查和真实窗口验收

**Files:**
- Modify: `docs/validation/raylib-host-refactor-baseline.md`
- Modify only if required by a proven defect: files owned by Tasks 0–8

- [ ] **Step 1: 检查资源并执行 Debug 全量构建**

  ```powershell
  Get-Process | Where-Object { $_.ProcessName -match '^(cl|link|ninja|cmake|ctest|arpg_)' } | Select-Object ProcessName,Id,CPU,WorkingSet64
  Get-CimInstance Win32_OperatingSystem | Select-Object FreePhysicalMemory,FreeVirtualMemory
  $env:CMAKE_BUILD_PARALLEL_LEVEL='1'
  cmake --build --preset windows-msvc-debug -- -j1
  ctest --preset windows-msvc-debug -j1
  ```

- [ ] **Step 2: 执行 Release 全量构建与门禁**

  ```powershell
  cmake --preset windows-msvc-release
  cmake --build --preset windows-msvc-release -- -j1
  ctest --preset windows-msvc-release -j1
  ```

- [ ] **Step 3: 对比正式证据**

  对 Stage10/11/11B/11C/11D/17 的 summary 字段顺序、截图文件名、图像尺寸和已有 validator 结果逐项对比基线；任何黄金字节/像素守卫变化都回到对应任务修复，不更新期望值。

- [ ] **Step 4: 启动真实 raylib 游戏验收**

  验收移动、攻击、跳跃、L 上挑与 0.5 秒浮空、两个主动技能、背包、掉落拾取、暂停/设置、房门、下一层洞、死亡继续、HUD/字体和重启存档。运行期间记录 CPU/内存，确认无空闲高占用或编译器残留。

- [ ] **Step 5: 最终独立双审查**

  一个审查者逐条核对设计/计划/范围，另一个审查者检查所有改动的生命周期、输入顺序、热路径分配、守卫完整性和异常安全。P0/P1/P2 必须为零；最多五轮修复，否则按设计停止条件保留现场并报告。

- [ ] **Step 6: 更新度量、提交、推送并读回远端哈希**

  文档记录生产总行数变化、`raylib_host.cpp` 变化、每个新文件行数、最大文件、测试总数/耗时、真实窗口结果和已知限制。

  ```powershell
  git diff --check
  git status --short
  git add docs/validation/raylib-host-refactor-baseline.md
  git commit -m "docs: close raylib host refactor validation"
  git push -u origin codex/whole-game-refactor-2026-07
  git ls-remote origin refs/heads/codex/whole-game-refactor-2026-07
  ```

## Completion Gate

- `raylib_host.cpp` 不再定义或直接操作任何 Stage 验收状态机，只调用 `HostValidationRuntime`。
- 六组正式验收实现仍由 `arpg_raylib` 编译，公共 CLI/config/scenario 接口不变。
- 输入、固定步、事件、截图、summary 和 clean-shutdown 顺序由源码守卫与正式证据共同证明未漂移。
- Debug/Release 全量 CTest、独立双审查和真实窗口验收全部通过；工作树干净，远端哈希与本地 HEAD 一致。
- 本计划不把子项目 B 的改动混入 A；A 的交付状态明确记录后，主控再按已批准总规格生成并审查 B 的独立计划。

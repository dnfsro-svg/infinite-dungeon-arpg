# Stage 11-A Task 13 执行报告

## 文档

- README 已更新 Stage 11-A 死亡/退层规则、死亡输入集合和正式报告链接。
- 新增 `docs/validation/stage11a-death-continue.md`，记录单向模块边界、`death_retreat` / `death_continue` 两次原子事务、V6/V5、五条正式路径、1000 次压力、证据目录、复现命令与测试名映射。
- 明确设置、物品过滤器、完整 HUD、macOS 和 Stage 11-B 未实现/不在范围内。

## 环境问题与处理

首次在普通 PowerShell 直接运行：

```powershell
cmake --preset windows-msvc-debug --fresh
```

退出 1，CMake 报 `No CMAKE_C_COMPILER could be found` 和 `No CMAKE_CXX_COMPILER could be found`。这不是源码或测试失败；当前 shell 未加载 MSVC。随后通过仓库固定的 VS 2022 Build Tools DevShell 参数 `-arch=x64 -host_arch=x64 -winsdk=10.0.26100.0` 进入 MSVC 19.44.35228.0 / SDK 26100 环境，并重新执行相同 exact 命令。未修改生产代码或测试。

## Debug fresh 全量

```powershell
cmake --preset windows-msvc-debug --fresh
cmake --build --preset windows-msvc-debug --clean-first
ctest --preset windows-msvc-debug --output-on-failure
```

- fresh configure：成功，MSVC 19.44.35228.0，raylib Debug。
- clean-first：237/237 成功。
- CTest：52/52，0 失败；real total 739.92 秒。
- `dungeon.units`：503.54 秒。
- `stage11.death_stress.determinism_zero_alloc`：通过，123.05 秒。
- `stage11.death_formal.five_paths`：通过，6.69 秒。
- formal、graphics、stress、evidence guard 均包含在完整 52 项中。

## Release fresh 全量

```powershell
cmake --preset windows-msvc-release --fresh
cmake --build --preset windows-msvc-release --clean-first
ctest --preset windows-msvc-release --output-on-failure
```

- fresh configure：成功，MSVC 19.44.35228.0，raylib Release。
- clean-first：237/237 成功。
- CTest：52/52，0 失败；real total 282.57 秒。
- `dungeon.units`：127.30 秒。
- `stage11.death_stress.determinism_zero_alloc`：通过，35.67 秒。
- `stage11.death_formal.five_paths`：通过，6.62 秒。

## Release raylib 与五路径证据

- fresh 下载树 `raylib.h`：major/minor/patch 为 6/0/0。
- `cmake/Dependencies.cmake` 配置期检查 6.0.0；`raylib_host.cpp` 编译期 `static_assert` 检查 6.0.0。
- Release `build.ninja` 的 `arpg_game.exe` 链接边包含 `lib/raylib.lib`；`dumpbin /dependents` 不含 raylib DLL，确认静态链接。
- Release 证据目录：`out/build/windows-msvc-release/bin/stage11-death-formal-validation/`。
- fresh 生成：`01-normal-death.png`、`02-restarted-death.png`、`03-deep-continued.png`、`04-floor-one-continued.png`、`05-abyss-death.png`。
- `formal-path-summary.txt` 的 normal/restart/deep/floor-one/abyss 五条 marker 全部 `PASS`；normal/restart target 与 hash 相同。

## 提交边界

- `git diff --check` 无错误。
- 基线 `a06eddbdf557b7209146f12ec1a6bc75a1aa75b3..HEAD` 已检查 log/stat。
- 该范围没有 build、Stage 11 PNG、存档或日志进入提交；`out/` 中 fresh build、截图、存档和 summary 均保持忽略。
- Task 13 初始提交只包含 `README.md` 与 `docs/validation/stage11a-death-continue.md`。
- 不进行代码审查修复，不合并 main，不开始 Stage 11-B；由主控继续两轮独立审查。

## 最终审查修复

审查发现并修复三个验证缺口：

1. 原 stress 每轮从 `case_root` 重建，实际只覆盖 `death_sequence 0 -> 1`。先新增 `death_sequence == index + 1` 断言，原实现在完整 1000 次后 RED（129.02 秒）。随后改为 direct/restart 各自持续演进同一稳定状态，序号最终为 1000，并比较每个死亡、累计 hash、最终完整状态和非默认 progression/item ownership 永久字段。
2. 继续 prepare 现在以 allocation counter 单独包围 `request_death_continue()`，随后使用 `pending_save_view()` 检查 pending，排除 `pending_save()` 测试副本分配干扰。最终 1000×2 trace 通过，探针累计为 0。
3. 原正式 deep/floor validation 直接调用第二个 `runtime.request_death_continue()`。增强 guard 后原 host 立即 RED，报缺少 `frame_input.keys.e = true`。正式验证现在只注入 `FrameKeyState.e`，共用 `death_input_gate`、唯一 request 分支和后续 `runtime.fixed_tick`。mutation self-test 会插入第二个直连请求，并确认 guard 拒绝。

连续 Session 初次 GREEN 运行还暴露两项测试驱动问题：旧测试因每轮重建而从未消费 Dungeon event queue，连续运行在第 6 次死亡进入 event overflow；补上与 host 相同的公共事件 drain。continue 后原 Session 为 `transitioning`，重启 Session 为 `locked`，按外层 tick 缓存移动输入导致 index 86 首个分歧；测试现在先通过公共 tick 归一到 `combat` 且 Combat tick 仍为 0，再启动相同确定性输入。

最终 Debug 定向验证：

- `stage11.death_stress.determinism_zero_alloc`：1/1，150.19 秒。
- `stage11.death_formal.five_paths`：1/1，7.06 秒。
- `stage11.death_evidence.*`：5/5，0.41 秒。
- `platform.*`：3/3，4.40 秒。
- `-L architecture`：21/21，87.19 秒。
- `git diff --check`：无输出。

Stress 相比审查前 Debug 123.05 秒增加 27.14 秒（约 22%）；覆盖从 1000 个独立 `0 -> 1` case 提升为两条各 1000 次的连续演进链。未开始 Stage 11-B，未合并 main。

## 第二轮最终证据加固

RED：旧 evidence guard 接受 `if (death_gate.continue_death || validation_continue)` 条件替换；旧 stress 加入“物品基线非空”断言后在 0.01 秒立即失败，证明此前 prepare/publication 只覆盖空 item vector。旧 stress 还在 receipt 后把 pending 预期副本赋给本地 `state`，因此 publication 若漏字段，后续与 `final_state` 都可能继续比较同一预期副本而漏报。

GREEN：正式 host 的唯一 `runtime.request_death_continue()` 现在只位于严格的 `if (death_gate.continue_death)` 明确作用域；`validation_continue` 只负责作用域前注入 `FrameKeyState.e` 和作用域后记录结果。guard 解析精确条件和配对花括号，确认唯一 request 在作用域内且 validation 不在条件/作用域；self-test 拒绝 `continue_death || validation_continue`、仅 `validation_continue` 与作用域内 validation 三种突变。

Stress 基线新增一件目录合法、真实装备的 normal weapon。每次 death/continue receipt 后均通过只读 `DungeonDeathStressFixture::stable_state_view` 获取 Session 实际稳定状态，与 `verified_state` 做完整 `same_run_state`，并以实际发布状态推进 trace；`final_state` 也直接取 Session。访问器只允许读取 `stable_state_`，architecture guard 拒绝 fixture 内写入或 fixture 外私有访问。非空路径的 continue prepare allocation probe 仍为零。

第二轮 Debug 最终定向验证：stress 1/1（160.97 秒）；formal 1/1（5.37 秒）；evidence 5/5（0.56 秒）；platform 3/3（4.65 秒）；architecture 21/21（96.42 秒）。未开始 Stage 11-B，未合并 main。

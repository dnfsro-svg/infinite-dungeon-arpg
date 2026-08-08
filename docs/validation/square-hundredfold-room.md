# 百倍方形房间验证记录

## 范围

本记录覆盖 Task 11 的可自动验证基线。自动化只证明无窗口的功能、确定性、固定容量和热路径分配约束；真实全屏画面仍必须由 Windows 本机验收，不能由单元测试替代。Release 性能数据已在 Windows 本机按下述隔离 A/B 流程产生。

## 自动化基线

`large_room_end_to_end` suite 已在当前 Windows Debug focused 运行中通过，共 4 个 case；其中确定性矩阵实际执行以下 24 条 `DungeonSession` 轨迹：

- 场景：普通最小 300、普通最大 750、深渊最大 1125。
- 计划重载次数：0、1、7、31。
- 每个场景/重载组合重复两次，共 `3 × 4 × 2 = 24` 条。
- 同一组合逐字段比较怪物/环境蓝图哈希、房间击败位、掉落与领取状态、出口开启 tick、地牢/战斗事件顺序、最终转移目标和完整 V10 字节。
- 不同重载次数比较最终 `SaveCheckpointSlot` 中的 `DungeonRunState`、`RoomProgressCheckpoint` 和转移目标，不要求瞬态事件流或中途字节跨重载次数相同，因此不会把 Task 5 明确版本化的房间瞬态清理差异误判成耐久化差异。
- 另外两个 active-room case 验证未结算经验留在 `pending_room_experience`，生命药拾取触发的 exact save 不会提前结算经验，重载后值不丢失，而死亡撤退不会把未结算经验写入永久进度。

该 suite 还验证 normal-max 750 与 abyss-max 1125 在预热后的同一主线程 32 次计数区段中，固定 tick、`DungeonSnapshot`、`DungeonRenderSnapshot` 和 checkpoint capture 合计零堆分配。V10 编码、写盘与读回不藏在该断言内；它们属于 worker/持久化独立测量范围。

`large_room_render_plan` suite 的 4 个 case 也已在当前 Windows Debug focused 运行中通过，覆盖以下验收断言：

- 房间总蓝图容量 1152 不会扩大每帧活动怪物 128、投射物 512、危险区 128、可见环境 128、背景块 25，以及可见掉落固定容量。
- 1920×1080 数学视口在四个边角钳制到房间边界。
- 背景块、环境道具投影和 material residency request 在预热后零堆分配。
- 相机移动会改变背景块与道具屏幕投影，但不会改写道具世界锚点。
- 25% 提前解锁与完全清场使用不同门装饰状态；洞口投影继续跟随共享相机。

## V10 持久化边界

验证过程中确认冻结的 V9 无法表达活动房间尚未结算的经验。当前实现保留 V9 编码器、校验语义和既有字节基线，最新写入格式升级为 V10：沿用完整 V9 持久化主体，并在扩展尾部追加一个显式小端 `u64 pending_room_experience`。V10 解码入口继续接受 V1-V9，并把旧格式标记为待迁移；生产 worker 的新提交写 V10，A/B 槽扫描同时识别 V9 与 V10。

## 已执行命令

RED 阶段已执行：

```powershell
. ./scripts/Configure.ps1 -Preset windows-msvc-debug
cmake --build --preset windows-msvc-debug --target arpg_dungeon_tests arpg_platform_tests -- -j1
ctest --test-dir out/build/windows-msvc-debug -R '^task11\.large_room\.(dungeon|platform)$' --output-on-failure
```

结果：两个 focused gate 均进入新增 suite，并在当时尚未实现的验收断言处失败，符合预期 RED。

当前 focused GREEN 已执行：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^persistence\.units$' --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R '^task11\.large_room\.(dungeon|platform)$' --output-on-failure
ctest --test-dir out/build/windows-msvc-debug -R '^(stage11b\.settings_formal|stage11b\.settings_evidence_validator)$' --output-on-failure
```

结果：`persistence.units` 内部报告 117 cases、0 failures（79.67 秒）；`task11.large_room.dungeon` 报告 4 cases、0 failures，并完成 24 条轨迹，`task11.large_room.platform` 通过；Stage 11-B 正式画面与证据 validator 为 2/2 通过（17.49 秒）。这些是本轮 focused 证据，不代表完整 Debug 单元门禁、完整正式兼容性门禁或 Release 基准已经通过。

## 待 Windows 本机验收

以下项目当前明确为 `PENDING`，本记录不声称通过：

- 真实 1920×1080 全屏：四边、相机钳制、角色尺寸、相机延迟、门/洞范围、纯色中文 HUD 可读性。
- 普通 300 与深渊 1125 蓝图房间的实际画面与操作手感。
- 25% 提前出口后继续战斗、完全清场、死亡/继续、保存/重载。
- 环境道具不存在静止屏幕空间表现。
- worker 的 V10 编码、实际 A/B 写盘与读回成本测量。

证据目录为 `docs/validation/evidence/square-hundredfold-room/`。只允许写入实际运行生成且可追溯的截图、日志或指标；禁止用占位数据冒充验收证据。

## Release 渲染准备实测

Windows 本机在固定逻辑 CPU 0 上执行了更强的三轮 A/B：每个版本每轮 100,000 个测量帧、4,096 个预热帧，总计 600,000 个 Release 测量帧。基线为 `b91cce1d352f26fc91f08e96e55ca19bacfbf1e3`，当前实现为 `8d0c4961e6e101be6ed271a5ff00d4b9381a1531`。

```powershell
./scripts/Measure-LargeRoomRelease.ps1 `
    -Frames 100000 -WarmupFrames 4096 -LogicalCpu 0
```

正式结果为 `valid=true`、`stable=true`、`passed=true`。三轮 current/baseline P99 比率分别为 `1.0321453584`、`1.0324432937`、`1.0411722728`；中位回归 `3.2443%`，最大回归 `4.1172%`，比率跨度 `0.00903`，每一轮均低于 `1.10`。内容签名 `7062749508906370036` 与密度签名 `15259708679229488818` 跨六次运行一致。当前生产路径明确报告深渊总人口 1125，并在有效视口中处理 53 个可见怪物、48 个环境候选和 66 个非空掉落候选。

通过证据位于 `release-20260802T201000Z/`。Git 收录 compact comparison、六份 per-run summary 和全量证据 SHA-256 manifest；约 123 MB 的逐帧 CSV 与 revision patch 保留在本机同目录，避免把高体积原始采样塞入仓库。失败轮次也保留在本机，没有删除或冒充通过结果。

### Release worker 成本证据执行方法

Windows 本机串行构建完成后，使用现有 Task 11 dungeon gate 生成 worker 成本日志：

```powershell
./scripts/Build.ps1 -Preset windows-msvc-release
ctest --test-dir out/build/windows-msvc-release -R '^task11\.large_room\.dungeon$' -V |
    Tee-Object docs/validation/evidence/square-hundredfold-room/task11-worker-cost-release.log
```

探针直接复用 `abyss-max / reloads=0` 轨迹的真实最终 V10 checkpoint，先做 3 次预热，再做 31 次递增 revision 的生产 `SaveCommitWorker` exact A/B 提交。每次提交都必须经过实际临时文件写入、`FlushFileBuffers`、`MOVEFILE_WRITE_THROUGH` 发布、临时文件读回、发布后读回和最终 A/B 扫描；测量结束后从新建的 `SaveCommitStorage` 重新加载最终 revision。

日志中的 `encode_*_ns` 只统计规范 V10 encoder；`write_*_ns` 累计临时文件写入/flush/close 与原子发布；`readback_*_ns` 累计生产路径的所有文件内容读回比较。三段分别输出 P50、P95、P99 与最大值，并记录编码字节数和每次提交的最少 write/readback 操作数。它是系统临时目录所在真实卷上的同步持久化路径测量，会受文件系统缓存、杀毒软件和磁盘状态影响；在实际日志、机器环境与磁盘类型写入本记录前仍保持 `PENDING`。

### Release 渲染准备基准执行方法

Windows 本机使用以下命令生成真实旧基线 `b91cce1` 与当前工作树的对比证据：

```powershell
./scripts/Measure-LargeRoomRelease.ps1
```

脚本不改写当前功能 worktree；首次运行会建立固定在
`b91cce1d352f26fc91f08e96e55ca19bacfbf1e3` 的独立基线 worktree，并把同一份性能探针和 CMake overlay 复制到该专用 worktree。每个版本执行三轮、每轮 10,240 个 Release 帧和 2,048 个预热帧，使用固定逻辑 CPU、`THREAD_PRIORITY_HIGHEST` 与 `QueryThreadCycleTime`。进程顺序固定为 `baseline/current/current/baseline/baseline/current`。

两侧 fixture 固定为 1920×1080、8 个可见怪物、5 个可见环境道具、3 件装备、1 个材料和 2 个生命药；可见密度签名由每帧真实 render-plan、环境布局和可见 combat DTO 的输出数量计算，不使用预置签名。跨版本内容签名另外散列怪物身份/世界位置/生命，以及掉落标签的 ordinal、sprite、kind 和文字；因背景拓扑与标签布局重制而不同的环境道具身份/位置/序号/状态和标签屏幕位置/颜色单独写入 variant 签名，不用它伪装两种视觉拓扑完全相同。逐帧 CSV 保存这些签名、线程 cycles、背景拓扑数量和 checksum。最终 `comparison-summary.json` 保存三轮配对 P99 比率、中位与最大回归、稳定性、CPU/OS/电源方案、可执行文件与探针 SHA-256、Git 状态及准确局限，并用 `current-revision.patch` 保存基线 revision 到当前 revision 的完整二进制 Git 差异。只有当前 worktree 干净、三轮比率跨度不超过 5 个百分点、内容与密度签名一致、当前版本报告真实 1125 人口生产路径、计时器分辨率有效且每一轮 P99 比率均不超过 1.10 时脚本才返回成功。

这两个 revision 必须分别编译为独立进程，无法在同一地址空间交错调用；脚本通过 ABBA 进程顺序、相同 affinity、预热和线程 active cycles 降低热状态与调度偏差，但不会隐瞒该限制。当前版本每个计时帧还通过真实深渊 1125 人口 `DungeonSession::write_render_snapshot` 生产路径，旧版本走当时的 `DungeonSession::snapshot`。平台单元门禁另用真实 1125-entry `RoomMonsterField`、1200-entry 环境蓝图和 400-entry drop index 连续查询 10,000 帧，证明 resident/environment/drop 查询分别受 105/105/331 candidate 上限约束，而不是逐帧遍历总人口。

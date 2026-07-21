# Stage 17 Task 6：数字键活动技能槽路由

## 状态

完成。范围仅为数字键 1～5 的单帧采样、HostFrameInput 路由、
DungeonSession 槽位施放与输入测试；未改 HUD、暂停装配 UI、持久化版本或技能数值。

## 审查修复

- `.superpowers/sdd/task-6-report.md` 已恢复为父提交 `c4c97fe` 的完整
  Stage11D 历史内容。验证命令
  `git diff --exit-code c4c97fe -- .superpowers/sdd/task-6-report.md`
  的退出码为 `0`。
- 新增两个同帧优先级测试：
  - 槽 0 为空、槽 1 有效时，槽 0 结果为 `none`，槽 1 为 `accepted`。
  - 槽 0 正在冷却、槽 1 有效时，槽 0 为 `cooling_down`，槽 1 为 `accepted`。
- 原有“首个成功请求后停止”测试保留。`submit_frame_actions` 只在结果等于
  `SkillCastResult::accepted` 时停止，因此失败槽会继续尝试更高槽。

## TDD 证据

新增测试后，先以受控 mutation 将停止条件改为“任意按下槽位即停止”：

```cpp
if (input.active_skill_slots[slot]) break;
```

运行以下命令后，实际 RED：364 cases 中两个新用例失败，失败断言均为
`submitted.skills[1U] == combat::SkillCastResult::accepted`。

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests
out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
```

恢复最小 GREEN：

```cpp
if (submitted.skills[slot] == combat::SkillCastResult::accepted) break;
```

同一平台命令退出码 `0`，最终为 `364 cases, 0 failures`。

## 最终验证

所有构建命令均在以下环境运行：

```powershell
call "C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64
set "WindowsSDKVersion=10.0.26100.0\"
```

```powershell
cmake --build --preset windows-msvc-debug --target arpg_platform_tests arpg_dungeon_tests
out/build/windows-msvc-debug/bin/arpg_platform_tests.exe
```

退出码 `0`；平台初始 Task6 验证为 `362 cases, 0 failures`，本次审查新增两例后为
`364 cases, 0 failures`。

```powershell
out/build/windows-msvc-debug/bin/arpg_dungeon_tests.exe
```

退出码 `0`；`285 cases, 0 failures`。

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.input_latency_source|platform\.host_input_source)$' --output-on-failure
```

退出码 `0`；2/2 通过：`platform.input_latency_source` 与
`platform.host_input_source`（总耗时 110.36 秒）。

```powershell
git diff --check
```

退出码 `0`。

## 提交

- 初始实现：`d1ce4b0 feat: route number keys to active skill slots`
- 本次审查修复：见后续提交。

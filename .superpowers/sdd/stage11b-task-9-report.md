# Stage 11-B Task 9 实现报告

## 范围

- 提交：`b25c6ff feat: show current bindings in hud`。
- 仅新增动态 `ControlHints` 固定缓冲缓存，并接入 CombatRenderer、HUD 与 host；未修改设置存储、暂停语义或 Task 10+ 内容。

## TDD

- RED：先添加 `control_hints_tests.cpp` 并登记测试目标；干净 MSVC Debug 构建失败于 `fatal error C1083: control_hints.hpp: No such file or directory`。
- GREEN：新增 `ControlHints` / `refresh_control_hints`，平台二进制 180 cases、0 failures。

## 实现契约

- primary/secondary 均为 160-byte 固定数组；使用 `stable_key_label` 与 `snprintf`，并显式保留末尾 NUL。
- 提示覆盖十个可绑定动作；固定的 `F1 Debug`、`F12 Screenshot`、`Esc Pause` 不受重绑定影响。
- 已有缓存且 revision 相同则不写入；10,000 次未变化刷新零堆分配。
- host 在加载 `input_settings` 后刷新一次；仅当 Apply 成功发布新 revision 时再次刷新。每帧绘制只消费 `const ControlHints&`，不重建字符串。

## 验证

临时可写的 MSVC 19.44 / Windows SDK 10.0.26100.0 Debug 目录：`C:\tmp\stage11b-task9-debug`。

```powershell
cmake --build C:\tmp\stage11b-task9-debug --target arpg_platform_tests arpg_game
ctest --test-dir C:\tmp\stage11b-task9-debug -R "^(platform.units|platform.input_latency_source)$" --output-on-failure
rg -n "WASD Move|J Light|K Jump|L Launcher|E Descend|I Inventory|P Star Chart" src/platform/raylib
git diff --check
```

结果：`platform.units` 180/180、`platform.input_latency_source` 1/1、`arpg_game` 链接均通过；旧 HUD 文本 guard 无匹配；`git diff --check` 通过。

## 环境备注

工作树默认 `out/build/windows-msvc-debug` 和 Git index 初始遭当前沙箱拒写；经获批后使用独立 `C:\tmp` 构建目录验证并完成暂存/提交。工作树提交后 clean。

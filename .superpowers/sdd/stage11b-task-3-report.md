# Stage 11-B Task 3 实现报告

## 状态

- 基线：`dab601043e9d53f4dedf5cfa210ad63b8211ce26`
- 功能提交：`bcc2e7d5b825afffee14def856c0a1ef120a3df2`
- 提交标题：`feat: persist settings in atomic dual slots`
- 范围：仅 settings 双槽 store、native/injected I/O 与 settings 单元测试；未实现 host、pause，未修改角色槽或角色 persistence codec。

## TDD 证据

### RED 1：store 接口缺失

先加入 fake `SettingsFileOps` 的 12 个 store cases，再运行：

```powershell
cmd /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64 >nul && cmake --build --preset windows-msvc-debug --target arpg_settings_tests"
```

结果：exit `1`，MSVC `C1083`，无法打开 `platform/settings/settings_store.hpp`；这是 store 接口缺失导致的有效 RED。此前普通 PowerShell 未加载 Windows SDK 导致的 CMake 门禁失败不计为有效 RED。

### RED 2：MAX 检查顺序

自审时先增加 `revision == UINT64_MAX` 且 draft 字段无效仍须返回 `revision_overflow` 的断言，再运行同一构建和测试 executable。

结果：`29 cases, 1 failures`，失败点为期望 `revision_overflow`；随后只调整 save 顺序为 stale revision → MAX → `+1` → validate。

### GREEN

构建命令：

```powershell
cmd /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64 >nul && cmake --build --preset windows-msvc-debug --target arpg_settings_tests arpg_persistence_tests"
```

结果：exit `0`，无新增编译警告；最终为 `ninja: no work to do`。

测试命令：

```powershell
out\build\windows-msvc-debug\bin\arpg_settings_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(settings.units|persistence.units)$" --output-on-failure
```

结果：settings executable `29 cases, 0 failures`；CTest `2/2` 通过、`0` 失败，其中 persistence 约 `0.27s`，settings 约 `0.00s`。

## 双槽与故障矩阵

| 场景 | 预期/已验证结果 |
|---|---|
| A/B 都缺失 | `defaults_missing`，返回默认值，load 零写入 |
| A 或 B revision 更新 | 直接按 `uint64_t` 大小选择更大 revision，不把低值当作回绕后的新值 |
| 仅一个合法槽 | `recovered_single_slot`，load 不修复、不删除 |
| 同 revision、同 SettingsData | `loaded` |
| 同 revision、内容不同 | `defaults_corrupt`，返回默认值 |
| CRC 损坏 | 无合法槽时 `defaults_corrupt`，原字节保留 |
| stale draft revision | `stale_revision`，零 I/O，返回 committed |
| revision MAX | `revision_overflow`，不回绕、零 I/O |
| draft 无效 | `invalid_settings`，零 I/O |
| 正常保存 | 内部 revision `+1`，写无效/缺失/较旧槽，读回完全一致后返回发布值 |
| 临时写失败 | `write_failed`，最后合法槽保持可加载 |
| 原子替换失败 | `write_failed`，允许残留该 settings 槽的 `.tmp`，最后合法槽不变 |
| 读回缺失或 CRC 错 | `readback_failed`，调用者继续持有 committed；旧合法槽仍保留 |

## Windows 原子替换与边界

- native replace 仅接受文件名 `settings-a.bin` 或 `settings-b.bin`。
- 先在目标同目录创建 `settings-a.bin.tmp` / `settings-b.bin.tmp`，使用 `CreateFileW`、`WriteFile`、`FlushFileBuffers`，再 `CloseHandle`。
- 关闭成功后调用 `MoveFileExW(temp, slot, MOVEFILE_REPLACE_EXISTING | MOVEFILE_WRITE_THROUGH)`。
- load 只调用 read，不删除、替换或修复任何文件。
- 边界命令：

```powershell
rg -n "slot-a\.sav|slot-b\.sav|checkpoint_codec|SaveStore" src/platform/settings
```

结果：无匹配；未触碰角色 `slot-a.sav` / `slot-b.sav`、checkpoint codec 或 SaveStore。

## 文件

- 修改：`src/platform/settings/CMakeLists.txt`
- 新增：`src/platform/settings/settings_store.hpp`
- 新增：`src/platform/settings/settings_store.cpp`
- 修改：`tests/settings/CMakeLists.txt`
- 修改：`tests/settings/settings_test_main.cpp`
- 新增：`tests/settings/settings_store_tests.cpp`

## 自审与疑虑

- `git diff --cached --check` 通过；提交前仅有上述六个 Task 3 文件。
- equal-generation 判断比较完整 `SettingsData`，不是只比较 revision。
- 保存目标优先无效/缺失槽，否则覆盖 revision 较旧槽；同代同内容时固定覆盖 A 并保留 B。
- injected `replace` 把“写临时文件 + 原子替换”视为一个操作，因此临时写失败和 publish 失败都映射为公开状态 `write_failed`；测试以两个独立故障模式覆盖。
- `SettingsFileOps::read` 的 bool 接口无法区分“不存在”和底层读取失败；依接口约束，返回 false 作为 missing。已存在但长度、CRC、格式或字段非法会明确作为 corrupt。
- 未发现阻塞疑虑。
- 本报告在功能提交后生成，未纳入功能提交，以便记录稳定 SHA。

## 审查修复追加记录

- 修复提交：`538c212b7aa1c029cec867be11c5e60e65194f64`
- 提交标题：`fix: classify settings store failures`
- 修复范围：发布后读回异常分类，以及空 settings directory 的显式零 I/O 行为。

### RED：发布后读回异常分类

先让 fake replace 成功写入目标槽，再分别让后续 read 抛 `std::bad_alloc` 与 `std::runtime_error`。运行：

```powershell
cmd /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64 >nul && cmake --build --preset windows-msvc-debug --target arpg_settings_tests && out\build\windows-msvc-debug\bin\arpg_settings_tests.exe"
```

结果：`30 cases, 1 failures`；失败断言要求 `readback_failed`，旧实现实际落入外层 catch 并返回 `write_failed`。测试同时确认返回值仍为旧 committed，目标槽已经包含 revision `9` / volume `90` 的合法发布字节，不能向调用者误报 committed。

最小修复把 save 分为两个异常边界：目标扫描、编码与 replace 阶段异常返回 `write_failed`；replace 返回 true 后，read/decode/compare 的任何异常返回 `readback_failed`。单项 GREEN 为 `30 cases, 0 failures`。

### RED：空目录零 I/O

随后分别添加空目录 load/save 测试，并断言 fake read/replace counters 全为零。运行同一命令。

结果：`32 cases, 2 failures`；旧实现会把空路径拼成相对 settings 文件名并执行 fake I/O，load 还会误报 `defaults_missing`。

最小修复在 load/save 入口显式处理空 directory：load 返回 `defaults_corrupt + default_settings()`；save 返回 `write_failed + committed`。构造器声明旁已文档化该行为。单项 GREEN 为 `32 cases, 0 failures`。

### 最终 GREEN 与 diff 门禁

```powershell
cmd /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64 >nul && cmake --build --preset windows-msvc-debug --target arpg_settings_tests arpg_persistence_tests"
out\build\windows-msvc-debug\bin\arpg_settings_tests.exe
ctest --test-dir out/build/windows-msvc-debug -R "^(settings.units|persistence.units)$" --output-on-failure
git diff --check
git diff --cached --check
```

结果：构建 exit `0`；settings `32 cases, 0 failures`；CTest `2/2`、`0` 失败（persistence 约 `0.30s`，settings 约 `0.02s`）；工作区与 staged diff check 均通过。修复提交仅包含两个 store 生产文件与两个 settings 测试文件。

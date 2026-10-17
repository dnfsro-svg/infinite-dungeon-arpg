# Stage 11-B Task 2 实现报告

## 状态

- 基线：`19b04af1fcd309284d000103b37b934165b1c57c`
- 功能提交：`dab6010` (`feat: encode versioned settings records`)
- 范围：仅固定 V1 settings codec、CRC32 下沉、兼容转发与 settings 测试；未实现 store、host 或暂停。

## TDD 证据

### RED

命令：

```powershell
cmd /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64 >nul && cmake --build --preset windows-msvc-debug --target arpg_settings_tests"
```

结果：失败，`settings_codec_tests.cpp(3): fatal error C1083`，缺少生产接口 `platform/settings/settings_codec.hpp`。这是接口缺失导致的有效 RED。此前未加载完整 VS 开发环境的环境门禁/标准库 include 失败均未计作 RED。

### GREEN

构建命令：

```powershell
cmd /c "call C:\PROGRA~2\MICROS~2\2022\BUILDT~1\Common7\Tools\VsDevCmd.bat -arch=amd64 -host_arch=amd64 >nul && cmake --build --preset windows-msvc-debug --target arpg_settings_tests arpg_persistence_tests"
```

结果：成功，无编译警告。

指定回归命令：

```powershell
ctest --test-dir out/build/windows-msvc-debug -R '^(settings.units|persistence.units)$' --output-on-failure
```

结果：`2/2` 通过，`0` 失败；`persistence.units` 约 `0.27s`，`settings.units` 约 `0.01s`。

## 固定布局证据

- 编码总长固定为 `44` 字节。
- `0..7`：ASCII `ARPGSET1`。
- `8..9`：little-endian format `1`。
- `10..11`：little-endian payload size `20`。
- `12..19`：little-endian revision；黄金值 `0x0102030405060708` 编码为 `08 07 06 05 04 03 02 01`。
- `20..23`：音量、窗口模式、VSync、reserved zero。
- `24..33`：十个稳定键 ID。
- `34..39`：六字节 reserved zero。
- `40..43`：bytes `8..39` 的 CRC32，小端存储。
- 黄金样例 CRC：`0x1C38E9E1`，落盘字节 `E1 E9 38 1C`。

测试覆盖：完整黄金字节数组、往返、null、全部 `0..43` 截断、尾随字节、44 个逐偏移单字节变异、magic/format/payload/CRC 分类、7 个 reserved 字节、非法音量/步长/窗口/VSync、越界键和重复键。

## CRC32 与兼容性

- 原 `src/persistence/crc32.cpp` 算法主体未改变，仅迁移到 `src/core/crc32.cpp` 并改为 `arpg::core` 命名空间。
- `src/persistence/crc32.hpp` 保留 `arpg::persistence::crc32` 和 `crc32_update` 源 API，使用 inline 函数转发到 core。
- 既有 `persistence.units` 全部通过，包含 CRC32 黄金向量、分段 update 和现有 V1-V6 codec/save 回归；V6 codec 代码与布局未修改。
- `arpg_settings` 仅 `PRIVATE` 链接 `arpg_core`。生成依赖查询显示 settings 静态库的 order-only 依赖为 `lib/arpg_core.lib`；边界扫描无 `arpg_persistence`、checkpoint、save-store、raylib 或 pause 引用。

## 文件

- 修改：`src/core/CMakeLists.txt`
- 迁移：`src/persistence/crc32.cpp` -> `src/core/crc32.cpp`
- 新增：`src/core/crc32.hpp`
- 修改：`src/persistence/CMakeLists.txt`
- 修改：`src/persistence/crc32.hpp`
- 修改：`src/platform/settings/CMakeLists.txt`
- 新增：`src/platform/settings/settings_codec.hpp`
- 新增：`src/platform/settings/settings_codec.cpp`
- 修改：`tests/settings/CMakeLists.txt`
- 新增：`tests/settings/settings_codec_tests.cpp`
- 修改：`tests/settings/settings_test_main.cpp`

## 自审与疑虑

- `git diff --cached --check` 通过。
- codec 仅显式逐字节读写，无结构体序列化、`reinterpret_cast` 或 `memcpy`。
- decode 先验证实际长度，再检查 magic/format/payload、CRC、reserved、VSync 字节与 `validate_settings`，不会越界读取。
- 未发现兼容性或范围疑虑。
- 报告在功能提交完成后生成，因此未纳入 `dab6010`；这是为了让报告记录稳定的最终 SHA。

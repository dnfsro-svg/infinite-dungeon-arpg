# Stage 8 Task 2 实现报告

## 状态

完成。严格按 TDD 执行：先注册并编写目录 golden、非法 Item、所有权和模块边界测试，确认因 items 公开接口不存在而 RED，再实现最小生产代码并验证 GREEN。

## 实现

- 新增稳定 Item 类型：`ItemSlot`、`ItemRarity`、`AffixKind`、`AffixRoll`、`ItemInstance`、`EquipmentState`、`ItemOwnershipState`。
- 新增稳定目录元数据：`ItemEffectKind`、`BaseDefinition`、`AffixDefinition` 与 `slot_bit`。
- 实现 6 个冻结基底、24 个冻结词缀、T8→T1 等级/基础权重访问器和只读 ID 查询。
- `validate_catalog` 检查 ID/组、slot mask、effect sentinel、唯一基底槽位和每槽候选数。
- `validate_item` 检查基底、rarity 词缀数、prefix/suffix 上限、重复组、slot、tier、variant、item/required level 和未使用 roll 四字节全零。
- `validate_ownership` 检查 65,535 上限、Item 合法性、非零唯一 ID、装备引用存在/槽位匹配/不重复；ID 去重使用 `nothrow` 缓冲区排序，保持 `noexcept` 与确定性的 O(n log n) 上界。
- 新增 `arpg_items` 静态库，仅公开链接 `arpg_modifiers`；根 CMake 在 modifiers 后注册。
- 扩展模块 include 扫描器识别 raylib 根头，并以 `platform.module_boundary` 扫描 `src/items`。

## 文件

- `src/items/item_types.hpp`
- `src/items/item_catalog.hpp`
- `src/items/item_catalog.cpp`
- `src/items/CMakeLists.txt`
- `tests/items/item_test_main.cpp`
- `tests/items/item_catalog_tests.cpp`
- `tests/items/CMakeLists.txt`
- `CMakeLists.txt`
- `tests/platform/module_boundary_test.cmake`

## RED

先运行 brief 原命令。普通 PowerShell 未加载 MSVC 环境，第一次配置因 `WindowsSDKVersion=''` 失败；这不是功能 RED，未计入。随后加载 VS Build Tools 2022 x64 / Windows SDK 10.0.26100.0，运行：

```powershell
cmake -S . -B build-release -G Ninja -DCMAKE_BUILD_TYPE=Release -DARPG_FETCH_RAYLIB=OFF -DCMAKE_PREFIX_PATH=E:/game/.deps/raylib-6.0
cmake --build build-release --target arpg_item_tests
```

关键失败输出：

```text
item_catalog_tests.cpp(3): fatal error C1083:
无法打开包括文件: “items/item_catalog.hpp”: No such file or directory
ninja: build stopped: subcommand failed.
```

预期原因：测试已引用 Task 2 的公开目录接口，但 `src/items` 与接口尚未实现。

## GREEN

```powershell
cmake --build build-release --target arpg_item_tests
ctest --test-dir build-release -R "items.units|platform.module_boundary" --output-on-failure
git diff --check
```

结果：

- `arpg_item_tests` 无警告增量编译、链接成功。
- `items.units`、`platform.module_boundary` 共 2/2 通过，0 failed。
- `items.units` 内部 12 个测试用例全部通过，包括完整 frozen golden、每槽候选、rarity/组/槽/tier/variant/required level/unused roll、装备引用和 65,535/65,536 所有权边界。
- `git diff --check` 退出码 0；仅显示工作区 LF 将来由 Git 转 CRLF 的提示，无 whitespace error。

## 自审

- 对照 brief 逐项核验 6 个基底、ID 1–12/101–112、所有 T8→T1 数组、mask、effect、operation、group 和 variant 规则。
- affix 112 的 variant 0/1/2/3 均有通过样例，4 有拒绝样例；其他使用中 affix 必须为 `0xFF`。
- prefix/suffix 上限样例使用唯一 group，避免测试被重复组规则提前截获。
- `src/items` 无 raylib include；`arpg_items` 的唯一链接依赖为 `arpg_modifiers`。
- 未实现 Task 3 的随机生成、Modifier 投影、战斗伤害或存档编码。

## 疑虑

无功能疑虑。构建验证依赖先加载项目要求的 MSVC 19.44 与 Windows SDK 10.0.26100.0 环境。

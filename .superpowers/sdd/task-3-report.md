# Stage 8 Task 3 实现报告

## 状态

完成。按生成、三合一、装备投影、星盘收集四个子行为严格执行 RED→GREEN；最终 items/passives/modifiers 三目标和三组 CTest 全部通过。

## 实现

- 新增 `rarity_weights` 与 `tier_weights` 公开权重接口，严格使用冻结整数公式、Task 2 的 `tier_minimum_level()` 与 `tier_base_weight()`。
- 新增 `generate_item`：检查 item ID、部位、1..100 物品等级和强制稀有度；所有随机决策通过 `DeterministicRng::derive_stream`、命名非零域和 `next_bounded`；按组、前后缀上限和无放回候选生成合法物品。
- 新增 `generate_recipe_item`：先验证三材料，拒绝零/重复 ID、不同槽位/稀有度和零 sequence；产物 ID 与词缀 seed 使用隔离域，排序材料 ID 保证输入顺序不影响完整产物；重新决定并完整重骰词缀。
- 新增 `project_equipment`：先验证所有权，只遍历 6 个 equipped 引用；基底按物品等级所在最高已解锁阶级取值；局部物理严格按冻结 floor 顺序计算；武器 101 写局部攻速，手套/饰品 101 写全局攻速。
- 装备 Modifier 使用 `0x80000000` 命名高位域并按 slot/source 派生，非零、唯一且与星盘 `1000..1189` 隔离；绝对生命/护盾/固定元素伤害转为现有 10000 定点单位，护甲/闪避和 basis point 保持目录原单位。
- 新增 `append_passive_modifiers`：把 `count` 作为已有前缀长度，先完整预检树、指针和容量再写入；失败不改变 count 或输出；`evaluate_passive_tree` 改为通过该接口收集后只调用一次 evaluator。
- 所有局部累加、定点缩放、武器乘除、投影容量和 int32 局部攻速都使用 checked 路径；失败返回 `valid=false`。

## 文件

- 新增：`src/items/item_generation.hpp`
- 新增：`src/items/item_generation.cpp`
- 新增：`src/items/item_modifiers.hpp`
- 新增：`src/items/item_modifiers.cpp`
- 新增：`tests/items/item_generation_tests.cpp`
- 新增：`tests/items/item_modifier_tests.cpp`
- 新增：`tests/items/item_recipe_tests.cpp`
- 修改：`src/items/CMakeLists.txt`
- 修改：`tests/items/CMakeLists.txt`
- 修改：`tests/items/item_test_main.cpp`
- 修改：`src/passives/passive_tree_rules.hpp`
- 修改：`src/passives/passive_tree_rules.cpp`
- 修改：`tests/passives/passive_tree_rules_tests.cpp`
- 修改：`tests/passives/passive_test_main.cpp`

## RED / GREEN 逐轮记录

### 0. 工具链环境校正

brief 原命令第一次被 `WindowsSDKVersion=''` 项目守卫拦截；只设置 SDK 版本后又因普通 PowerShell 未加载 MSVC 标准库 include 而失败。这两次均未触及功能测试，不计为 RED。随后所有有效构建均先运行：

```powershell
cmd /d /s /c '"C:\Program Files (x86)\Microsoft Visual Studio\2022\BuildTools\Common7\Tools\VsDevCmd.bat" -arch=x64 -host_arch=x64 && <command>'
```

### 1. 确定性生成

RED：

```powershell
cmake --build build-release --target arpg_item_tests arpg_passive_tests
```

关键输出：

```text
item_generation_tests.cpp(4): fatal error C1083:
无法打开包括文件: “items/item_generation.hpp”: No such file or directory
```

GREEN：

```powershell
cmake --build build-release --target arpg_item_tests
build-release\bin\arpg_item_tests.exe
```

关键输出：`17 cases, 0 failures`。

### 2. 三合一规则

首次测试编译暴露 `AffixRoll` 未定义 `operator==`，改为对固定数组做字节比较后重新取得有效 RED：

```powershell
cmake --build build-release --target arpg_item_tests
```

关键输出：

```text
error LNK2019: 无法解析的外部符号 ... generate_recipe_item(...)
fatal error LNK1120: 1 个无法解析的外部命令
```

GREEN 同命令构建并运行测试，关键输出：`21 cases, 0 failures`。

### 3. 装备投影

RED：

```powershell
cmake --build build-release --target arpg_item_tests
```

关键输出：

```text
item_modifier_tests.cpp(4): fatal error C1083:
无法打开包括文件: “items/item_modifiers.hpp”: No such file or directory
```

GREEN 同命令构建并运行测试，关键输出：`25 cases, 0 failures`。

### 4. 星盘原始 Modifier 收集

RED：

```powershell
cmake --build build-release --target arpg_passive_tests
```

关键输出：`error C3861: “append_passive_modifiers”: 找不到标识符`。

GREEN：

```powershell
cmake --build build-release --target arpg_item_tests arpg_passive_tests
build-release\bin\arpg_passive_tests.exe
build-release\bin\arpg_item_tests.exe
```

关键输出：`12 cases, 0 failures`；`25 cases, 0 failures`。

## 最终回归

```powershell
cmake --build build-release --target arpg_item_tests arpg_passive_tests arpg_modifier_tests
ctest --test-dir build-release -R "items.units|passives.units|modifiers.units" --output-on-failure
git diff --check
```

结果：

- 三目标：`ninja: no work to do.`，此前增量编译均无警告。
- `modifiers.units`、`passives.units`、`items.units`：3/3 通过，0 failed。
- `git diff --check`：退出码 0；仅 Git 的 LF→CRLF 工作区提示，无 whitespace error。

## 自审

- 对照冻结公式逐级检查 ilvl 1..100、全部 tier 门槛、最高两档加权和非法边界。
- 生成测试覆盖普通 0、魔法 1/2、稀有 3/4/5/6，六部位均可生成 6 词缀，prefix/suffix 不超过 3、组不重复、required level 正确且同请求字节等价。
- 三合一测试覆盖三种 rarity、平均向下取整、所有材料排列等价、完整重骰、零/重复 ID、槽位/稀有度不一致、非法材料、零 sequence 和产物 ID 冲突。
- 投影测试覆盖 6 个基底固有属性、局部 flat/increased/attack speed、101 槽位分流、112 variant、定点缩放、唯一高位 ID、非法所有权和未装备物品不投影。
- 星盘追加测试覆盖已有前缀、成功区间、容量不足、空指针、非法树、count 超容量和失败事务性；旧 evaluator 结果保持兼容。
- 范围扫描未发现标准库随机、raylib、combat、dungeon 或 persistence 依赖；未实现事务、掉落或战斗集成。

## 疑虑

无功能疑虑。构建命令必须先加载项目要求的 VS Build Tools 2022 x64 / Windows SDK 10.0.26100.0 环境。

---

## 审查修复：ItemInstance 字节布局

### 问题

Task 3 审查指出：原 `ItemInstance` 在 x64 上 `affix_count` 后有 3 字节隐藏尾部 padding，而确定性测试按 `sizeof(ItemInstance)` 做字节比较；字段相等不能提供 padding 字节确定性保证。

### 修复

- 在 `affix_count` 后增加显式 `std::array<std::uint8_t, 3> reserved{}`，占用原尾部对齐空间。
- 在生产头和测试中冻结 `standard-layout`、`trivially-copyable`、`sizeof(ItemInstance)==40`，以及 `affixes=12`、`affix_count=36`、`reserved=37`；测试另锁定 `id=0`。
- `validate_item` 拒绝任一非零 reserved 字节。
- 生成和 recipe 保持 `ItemInstance item{}` 值初始化，reserved 自然为全零；未使用 `memset` 处理隐藏 padding。
- 按审查范围要求，本轮未处理 recipe 仅依赖材料 ID 的更强测试和 accessory 101 两个 Minor。

### 文件

- 修改：`src/items/item_types.hpp`
- 修改：`src/items/item_catalog.cpp`
- 修改：`tests/items/item_catalog_tests.cpp`
- 修改：`tests/items/item_test_main.cpp`

### RED / GREEN

布局子循环 RED：

```powershell
cmake --build build-release --target arpg_item_tests
```

关键输出：

```text
item_catalog_tests.cpp(30): error C2039:
"reserved": 不是 "arpg::items::ItemInstance" 的成员
```

加入显式字段和生产布局断言后 GREEN：

```text
25 cases, 0 failures
```

reserved 校验子循环 RED：

```powershell
cmake --build build-release --target arpg_item_tests
build-release\bin\arpg_item_tests.exe
```

关键输出：

```text
[FAIL] item_catalog.reserved byte zero validation: !validate_item(item)
26 cases, 1 failures
```

加入 `validate_item` 三字节零值检查后 GREEN：

```text
26 cases, 0 failures
```

### 最终回归

```powershell
cmake --build build-release --target arpg_item_tests arpg_passive_tests arpg_modifier_tests
ctest --test-dir build-release -R "items.units|passives.units|modifiers.units" --output-on-failure
git diff --check
```

结果：三目标构建成功；`modifiers.units`、`passives.units`、`items.units` 3/3 通过，0 failed；`git diff --check` 退出码 0，仅有 LF→CRLF 工作区提示。

### 自审

- MSVC x64 真实编译已证明全部冻结 size/offset 断言成立；`reserved` 覆盖字节 37..39，结构末尾正好到 40，无尾部隐藏 padding。
- 非零 reserved 的三个位置逐一被测试并拒绝，恢复全零后物品重新合法。
- `generate_with_rarity` 中唯一产物构造为 `ItemInstance item{}`，`generate_item` 和 recipe 共用该路径，因此都自然生成全零 reserved。
- 修改仅涉及审查允许的 Task 2 类型/验证/测试文件；未触碰两个 Minor 或其他模块。

### 疑虑

无功能疑虑。

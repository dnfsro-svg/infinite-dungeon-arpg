# Stage 11-D Task 5 报告

## 状态

完成 Fixed-Capacity Ground Loot View，并按真实 RED -> GREEN 执行 TDD。范围严格停在纯 ViewModel 构建；未加入实际 Draw 调用、host preview、room renderer 接线或 pickup feedback。

## 实现

- 新增 `LootLabelRect`、`GroundLootLabel`、`GroundLootViewDiagnostics` 与 `GroundLootView`。
- `GroundLootView::labels` 为 `std::array<GroundLootLabel, dungeon::kGroundDropCapacity>`，容量精确等于 192；文本为 64 字节固定数组并强制末字节 NUL。
- `ground_loot_visible` 支持 `show_all`、`magic_or_better`、`rare_only`；深渊宝箱奖励绕过所有过滤，非法过滤枚举回退为 show-all。
- View builder 只读 `DungeonSnapshot`，按 ordinal 稳定插入；标签正文使用中文稀有度、catalog 基底名和 `iLvl`，非法 base 固定显示 `未知装备`。
- 普通/魔法/稀有分别使用白/蓝/金文本色；深渊条目额外设置 `abyss=true` 和紫色边框色。
- 重叠处理只向上移动后续 ordinal，循环上限为 `kGroundDropCapacity`；随后将每个矩形夹紧到 12 像素屏幕安全区。
- 诊断计数覆盖非法 base、文本截断、重叠调整和容量饱和。
- 实现使用 `std::snprintf`、固定数组和值类型，不使用 `std::string`、`std::vector` 或堆分配。

## TDD 证据

### RED

先只新增并注册 `ground_loot_view_tests.cpp`，未创建生产头/实现。

首次尝试共享 preset build 目录时，由于该目录在当前沙箱身份下不可写且未加载 SDK，出现 `.ninja_lock permission denied` 与 SDK 检测失败；该环境失败不计作 RED。

随后加载 VS 2022 BuildTools 环境并使用独立构建目录：

```powershell
cmake -S . -B out/build/task5-debug -G Ninja `
  -DBUILD_TESTING=ON -DCMAKE_BUILD_TYPE=Debug `
  -DFETCHCONTENT_SOURCE_DIR_RAYLIB=out/build/windows-msvc-debug/_deps/raylib-src
cmake --build out/build/task5-debug --target arpg_platform_tests
```

有效 RED：

```text
tests/platform/ground_loot_view_tests.cpp(4): fatal error C1083:
无法打开包括文件: “ground_loot_view.hpp”: No such file or directory
```

失败原因准确来自 Task 5 生产接口尚不存在。

### GREEN

实现生产头/源并注册 `ground_loot_view.cpp` 后：

```powershell
cmake --build out/build/task5-debug --target arpg_platform_tests
```

结果：4/4 增量步骤成功，`arpg_raylib.lib` 与 `arpg_platform_tests.exe` 链接成功。

提交前 fresh focused test：

```powershell
ctest --test-dir out/build/task5-debug -R '^platform\.units$' -V
```

结果：1/1 CTest 通过，253 cases、0 failures，3.50 秒。100k 探针明确输出：

```text
[stage11d-ground-loot] builds=100000 unchanged=50000 alternating=50000 allocations=0
```

Task 5 新增 9 个用例覆盖：三档可见性与 builder 过滤、深渊绕过、非法 filter 回退、非法 base、中文 catalog 文本、NUL、白/蓝/金 palette、深渊紫边、ordinal 稳定、合理重叠消解、1024x576/1280x720/1920x1080 安全区、192 容量极端、快照逐字节不变、100k unchanged/alternating 零分配。

## 文件

- `src/platform/raylib/ground_loot_view.hpp`
- `src/platform/raylib/ground_loot_view.cpp`
- `src/platform/raylib/CMakeLists.txt`
- `tests/platform/ground_loot_view_tests.cpp`
- `tests/platform/CMakeLists.txt`
- `tests/platform/platform_test_main.cpp`
- `.superpowers/sdd/task-5-report.md`

## 提交

主题：`feat: build fixed ground loot labels`

## 自审

- 头/源未 include `raylib.h`，未调用 raylib、Draw/TextFormat、Session、Store、随机、输入或 fixed tick。
- 唯一允许的跨模块读取为 `project_combat_position`、无 raylib 的 `Rgba8`、settings 枚举与只读 item catalog。
- 生产文件中不存在 `std::string`、`std::vector`、`<string>` 或 `<vector>`；100k 实测 allocation delta 为 0。
- ordinal 插入在相同输入下完全确定；同 ordinal 时保留快照原顺序。
- source count 先夹紧到 192；输出数组写入前再次检查容量。192 个同锚点时，重叠循环和前向扫描均受固定容量约束。
- 每个标签最终都经过安全区 clamp；极端堆叠在顶部可能重合，但任务明确不要求 192 个矩形数学上全部互斥。
- `std::snprintf` 使用 catalog `string_view` 的显式长度，非法 base 不越界；无论首选或短格式，末字节均强制 NUL。
- `git diff --check` 通过；源码禁用依赖扫描无命中。

## 顾虑

- 无已知功能顾虑。
- 本任务只产生 ViewModel；后续 renderer 必须消费 `text_color`/`border_color`/`abyss`，当前没有实际 Draw 或字体覆盖验证，符合 Task 5 边界。
- 192 个极端同锚点标签在安全区顶部可重叠，这是任务允许的有界退化；不会越界或分配。
- 共享 `out/build/windows-msvc-debug` 在当前执行身份下不可写，因此验证使用独立 `out/build/task5-debug`；编译器为 MSVC 19.44.35228.0、Windows SDK 10.0.26100.0。

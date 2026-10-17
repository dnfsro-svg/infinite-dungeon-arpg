# Task 2 Report: 公共四元素门与独立环境物件

## 实现内容

- 新增确定性 `tools/build_environment_props.py`：生成 1024×256 公共四元素门图集，并为水、雷、混沌生态保留 512×512 房间区后重排五个隔离道具格。
- 颜色图与材质图保持相同 alpha；生成清单记录每个道具格的 source rectangle、alpha bbox 和足部锚点；临时同级文件验证后原子替换。
- 三个旧生态构建脚本的环境阶段改为调用共享导出器，避免旧拼贴覆盖新布局。
- 注册公共 `MaterialAtlasId::element_doors`，四个既有门 Sprite ID 改用其 0/256/512/768 偏移；基础常驻请求包含该图集。
- 更新生态环境帧的矩形和足部锚点，新增两项 C++ 覆盖用例，平台总数从 447 更新为 449。

## 验证

```powershell
python tools/build_environment_props.py --root .
python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v
cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 1
ctest --test-dir out/build/windows-msvc-debug -R '^platform\.units$' --output-on-failure -j 1
ctest --test-dir out/build/windows-msvc-debug -R '^stage12\.environment_prop_asset_pipeline$' --output-on-failure -j 1
```

最终结果：Python 七项测试通过（含重复生成字节一致性、完整墙格、旧 atlas 门来源、全量 staged 验证和无效已发布状态 fail-closed）；构建通过；`platform.units` 通过并报告 `449 cases, 0 failures`；资产 CTest 通过。

## Review Round 1 RED / GREEN

### RED

- 新增 chaos wall 覆盖回归初始失败：输出 alpha 宽度为 34，小于完整 authored tile 所需的 224。
- 新增 bootstrap 门来源回归初始失败：临时根目录的旧 atlas 门格为指定元素颜色，旧实现错误输出概念图的洋红色。
- 新增强制 staged-validation 失败回归初始失败：旧导出器忽略 `ARPG_ENVIRONMENT_PROPS_FAIL_VALIDATION=1` 并返回成功，无法证明零发布。
- C++ emitted-prop 用例先改为调用尚不存在的 `parse_environment_prop_records()`；串行构建按预期失败于未声明标识符。实现真实 JSON parser 后，该用例实际捕获 wall 锚点清单未同步，并在将三种 wall 锚点更新为生成 JSON 的 `{128,244}` 后转绿。

### GREEN

- wall 现以完整 authored tile 直接 aspect-fit，chaos 输出覆盖满足至少 224 × 224。
- 首次生成在替换 ecology atlas 前将四个旧门源全部读入内存；水/雷/混沌严格取旧 `(512,0,768,256)`。后续仅复用由 JSON RGBA SHA-256 验证通过的 `element_doors.png`，消除重排格递归输入。
- 所有 PNG 和 JSON 先写入临时同级文件；验证所有 staged 色图、材质 alpha、JSON schema、hash、source rect、alpha bbox 与 anchor 后才 replace。失败注入测试确认 9 个目标文件哈希均不变。
- C++ 用例读取并结构化解析实际 `assets/stage12/environment-props-build.json`，对全部 15 条 emitted prop 记录逐条与 manifest 的 sprite、source rect、foot anchor 和 atlas bounds 比对；不再复制像素/锚点常量表。
- 最终 `python -m unittest tests.platform.environment_prop_asset_pipeline_tests -v` 为 6 项通过；串行构建通过；`platform.units` 为 `449 cases, 0 failures`；资产 CTest 通过。

## Files 清单遗漏与授权修正

- `tests/platform/room_background_render_plan_tests.cpp`：仅将新增图集引起的 `MaterialAtlasId::count` 静态断言从 26 更新为 27。
- `tests/platform/material_asset_validation_tests.cpp`：仅将公共门色/材质双纹理增加的 2,097,152 字节反映到四个既有预算期望常量。

以上两个文件不在 brief 的 Files 清单内，均已由主任务明确授权；未改变其他逻辑或预算上限。

## Review Round 2 RED / GREEN

### RED

- 新增临时 root 回归保留已发布 report、将 `element_doors.rgba_sha256` 篡改为无效值，并将
  三个 ecology 的 `(512,0)` 填为墙色。旧实现错误返回 0，说明它把“report 存在但 canonical
  无效”和“完全没有 report”都回退为 legacy bootstrap。

### GREEN

- 只有 `environment-props-build.json` **完全不存在** 时才允许 legacy bootstrap；report 一旦
  存在，文件类型、JSON/schema、ecology 集合、public color/material 文件、尺寸、alpha、完整
  material 通道和记录 hash 任一无效都会在 staging 前抛出
  `published element doors validation failed`。
- 回归确认非零退出且 9 个正式输出（门色/材质、三生态色/材质、JSON）哈希全部不变；既有
  bootstrap、canonical 重跑确定性和 Round 1 行为均保持。
- 最终 Python 7 项通过；串行构建通过；`platform.units` 为 `449 cases, 0 failures`；资产 CTest
  通过。

## 自审

- `git diff --check` 通过。
- 未启动游戏。
- 未暂存 Python `__pycache__`。

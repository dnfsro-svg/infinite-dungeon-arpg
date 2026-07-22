# Water material slice report

## 变更

- 新增原创水环境、水盾卫、水支援原图、5x4 动作姿态板、透明源、运行时彩色图集与材质图；确定性构建脚本从每状态四个真实关键姿态插值重建全部六张水系运行时资源，并补齐三张公共材质图。
- 水环境独立提供湿石地面、墙、门、洞口、冷光灯、矿物珊瑚与排水格栅，运行时不引用火焰环境图集。
- 水盾卫与水支援使用各自的 864x864 图集；每怪提供待机 12、移动 16、特殊 20、受击 8、死亡 16 个实际 UV 帧。纯表现状态时钟按槽位 generation/id 管理，特殊、受击、死亡从第 0 帧启动，死亡播放完成后钳制最后一帧。
- manifest、MaterialPack 双图加载与材质合成、房间渲染、角色渲染和 Stage 12 正式验证资源复制均已接入；完整 manifest 的 color+material RGBA 预算为 134,103,040 bytes，低于 256 MiB 上限。
- 新增确定性平台覆盖，验证全部 manifest 双图文件真实存在且尺寸匹配、加载并消费双图、水资源归属、完整帧数、关键姿态、逐帧 UV、像素变化、死亡倒地轮廓、生命周期与状态重启。

## 第二轮复审修复

- packed material 不再以透明度直接覆盖彩色图。运行时单次 shader 合成同时绑定 color/material：R 粗糙度降低漫反射，G 发光通道增加青色自发光，B 金属度增加金属高光；测试记录并断言双纹理、通道索引及三种强度参数。
- `MaterialPack::load(MaterialEcology)` 常驻 common/player/UI，只加载当前 fire 或 water 环境与怪物图集；重复生态不重载，切换生态立即释放旧生态六张纹理。`CombatRenderer::draw` 在真实房间绘制前同步 `current.ecology`。
- 水怪构建器删除完整 RGBA 的 `Image.blend`。同一怪物使用固定参考比例、统一脚底 y=186，并按四个离散关键姿态保持；相邻帧测试约束高度跳变、质心位移与 55% 软轮廓上限。
- 正式验证新增强制 water ecology 的 `water-monsters-1280x720.png`，因此 fire 与 water 选择性加载都经过真实 raylib/OpenGL 路径；shader 编译失败会在日志中显式暴露。

## 第三轮复审修复

- 水怪逐帧生成改为固定 192x192 画布、统一脚锚的 6x6 独立部件过渡；每个关键姿态间按 RGBA 差异权重均衡分批替换头部、肢体、武器、躯干与腿部区域，不做整帧复制、整图抖动或半透明混合。
- 生成器与平台验收同时硬断言：每状态所有 12/16/20/8/16 帧的分块感知哈希唯一，任意相邻帧至少 3% 可见像素发生实质变化，并继续约束高度、质心、软轮廓、固定比例和脚锚。
- `MaterialPack` 与 `CombatRenderer` 暴露只读 shader/生态/图集驻留状态；正式 water 截图只有在 shader pipeline 已就绪且 water_environment、water_bulwark、water_support 三对 color/material 图集全部驻留时才返回成功，并把每项状态写入证据报告。

## Commit

- `feat: add water ecology material slice`（本报告随该提交保存；提交哈希由 Git 生成。）
- `fix: complete paired water material presentation`（审核修复波；提交哈希由 Git 生成。）
- `fix: enforce material semantics and ecology residency`（第二轮审核修复波；提交哈希由 Git 生成。）
- `fix: require complete material animation evidence`（第三轮审核修复波；提交哈希由 Git 生成。）

## 测试

- RED：`cmake --build out/build/windows-msvc-debug --target arpg_platform_tests` — 按预期失败于缺少 `water_room_material_slice.hpp`，随后才实现生产代码。
- `cmake --build out/build/windows-msvc-debug --target arpg_platform_tests` — 通过。
- `out/build/windows-msvc-debug/bin/arpg_platform_tests.exe` — 通过，398/398。
- `ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure` — 通过，1/1，398 cases。
- 第二轮：`ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure` — 通过，1/1，399 cases。
- `cmake --build out/build/windows-msvc-debug --target arpg_stage12_material_formal` — 通过。
- `ctest --test-dir out/build/windows-msvc-debug -R "^stage12\\.material_formal$" --output-on-failure` — 通过，1/1，真实 raylib 窗口流程 19.06 秒。
- `ctest --test-dir out/build/windows-msvc-debug -R "^stage12\\.material_(formal|evidence_validator|root_safety)$" --output-on-failure` — 通过，3/3，双分辨率截图、缺图 fallback、证据预算和安全边界流程。
- 第二轮：`ctest --test-dir out/build/windows-msvc-debug -R "^(platform\\.units|stage12\\.material_(formal|evidence_validator|root_safety))$" --output-on-failure` — 通过，4/4；包含 fire 与 water 生态真实 shader 截图。

以上 MSVC 构建均在 `VsDevCmd.bat -arch=x64 -host_arch=x64` 环境下执行。

## 第三轮验证

- `out/build/windows-msvc-debug/bin/arpg_platform_tests.exe` — 通过，401/401；包含 shader 初始化失败、缺少任一水系双图以及逐状态全帧感知哈希/相邻变化门禁。
- `ctest --test-dir out/build/windows-msvc-debug -R "^(platform\.units|stage12\.material_(formal|evidence_validator|root_safety))$" --output-on-failure` — 通过，4/4；正式报告强制记录 shader 与水系三对图集运行时驻留状态。

## 已验证范围

- 八怪总览继续使用火焰展示背景；另外生成独立水房间背景的 `water-monsters-1280x720.png`，并由正式返回值同时约束真实 shader pipeline 和三对水系图集驻留，已关闭此前“没有水房间背景截图”的缺口。

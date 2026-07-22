# Water material slice report

## 变更

- 新增原创水环境、水盾卫、水支援原图、透明源、运行时彩色图集与材质图；确定性构建脚本可从原图重建全部六张运行时资源。
- 水环境独立提供湿石地面、墙、门、洞口、冷光灯、矿物珊瑚与排水格栅，运行时不引用火焰环境图集。
- 水盾卫与水支援使用各自的 864x864 图集；每怪提供待机 12、移动 16、特殊 20、受击 8、死亡 16 个实际 UV 帧，并由角色渲染器按 AI 相位、受击反馈和世界 tick 消费。
- manifest、MaterialPack 精灵到图集映射、房间渲染、角色渲染和 Stage 12 正式验证资源复制均已接入；完整 manifest 为 67,051,520 RGBA bytes，低于 64 MiB 上限 57,344 bytes。
- 新增确定性平台覆盖，验证水资源真实存在且含颜色、环境材质归属、完整帧数、逐帧 UV、实际像素哈希变化、运行时状态映射和加载预算。

## Commit

- `feat: add water ecology material slice`（本报告随该提交保存；提交哈希由 Git 生成。）

## 测试

- RED：`cmake --build out/build/windows-msvc-debug --target arpg_platform_tests` — 按预期失败于缺少 `water_room_material_slice.hpp`，随后才实现生产代码。
- `cmake --build out/build/windows-msvc-debug --target arpg_platform_tests` — 通过。
- `ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure` — 通过，1/1，394 cases。
- `cmake --build out/build/windows-msvc-debug --target arpg_stage12_material_formal` — 通过。
- `ctest --test-dir out/build/windows-msvc-debug -R "^stage12\\.material_formal$" --output-on-failure` — 通过，1/1，真实 raylib 窗口与截图流程 17.24 秒。

以上 MSVC 构建均在 `VsDevCmd.bat -arch=x64 -host_arch=x64` 环境下执行。

## 未验证项

- 现有 `stage12.material_formal` 固定以火焰房间作为八怪展示背景；截图中水盾卫/水支援已由新图集渲染，但没有单独生成水房间背景截图。水环境加载、各部件归属与房间消费由确定性平台测试覆盖。

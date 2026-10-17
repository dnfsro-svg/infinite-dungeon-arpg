# Fire crate integration report

## 变更

- CombatWorld 为火焰房间维护两只木箱的确定性运行态；有效玩家攻击 AABB 命中后只会将对应木箱由完整切换为破坏一次。
- CombatSnapshot 暴露木箱位置、完整状态和首次破坏 tick；combat 不依赖 raylib。
- 火焰房间 raylib 渲染从战斗快照读取两只木箱状态，仅隐藏已破坏的对应木箱；中央火盆原有阻挡逻辑保持不变，木箱不参与永久移动阻挡。
- `fire_room_material_slice` 新增闭环测试，覆盖左、右木箱的真实轻攻击命中、状态快照、渲染可见性和重复命中稳定性。
- Stage 12 正式验证复制 `fire_environment`、`fire_bomber`、`fire_charger` 的图片及材质资源。

## Commit

- `feat: integrate fire room destructible crates`（本报告随该提交保存；提交哈希由 Git 生成。）

## 测试

- `cmake --build out/build/windows-msvc-debug --target arpg_platform_tests` — 通过。
- `ctest --test-dir out/build/windows-msvc-debug -R platform.units --output-on-failure` — 通过，1/1。
- `ctest --test-dir out/build/windows-msvc-debug -R stage12.material_formal --output-on-failure` — 通过，1/1（真实 raylib 窗口与截图验证）。

以上命令在 `VsDevCmd.bat -arch=x64 -host_arch=x64` 环境下执行。仓库中没有 `platform_tests` 这个构建目标，等价实际目标为 `arpg_platform_tests`；构建目录为项目预设的 `out/build/windows-msvc-debug`，不是根目录 `build`。

## 未能验证事项

- 无。默认 MSVC shell 缺少标准库环境且默认开发命令提示符为 x86；切换到 x64 开发环境后完成编译、单元测试和正式窗口验证。

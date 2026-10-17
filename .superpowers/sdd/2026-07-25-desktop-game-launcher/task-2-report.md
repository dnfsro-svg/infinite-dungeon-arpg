# Task 2 完成报告：固定 DIP 布局与确定性 ICO

提交：`5add413 feat: add launcher layout and icon`

## 实现范围

- 新增 `launcher_layout.hpp/.cpp`：提供 `RectF`、`LauncherLayout`、`PixelSize`，以及固定逻辑尺寸布局、矩形重叠判定和 DPI 像素尺寸接口。
- 920×560 客户区使用需求中逐字给定的八个矩形；更大区域将整个设计面居中偏移；任意边小于 800×450 返回全零布局。
- `overlaps` 将边缘接触视为不重叠；DPI 0 返回 `{0, 0}`，其他尺寸使用 `(logical * dpi + 95) / 96`。
- 新增确定性图标生成器：从 `assets/stage12/ui_material.png` 的 `(128, 128, 256, 256)` 单元裁剪 RGBA 内容，居中放置到透明 256×256 画布后，以 Lanczos 生成 16、32、48、64、128、256 六帧 ICO。
- 新增布局单测和 Pillow 图标资产测试，并将 launcher 单测总数由 7 提升为 12；图标测试注册为 `launcher.icon_asset`（`headless;launcher;assets`）。

## RED 到 GREEN 证据

1. 布局测试先行：仅加入接口声明与空翻译单元后，`arpg_launcher_tests` 链接失败，明确报出 `make_launcher_layout`、`overlaps`、`launcher_pixel_size` 三个未解析符号。
2. 完成最小布局实现后，定向 `launcher.units` 通过；测试主程序的固定案例总数校验为 12。
3. 图标资产测试先行：ICO 不存在时，`launcher.icon_asset` 因 `FileNotFoundError` 失败。
4. 运行 `python tools/build_launcher_icon.py` 后，定向图标资产测试通过。

## 最终验证

在 VS x64 开发环境中串行执行，未运行全量构建或全量测试：

```powershell
cmake --build --preset windows-msvc-debug --target arpg_launcher_tests
ctest --test-dir out/build/windows-msvc-debug -R '^(launcher\.units|launcher\.icon_asset)$' --output-on-failure
```

结果：2/2 个定向 CTest 通过；`git diff --check` 通过。当前交付仅覆盖 Task 2，不涉及玩法、ledger 或计划文件。

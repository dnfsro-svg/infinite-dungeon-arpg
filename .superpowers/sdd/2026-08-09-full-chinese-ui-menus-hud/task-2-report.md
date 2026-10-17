# Task 2 Report — 中文菜单 ViewModel 和居中布局

## Scope

- 完成根页、设置主页、音频、显示、按键、捕获和退出确认的中文 ViewModel。
- `PauseMenuRowView` 使用 `primary[96]` 与 `saved[64]`；`saved` 只在草稿值不同于已保存值时生成。
- 800×450、1280×720、1920×1080 的纯布局测试验证主文本 X/Y 居中及 backing 等于完整行矩形。
- renderer 删除旧左贴 `bounds.x + 14` 和按文字宽度 backing，改为整行内底、主文本共同中心、右侧辅助值安全间隔。
- 宿主鼠标命中与 renderer 均使用当前 View `row_count` 的三参 `pause_menu_layout()`；保留旧守卫并新增计划要求的 CTest 名。
- 未修改 Task 3 的设置反馈消息或物理按键中文标签。

## TDD evidence

1. 初始 RED：在 VS DevShell 下构建失败于 `PauseMenuRowView` 和 `centered_pause_text_layout` 不存在。
2. 字形覆盖 RED：把 View suite 纳入定向入口后得到 `47 cases, 1 failures`，失败于 Task 2 中文页脚/捕获行未被共享字形计划覆盖。
3. 集成计数 RED：CPLAY-014 得到 `actual 55 != expected 53`，仅更新本任务新增两个 View case 导致的精确计数。

## Fresh GREEN verification

- `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2`：成功。
- `ARPG_FULL_CHINESE_UI_MENU_HUD_ONLY=1 arpg_platform_tests.exe`：`47 cases, 0 failures`。
- `ctest --preset windows-msvc-debug --output-on-failure -R 'platform.pause_menu_state_boundary|platform.pause_menu_host_layout_source|platform.pause_menu_hit_test_layout_source'`：`3/3` 通过。
- `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY=1 arpg_platform_tests.exe`：`15 cases, 0 failures`，新增菜单码点未破坏共享字形容量/目录约束。
- `ARPG_CPLAY014_PAUSE_MENU_ONLY=1 arpg_platform_tests.exe`：`55 cases, 0 failures`；材质测试打印既有程序 fallback 警告，但退出码为 0。
- `git diff --check`：通过。

## Guard coverage

- Host：`make_pause_menu_view(pause_menu)` → 三参 `pause_menu_layout(..., pause_view.row_count)` → `hit_test_pause_row(...)`。
- Renderer：`make_pause_menu_view(state)` → 三参 `pause_menu_layout(..., view.row_count)`。
- Renderer 禁止恢复 `bounds.x + 14` 与 `row_text_width` backing，并要求 `DrawRectangleRounded(bounds, ..., style.backing)`。

## Known baseline boundary

- 初始实现按当轮任务要求未运行或修正全平台 suite 的既有 ecology/总数漂移。
- Commit: `4050666` (`feat: center fully Chinese pause menus`)

## Review fix round 1 — 真实字体自适应

- 复审前全量基线：`659 cases, 2 failures`，失败仅为既有 ecology 记录漂移，以及 `actual 659 != expected 656`。
- 新增真实捕获主文本 `请按下用于轻攻击的按键（当前：Right Shift）` 的 NotoSansCJK CPU 字形度量，覆盖 800×450、1280×720、1920×1080 的 X/Y 居中与行内 containment；测试不创建 raylib 窗口或 OpenGL 上下文。
- RED：测试先引用 `fit_centered_pause_text_layout` 与 `kPauseMenuMinimumRowFontSize`，编译按预期失败于接口不存在。
- GREEN：renderer 使用最终测量得到的字号、spacing 与 UTF-8 文本绘制；超宽时缩小字号/spacing，达到最小字号后仍超宽则按 UTF-8 边界追加省略号。普通行保留首选字号，saved 路径未改。
- 热路径固定使用调用方 96-byte 栈缓冲；10,000 次 fit 的 allocation guard 为 0。
- `MENU_HUD`：`47 cases, 0 failures`；`CPLAY014`：`55 cases, 0 failures`；三个布局 source guard：`3/3` 通过。
- 全量 expected 只按 Task 2 净增 2，从 656 改为 658。改后全量为 `659 cases, 2 failures`，仅保留既有 ecology 失败与 `actual 659 != expected 658`。
- Fix commit: `7c52b96` (`fix: fit centered pause menu labels`)

# 深渊顶部重复提示修复

## 根因

- `room_renderer.cpp` 在 room stage 以屏幕 `y=18` 绘制“危险度｜规则”。
- normal HUD 随后在同一区域绘制 objective panel，覆盖前一条文字。
- `HudViewModelProjector` 已从同一 `abyss_hud_values` 构造
  “深渊 危险度 · 规则 · 怪群规模”，因此顶部 world prompt 既重复又不可见。

## TDD

- 先把 direct guard 改为：深渊 room pass 必须保留色调与边框，但不得再消费
  `draw_world_prompt` / `abyss_hud_values` / `ui_utf8_format`。
- RED：direct guard exit 1，首因精确为 `draw_world_prompt(` 仍存在。
- GREEN：删除重复文字与不再需要的字体参数后，direct guard exit 0，
  输出 `[CPLAY-030] ... abyss stays generic`。
- Debug `arpg_platform_tests` 增量编译/链接 exit 0。

独立审查发现守卫初版可由字符串诱饵伪造视觉 token，且未锁私有函数签名。
修订后视觉要求只查已屏蔽字符串/注释的 `ABYSS_CODE`，精确提取两参数
`draw_abyss` 声明，并加入 tint、border、字符串诱饵与旧字体签名 mutants；
direct guard 再次 exit 0。

## 范围

仅修改：

- `src/platform/raylib/room_renderer.cpp`
- `tests/platform/hole_prompt_binding_source_test.cmake`
- `tests/platform/hole_prompt_binding_source_self_test.cmake`

按“尽量不测试”的约束，未运行长 self-test、CTest、`Test.ps1`、Release 或窗口。

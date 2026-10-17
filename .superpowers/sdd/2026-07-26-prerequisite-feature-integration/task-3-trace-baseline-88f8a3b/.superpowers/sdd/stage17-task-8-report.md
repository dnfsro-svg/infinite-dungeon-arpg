# Stage 17 Task 8 实施报告

## 交付结果

- 注册并实现 `stage17.skill_stones.real_raylib`、证据验证器和破坏性自测。
- 真实 1280×720 Raylib 场景通过公开数字键输入、生产 `DungeonRuntime`、暂停页技能石事务和真实 `SaveStore` 完成两技能施放、五槽重排及跨进程重启核验。
- 生成五张新鲜且像素互异的 PNG 与 23 字段状态文件；验证器严格检查尺寸、时间戳、像素哈希和精确状态，负向自测覆盖 PNG、字段、时间戳三种篡改。
- 审查修复后，证据清理会在 `Remove-Item` 前解析并验证真实路径，拒绝路径链或 MutationRoot 子树中的任何 reparse/junction；新增 junction 负向用例同时证明目标哨兵不被删除。
- 修复真实宿主大型运行时/快照的 Windows 主线程栈溢出，将所有权移到堆上；未增加栈大小或引入测试后门。
- 适配 Stage16 旧正式场景到当前 `InventoryRenderer::draw` 签名，并同步收紧/更新输入链和渲染集成源码守卫，使既有破坏性自测继续有效。

## TDD 与验证

- 有效 RED：Stage17 真实测试因“场景未实现”失败，两个证据依赖项未运行。
- 安全修复 RED：旧 self-test 接受已存在的 junction MutationRoot、输出 PASS，外层断言按预期失败。
- Debug Stage17（最终复验）：3/3 通过，10.96 秒；其中含根及子树 junction 拒绝的 self-test 0.54 秒。
- Debug 非图形完整回归：92/92 通过，979.69 秒。
- Debug 既有 formal/validator 聚焦复测（安全修复后）：9/9 通过，177.62 秒。
- Release 全量构建：213/213 成功。
- Release Stage17（安全修复后）：3/3 通过，7.62 秒。
- Release 非图形完整回归：92/92 通过，740.11 秒。
- Debug Stage16/17 联合真实 Raylib（安全修复后）：4/4 通过，11.63 秒。
- Debug `^architecture\.`（安全修复后）：18/18 通过，83.99 秒。

## 证据状态

固定种子 `170017`：默认槽为 `draw_slash,storm_swords,none,none,none`；重排并重启后为 `none,draw_slash,none,none,storm_swords`。owned bits 为 3，25 个辅助槽均为空；拔刀斩场景命中 2，暴风式普通剑段命中累计 3、终结命中 1、完整 12 段，移动期间中心锁定，冷却不跨重启持久化。

详细绝对证据路径、技能常量和命令记录见 `docs/validation/stage17-swordmaster-skill-stones.md`。

## 最终审查修复（2026-07-21）

- 技能目录名称改为精确 UTF-8 `拔刀斩`、`极·鬼剑术（暴风式）`；HUD 与暂停页测试直接断言这两个中文字符串，不再与 catalog 自引用。
- 共享 HUD 字体补全两个技能名的所有 codepoint，并将完整名称加入 required text 自检。Debug/Release 真实 Raylib 日志均显示 HUD CJK 字体成功加载 335 glyphs，新截图目视确认 HUD 与暂停页显示中文名。
- 拔刀斩测试新增 `x=5.0001,y=0` 不命中与 normal 目标左右精确速度 `-0.22/+0.22`；暴风式终结测试新增 normal 目标左右精确速度 `-0.26/+0.26` 和 `z=0.24`。生产常量原本正确，本次只收紧边界覆盖。
- Debug/Release 的 skills/combat/platform 分别为 10/10、228/228、374/374 通过；Stage17 真实 Raylib 分别 3/3 通过（11.11 秒 / 7.56 秒）。
- Debug/Release 五图均已重新生成；验证器现在逐图输出绝对路径与解码 ARGB 像素 SHA-256。两套证据根路径与 10 个实测哈希已记入 `docs/validation/stage17-swordmaster-skill-stones.md`。

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

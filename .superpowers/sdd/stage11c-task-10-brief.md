# Stage 11-C Task 10 Brief — Documentation, Full Gates and Milestone Stop

## Scope

只完成 Stage 11-C 文档、Fresh Debug/Release 全门禁、分支边界审计和干净里程碑收尾。不要实现物品过滤、主菜单、手柄、分辨率/质量设置、macOS 或 Stage 11-D；不要合并 main。

## Files

- Modify: `README.md`
- Create: `docs/validation/stage11c-complete-hud.md`
- Do not track generated PNG/log/save/settings/build artifacts.

## Documentation requirements

- README 精确描述玩家可见 HUD：左下生命/护盾/经验/等级/未分配点/状态；上中目标、波次、清理、深渊；右上深度、层内房间、生态、偏向；下中两级优先通知；F1 仅开发诊断。
- 控制提示使用当前实际绑定，不硬编码已经失效的旧键位。
- 验证文档把设计每一节映射到精确 CTest 名称和正式 evidence 路径，列出六张 1280x720 PNG 与 summary。
- 明确记录 fixed-capacity/read-only、三分辨率、100k cold/steady 零分配、中文字体、input/death/settings 回归和反注入 mutation 覆盖。
- 明确列出排除项。

## Fresh gates

按顺序执行，不复用旧 configure 结果：

```powershell
.\scripts\Configure.ps1 -Preset windows-msvc-debug -Fresh
cmake --build --preset windows-msvc-debug --clean-first
ctest --preset windows-msvc-debug --output-on-failure

.\scripts\Configure.ps1 -Preset windows-msvc-release -Fresh
cmake --build --preset windows-msvc-release --clean-first
ctest --preset windows-msvc-release --output-on-failure
```

- 长门禁每 <=60 秒汇报；失败先定位并修复，不降低测试或跳过正式 evidence。
- 记录实际测试总数、耗时、Stage11-C formal/stress/evidence 结果和 raylib 6.0.0 static 证据。
- 若最终修复发生在完整门禁后，至少重跑相关 focused，并在最终分支复审修复结束后重跑完整 Debug CTest。

## Boundary audit

```powershell
git diff --check bd8c8be6dd02614c85a117d869520c57fa7aca1f..HEAD
git status --short
git diff --stat bd8c8be6dd02614c85a117d869520c57fa7aca1f..HEAD
git ls-files | rg "(^|/)(out|build)/|\.png$|\.log$|\.sav$|settings-[ab]\.bin$"
```

- 解释命中的历史受控资源，确认没有新生成 Stage11-C artifact 被跟踪。
- 不删除用户/其他任务文件，不 reset，不覆盖无关修改。

## Commit and report

- 文档中填写真实命令、数量、路径和结果后提交：`docs: complete stage 11c hud milestone`。
- 更新 ignored `.superpowers/sdd/stage11c-task-10-report.md`，记录 fresh Debug/Release、边界审计、提交和 concern。
- Task10 自身需独立 spec/quality review；之后父任务再做两名独立 reviewer 的 `bd8c8be..HEAD` 完整分支复审。
- 所有 Critical/Important/Minor 都修复并复审，最终工作树 clean 后停止，不合并、不开始 Stage11-D。

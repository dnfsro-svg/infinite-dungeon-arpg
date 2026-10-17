# Stage 15 低资源音频混音实现计划

执行设计：`docs/superpowers/specs/2026-07-20-stage15-audio-mix-design.md`

## Task 1：资源与不可变清单

- 下载四个 CC0 OGG 和 Kenney UI Audio 官方包到临时目录；只把选定的 10 个发布资源加入 `assets/stage15/audio/`。
- 新增 `SOURCES.md`，记录页面、作者、许可、源文件名、发布名、SHA-256、大小和用途。
- 新增资源校验脚本与 CTest：10 项精确集合、哈希、格式、时长和 16 MiB 预算；增加哈希和多余文件的 RED 变异。
- 提交：`assets: add stage 15 audio mix sources`。

## Task 2：纯音频场景与混音逻辑

- 先写场景选择、相同目标不重启、0.40/0.25 秒淡化、暂停/死亡 duck、五路线性增益和 UI 边沿去重 RED 测试。
- 实现不依赖 raylib 的 `audio_scene` 与 `audio_mix` 固定容量逻辑。
- 跑平台单元测试和零分配压力测试。
- 提交：`feat: add deterministic audio scene mixer`。

## Task 3：流和 UI 资源生命周期

- 以函数表封装 `Music`/`Sound` API，先写 fake API 生命周期测试。
- 实现四流 `StreamPack`、六短音效 `UiAudioPack`、逐资源静音/程序化回退、幂等 unload 和失败回滚。
- 保持 Stage 14 `AudioPack` 行为不变。
- 提交：`feat: load stage 15 streamed and ui audio`。

## Task 4：设置 V3 与五路设置 UI

- 先扩展 settings 类型、codec、store、菜单状态、视图和窗口设置测试。
- 复用 44 字节记录，V3 使用 34–37 字节；V1/V2 迁移填默认总线值，38–39 保持零。
- 设置页增加四行并保持草稿预览、取消恢复、应用失败回滚和十项按键重绑语义。
- 提交：`feat: add five-bus volume settings`。

## Task 5：统一 GameAudio 与宿主接线

- 用 `GameAudio` 统一设备所有权、Stage 14 战斗音效、Stage 15 流/UI、每帧 stream update 和总线增益。
- 只从现有公开 host/presentation 状态构造 `AudioSceneInput`；为 UI 离散边沿产生 cue。
- 增加源码边界、无运行期进程/下载和生产接线测试。
- 提交：`feat: integrate stage 15 game audio`。

## Task 6：发行复制与正式证据

- 构建时复制 `assets/stage15/audio/` 到 EXE 旁。
- 正式 raylib 工具真实打开 4 个 OGG 和 6 个 WAV，记录格式、流状态、回退、路由 trace 和磁盘预算。
- 证据只写入调用方指定目录，校验器必须用负变异证明会拒绝伪造或越界输出。
- 提交：`test: add stage 15 audio evidence`。

## Task 7：完整验收与报告

- 执行一次 VS x64 Release `-Fresh` 全量构建和全部 CTest，记录实际测试数和耗时。
- 恢复全量测试重新生成的历史 Stage 11D 证据，不提交代理报告或临时日志。
- 不带测试参数启动发布 EXE，用现有外置键盘驱动触发公开输入，检查进程、设备和全部 Stage 14/15 资源日志。
- 更新 README 和 `docs/validation/stage15-audio-mix.md`，明确自动证据与人工试听边界。
- 提交：`docs: validate stage 15 audio mix`。

每个 Task 完成后只提交自身范围。任何修复先增加可复现 RED，完成后运行相应目标；Task 7 之前不重复跑完整测试套件。

# Stage 12 角色图集来源

- 原创源图：`art_source/stage12/actors-concept-v1.png`
- 上游文件：`E:\codex1\.codex\generated_images\019f4e7f-6318-75d2-b82a-d69ac1304f04\exec-8505f9a2-e206-41bd-9a2f-79f6f89b67f5.png`
- 处理：按 3×3 概念图切出固定玩家和八种怪物，移除 `#00ff00` 及其抗锯齿绿边，排入 2048×2048、8 列×9 行的透明 PNG 图集。

`actors.png` 的每格为 224×224，脚锚点为 `(112, 224)`，共 9 列×9 行（只使用前 68 格）。前 12 格依次为玩家 idle、move、J1、J2、J3、L 上挑、跳跃上升、跳跃下降、空中 J、落地、受击、死亡；其余每种怪物连续 7 格：idle、move、telegraph、active、recovery、cooldown、defeated。阶段格共享同一固定角色外观，并用位移、提亮、冷却暗化或败北淡出表达阶段，不另造角色。

# 外置键盘驱动

这是一个独立 Windows 工具，不链接、修改或注入游戏进程。它只会先定位并前台激活指定 EXE 的窗口，然后用标准 `SendInput` 发送按下和松开事件。

构建：

```powershell
cmake -S E:\game\tools\keyboard_driver -B E:\game\out\keyboard-driver -G "Visual Studio 17 2022" -A x64
cmake --build E:\game\out\keyboard-driver --config Release
```

示例：

```powershell
# 先检查参数，不发送输入
E:\game\out\keyboard-driver\Release\arpg_keyboard_driver.exe --key d --hold-ms 600 --dry-run

# 向已运行的 arpg_game.exe 按住 D 600ms
E:\game\out\keyboard-driver\Release\arpg_keyboard_driver.exe --key d --hold-ms 600

# 移动、攻击、上挑、回撤的完整序列
E:\game\out\keyboard-driver\Release\arpg_keyboard_driver.exe --sequence "d:600,j:70,l:70,a:450"
```

允许按键：`1 2 3 4 5 W A S D J K L E R`。其中 `1–5` 对应主动技能槽；每次保持 1–10000ms；工具结束、Ctrl+C 或异常关闭时会释放本次已按下的键。

若在 EXE 同目录创建 `keyboard_driver_autorun.txt`，内容为一行序列（例如 `d:600,j:70,l:70,a:450`），无参数启动时才会执行该序列。文件不存在时，无参数启动只显示帮助，不会发送输入。

脚本也支持两行重复格式：第一行 `repeat=1800`，第二行放序列；这会安全地重复整段序列。每个动作前工具都会确认游戏窗口仍在前台；失焦时会尝试重新激活目标游戏窗口，无法恢复才会停止并释放按键。

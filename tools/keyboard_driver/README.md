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

# 同时运行多个实例时，用十进制 PID 明确指定其中一个
E:\game\out\keyboard-driver\Release\arpg_keyboard_driver.exe --pid 12345 --key d --hold-ms 600

# 同时按下 D、J，保持 300ms 后按 J、D 的顺序释放
E:\game\out\keyboard-driver\Release\arpg_keyboard_driver.exe --chord "d+j" --hold-ms 300

# dry-run 会明确显示 chord 的按下、保持和释放顺序
E:\game\out\keyboard-driver\Release\arpg_keyboard_driver.exe --chord "F1+Esc" --hold-ms 100 --dry-run

# 导航暂停菜单（token 不区分大小写）
E:\game\out\keyboard-driver\Release\arpg_keyboard_driver.exe --sequence "Down:70,Enter:70,Up:70,Left:70,Right:70"

# 移动、攻击、上挑、回撤的完整序列
E:\game\out\keyboard-driver\Release\arpg_keyboard_driver.exe --sequence "d:600,j:70,l:70,a:450"
```

默认 `--target arpg_game.exe` 只在所有同名进程合计恰好有一个可见、无 owner 的顶层窗口时发送输入。若存在多个候选，工具会拒绝发送并列出候选 PID；不会自动选择第一个、最新或当前前台实例。`--pid <decimal>` 可明确指定进程，但该 PID 的 EXE 名必须仍与 `--target` 匹配，且该进程必须恰好有一个符合条件的窗口。

允许按键：`1 2 3 4 5 W A S D I P J K L E R F1 F12 Esc Enter Up Down Left Right`，token 不区分大小写。`Enter` 与方向键分别发送 Win32 的 `VK_RETURN`、`VK_UP`、`VK_DOWN`、`VK_LEFT`、`VK_RIGHT`，可用于真实导航暂停菜单。其中命令字符 `1–5` 会发送小键盘 `Num1–Num5`，对应主动技能槽；不会发送主键盘顶部数字。`--chord` 使用 `+` 连接至少两个 token，按书写顺序全部按下，只保持一次 `hold-ms`，再按反向顺序释放。每次保持 1–10000ms；工具结束、Ctrl+C 或异常关闭时会反序释放本次已按下的键。

若在 EXE 同目录创建 `keyboard_driver_autorun.txt`，内容为一行序列（例如 `d:600,j:70,l:70,a:450`），无参数启动时才会执行该序列。文件不存在时，无参数启动只显示帮助，不会发送输入。

脚本也支持两行重复格式：第一行 `repeat=1800`，第二行放序列；这会安全地重复整段序列。每个动作前工具都会确认游戏窗口仍在前台；失焦时会尝试重新激活目标游戏窗口，无法恢复才会停止并释放按键。

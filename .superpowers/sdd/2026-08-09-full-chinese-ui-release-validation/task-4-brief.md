### Task 4: Release 部署一致性

**Files:**
- Update: `docs/validation/full-chinese-ui.md`。
- No production source changes in this task; a packaging mismatch becomes a separate TDD fix and this deployment task restarts from Step 1.

**Interfaces:**
- Consumes: 已通过的 `windows-msvc-release/bin`。
- Produces: 安装目录与构建目录逐文件一致的 103 文件应用树。

- [ ] **Step 1: 确认没有运行中的游戏或部署进程**

```powershell
Get-Process arpg_game,InfiniteDungeonLauncher,cmake,ninja,cl -ErrorAction SilentlyContinue
```

若存在本任务启动的游戏，先正常 `CloseMainWindow()` 并等待退出；不得终止无关进程。

- [ ] **Step 2: 执行唯一一次部署**

```powershell
& '.\scripts\DeployLauncher.ps1' -GameBuildDirectory `
  '.\out\build\windows-msvc-release\bin'
```

Expected: exit 0，staging 原子替换成功。

- [ ] **Step 3: 比较 build/install 哈希**

递归比较 `arpg_game.exe`、launcher、assets 和字体文件的相对路径、长度、SHA256；差异数必须为 0。把 exe 和字体哈希写入验证文档。

- [ ] **Step 4: 提交部署证据文档**

```powershell
git add docs/validation/full-chinese-ui.md
git diff --cached --check
git commit -m "docs: record Chinese UI deployment hashes"
```

---


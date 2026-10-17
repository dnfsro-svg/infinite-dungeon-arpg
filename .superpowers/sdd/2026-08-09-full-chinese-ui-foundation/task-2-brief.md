### Task 2: UTF-8 安全固定缓冲工具

**Files:**
- Create: `src/platform/raylib/ui_utf8.hpp`
- Create: `src/platform/raylib/ui_utf8.cpp`
- Create: `tests/platform/ui_utf8_tests.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/platform_test_main.cpp`

**Interfaces:**
- Consumes: NUL 终止的 UTF-8 输入和固定大小输出缓冲。
- Produces: `UiUtf8CopyResult`、`ui_utf8_is_valid()`、`ui_utf8_copy()`、`ui_utf8_fit_with_ellipsis()`、`ui_utf8_format()`。

- [ ] **Step 1: 写 6 个边界失败测试**

测试合法/非法序列、每个码点边界、中文省略号、格式化尾部修复、空目标和 10000 次零分配：

```cpp
char buffer[7]{};
const auto result = platform::ui_utf8_fit_with_ellipsis(
    buffer, sizeof(buffer), u8"设置界面");
ARPG_REQUIRE(result.truncated);
ARPG_REQUIRE(platform::ui_utf8_is_valid(buffer));
ARPG_REQUIRE(buffer[sizeof(buffer) - 1U] == '\0');
ARPG_REQUIRE(!platform::ui_utf8_is_valid("\xE8\xAE"));
```

- [ ] **Step 2: 运行并确认 RED**

Run: `cmake --build --preset windows-msvc-debug --target arpg_platform_tests --parallel 2`

Expected: 编译失败于 `ui_utf8.hpp` 缺失。

- [ ] **Step 3: 实现严格有界 UTF-8 工具**

公开结构和函数：

```cpp
struct UiUtf8CopyResult final {
    std::size_t bytes_written{};
    bool input_was_valid{};
    bool truncated{};
};

[[nodiscard]] bool ui_utf8_is_valid(const char* text) noexcept;
[[nodiscard]] UiUtf8CopyResult ui_utf8_copy(
    char* destination, std::size_t capacity, const char* source) noexcept;
[[nodiscard]] UiUtf8CopyResult ui_utf8_fit_with_ellipsis(
    char* destination, std::size_t capacity, const char* source) noexcept;
[[nodiscard]] UiUtf8CopyResult ui_utf8_format(
    char* destination, std::size_t capacity, const char* format, ...) noexcept;
```

解码器逐字节验证过长编码、孤立续字节、代理区和 `> U+10FFFF`；非法码点写单字节 `?`。除 `capacity == 0` 外始终 NUL 终止。省略号只在能完整写入 UTF-8 `…` 时使用，否则输出空串。

- [ ] **Step 4: 运行 foundation 定向测试**

Run: 使用 Task 1 的 `ARPG_FULL_CHINESE_UI_FOUNDATION_ONLY` 命令。

Expected: 目录 5 case + UTF-8 6 case，共 11 case 通过。

- [ ] **Step 5: 提交 UTF-8 工具**

```powershell
git add src/platform/raylib/ui_utf8.hpp src/platform/raylib/ui_utf8.cpp src/platform/raylib/CMakeLists.txt tests/platform/ui_utf8_tests.cpp tests/platform/CMakeLists.txt tests/platform/platform_test_main.cpp
git diff --cached --check
git commit -m "feat: add bounded UTF-8 UI formatting"
```

---


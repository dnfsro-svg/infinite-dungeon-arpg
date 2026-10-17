### Task 8C: 独立引入窗口生命周期兜底

**Files:**
- Create: `src/platform/raylib/host_window_lifetime.hpp`
- Create: `src/platform/raylib/host_window_lifetime.cpp`
- Modify: `src/platform/raylib/raylib_host.cpp`
- Modify: `src/platform/raylib/CMakeLists.txt`
- Create: `tests/platform/host_window_lifetime_tests.cpp`
- Modify: `tests/platform/platform_test_main.cpp`
- Modify: `tests/platform/CMakeLists.txt`
- Modify: `tests/platform/host_validation_source_test.cmake`
- Modify: `docs/validation/raylib-host-refactor-baseline.md`

**Interfaces:**

```cpp
struct HostWindowBackend final {
    void (*set_config_flags)(unsigned int){};
    void (*init_window)(int, int, const char*){};
    bool (*is_window_ready)(){};
    void (*close_window)(){};
};
class HostWindowLifetime final {
public:
    explicit HostWindowLifetime(HostWindowBackend) noexcept;
    bool initialize(const RaylibHostConfig&,
        const settings::SettingsData&) noexcept;
    void close() noexcept;
    [[nodiscard]] bool ready() const noexcept;
    ~HostWindowLifetime();
    HostWindowLifetime(const HostWindowLifetime&) = delete;
    HostWindowLifetime& operator=(const HostWindowLifetime&) = delete;
private:
    HostWindowBackend backend_{};
    bool ready_{};
};
[[nodiscard]] HostWindowBackend raylib_host_window_backend() noexcept;
```

- [ ] **Step 1: 写窗口 backend/lifetime RED 测试**

  用计数 backend 证明初始化失败不关闭、成功显式关闭一次、重复 close 不重复、异常/早退由析构兜底；测试不得打开真实窗口。

- [ ] **Step 2: 实现 RAII 并保持初始化顺序**

  `SetConfigFlags -> InitWindow -> IsWindowReady` 保持原顺序；`SetWindowMinSize/SetExitKey/ChangeDirectory/foreground` 仍由 host 在成功后原位调用。

- [ ] **Step 3: 替换 catch/成功路径关闭逻辑**

  显式 audio/renderer/pause renderer shutdown 必须发生在 `HostWindowLifetime::close()` 前；catch 不再手写重复 `CloseWindow()`，但返回码与日志文本不变。

- [ ] **Step 4: 编译并运行平台回归**

  ```powershell
  cmake --build --preset windows-msvc-debug --target arpg_game arpg_platform_tests -- -j1
  ctest --test-dir out/build/windows-msvc-debug -R '^(platform\.units|platform\.host_validation_(source|sequence)|stage11b\.|architecture\.)' --output-on-failure -j1
  git diff --check
  ```

- [ ] **Step 5: 检查完成度并提交**

  `raylib_host.cpp` 不得包含 Stage 状态机定义，目标约不超过 1,200 行；若仍超过约 1,200 行，在基线文档逐段解释剩余职责，不用机械拆分换数字。

  ```powershell
  git add src/platform/raylib tests/platform docs/validation/raylib-host-refactor-baseline.md
  git commit -m "refactor: guard raylib window lifetime"
  ```

---


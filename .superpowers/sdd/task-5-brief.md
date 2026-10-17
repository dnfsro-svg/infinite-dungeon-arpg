## Task 5: V4 可变长度检查点与迁移

### Frozen V4 binary and failure decisions

- 本任务允许并要求同步修改 `src/dungeon/CMakeLists.txt` 与 `src/persistence/CMakeLists.txt`：`DungeonRunState` 的公开头包含 `ItemOwnershipState`，所以 `arpg_dungeon` PUBLIC 链接 `arpg_items`；`arpg_persistence` 实现调用 items 校验，必须直接链接 `arpg_items`。禁止反向依赖 persistence→raylib。
- `DungeonRunState` 在现有 V3 字段之后新增 `items::ItemOwnershipState item_ownership{}`；V1/V2/V3 解码统一得到 empty items/equipment/claimed、`next_item_sequence=1`。
- V4 header 仍为 32 bytes；magic=`IARPGS05`，format=4，rules=1。保留 V1/V2/V3 的既有 magic/尺寸常量供解码，不得把旧 fixture 当 V4 解析。
- V4 payload offset：0..87 与 V3 完全相同；88=`item_count:u32`；92=`next_item_sequence:u64`；100=`claimed_drop_bits[3]:u64`；124=`equipped_ids[6]:u64`；172 起为 N 个 40-byte records。payload=`172+40*N`，file=`204+40*N`，全部 little-endian，N<=65535。所有乘加在分配前 checked，文件长度必须精确相等，无尾随字节。
- 每个 record：0=`item_id:u64`，8=`base_id:u8`，9=`rarity:u8`，10=`item_level:u8`，11=`required_level:u8`，12=`affix_count:u8`，13..15 必须写/读零，16..39 为六个 rolls，每 roll `affix_id:u16,tier:u8,variant:u8`。未使用 roll 四字节全零。
- 绝对禁止 `memcpy(ItemInstance)`：内存对象虽固定 40 bytes，但字段次序与 V4 record 不同（内存 affixes offset12、affix_count36、reserved37）。编码/解码必须逐字段映射。
- V4 encode 与 decode 都必须以 `items::validate_ownership()` 为统一物品语义门；`next_item_sequence==0` 视为 invalid state，并在本任务把该检查加入统一 validator。encode 对非法 state 返回 nullopt，decode 返回 `CodecError::invalid_state`。分配失败返回 nullopt/新增明确 `CodecError::allocation_failure`，不得从 noexcept 逃逸。
- `crc32_update(previous_crc, bytes, size)` 采用可链式的 finalized CRC 语义：`crc32_update(0,a)==crc32(a)`，且 `crc32_update(crc32(a),b)==crc32(a||b)`。size=0 或 bytes=nullptr 时原样返回 previous；因此现有 `crc32(nullptr,size)==0` 继续由 `crc32_update(0,nullptr,size)` 保持兼容且不得解引用。
- `EncodedCheckpoint` 是唯一新编码结果类型。旧固定数组 encode overload 必须删除，所有 save-store/transaction/fault injection 调用点机械迁移到 `const std::vector<std::uint8_t>&`；不得保留第二套 V4 编码路径。
- 解码顺序固定为：最小 header→magic/version/rules→声明 payload length 与实际总长→CRC→V4 item_count/checked exact length→一次有界分配→逐记录读取→完整 DungeonRunState/ownership 校验。任何前置失败不得按攻击者声明 count 分配。
- V4 `same_state` 比较 item 获得顺序和全部字段（含显式 reserved/未用 rolls）、equipment、claimed bits、sequence；不得排序物品后比较。

**Files:**
- Modify: `src/dungeon/dungeon_checkpoint.hpp`
- Modify: `src/persistence/checkpoint_codec.hpp`
- Modify: `src/persistence/checkpoint_codec.cpp`
- Modify: `src/persistence/crc32.hpp`
- Modify: `src/persistence/crc32.cpp`
- Modify: `src/persistence/save_store.hpp`
- Modify: `src/persistence/save_store.cpp`
- Modify: `src/persistence/save_store_detail.hpp`
- Modify: `src/persistence/save_transaction.cpp`
- Test: `tests/persistence/checkpoint_codec_tests.cpp`
- Test: `tests/persistence/passive_tree_checkpoint_tests.cpp`
- Test: `tests/persistence/save_store_tests.cpp`
- Test: `tests/persistence/save_store_fault_tests.cpp`

- [ ] **Step 1: 写 V4 布局和迁移失败测试**

逐字节断言 magic `IARPGS05`、format 4、payload 172+40N、文件 204+40N、偏移 88/92/100/124/172 和 40 字节记录。固定样本至少含普通、魔法、稀有及词缀 112 variant。V1–V3 fixture 解码后断言空物品/装备/领取位及 sequence=1。

- [ ] **Step 2: 写损坏输入失败测试**

覆盖超 65,535、乘加溢出、截断、尾随字节、CRC、reserved、未用 roll 非零、重复/零 ID、非法目录字段、需求等级、词缀组、装备悬空/错槽/重复引用及超 95% cap。

- [ ] **Step 3: 运行 RED**

```powershell
cmake --build build-release --target arpg_persistence_tests
ctest --test-dir build-release -R persistence.units --output-on-failure
```

Expected: V4 magic、动态编码结果和物品状态字段不存在。

- [ ] **Step 4: 改为有界动态编码**

接口统一为：

```cpp
using EncodedCheckpoint = std::vector<std::uint8_t>;

[[nodiscard]] std::optional<EncodedCheckpoint> encode_checkpoint(
    const dungeon::checkpoint::DungeonRunState& state) noexcept;
```

编码前 checked 计算长度并一次 `resize`；捕获 `std::bad_alloc` 返回 `nullopt`。CRC 增加增量接口：

```cpp
[[nodiscard]] std::uint32_t crc32_update(
    std::uint32_t state, const std::uint8_t* data, std::size_t size) noexcept;
```

事务写入接受 `const std::vector<std::uint8_t>&`，保留临时文件、flush、轮换和故障注入语义。

- [ ] **Step 5: 解码先验边界后再分配**

先验证 magic/version/header/payload 长度与 `item_count <= 65535`，checked 得到精确期望长度且无尾随字节，再 reserve/resize。读取完成后只调用 `items::validate_ownership()` 作为统一语义门。

- [ ] **Step 6: 扩展状态相等与双槽恢复**

`same_state` 比较获得顺序、全部 roll、equipment、claimed bits、sequence。增加一大一小两个 V4 轮换，验证恢复选择最新有效 generation。

- [ ] **Step 7: 运行 GREEN**

```powershell
cmake --build build-release --target arpg_persistence_tests
ctest --test-dir build-release -R persistence --output-on-failure
git diff --check
```

- [ ] **Step 8: 提交**

```powershell
git add src/dungeon/dungeon_checkpoint.hpp src/persistence tests/persistence
git commit -m "feat: persist variable length equipment checkpoints"
```

---

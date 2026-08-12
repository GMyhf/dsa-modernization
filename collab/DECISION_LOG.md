# DECISION_LOG · 已生效的项目决策

> **本文件是「已拍板决策」的权威出处。** `collab/PLAN.md` 的 Decision Log 只记一句摘要
> 加一个指向这里的锚，避免两份会各自腐烂的副本。
>
> 决策一旦写进这里就是项目标准，两个 agent 无需再问；要推翻必须由人重新拍板，
> 并在原条目下**追加**「被 XXXX-XX-XX 的决策取代」，**不要删除原条目**——
> 来回可查比看起来干净重要。

---

## D-001 · 2026-08-12 · 人已拍板：DSA 教材 C++ 现代化风格公约（T-006）

改造数据结构教材的核心矛盾：**全用 STL（如 `std::stack`）就失去了手写数据结构的
教学意义；全用旧 C++ 又会给学生灌输危险的内存习惯。** 以下四条为红线。

### 1. 语言标准：C++17

所有单元默认 `-std=c++17`。理由：GCC/Clang/MSVC 支持完善，且 `std::optional`、
`std::string_view` 足以支撑安全、无副作用的数据结构接口。

`unit.json` 的 `standard` 字段仍可按单元覆盖，但**偏离 C++17 要在 `legacy.md` 里
写明理由**，并在交接记录里点出来。

> **取代记录**：本条取代 2026-08-12 早些时候 Claude 提议的「C++20 + concept」。
> 原提议的动机是用 concept 替掉原书那个「非纯虚、非虚析构」的假抽象基类 `Stack<T>`；
> 在 C++17 下这一角色由 **`static_assert` + `<type_traits>`** 承担——同样是编译期检查、
> 同样不付虚表代价，报错信息也在实例化处，只是不能像 concept 那样参与重载决议。
> 落地见 `code/ch03/array_stack/modern.hpp` 顶部的三条 `static_assert`。

### 2. STL 使用边界（核心权衡）

- **禁用（教学核心）**：不得用 `std::vector` / `std::stack` / `std::list` / `std::map`
  等容器**直接替代**该章节要讲的手写实现。把 `ArrayStack` 做成 `std::stack` 的薄封装
  等于把这一节删掉。
- **允许（现代基础设施）**：
  - 类型与辅助：`std::size_t`（替换 `int` 表示长度/下标）、`std::swap`、
    `std::initializer_list`、`<type_traits>`、`<utility>`。
  - 接口返回值：推荐 `std::optional<T>`，替换原书「返回 `bool` + 出参」的双通道设计。
  - 内存管理：**演示手写指针/动态数组时，显式编写 Rule of Five**；
    不涉及指针教学的上下文，优先 `std::unique_ptr` 管理底层数组。

> **第 3 章顺序栈按前者办**：存储结构本身就是这一节的教学内容，所以用裸 `T* data_`
> 加显式五法则。这是有意的——用 `unique_ptr` 时五法则基本是仪式（编译器生成的就够用），
> 用裸指针它才是承重的，学生才看得见「为什么必须写这五个」。
> 代价一并明写：扩容要手写 `try/catch` 清理新缓冲区，而 RAII 版本不用。

### 3. 错误处理与 I/O（彻底清除 `cout`）

- **红线**：数据结构类内部**严禁**出现 `std::cout` / `std::cerr`。原书「溢出时
  `cout << "栈满溢出"`」这类写法必须彻底清除，由 `test.cpp` 重定向流并断言其为空来守住。
- **可预期的空状态**（`pop()` / `top()` 在空容器上）：返回 `std::optional<T>`，
  空时 `std::nullopt`。不是错误，不抛异常。
- **非法参数 / 越界 / 容量溢出**：抛标准异常——`std::out_of_range`（下标越界）、
  `std::overflow_error`（容量翻倍溢出）、`std::invalid_argument`（参数非法）。

### 3b. 只读访问：`optional` 与裸指针各司其职（2026-08-12 补充条款，人已拍板）

「读栈顶」这件事有两种正当需求，公约同时提供两个接口，**不要合并**：

| 接口 | 返回 | 用在什么时候 | 代价 |
| --- | --- | --- | --- |
| `std::optional<T> top() const` | **副本** | 要把值带走、存起来、跨越后续修改 | 拷贝一次；`T` 必须可拷贝 |
| `const T* peek() const noexcept` | **指针**，空栈为 `nullptr` | 只看一眼、零拷贝；move-only 元素唯一可用的观望方式 | **指针在下一次 push / pop / clear 后失效** |

写下这条的理由：`optional<T>` 的安全是靠拷贝换来的，对 `std::unique_ptr` 这类
move-only 元素直接不可用；而裸指针零拷贝，代价是生命周期由调用方负责。
两种代价都真实存在，**藏起任何一种都是把选择权从读者手里拿走**。
失效契约必须写进接口注释与书稿正文，不能靠使用者猜。

落地：`code/ch03/array_stack/modern.hpp`；守门用例
`test_peek_does_not_copy`（`Counted` 计拷贝次数，断言 peek 为 0、top ≥ 1）、
`test_peek_on_empty_is_nullptr`、`test_peek_after_growth_is_refetched`。

### 4. 命名与 API 风格

- 类名 `PascalCase`；函数与变量 `snake_case` 或 `camelCase`（单元内保持一致）；
  私有数据成员后缀下划线（`capacity_`、`data_`）。
- **彻底消除成员变量与成员函数重名**——原书 `int top` 与 `bool top(T&)` 的冲突
  正是全书第一个被编译器拒绝的硬伤。栈顶位置一律用 `top_index_` 之类的名字，
  `top()` 留给成员函数。
- 查询函数标 `[[nodiscard]]` 与 `noexcept`（当且仅当真的不抛）。

---

## D-002 · 2026-08-12 · Claude 记录：`dsa_raw.md` 永久只读

它是「原书到底怎么写的」唯一凭据，修订一律落在 `book/`。
`tests/test_ledger.py::test_real_book_inventory_is_stable` 用
105 条清单 / 70 算法 / 35 代码 / 5 条缺结束标记这四个数字锚住它，被动过立刻变红。

**收益已兑现**：T-001 抓到原书两处编译错误，正是靠这条才说得清那是原书的问题，
不是我们改出来的。**代价**：书稿与底稿之间没有自动 diff，靠人工对照。

## D-003 · 2026-08-12 · Claude 记录：书稿代码块与源码的一致性靠机器保证

书稿里的 ```cpp 块必须写 `file=code/.../modern.hpp#anchor`，`check_doc.py` R3 逐字核对，
`sync_book.py --write` 负责灌入。理由：原书的错误之所以能印进教材，
正是因为「书上的代码」和「跑过的代码」从来不是同一份东西。

## D-004 · 2026-08-12 · Claude 记录：闸门跑两种构建，缺一不可

`-Wall -Wextra -Wpedantic -Werror` 之上，Debug 挂 ASan+UBSan（`-fno-sanitize-recover=all`），
Release 跑 `-O2`。**实测依据**：同一段堆越界在 Debug 档被 UBSan 当场 abort，
在 `-O2` 档**静默通过**（`tests/test_check_code.py::test_sanitizer_catches_heap_overflow`
把这个现象本身写成了断言）。只跑一种构建会漏。

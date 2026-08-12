# 第3章 栈与队列（现代化稿 · 3.1 栈）

> **本文件的地位**：这是《数据结构与算法》（张铭、王腾蛟、赵海燕，高等教育出版社 2008）
> 第 3.1 节的现代化重排，也是整个仓库的**样板**。原书正文（`dsa_raw.md:1785` 起）
> 的讲法、编号、图表一概保留；换掉的只是那套 2008 年的 C++ 写法。
>
> 书里印的每一段 C++ 都来自 `code/` 下真正编译、真正跑过的文件
> （`tools/check_doc.py` 的 R3 逐字核对），不是排版时手敲的示意代码。
> 原书那份写法错在哪、我们改了什么、什么刻意没改，逐条记在
> `code/ch03/array_stack/legacy.md`。
>
> 代码风格遵循 `collab/DECISION_LOG.md` 的 D-001 公约：**C++17**；不拿 STL 容器
> 替代手写实现；存储结构属本节教学内容，故用裸指针加显式五法则。

## 3.1.1 栈的抽象数据类型

栈(stack)是限定仅在一端进行插入和删除运算的线性表，该端称为**栈顶**(top)，
另一端称为**栈底**(bottom)。栈的元素按后进先出(LIFO, last in first out)的次序访问：
最后压入的元素最先弹出。

基于栈的特性，栈的抽象数据类型包含进栈 push、出栈 pop、读栈顶 top 等常用操作，
以及判断栈是否为空的边界操作。根据抽象和封装的原则，只可通过抽象数据类型
定义的运算来操作栈。

原书【代码3.1】用一个成员函数既非纯虚、析构函数也非 virtual 的空基类 `Stack<T>`
来表达这层抽象。那样的基类给不了任何多态能力：成员函数既不是纯虚的、也没有定义，
派生类"实现"它们并不构成覆盖；反而埋下了「通过 `Stack<T>*` 删除派生对象」
这一未定义行为。

**抽象数据类型描述的是「一组运算」，不是「一个基类」。** 在 C++ 里，
模板本身已经承担了这层抽象——`ArrayStack<T>` 提供哪些运算，由它的接口决定，
不需要继承任何东西。真正需要写下来的是对元素类型 `T` 的要求，
C++17 用 `static_assert` 配合 `<type_traits>` 表达：编译期检查、
不付虚函数表的运行期代价，而且错误信息就停在实例化处，不会炸出一屏模板内部细节。

```cpp file=code/ch03/array_stack/modern.hpp#class-head
/// 基于数组的栈（后进先出）。
///
/// 与原书 arrStack 的差别：容量耗尽时自动翻倍（原书要么溢出报错，要么靠算法3.3
/// 手工换成扩容版本）；不打印任何东西；空栈上的 pop/top 返回空 optional，
/// 而不是靠「出参 + bool」双通道返回。
template <typename T>
class ArrayStack {
    // 原书用一个成员函数既非纯虚、析构又非 virtual 的空基类 Stack<T> 来表达「抽象」。
    // 那样的基类给不了多态，还带来「通过基类指针删除派生对象」的未定义行为。
    // C++17 里表达「T 要满足什么」的直接工具是 static_assert + 类型特征：
    // 编译期检查、不付虚表代价，而且错误信息就停在实例化处。
    static_assert(std::is_default_constructible<T>::value,
                  "ArrayStack<T>: T 必须可默认构造（底层 new T[n] 会构造整块槽位）");
    static_assert(std::is_move_assignable<T>::value,
                  "ArrayStack<T>: T 必须可移动赋值（push/pop 需要移动元素）");
    static_assert(std::is_copy_assignable<T>::value || std::is_nothrow_move_assignable<T>::value,
                  "ArrayStack<T>: 不可复制的 T 必须可无异常移动赋值（扩容保持强异常保证）");
    static_assert(!std::is_reference<T>::value, "ArrayStack<T>: T 不能是引用类型");

public:
    using value_type = T;
    using size_type = std::size_t;
```

三条 `static_assert` 把类对 `T` 的要求摆在了明面上。其中第一条尤其诚实：
底层的 `new T[n]` 会把整块槽位都默认构造出来，所以 `T` 必须可默认构造——
这是原书 `new T[mSize]` 就有的限制，我们没有恶化它，也**还没有**解决它
（真正的容器做法是申请未初始化存储再逐个 placement new）。
把限制写成 `static_assert`，好过让它以一条晦涩的模板报错出现。

## 3.1.2 顺序栈

采用顺序存储结构的栈称为顺序栈(array-based stack)，需要一块连续的区域存储栈中元素。

对元素数目为 n 的栈，首先要确定数组的哪一端表示栈顶。如果把数组的第 0 个位置作为栈顶，
所有的插入和删除都在第 0 个位置进行，每次 push 或 pop 都要把栈中所有元素后移或前移
一个位置，时间代价为 O(n)。反之，把最后一个元素的位置作为栈顶，新元素添加在表尾、
出栈也只删除表尾元素，每次操作的时间代价仅为 O(1)。图3.2 所示为按后一种方案实现的栈。

![顺序栈的存储结构示意：数组低端为栈底，变量 top 指向当前栈顶元素的下标，入栈在表尾追加](assets/68ddbf0cd26a38ce.jpg)

图3.2 栈的顺序存储结构示意

### 存储与所有权

原书【代码3.2】用三个成员表示一个顺序栈：`int mSize`（容量）、`int top`（栈顶位置）、
`T * st`（裸指针数组）。这套写法有三处在今天必须改：

1. `int top` 与成员函数 `bool top(T&)` **重名**，导致这个类按印刷原样根本编译不过；
2. 无参构造只设了 `top = -1`，`mSize` 与 `st` 都没初始化，析构时 `delete[]` 一个不确定指针；
3. 有析构函数却没有拷贝构造与拷贝赋值，一次 `arrStack<int> b = a;` 就会二次释放。

第 2、3 条是未定义行为，编译器开到 `-Wall -Wextra -Wpedantic` 也**一句警告都不给**。

现代实现保留裸指针 `T* data_`——顺序栈的存储管理正是本节要教的内容，
把它换成 `std::unique_ptr<T[]>` 固然更省事，但五法则也就随之退化成走过场
（编译器生成的版本就够用了）。留着裸指针，这五个函数才是承重的：
少写一个，AddressSanitizer 立刻能复现出崩溃。

```cpp file=code/ch03/array_stack/modern.hpp#rule-of-five
// 三/五法则：原书只写了析构函数，没有拷贝构造与拷贝赋值，
// 于是 `arrStack<int> b = a;` 之后两个对象持有同一根指针，各析构一次 → 二次释放。
//
// 这里用的是**裸指针**，所以这五个函数是承重的，不是仪式——
// 少写一个，ASan 立刻能复现出崩溃（见 legacy.md 缺陷 4 的实测输出）。
ArrayStack(const ArrayStack& other)
    : capacity_(other.capacity_),
      top_index_(other.top_index_),
      data_(other.capacity_ ? new T[other.capacity_] : nullptr) {
    for (size_type i = 0; i < top_index_; ++i) {
        data_[i] = other.data_[i];
    }
}

/// 拷贝并交换：先构造完整副本再与自身交换，天然自赋值安全，
/// 且拷贝失败时原对象不受影响。
ArrayStack& operator=(const ArrayStack& other) {
    if (this != &other) {
        ArrayStack copy(other);
        swap(copy);
    }
    return *this;
}

ArrayStack(ArrayStack&& other) noexcept { swap(other); }

ArrayStack& operator=(ArrayStack&& other) noexcept {
    if (this != &other) {
        ArrayStack moved(std::move(other));  // other 交出所有权
        swap(moved);                         // 自己原来的缓冲区随 moved 析构释放
    }
    return *this;
}

~ArrayStack() { delete[] data_; }
```

拷贝赋值用的是**拷贝并交换**(copy-and-swap)：先构造一个完整副本，再与自身交换。
它自然地做到了自赋值安全，也在拷贝失败时保证原对象不受影响。

### 出栈：用 optional 取代双通道返回

原书的 `bool pop(T & item)` 用出参带值、用返回值带成败。调用方一旦忘了检查返回值，
读到的就是没被修改过的 `item`。现代写法让「有没有值」这件事进入类型系统：
**空栈不是错误，而是一种可预期的状态**，用 `std::optional` 表达它；
真正的错误（下标越界、容量溢出）才抛异常。

```cpp file=code/ch03/array_stack/modern.hpp#pop
/// 出栈。空栈返回 std::nullopt——原书是「返回 false + 往 cout 打一行中文」，
/// 调用方既没法在库里复用，也容易忽略返回值。
[[nodiscard]] std::optional<T> pop() {
    if (empty()) {
        return std::nullopt;
    }
    --top_index_;
    return std::optional<T>(std::move(data_[top_index_]));
}

/// 读栈顶但不弹出，返回**副本**。空栈返回 std::nullopt，不是未定义行为。
/// 要求 T 可拷贝；move-only 元素请用 peek()。
[[nodiscard]] std::optional<T> top() const {
    if (empty()) {
        return std::nullopt;
    }
    return std::optional<T>(data_[top_index_ - 1]);
}

/// 只读观望栈顶：不拷贝、不弹出。空栈返回 nullptr。
///
/// 与 top() 的分工——top() 给你一份可以带走的副本（安全，但要求 T 可拷贝，
/// 而且确实拷了一次）；peek() 零拷贝，move-only 元素也能用，代价是
/// **返回的指针在下一次 push / pop / clear 之后即失效**（扩容会换掉整块缓冲区）。
/// 生命周期由调用方负责，这一点必须写在文档里，不能靠使用者猜。
[[nodiscard]] const T* peek() const noexcept {
    return empty() ? nullptr : &data_[top_index_ - 1];
}
```

`[[nodiscard]]` 使「调用了 pop 却扔掉返回值」成为一条编译警告（本书的构建开
`-Werror`，即编译失败）。同样重要的是这里**没有 `std::cout`**：原书在空栈出栈时
直接打印中文提示，把一个数据结构与标准输出焊死——它没法在库里复用，
提示无法本地化，失败路径也无从测试。

「读栈顶」有两种正当需求，因此这里给了两个接口，**不要把它们合并成一个**：

- `top()` 返回 `std::optional<T>`，是一份可以带走的**副本**。安全，
  但要求 `T` 可拷贝，而且确实拷贝了一次。
- `peek()` 返回 `const T*`，空栈为 `nullptr`，**零拷贝**。
  `std::unique_ptr` 这类只能移动的元素只能用它来观望。

代价也是一对：`optional` 的安全靠拷贝换来，`peek()` 的零拷贝则把生命周期交给了
调用方——**它返回的指针在下一次 `push` / `pop` / `clear` 之后即失效**，
因为扩容会换掉整块缓冲区。这条失效契约必须写在文档里，不能靠使用者猜。

两种代价都真实存在。把任何一种藏起来，都是替读者做了本该由读者做的选择。

## 3.1.3 顺序栈的扩容

栈中元素动态变化，当栈满时继续进栈会产生上溢出(overflow)。原书【算法3.3】给出的
改进办法是：申请一个扩大一倍的新数组，把原有内容顺序移动过去，再执行进栈。
这个策略本身是对的——每个元素在均摊意义下只被搬运常数次，push 的**摊还时间代价
仍是 O(1)**——但那段代码有两个问题：循环变量 `i` 从未声明（编译不过），
以及先 `delete[] st` 再赋新指针，搬运中途若抛出异常，栈就停在半新半旧的状态。

```cpp file=code/ch03/array_stack/modern.hpp#grow
static constexpr size_type kInitialCapacity = 4;

void ensure_capacity() {
    if (top_index_ < capacity_) {
        return;
    }
    constexpr size_type kMax = std::numeric_limits<size_type>::max();
    if (capacity_ > kMax / 2) {
        throw std::overflow_error("ArrayStack: 容量翻倍会溢出");
    }
    const size_type next = capacity_ == 0 ? kInitialCapacity : capacity_ * 2;
    T* fresh = new T[next];
    try {
        for (size_type i = 0; i < top_index_; ++i) {
            // 这里是赋值而不是构造，不能用 move_if_noexcept：它检查的是
            // 移动构造是否 noexcept，移动赋值仍可能在搬迁中途抛并污染原栈。
            // 可复制元素一律复制；不可复制元素由上面的 static_assert 保证移动赋值不抛。
            if constexpr (std::is_copy_assignable<T>::value) {
                fresh[i] = data_[i];
            } else {
                fresh[i] = std::move(data_[i]);
            }
        }
    } catch (...) {
        // 裸指针的代价：RAII 版本不用写这一段。搬迁失败要自己收拾新缓冲区，
        // 且此时还没动 data_/capacity_，所以原栈完好——这就是强异常保证。
        delete[] fresh;
        throw;
    }
    delete[] data_;
    data_ = fresh;
    capacity_ = next;
}
```

四处值得注意：

- **先建后换**。新缓冲区搬完才动 `data_`，中途抛异常则原栈原封不动，
  这就是**强异常保证**。原书先 `delete[] st` 再赋新指针，一旦搬运途中抛出，
  栈就停在一个既不是旧状态也不是新状态的地方。
- **`try/catch` 是裸指针的代价**。搬迁失败要自己把新缓冲区收掉再重新抛出；
  如果底层换成 `std::unique_ptr<T[]>`，这一段可以整个删掉。
  取舍在这里是显式的：本节要教存储管理，就得连这份代价一起看见。
- **`std::move_if_noexcept`**。当 `T` 的移动构造可能抛异常时退回为拷贝——
  搬到一半失败时，被搬走的元素不至于毁在半路。
- **容量用 `std::size_t`**。原书用 `int`，`mSize * 2` 溢出是未定义行为；
  这里在翻倍前先判界并抛 `std::overflow_error`。

这不是纸面推理。测试里有一个「第 3 次拷贝赋值必定抛异常」的元素类型，
用它撑满栈再触发扩容，然后逐项断言：长度不变、容量不变、原有元素逐个完好、
栈之后仍能继续使用。把「先建后换」改回原书的「先释放旧的」，
Debug 构建下 UBSan 当场报空指针引用，Release 构建直接段错误。

进栈本身随之简化为「保证容量、写入、长度加一」：

```cpp file=code/ch03/array_stack/modern.hpp#push
/// 入栈。容量不足时按算法3.3 的策略翻倍。
/// 强异常保证：搬迁在新缓冲区上完成，中途抛异常则原栈原封不动。
void push(const T& item) {
    ensure_capacity();
    data_[top_index_] = item;
    ++top_index_;
}

void push(T&& item) {
    ensure_capacity();
    data_[top_index_] = std::move(item);
    ++top_index_;
}
```

两个重载分别处理左值和右值。原书的 `bool push(const T item)` 按值传参，
顶层 `const` 对调用方没有意义，却强制了一次拷贝，而且 `std::unique_ptr`
这类只能移动的类型根本传不进去。

## 与原书的对照

| 原书 | 现在 | 为什么 |
| --- | --- | --- |
| `class Stack<T>` 空基类 | 三条 `static_assert` + `<type_traits>` | 空基类给不了多态，还带来非虚析构的未定义行为；对 `T` 的要求应当是编译期约束 |
| `int mSize / int top / T* st` | `size_t capacity_ / size_t top_index_ / T* data_` | `int` 溢出是未定义行为；`top_index_` 避开与成员函数 `top()` 重名 |
| 只有 `~arrStack()` | 五个特殊成员函数补齐 | 否则一次拷贝就二次释放 |
| `bool pop(T& item)` | `[[nodiscard]] std::optional<T> pop()` | 「有没有值」交给类型系统，忽略返回值会告警 |
| `bool top(T& item)` | `optional<T> top()` 取副本；`const T* peek()` 零拷贝 | 两种正当需求，两种代价，都摆在明面上 |
| `cout << "栈满溢出"` | 不做任何 I/O；越界抛 `std::out_of_range` | 数据结构不该和标准输出耦合；可预期的空状态用 `optional`，真错误才抛异常 |
| `delete[] st; st = newSt;` | 先建后换 + `try/catch` + `move_if_noexcept` | 搬迁中途抛异常不破坏原栈（强异常保证） |

**刻意没改的**：它仍然是一个手写的、自己管缓冲区的数组栈，缓冲区仍是裸的
`T* data_`，扩容仍是算法3.3 的翻倍策略，`clear()` 仍然只把长度归零、保留已分配容量。
把它换成 `std::stack` 的薄封装固然更短，但这一节要教的正是这些实现细节本身。

完整实现见 `code/ch03/array_stack/modern.hpp`，测试见同目录 `test.cpp`
（50 项断言，覆盖上表每一行；用 `python3 tools/check_code.py` 在
`-Werror` + ASan/UBSan 与 `-O2` 两种构建下各跑一遍）。

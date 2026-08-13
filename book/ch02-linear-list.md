# 第2章 线性表（现代化稿 · 2.1–2.2 顺序表）

## 本章先读什么

线性表把一串同类型元素排成唯一的先后次序。顺序表把元素连续放在数组里，能 O(1) 按下标访问；
链表用链接保存相邻关系，能在已知结点位置 O(1) 插入或删除。关键是区分“按位置找元素”和
“已知结点后改链接”这两类操作。

源码入口：[顺序表实现](../code/ch02/array_list/modern.hpp)、
[顺序表测试](../code/ch02/array_list/test.cpp)、[链表实现](../code/ch02/linked_list/modern.hpp)、
[链表测试](../code/ch02/linked_list/test.cpp)。运行：
`python3 tools/check_code.py --allow-degraded code/ch02/array_list code/ch02/linked_list`。

> **本文件的地位**：《数据结构与算法》（张铭、王腾蛟、赵海燕，高等教育出版社 2008）
> 第 2.1–2.2 节的现代化重排。原书正文（`dsa_raw.md:1145` 起）的讲法、编号、图表一概保留；
> 换掉的只是那套 2008 年的 C++ 写法。2.3 节链表另开一轮。
>
> 书里印的每一段 C++ 都来自 `code/ch02/array_list/`，真正编译、真正跑过
> （`tools/check_doc.py` 的 R3 逐字核对）。原书那份写法错在哪、改了什么、
> 什么刻意没改，逐条记在 `code/ch02/array_list/legacy.md`。
>
> 代码风格遵循 `collab/DECISION_LOG.md` 的 D-001 与 D-005。

## 2.1 线性表的概念

线性表(linear list)是由 n(n ≥ 0) 个类型相同的数据元素组成的有限序列。
除第一个元素外，每个元素有且仅有一个直接前驱；除最后一个元素外，
每个元素有且仅有一个直接后继。

线性表的抽象数据类型给出的是**一组运算**：置空、判空、在表尾追加、
在指定位置插入、删除指定位置的元素、按位置取值与改值、按值查找位置。

原书【代码2.1】用一个类模板 `List<T>` 来写这组运算。那份清单有两处今天必须改：

1. 它声明了 `bool delete(const int p);`——**`delete` 是 C++ 关键字**，不能作函数名。
   整章的删除操作都建立在这个编译不过的名字上。
2. 它写的是 `class List { void clear(); ... };`，**通篇没有 `public:`**。
   `class` 的默认访问权限是 private，于是这个抽象数据类型的每一个运算都调不到。
   （同一本书第 3 章的代码3.1 是写了 `public:` 的。）

第 2 点尤其值得停一下：抽象数据类型的意义就是**对外**给出一组运算。
一个所有运算都私有的 ADT，在语法上成立、在语义上是空的。

本书把这组运算直接定义在 `ArrayList<T>` 上，不再另设一个基类——
理由与第 3 章相同：那样的空基类给不了多态，却会带来非虚析构的未定义行为。
对元素类型的要求用 `static_assert` 写在类头：

```cpp file=code/ch02/array_list/modern.hpp#class-head
/// 顺序表（按顺序方式存储的线性表，又称向量）。
///
/// 与原书 arrList 的差别：容量不足时自动翻倍，而不是打印 "The list is overflow"
/// 然后返回 false；位置非法抛 std::out_of_range，而不是打印一行再返回 false；
/// 查找返回 std::optional<size_type>，而不是「出参 + bool」双通道。
template <typename T>
class ArrayList {
    static_assert(std::is_default_constructible<T>::value,
                  "ArrayList<T>: T 必须可默认构造（底层 new T[n] 会构造整块槽位）");
    static_assert(std::is_move_assignable<T>::value,
                  "ArrayList<T>: T 必须可移动赋值（插入/删除要搬动元素）");
    static_assert(std::is_copy_assignable<T>::value || std::is_nothrow_move_assignable<T>::value,
                  "ArrayList<T>: 不可复制的 T 必须可无异常移动赋值（扩容保持强异常保证）");
    static_assert(!std::is_reference<T>::value, "ArrayList<T>: T 不能是引用类型");

public:
    using value_type = T;
    using size_type = std::size_t;
```

## 2.2 顺序表

按顺序方式存储的线性表称为顺序表(array-based list)，又称向量(vector)，通过数组建立。

假设每个元素占用 L 个存储单元，顺序表的开始结点 k₀ 的存储位置记为
b = loc(k₀)，称为首地址；则下标为 i 的元素 kᵢ 的存储位置为

$$\mathrm{loc}(k_i) = b + i \times L$$

每个元素的存储位置都与起始位置相差一个与位序成正比的常数。只要确定了基地址，
表中任一元素的地址都能直接算出——**顺序表因此是一种随机存取的存储结构**，
按下标取值的时间代价为 O(1)。物理相邻表示了逻辑相邻。

**(a) 线性表的逻辑结构**

| 数据元素 | k₀ | k₁ | … | kᵢ | … | kₙ₋₁ | … | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 逻辑地址 | 0 | 1 | … | i | … | n−1 | … | maxSize−1 |

**(b) 线性表的顺序存储结构**

| 数据元素 | k₀ | k₁ | … | kᵢ | … | kₙ₋₁ | … |
| --- | --- | --- | --- | --- | --- | --- | --- |
| 存储地址 | b | b+L | … | b+i·L | … | b+(n−1)·L | … |

图 2.1 顺序表的示意图

### 2.2.1 存储与所有权

原书【代码2.2】用四个成员表示顺序表：`T* aList`、`int maxSize`、`int curLen`、
`int position`。前三个对应缓冲区、容量、当前长度，现代实现一一对应
（改用 `std::size_t`，因为「负下标」不该在类型层面存在）。

第四个 `position` 是个**当前处理位置游标**，配合正文提到的
`setPos / setStart / next / prev` 用来「依次处理元素」。这个设计今天不能要：
遍历状态一旦住进容器，`const` 对象就没法遍历（游标要改），两处代码不能同时遍历，
嵌套遍历直接互相踩。本书删掉这个成员，改为提供 `begin()/end()`，
把遍历状态交回调用方——range-for 因此可以直接用在顺序表上。
（顺带一提：原书那个 `position`，在书中展示的所有算法里一次都没被用到。）

与第 3 章一样，缓冲区是**裸指针**加显式五法则：顺序表的存储管理正是本节的教学内容，
换成 `std::unique_ptr<T[]>` 会让五法则退化成走过场。原书 `arrList` 有析构函数
却没有拷贝构造与拷贝赋值，一次 `arrList<int> b = a;` 就二次释放——
与第 3 章 `arrStack` 是同一个错误，同一份 ASan 报告可以复现。

另外原书的 `clear()` 是这样写的：

```text
void clear() { delete [] aList; curLen = position = 0; aList = new T[maxSize]; }
```

释放整块再重新分配。既没必要（把长度归零即可，容量留着复用），也不是异常安全的：
若 `new` 抛异常，对象就停在「指针已释放、长度已归零」的破碎状态，
之后析构还会再 `delete[]` 一次。本书的 `clear()` 是 `noexcept` 的，只把长度归零。

### 2.2.2 顺序表的检索

顺序表上的检索分按位置和按内容两类。按位置的检索直接由地址公式算出，O(1)：

```cpp file=code/ch02/array_list/modern.hpp#access
/// 按下标读取，O(1)。越界抛 std::out_of_range。
/// 原书 getValue 用「出参 + bool」，越界时打印一行再返回 false。
[[nodiscard]] const T& at(size_type index) const {
    check_index(index, "ArrayList::at");
    return data_[index];
}

[[nodiscard]] T& at(size_type index) {
    check_index(index, "ArrayList::at");
    return data_[index];
}

/// 修改指定位置的值。越界抛 std::out_of_range。
void set(size_type index, const T& value) {
    check_index(index, "ArrayList::set");
    data_[index] = value;
}
```

按内容的检索是把待查值与表中元素依次比较，O(n)。原书【算法2.3】写作
`bool getPos(int& p, const T value)`——位置由出参带出、成败由返回值带出。
这个形状的问题是：调用方忘了检查返回值，读到的就是没被写过的 `p`。

（顺带一提，算法2.3 那段按印刷原样是编译不过的：循环写的是 `for (i = 0; i < n; i++)`，
而 `n` 从未声明，按上下文应为 `curLen`。）

```cpp file=code/ch02/array_list/modern.hpp#find
/// 按内容查找，返回第一次出现的下标；没有则返回 std::nullopt。O(n)。
///
/// 原书【算法2.3】是 `bool getPos(int& p, const T value)`：出参带位置、
/// 返回值带成败。调用方忘了检查返回值，读到的就是没被写过的 p。
/// 这里让「找没找到」进入类型系统，忽略返回值还会被 -Wunused-result 拦下。
[[nodiscard]] std::optional<size_type> find(const T& value) const {
    for (size_type i = 0; i < size_; ++i) {
        if (data_[i] == value) {
            return i;
        }
    }
    return std::nullopt;
}
```

检索的时间代价体现在比较次数上。最好情况是第 1 个元素即为所求，比较 1 次；
最差情况是表中没有该元素，比较 n 次。等概率假设下平均比较次数为

$$\sum_{i=1}^{n} p \times i = \frac{1}{n}(1 + 2 + \cdots + n) = \frac{n+1}{2}$$

即平均需要检查表中一半的元素，时间开销为 O(n)。

### 2.2.3 顺序表的插入

插入要在指定位置腾出一个空位：从表尾起，把 pos 之后的元素逐个右移一位。

```cpp file=code/ch02/array_list/modern.hpp#insert
/// 在位置 pos 插入元素，pos 可以等于 size()（即追加到表尾）。
/// 位置非法抛 std::out_of_range；容量不足自动翻倍。
///
/// 时间代价仍是 O(n)——pos 之后的元素都要右移一位。这是顺序表的固有代价，
/// 也是第 2.3 节要拿它和链表对比的地方，没有被"优化"掉。
void insert(size_type pos, const T& value) {
    make_gap(pos);
    data_[pos] = value;
    ++size_;
}

void insert(size_type pos, T&& value) {
    make_gap(pos);
    data_[pos] = std::move(value);
    ++size_;
}

void append(const T& value) { insert(size_, value); }
void append(T&& value) { insert(size_, std::move(value)); }
```

两处与原书不同：

- **容量不足时自动翻倍**，而不是打印 `"The list is overflow"` 然后返回 false。
  扩容策略与第 3 章算法3.3 相同，搬迁判据见 3.1.3 节末（移动赋值是否 `noexcept`）。
- **位置非法抛 `std::out_of_range`**，而不是打印一行再返回 false。
  按公约，可预期的空状态用 `optional`，真正的错误抛异常——插到表外是错误。

插入位置 p 处腾空位的过程如下（图 2.2）：p 及其之后的元素整体右移一位，
空出的 p 位置写入新元素。

| aList | k₀ | k₁ | … | **value** | kₚ | … | kₙ₋₁ | … | |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 下标 | 0 | 1 | … | p | p+1 | … | n | … | maxSize−1 |

图 2.2 顺序表元素插入示意图

时间代价仍然是 O(n)。最好情况是插在表尾，移动 0 次；最差情况是插在表头，
n 个元素全要移动；等概率假设下平均移动次数为

$$\sum_{i=0}^{n} p \times (n - i) = \frac{1}{n+1}\sum_{i=0}^{n}(n-i) = \frac{n}{2}$$

**扩容没有改变这个结论**：翻倍带来的搬迁摊还到每次插入是 O(1)，
而元素右移本身仍是 O(n)。这一点是 2.3 节拿顺序表与链表对比的依据，不能被优化掉。

### 2.2.4 顺序表的删除

删除是插入的镜像：把 pos 之后的元素逐个左移一位。

```cpp file=code/ch02/array_list/modern.hpp#remove
/// 删除位置 pos 上的元素并返回它。位置非法抛 std::out_of_range。
///
/// 空表上删除必然越界，所以不需要原书那句单独的空表检查——
/// 「表空」在这里不是一种可预期状态，而就是下标非法的一个特例。
T remove(size_type pos) {
    check_index(pos, "ArrayList::remove");
    T removed = std::move(data_[pos]);
    for (size_type i = pos; i + 1 < size_; ++i) {
        data_[i] = std::move(data_[i + 1]);  // 左移一位，O(n)
    }
    --size_;
    return removed;
}
```

删除位置 p 上的元素后，其后的元素整体左移一位（图 2.3）：

删除前：

| aList | k₀ | k₁ | k₂ | … | **kₚ** | … | kₙ₋₁ | … |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 下标 | 0 | 1 | 2 | … | p | … | n−1 | … |

删除后：

| aList | k₀ | k₁ | k₂ | … | kₚ₊₁ | … | kₙ₋₁ | … |
| --- | --- | --- | --- | --- | --- | --- | --- | --- |
| 下标 | 0 | 1 | 2 | … | p | … | n−2 | … |

图 2.3 顺序表元素删除示意图

原书【算法2.5】的函数名是 `delete`——C++ 关键字，编译不过；本书叫 `remove`。
另外它返回 `bool`，被删掉的值就此丢失；这里把它返回给调用方，
「删除并取用」不必先 `getValue` 再 `delete` 两步走。

删除的时间代价分析与插入相同，平均为 O(n)。

## 与原书的对照

| 原书 | 现在 | 为什么 |
| --- | --- | --- |
| `bool delete(const int p)` | `T remove(size_type pos)` | `delete` 是关键字，原样编译不过；顺带把被删元素还给调用方 |
| `class List { ... }` 无 `public:` | 不设基类，要求写成 `static_assert` | 所有运算私有的 ADT 在语义上是空的；空基类给不了多态 |
| `for (i = 0; i < n; i++)` | `i < size_` | 原书 `n` 从未声明，编译不过 |
| `bool getPos(int& p, T v)` | `std::optional<size_type> find(const T&)` | 「找没找到」交给类型系统，忽略返回值会告警 |
| `bool getValue(int p, T& v)` | `const T& at(size_type)`，越界抛异常 | 同上；越界是错误，不是可预期状态 |
| `int maxSize / curLen`，满了拒绝 | `size_t capacity_ / size_`，自动翻倍 | `int` 溢出是未定义行为；「负下标」不该存在；容量不该是使用者的负担 |
| `int position` 游标 + `next/prev` | `begin()/end()` | 遍历状态住在容器里，const 不能遍历、不能嵌套遍历 |
| `cout << "The list is overflow"` | 不做任何 I/O | 数据结构不该和标准输出耦合 |
| `clear()` 释放后重新分配 | `clear() noexcept` 只归零长度 | 原写法没必要，且 `new` 抛异常会留下破碎对象 |

**刻意没改的**：连续存储、按下标 O(1) 随机存取、插入与删除搬动 O(n) 个元素。
这三条正是 2.3 节拿顺序表和链表做对比的全部依据，一条都不能优化掉。

完整实现见 `code/ch02/array_list/modern.hpp`，测试见同目录 `test.cpp`
（47 项断言，覆盖上表每一行；用 `python3 tools/check_code.py` 在
`-Werror` + ASan/UBSan 与 `-O2` 两种构建下各跑一遍）。

## 2.3 链表

顺序表用物理相邻表示逻辑相邻，所以按下标读取是 O(1)，但中间插入和删除需要搬动
后续元素。链表把逻辑相邻写进结点的链接域：结点可以散落在内存中；给定一个前驱结点后，
插入或删除只改常数条指针。代价也必须如实保留：要按位置找到那个前驱，仍须从表头循链，
所以按位置访问、查找、插入和删除的总时间仍是 O(n)。

### 2.3.1 单链结点与头结点

【代码2.6】单链表的结点定义。

```cpp file=code/ch02/linked_list/modern.hpp#node-types
/// 原书【代码2.6】的单链结点：数据域与指向后继的链接域。
///
/// 实际容器把它藏在私有实现里，避免调用方任意改坏链；这里保留独立类型，
/// 因为后续栈、队列可复用同一种结点形状。
template <typename T>
struct SinglyLink {
    T data;
    SinglyLink* next{nullptr};

    template <typename U>
    explicit SinglyLink(U&& value, SinglyLink* successor = nullptr)
        : data(std::forward<U>(value)), next(successor) {}
};

/// 原书【代码2.12】的双链结点：额外保存前驱链接。
template <typename T>
struct DoublyLink {
    T data;
    DoublyLink* next{nullptr};
    DoublyLink* prev{nullptr};

    template <typename U>
    explicit DoublyLink(U&& value, DoublyLink* predecessor = nullptr, DoublyLink* successor = nullptr)
        : data(std::forward<U>(value)), next(successor), prev(predecessor) {}
};
```

【代码2.6结束】

`LinkedList<T>` 在对象内嵌一个不承载 `T` 的头结点。它等价于原书的“第 -1 个结点”，
从而表头插入和删除都变成“修改某个前驱的 `next`”；空表不必另写一套分支。尾指针指向
最后一个实际结点，空表时回指头结点，故 `append` 不需要从头寻找末尾。

【代码2.7】单链表的类型定义。

```cpp file=code/ch02/linked_list/modern.hpp#class-head
/// 带头结点、尾指针的单链表。
///
/// 原书的 `setPos(-1)` 返回头结点；本实现把这个实现细节留在 predecessor_at，
/// 对外位置统一为 [0, size()]。按值查找返回 optional，位置错误抛 out_of_range。
template <typename T>
class LinkedList {
    struct NodeBase {
        NodeBase* next{nullptr};
    };
    struct Node final : NodeBase {
        T value;

        template <typename U>
        explicit Node(U&& item, NodeBase* successor = nullptr)
            : NodeBase{successor}, value(std::forward<U>(item)) {}
    };

public:
    using value_type = T;
    using size_type = std::size_t;
```

【代码2.7结束】

### 2.3.2 构析与循链定位

【算法2.8】带有头结点的单链表构造函数与析构函数。

本实现的头结点嵌入对象本身，不需要动态分配；`clear()` 沿 `next` 逐结点释放实际结点，
析构函数调用它。复制构造采用“先逐个接入新链，失败即清理”的规则，避免半成品对象在
元素复制抛异常时泄漏。

【算法2.8结束】

【算法2.9】寻找链表的第 i 个结点。

定位从头结点开始逐步走 `next`，不新建任何结点。原书的 `setPos(-1)` 是处理头结点的
技巧；现代接口不暴露 -1 这个特殊位置，内部 `predecessor_at(0)` 直接返回头结点。

【算法2.9结束】

### 2.3.3 插入与删除

【算法2.10】插入单链表的第 i 个结点。

```cpp file=code/ch02/linked_list/modern.hpp#insert
/// 尾插直接经 tail_ 接链，O(1)。不能转调 insert(size_)，后者必须循链定位前驱。
void append(const T& value) { append_impl(value); }
void append(T&& value) { append_impl(std::move(value)); }

void insert(size_type pos, const T& value) { insert_impl(pos, value); }
void insert(size_type pos, T&& value) { insert_impl(pos, std::move(value)); }
```

【算法2.10结束】

插入先定位前驱、再构造新结点、最后接入链接；结点构造或分配失败时，链接和长度尚未变化。
若前驱恰是尾结点，同步让 `tail_` 指向新结点。给定前驱后的接链是 O(1)，定位仍是 O(n)。

【算法2.11】单链表的删除算法。

```cpp file=code/ch02/linked_list/modern.hpp#remove
T remove(size_type pos) {
    NodeBase* predecessor = predecessor_at(pos);
    NodeBase* removed = predecessor->next;
    if (removed == nullptr) {
        throw std::out_of_range("LinkedList::remove: 下标越界");
    }
    // 先移动出值；若 T 的移动构造抛，链接尚未改变，容器仍完整。
    T value = std::move(static_cast<Node*>(removed)->value);
    predecessor->next = removed->next;
    if (removed == tail_) {
        tail_ = predecessor;
    }
    delete static_cast<Node*>(removed);
    --size_;
    return value;
}
```

【算法2.11结束】

删除不能只摘除链接：被删结点必须释放，且删除尾结点后 `tail_` 必须回退到前驱。否则下一次
`append` 会写入已释放内存。位置不合法统一抛 `std::out_of_range`，容器不打印提示。

### 2.3.4 双链结点

【代码2.12】双链表的结点定义和实现。

上面的 `DoublyLink<T>` 已保留 `prev` 与 `next` 两个链接域。双链表删除某一结点时，需要
同时维护前驱的 `next` 和后继的 `prev`；这能高效向前走，但每个结点多一个指针且不变量更多。
原书在此只给出结点定义，没有给出完整双链表算法，因此本轮不假装已经现代化完整双链表。

【代码2.12结束】

完整可运行实现见 `code/ch02/linked_list/modern.hpp`，测试覆盖头/中/尾插入、删尾后的尾指针
修复、深拷贝、移动、元素构造异常和 move-only 元素；变异自检还确认“删尾不回退 tail”会在
后续尾插崩溃，“复制构造失败不清理”会留下元素对象。原书逐条证据见
`code/ch02/linked_list/legacy.md`。

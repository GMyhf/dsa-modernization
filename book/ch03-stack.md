# 第3章 栈与队列

栈是「最后放入、最先取出」，适合递归调用、括号匹配和表达式计算；队列是「先放入、先取出」，
适合排队服务和广度优先搜索。空栈、空队列是正常状态，现代接口以 `std::optional` 返回。

源码：[顺序栈·教学版](../code/ch03/array_stack/teaching.hpp)、
[顺序栈·工程版](../code/ch03/array_stack/modern.hpp)、
[栈示例](../code/ch03/array_stack/demo.cpp)、
[链式栈](../code/ch03/linked_stack/modern.hpp)、
[表达式求值](../code/ch03/expression_eval/modern.hpp)、
[背包](../code/ch03/knapsack/modern.hpp)、
[队列](../code/ch03/queue/modern.hpp)、
[队列示例](../code/ch03/queue/demo.cpp)。

## 先跑一遍

```cpp file=code/ch03/array_stack/demo.cpp
// 第 3 章「先跑一遍」：用教学版 ArrayStack 走一遍 push / top / pop。
// 编译运行：
//   g++ -std=c++17 -I code/ch03/array_stack code/ch03/array_stack/demo.cpp -o demo && ./demo
#include "teaching.hpp"

#include <iostream>

int main() {
    ArrayStack<int> stack;
    stack.push(1);
    stack.push(2);
    stack.push(3);

    // top() 返回 optional：有值才解引用，空栈不会崩
    if (auto value = stack.top()) {
        std::cout << "栈顶是 " << *value << '\n';
    }

    std::cout << "依次弹出:";
    while (auto value = stack.pop()) {
        std::cout << ' ' << *value;
    }
    std::cout << "\n空栈再弹? " << (stack.pop() ? "有值" : "空") << '\n';
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch03/array_stack \
    code/ch03/array_stack/demo.cpp -o /tmp/stack-demo
/tmp/stack-demo
```

```console
栈顶是 3
依次弹出: 3 2 1
空栈再弹? 空
```

后进先出：最后压入的 3 最先出来。空栈上 `pop()` 返回空 optional，不打印、不崩溃。

循环队列牺牲一个槽位区分空与满，所以逻辑容量 3 实际申请 4 个槽：

```cpp file=code/ch03/queue/demo.cpp
#include "modern.hpp"

#include <iostream>

int main() {
    dsa::ArrayQueue<int> queue(3);
    if (!queue.enqueue(1) || !queue.enqueue(2) || !queue.enqueue(3)) {
        std::cout << "入队失败\n";
        return 1;
    }
    std::cout << "逻辑容量 3 时再入队? " << (queue.enqueue(4) ? "成功" : "已满") << '\n';
    std::cout << "依次出队:";
    while (auto value = queue.dequeue()) {
        std::cout << ' ' << *value;
    }
    std::cout << '\n';
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch03/queue \
    code/ch03/queue/demo.cpp -o /tmp/queue-demo
/tmp/queue-demo
```

```console
逻辑容量 3 时再入队? 已满
依次出队: 1 2 3
```

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

## 3.1 栈

### 3.1.1 栈的抽象数据类型

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
不需要继承任何东西。我们不关心它是否继承自某个 `Stack<T>`，只关心它有没有
`push`、`pop`、`top` 这几个运算。所以这一节要定下来的是**这张表**：

| 运算 | 含义 | 时间代价 |
| --- | --- | --- |
| `push(x)` | 把 x 压到栈顶 | 摊还 O(1) |
| `pop()` | 弹出栈顶并把它带回来；空栈返回「没有」 | O(1) |
| `top()` | 只看栈顶，不弹出；空栈返回「没有」 | O(1) |
| `empty()` | 栈里还有没有元素 | O(1) |
| `size()` | 栈里有几个元素 | O(1) |
| `clear()` | 清空 | O(1) |

表里有两处「空栈返回『没有』」。这四个字在 C++17 里有一个精确的表达方式，
下一节的代码就是这么写的。

### 3.1.2 顺序栈

采用顺序存储结构的栈称为顺序栈(array-based stack)，需要一块连续的区域存储栈中元素。

对元素数目为 n 的栈，首先要确定数组的哪一端表示栈顶。如果把数组的第 0 个位置作为栈顶，
所有的插入和删除都在第 0 个位置进行，每次 push 或 pop 都要把栈中所有元素后移或前移
一个位置，时间代价为 O(n)。反之，把最后一个元素的位置作为栈顶，新元素添加在表尾、
出栈也只删除表尾元素，每次操作的时间代价仅为 O(1)。图3.2 所示为按后一种方案实现的栈。

图3.2 顺序栈：下标 0 是栈底，`size_` 是元素个数，也是下一个空位。

```text
下标     0     1     2     3     4     …
内容    [A]   [B]   [C]    ?     ?     …
               ▲
            size_ = 3   （栈顶是 C，下一个 push 写到 3）
```

#### 教学版：完整实现

下面是一份**完整的、能直接编译运行的**顺序栈。一个文件、一个类，没有省略号，
没有「此处略去若干行」。它保留原书【代码3.2】【算法3.3】要教的全部内容——
连续数组、栈顶在表尾、满了就把容量翻倍——只把原书那几处会崩的写法换掉。

```cpp file=code/ch03/array_stack/teaching.hpp
// 顺序栈 ArrayStack —— 教学版。
//
// 这一份是给「第一次读这一节」的人看的：一个文件、一个类、能直接编译运行。
// 它保留原书【代码3.2】【算法3.3】要教的全部内容——连续数组、栈顶在表尾、
// 满了就把容量翻倍——只把原书那几处会崩的写法换掉。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  遵守**三法则**（析构 + 拷贝构造 + 拷贝赋值），正确，但拷贝多一点；
//   工程版  在此之上补齐移动构造/移动赋值、强异常保证、编译期类型约束。
// 两份都在闸门里真编译真运行。先读这一份，3.1.2a「进阶（选读）」再读那一份。
#pragma once

#include <cstddef>
#include <optional>

template <typename T>
class ArrayStack {
public:
    using value_type = T;
    using size_type = std::size_t;

    // 构造：先要一小块数组。容量不够时会自动翻倍，所以初值给多少都不影响正确性。
    explicit ArrayStack(size_type initial_capacity = 8)
        : data_(new T[initial_capacity]), capacity_(initial_capacity), size_(0) {}

    // 析构：数组是 new[] 来的，就得 delete[] 回去。
    ~ArrayStack() { delete[] data_; }

    // 拷贝构造：**必须自己写**。
    // 不写的话编译器生成的版本会把 data_ 这根指针照抄一份，于是两个栈指向同一块
    // 内存，各析构一次 —— 同一块内存被释放两次。原书 arrStack 正是漏了这个。
    ArrayStack(const ArrayStack& other)
        : data_(new T[other.capacity_]), capacity_(other.capacity_), size_(other.size_) {
        for (size_type i = 0; i < size_; ++i) {
            data_[i] = other.data_[i];
        }
    }

    // 拷贝赋值：同理。注意三件事的顺序——先把新数组备好，再释放旧的，最后接管。
    ArrayStack& operator=(const ArrayStack& other) {
        if (this == &other) {   // 自己赋值给自己，什么都不用做
            return *this;
        }
        T* fresh = new T[other.capacity_];
        for (size_type i = 0; i < other.size_; ++i) {
            fresh[i] = other.data_[i];
        }
        delete[] data_;
        data_ = fresh;
        capacity_ = other.capacity_;
        size_ = other.size_;
        return *this;
    }

    // 入栈。满了就翻倍，所以不会有「栈满溢出」这回事。
    void push(const T& value) {
        if (size_ == capacity_) {
            grow();
        }
        data_[size_] = value;
        ++size_;
    }

    // 出栈并把元素带回来。空栈返回空的 optional，不是错误，也不打印任何东西。
    std::optional<T> pop() {
        if (empty()) {
            return std::nullopt;
        }
        --size_;
        return data_[size_];
    }

    // 只看栈顶，不弹出。空栈同样返回空 optional。
    std::optional<T> top() const {
        if (empty()) {
            return std::nullopt;
        }
        return data_[size_ - 1];
    }

    bool empty() const { return size_ == 0; }
    size_type size() const { return size_; }
    size_type capacity() const { return capacity_; }

    // 清空：把长度归零就行，已经申请的数组留着接着用。
    void clear() { size_ = 0; }

private:
    // 扩容：申请一块两倍大的，把老元素搬过去，再把老的还回去。
    // 每个元素在均摊意义下只被搬运常数次，所以 push 的摊还代价仍是 O(1)。
    void grow() {
        size_type next = (capacity_ == 0) ? 1 : capacity_ * 2;
        T* fresh = new T[next];
        for (size_type i = 0; i < size_; ++i) {
            fresh[i] = data_[i];
        }
        delete[] data_;       // 先搬完再释放旧的，顺序反了就会读到已释放的内存
        data_ = fresh;
        capacity_ = next;
    }

    T* data_;             // 指向底层数组
    size_type capacity_;  // 数组能放多少个
    size_type size_;      // 现在放了几个，同时也是下一个空位的下标
};
```

把它存成 `teaching.hpp`，配上本章开头那个 `demo.cpp`，一条命令就能跑：

```bash
g++ -std=c++17 -Wall -Wextra demo.cpp -o demo && ./demo
```

#### 关键要点

**1. 空栈上的 `pop`/`top` 返回空 `optional`。**
原书的写法是 `bool pop(T & item)`——用出参带值、用返回值带成败。调用方一旦忘了检查
返回值，读到的就是那个从没被修改过的 `item`：程序不崩，只是默默按错误数据继续跑。
`std::optional<T>` 把「有没有值」搬进了**类型**里，取值之前必须先判断。
**空栈不是错误，是一种可预期的状态**，所以它返回空盒子而不是抛异常；
真正的错误（参数非法、容量溢出）才抛异常。

**2. 拷贝构造和拷贝赋值必须自己写——这是原书最硬的一处错。**
原书的 `arrStack` 有析构函数，却没有拷贝构造，也没有拷贝赋值。于是一句
`arrStack<int> b = a;` 会走编译器生成的拷贝——**把指针照抄一份**，
之后 `a` 和 `b` 持有**同一根指针**，各析构一次，**同一块内存被释放两次**。
这类错误的可怕之处在于它安静：`-Wall -Wextra -Wpedantic` 一句警告都不给，
小数据量下往往也不当场崩。本书用 AddressSanitizer 把它抓了出来，
报告抄在 `code/ch03/array_stack/legacy.md` 里。

规则叫**三法则**：一个类只要自己写了析构函数、拷贝构造、拷贝赋值中的**任意一个**，
通常这三个都得写。原因很直白——你之所以要写析构函数，是因为你在管资源；
既然在管资源，编译器那份「逐成员照抄」的拷贝就一定是错的。

**3. 翻倍扩容让 `push` 的摊还代价保持 O(1)。**
`grow()` 每次把容量乘 2，而不是加 1。差别不是常数：加 1 的话，push n 个元素总共要搬
$1+2+\cdots+n = O(n^2)$ 次；翻倍的话，总搬运次数是 $1+2+4+\cdots+n < 2n$，
平摊到每次 push 就是常数。这就是原书【算法3.3】的策略，一个字都没改。

顺带一提 `grow()` 里那三行的**顺序**：先申请新数组、再搬元素、最后才 `delete[]` 旧的。
反过来写（先释放再搬）就是读已经还回去的内存，ASan 当场报 `heap-use-after-free`。

**4. `new T[n]` 有一条限制，值得知道。**
它会把整块槽位**默认构造**出来。所以 `T` 必须能默认构造——一个只有带参构造函数的类
放不进这个栈。这是原书 `new T[mSize]` 就有的限制，本书的教学版没有改善它。
标准库容器的做法是先申请**未初始化**的原始内存，push 时再用 placement new
在指定位置构造对象；那要手写内存对齐和逐个析构，属于另一个话题。

**5. 容器里一个 `cout` 都没有。**
原书在空栈出栈时直接 `cout << "栈空"`。那样做把一个数据结构和标准输出焊死了：
它没法在库里复用，提示无法本地化，失败路径也无从测试。**数据结构负责数据结构，
报错交给调用方。** 本书的测试里有一条专门重定向 `cout`/`cerr` 并断言它们为空。

### 3.1.2a 进阶（选读）：从教学版到工程版

**这一节可以整节跳过。** 上面那份教学版是**正确**的，跑得起来，也经得起
AddressSanitizer 检查。这一节讲的是把它变成一个**工业级容器**还要补哪些东西——
移动语义、异常安全、编译期类型约束。等你哪天要写自己的容器时再回来读。

完整的工程版在 `code/ch03/array_stack/modern.hpp`，与教学版一样进闸门、
一样双档编译运行。下面按主题一段一段拆开看。

#### 一、存储与所有权：把三法则补成五法则

先把原书【代码3.2】的问题列全。它用三个成员表示一个顺序栈：`int mSize`（容量）、
`int top`（栈顶位置）、`T * st`（裸指针数组）。这套写法有三处在今天必须改：

1. `int top` 与成员函数 `bool top(T&)` **重名**，导致这个类按印刷原样根本编译不过；
2. 无参构造只设了 `top = -1`，`mSize` 与 `st` 都没初始化，析构时 `delete[]` 一个不确定指针；
3. 有析构函数却没有拷贝构造与拷贝赋值，一次 `arrStack<int> b = a;` 就会二次释放。

第 2、3 条是未定义行为，编译器开到 `-Wall -Wextra -Wpedantic` 也**一句警告都不给**。
教学版已经把这三条都修掉了：`size_` 不与任何成员函数重名，三个成员全部有初值，
三法则写全。

工程版在这之上再补两个函数——**移动构造**与**移动赋值**，合称**五法则**。
它们不修正确性，只修性能：教学版把一个临时栈赋给别人时会老老实实深拷贝一遍，
而那个临时对象下一行就要销毁了，这次拷贝纯属浪费。移动版直接把指针「偷」过来，
再把对方置空，O(1) 完事。

这里保留裸指针 `T* data_`——顺序栈的存储管理正是本节要教的内容，
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

#### 二、读栈顶的第二个接口：`peek()`

`optional` 那部分教学版已经有了，这里补的是它的**代价**：`std::optional<T> top()`
返回的是一份**副本**。副本要求 `T` 可拷贝，而且确实拷了一次。对
`std::unique_ptr` 这类只能移动的元素，这个接口根本用不了。

所以工程版另给一个零拷贝的观望接口 `peek()`：

```cpp file=code/ch03/array_stack/modern.hpp#pop
/// 出栈。空栈返回 std::nullopt——原书是「返回 false + 往 cout 打一行中文」，
/// 调用方既没法在库里复用，也容易忽略返回值。
[[nodiscard]]
std::optional<T> pop() {
    if (empty()) {
        return std::nullopt;
    }
    --top_index_;
    return std::optional<T>(std::move(data_[top_index_]));
}

/// 读栈顶但不弹出，返回**副本**。空栈返回 std::nullopt，不是未定义行为。
/// 要求 T 可拷贝；move-only 元素请用 peek()。
std::optional<T> top() const {
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
const T* peek() const noexcept {
    return empty() ? nullptr : &data_[top_index_ - 1];
}
```

「读栈顶」有两种正当需求，因此这里给了两个接口，**不要把它们合并成一个**：

- `top()` 返回 `std::optional<T>`，是一份可以带走的**副本**。安全，
  但要求 `T` 可拷贝，而且确实拷贝了一次。
- `peek()` 返回 `const T*`，空栈为 `nullptr`，**零拷贝**。
  `std::unique_ptr` 这类只能移动的元素只能用它来观望。

代价也是一对：`optional` 的安全靠拷贝换来，`peek()` 的零拷贝则把生命周期交给了
调用方——**它返回的指针在下一次 `push` / `pop` / `clear` 之后即失效**，
因为扩容会换掉整块缓冲区。这条失效契约必须写在文档里，不能靠使用者猜。

两种代价都真实存在。把任何一种藏起来，都是替读者做了本该由读者做的选择。

#### 三、扩容中途抛异常：强异常保证

翻倍扩容的策略教学版已经照原书【算法3.3】实现了。工程版在同一段代码上多做一件事：
**保证搬到一半失败时，原来的栈仍然完好。**

原书【算法3.3】那段代码本身有两个问题：循环变量 `i` 从未声明（编译不过），
以及先 `delete[] st` 再赋新指针，搬运中途若抛出异常，栈就停在半新半旧的状态。
教学版修掉了前者，也把顺序改成「先搬后释放」；但它没有处理「搬到一半抛异常」——
那时新申请的 `fresh` 会漏掉。教学版接受这个代价（元素类型是 `int`、`std::string`
这类东西时它根本不会发生），工程版不接受。

下面这段实现里会反复出现两个词，先说清楚：

- **强异常保证**：一个操作要么完全成功，要么**像没发生过一样**——绝不留下半成品状态。
  做法就是「先在新缓冲区上把事情做完，全部成功了再换过去」。原书的写法没有这个保证：
  旧缓冲区已经 `delete[]` 掉了，搬到一半抛异常，栈既回不到旧状态也到不了新状态。
- **RAII**：把资源的生命周期绑在对象的生命周期上，析构时自动释放。用了 RAII 的类型
  （例如 `std::unique_ptr`）不需要手写下面那段 `try/catch` 清理——这里用裸指针，
  所以清理要自己写。这份多出来的代价是本节要看见的东西之一。

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
            // 这里是**赋值**而不是构造，所以不能用 std::move_if_noexcept：
            // 它检查的是移动**构造**是否 noexcept，而可抛的移动赋值会在搬迁
            // 中途把原栈的元素掏空——红队 T-002 实测复现过（见 legacy.md 缺陷 11）。
            //
            // 判据必须落在「移动赋值抛不抛」这一个维度上：
            //   移动赋值 noexcept → 移动。不可能抛，强异常保证不受影响，也不白白深拷贝。
            //   否则             → 复制。拷贝赋值取 const&，抛了也动不了原栈；
            //                      上面的 static_assert 保证走到这里的 T 一定可复制赋值。
            if constexpr (std::is_nothrow_move_assignable<T>::value) {
                fresh[i] = std::move(data_[i]);
            } else {
                fresh[i] = data_[i];
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
- **搬迁用移动还是拷贝，判据必须选对**。这一条值得单独讲，见下一小节。
- **容量用 `std::size_t`**。原书用 `int`，`mSize * 2` 溢出是未定义行为；
  这里在翻倍前先判界并抛 `std::overflow_error`。

这不是纸面推理。测试里有一个「第 3 次拷贝赋值必定抛异常」的元素类型，
用它撑满栈再触发扩容，然后逐项断言：长度不变、容量不变、原有元素逐个完好、
栈之后仍能继续使用；另一个类型让 `new T[]` 本身抛 `std::bad_alloc`，同样逐项断言。
把「先建后换」改回原书的「先释放旧的」，Debug 构建下 UBSan 当场报空指针引用，
Release 构建直接段错误。

#### 四、一个容易踩空的地方：`move_if_noexcept` 在这里是错的

搬迁元素时，一个几乎条件反射的写法是 `std::move_if_noexcept(data_[i])`——
「移动可能抛就退回拷贝」，标准库容器扩容正是这么做的。**但在这段代码里它是错的。**

`std::move_if_noexcept` 的判据是 `T` 的**移动构造**是否 `noexcept`。而这里
`fresh` 已经由 `new T[next]` 默认构造好了，搬迁执行的是**移动赋值**。
两者是不同的函数，可以有不同的异常规格：一个类完全可以移动构造 `noexcept`、
移动赋值却会抛。这时 `move_if_noexcept` 会放行移动，而抛出发生在搬到一半时——
**原栈里已经被移动走的那些元素，已经被掏空了**，强异常保证就此破裂。

这个洞不是推演出来的：本书的测试里有一个正是这种形状的类型
（移动构造 `noexcept`，第 3 次移动赋值抛异常），它让原来的实现真的失败了。

判据要落在**实际执行的那个操作**上：

- 移动赋值 `noexcept` → 移动。不可能抛，强异常保证不受影响，也不必白白深拷贝。
- 否则 → 拷贝。拷贝赋值取 `const&`，抛了也动不了原栈。

第二条要求 `T` 可拷贝赋值，所以类头处那条 `static_assert` 写明：
不可拷贝的 `T` 必须满足 `is_nothrow_move_assignable`。**这是本容器对元素类型的
一条真实约束**，写成编译期断言，好过让它变成运行期某次扩容失败后的谜案。

反过来也要小心：判据若写成「可拷贝就拷贝」，`std::string`
（移动赋值本就是 `noexcept`）每次扩容都会退化成深拷贝，
算法3.3 摊还 O(1) 的分析就打了折扣。测试里因此有一条用例专门数拷贝次数——
**这两种写法都能通过其他所有断言，只有这条能把它们分开。**

#### 五、`push` 的两个重载

进栈本身随之简化为「保证容量、写入、长度加一」，但工程版给了**两个**重载：

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

两个重载分别处理左值和右值。教学版只写了 `push(const T&)`——正确，
但压入一个临时对象时会多拷贝一次。原书的 `bool push(const T item)` 更糟：
按值传参，顶层 `const` 对调用方没有意义，却强制了一次拷贝，
而且 `std::unique_ptr` 这类只能移动的类型根本传不进去。

#### 六、对元素类型的编译期约束

工程版的类头上还有四条 `static_assert`。它们不参与运行，只在编译期检查
「你放进来的这个 `T` 合不合格」：必须能默认构造（`new T[n]` 会构造整块槽位）、
必须能移动赋值（搬迁要用）、不可拷贝的 `T` 必须能无异常移动赋值（上一小节的判据）、
不能是引用类型。

这四条写在类里而不是文档里，是因为**编译期报错永远比运行期谜案便宜**。
教学版没有它们：`T` 不合格时报错会发生在模板实例化的深处，信息难看，但同样报错。
这是可读性与报错质量之间的一次交换，教学版选了前者。

### 3.1.3 链式栈

采用链式存储结构的栈称为链式栈。结点分散在堆上，压栈只是接一个新结点，
**不需要连续空间，也没有"栈满"这回事**——这正是它与顺序栈的核心差别。

```cpp file=code/ch03/linked_stack/modern.hpp#class-head
/// 链式栈：结点分散在堆上，压栈只是接一个新结点，不需要连续空间、也不需要扩容。
///
/// 与原书 lnkStack 的差别：`top` 这个名字只留给成员函数（原书 `Link<T>* top`
/// 与 `bool top(T&)` 重名，导致整个类编译不过）；补齐五法则；不做任何 I/O；
/// 出栈返回 `std::optional<T>`。
template <typename T>
class LinkedStack {
    struct Node {
        T value;
        Node* next;

        template <typename U>
        Node(U&& item, Node* successor) : value(std::forward<U>(item)), next(successor) {}
    };

public:
    using value_type = T;
    using size_type = std::size_t;
```

原书【代码3.4】的 `lnkStack` 有一处与顺序栈**完全相同**的错误：
成员 `Link<T>* top` 与成员函数 `bool top(T&)` 重名，整个类编译不过。
同一本书在两个存储结构上犯了同一个命名错误，两处都没有被编译器验证过。

另有两处值得指出：

- **构造函数是 `lnkStack(int defSize)`，而那个参数从未被使用。**
  链式栈不需要预设容量，这个参数是从 `arrStack(int size)` 照抄来的；
  更麻烦的是它没有默认构造函数，使用者被迫为一个无意义的参数编个数出来。
- **有 `~lnkStack(){ clear(); }` 却没有拷贝构造与拷贝赋值。**
  这是本书第五次遇到同一个错误（顺序栈、顺序表、链表、字符串、链式栈）。

```cpp file=code/ch03/linked_stack/modern.hpp#rule-of-five
// 原书有 `~lnkStack(){ clear(); }` 却没有拷贝构造与拷贝赋值：
// 一次 `lnkStack<int> b = a;` 之后两个栈共享同一串结点，各自析构一次 → 二次释放。
// 与顺序栈、顺序表、链表、字符串是同一个错误，本书第五次遇到它。
LinkedStack(const LinkedStack& other) {
    // 先按原序收集，再逆序压回，避免递归拷贝（深链会爆栈，见 UNVERIFIED-RISKS.md）
    Node* source = other.top_;
    Node** tail = &top_;
    try {
        while (source != nullptr) {
            *tail = new Node(source->value, nullptr);
            tail = &(*tail)->next;
            ++size_;
            source = source->next;
        }
    } catch (...) {
        clear();  // 半截链必须自己收拾：构造函数抛出时析构函数不会运行
        throw;
    }
}

LinkedStack& operator=(const LinkedStack& other) {
    if (this != &other) {
        LinkedStack copy(other);
        swap(copy);
    }
    return *this;
}

LinkedStack(LinkedStack&& other) noexcept
    : top_(std::exchange(other.top_, nullptr)), size_(std::exchange(other.size_, 0)) {}

LinkedStack& operator=(LinkedStack&& other) noexcept {
    if (this != &other) {
        clear();
        top_ = std::exchange(other.top_, nullptr);
        size_ = std::exchange(other.size_, 0);
    }
    return *this;
}

~LinkedStack() { clear(); }
```

压栈与出栈：

```cpp file=code/ch03/linked_stack/modern.hpp#push-pop
/// 入栈：接一个新结点。**没有"栈满"这回事**——这正是链式栈相对顺序栈的差别，
/// 原书顺序栈那边要判 `top == mSize - 1` 并打印"栈满溢出"。
void push(const T& item) { top_ = new Node(item, top_); ++size_; }
void push(T&& item) { top_ = new Node(std::move(item), top_); ++size_; }

/// 出栈。空栈返回 std::nullopt（D-001 §3c：空是可预期状态，不抛异常）。
[[nodiscard]] std::optional<T> pop() {
    if (top_ == nullptr) {
        return std::nullopt;
    }
    Node* dying = top_;
    std::optional<T> result(std::move(dying->value));
    top_ = dying->next;
    delete dying;
    --size_;
    return result;
}

/// 取栈顶副本；零拷贝的观望用 peek()（D-001 §3b）。
[[nodiscard]] std::optional<T> top() const {
    return top_ == nullptr ? std::nullopt : std::optional<T>(top_->value);
}

[[nodiscard]] const T* peek() const noexcept {
    return top_ == nullptr ? nullptr : &top_->value;
}
```

**一处实现上的取舍**：`clear()` 与析构都写成**迭代**而非递归。
链式结构最自然的写法是递归释放，但深链会耗尽调用栈——第 5 章有实测数字。
本书的测试用 20 万个结点压栈再全部弹出，正是为这条兜底。

### 3.1.4 表达式求值

后缀（逆波兰）表达式不需要括号，也不需要优先级规则：求值时**遇操作数压栈，
遇操作符弹出两个算完再压回**，读到末尾时栈里剩下的唯一元素就是结果。
这条主线本书一字未改，用的还是本章自己的 `ArrayStack<double>`。

```cpp file=code/ch03/expression_eval/modern.hpp#evaluate
/// 对后缀（逆波兰）表达式求值。记号之间用空白分隔，例如 "3 4 + 2 *" → 14。
///
/// 与原书 `class Calculator` 的差别，逐条对应它的三个问题：
///
/// 1. **原书 `Run()` 直接从 `cin` 读、往 `cout` 写**，算法与终端焊死：没法写测试、
///    没法在库里复用、没法处理来自别处的表达式。这里接受一个 `string_view`、返回结果。
/// 2. **原书 `GetTwoOperands` 在第二个操作数缺失时已经把第一个弹掉了**，
///    返回 false 时栈已被破坏（legacy.md 缺陷 1）。
///    但要说准确：真正让这个 bug 无害的，**不是**下面那句"先查够不够"，
///    而是**出错即抛出、整次求值随即作废**——栈根本没有机会被下一步用到。
///    预检查只是让错误信息更早、更准，属于锦上添花。
///    原书的 bug 之所以要命，是因为它 `cerr` 一行之后**继续跑**。
/// 3. **原书出错时 `cerr` 打一行然后 `s.clear()` 继续跑**，调用方拿不到任何信号。
///    这里抛 `std::invalid_argument`（表达式不合法）或 `std::domain_error`（除零）。
[[nodiscard]] inline double evaluate_postfix(std::string_view expression) {
    ArrayStack<double> operands;
    std::size_t i = 0;

    const auto pop_two = [&operands](double& left, double& right) {
        // 先确认有两个再弹。注意这不是"修复"原书 bug 的关键（见函数注释第 2 条），
        // 而是让报错更早、更准。
        if (operands.size() < 2) {
            throw std::invalid_argument("后缀表达式：操作数不足");
        }
        right = *operands.pop();  // 先弹出的是右操作数
        left = *operands.pop();
    };

    while (i < expression.size()) {
        const char c = expression[i];
        if (c == ' ' || c == '\t' || c == '\n') {
            ++i;
            continue;
        }

        // 操作符：+ - * / 各弹两个。注意 '-' 也可能是负号，靠后面是不是数字来区分。
        const bool is_sign = (c == '-' || c == '+') && i + 1 < expression.size()
                             && (std::isdigit(static_cast<unsigned char>(expression[i + 1]))
                                 || expression[i + 1] == '.');
        if (!is_sign && (c == '+' || c == '-' || c == '*' || c == '/')) {
            double left = 0.0;
            double right = 0.0;
            pop_two(left, right);
            switch (c) {
                case '+': operands.push(left + right); break;
                case '-': operands.push(left - right); break;
                case '*': operands.push(left * right); break;
                default:
                    // 原书写 `if (operand1 == 0.0)`，正文又说这样比较浮点数不对、
                    // 该用阈值。两者都值得推敲：除以 1e-300 是**合法**的，
                    // 结果是个很大的数或 inf，用阈值把它当成错误是另一个决定。
                    // 这里只拦精确的 0.0（含 -0.0），并把这个取舍写在明面上。
                    if (right == 0.0) {
                        throw std::domain_error("后缀表达式：除数为零");
                    }
                    operands.push(left / right);
                    break;
            }
            ++i;
            continue;
        }

        // 操作数：交给标准库解析，顺便拿到它吃掉了多少字符
        std::size_t consumed = 0;
        double value = 0.0;
        try {
            value = std::stod(std::string(expression.substr(i)), &consumed);
        } catch (const std::exception&) {
            throw std::invalid_argument(std::string("后缀表达式：无法识别的记号 '") + c + "'");
        }
        operands.push(value);
        i += consumed;
    }

    if (operands.size() != 1) {
        // 空表达式、操作数多余、操作符不足，都落在这里
        throw std::invalid_argument("后缀表达式：不是一个完整的表达式");
    }
    return *operands.pop();
}
```

原书【算法3.5】把它写成一个 `class Calculator`，有三处今天必须改。

**一、算法与终端焊死。** `Run()` 直接 `cin >> c` 读、`cout << res` 写，
出错时 `cerr` 打一行。后果是这个算法没法写测试、没法在库里复用、
也没法处理来自别处的表达式。本书改为接受一个字符串、返回结果、出错抛异常。

**二、`GetTwoOperands` 在第二个操作数缺失时，第一个已经被弹掉了。**
栈就此少一个元素。但要说准确——**真正让这件事变成 bug 的是调用方继续跑**：
`Compute` 拿到 `false` 之后清空栈然后接着读下一个记号，`Run()` 的循环照转不误。

这一点改变了"怎样才算修好"：本书的实现**出错即抛出、整次求值作废**，
栈根本没有机会被下一步用到。（写这一节时验证过：把实现改回"先弹一个再查"，
全部断言依然通过——因为在抛异常即作废的设计里，栈被破坏与否观测不到。
于是补了一条测试，钉住"上一次失败不给下一次留残留"这条真正可测的性质。）

**三、除零判断。** 原书正文自己写道：

> 在实际编写程序时，浮点数是否为0不能直接与0进行相等比较，
> 而是要通过某个很小的阈值来判断，例如采用 `if(abs(operand1) < 1E-7)`

**但印出来的代码仍然是 `if (operand1 == 0.0)`。** 书知道更好的写法，却没有印它。

不过原书这条建议本身也值得推敲：**除以 1e-300 是合法的**，
结果是一个极大的有限值或无穷，把它当作错误是另一个决定，不是"更正确"。
本书只拦精确的零，并把这个取舍写在代码注释里；测试中有一条断言专门钉住
"除以 1e-300 得到极大值而非报错"。

### 3.1.5 栈与递归

> **本节开篇先摆一组实测数字。** 原书正文说递归「需要在内存中开辟一个称为
> 运行栈(runtime stack)的**足够大**的动态区」。多大算足够大？这是可以量的，
> 而量出来的结果里有一条相当反直觉。

### 运行栈有多大：三个档位，三种结果

同一份递归源码（每层做一次加法后返回），在一台 Linux 机器上
（gcc 13.3，`ulimit -s` 为默认的 8 MB）逐档加深度：

| 构建档 | 20 万层 | 50 万层 | 100 万层 |
| --- | --- | --- | --- |
| `-O0`（不优化） | 通过 | **段错误** | 段错误 |
| `-O1` + ASan/UBSan | 通过 | **stack-overflow** | stack-overflow |
| `-O2` | 通过 | 通过 | **通过** |

把同样的计算改成显式栈（数据压进本章的 `ArrayStack`，也就是放到堆上）：
三个档位在 **1000 万层**都通过。

### 反直觉的那一条：`-O2` 为什么不崩

不是因为它栈更大，而是因为**编译器把递归消掉了**。查汇编可以确认：

```text
-O0: recursive_sum 函数体内调用自己的次数 = 2
-O2: recursive_sum 函数体内调用自己的次数 = 0
```

`-O2` 下这个函数根本没有自调用——它被转成了循环。所以：

> **「这段递归会不会爆栈」不是源码单独决定的，
> 是源码 × 编译器 × 优化档共同决定的。**

这件事对学习者的实际含义是：在 `-O2` 下跑通的递归程序，
换成 `-O0` 调试、或者换个编译器、或者递归形状稍微复杂到无法被转换，
就可能在同样的输入上崩掉。而崩掉时你看到什么，也取决于构建方式——
开了 sanitizer 的构建会明确告诉你 `stack-overflow` 并给出递归回溯，
`-O0` 的构建只有一个段错误，**一行解释都没有**。

### 递归吃运行栈，显式栈吃堆

这正是本节的正题。原书用阶乘作例子，给了三个版本：

**【算法3.6】递归**——每一层的返回地址与局部变量都压在运行栈上，深度由进程栈上限决定：

```cpp file=code/ch03/recursion_and_stack/modern.hpp#factorial-recursive
/// 【算法3.6】递归实现。保留原书的递归形状——那正是本节要教的东西。
///
/// 加了原书没有的两道检查：负数是定义域错误，溢出是真错误（D-001 §3）。
/// 原书 `if (n <= 0) return 1;` 把负数静默当成 0 处理，返回 1。
[[nodiscard]] inline factorial_type factorial_recursive(long long n) {
    if (n < 0) {
        throw std::invalid_argument("factorial: 负数没有阶乘");
    }
    if (static_cast<factorial_type>(n) > kMaxFactorialInput) {
        throw std::overflow_error("factorial: 结果超出 64 位无符号范围（20! 是上限）");
    }
    if (n <= 1) {
        return 1;  // 递归出口
    }
    return static_cast<factorial_type>(n) * factorial_recursive(n - 1);
}
```

**【算法3.8】迭代**——不用栈，也不占深度：

```cpp file=code/ch03/recursion_and_stack/modern.hpp#factorial-iterative
/// 【算法3.8】迭代实现。不用栈，也不占运行栈深度。
[[nodiscard]] inline factorial_type factorial_iterative(long long n) {
    if (n < 0) {
        throw std::invalid_argument("factorial: 负数没有阶乘");
    }
    if (static_cast<factorial_type>(n) > kMaxFactorialInput) {
        throw std::overflow_error("factorial: 结果超出 64 位无符号范围（20! 是上限）");
    }
    factorial_type m = 1;
    for (long long i = 2; i <= n; ++i) {
        m *= static_cast<factorial_type>(i);
    }
    return m;
}
```

**【算法3.9】显式栈**——把待处理的数据压进一个自己管理的栈，
用原书的话说是「模拟编译系统处理递归的机制，使用栈等数据结构保存回溯点」：

```cpp file=code/ch03/recursion_and_stack/modern.hpp#factorial-explicit-stack
/// 【算法3.9】用显式栈模拟递归。
///
/// 这一版存在的意义不是"更快"——它比迭代版慢——而是**演示编译系统处理递归的机制**：
/// 遇到递归规则就压栈，遇到递归出口就出栈返回。原书的话是
/// 「模拟编译系统处理递归的机制，使用栈等数据结构保存回溯点」。
///
/// 关键差别在**数据放在哪**：递归版把每层的返回地址与局部变量放在**运行栈**上，
/// 大小由进程栈上限决定；这一版把待处理的数据压进 ArrayStack，**在堆上**，
/// 只受内存限制。实测数字见书稿 3.1.5 节。
///
/// 原书写的是 `while (s.pop(&tmp))`——传的是**指针**，而同书代码3.1 的栈 ADT
/// 声明的是 `bool pop(T& item)`（引用）。两处对不上（legacy.md 缺陷 3）。
[[nodiscard]] inline factorial_type factorial_with_explicit_stack(long long n) {
    if (n < 0) {
        throw std::invalid_argument("factorial: 负数没有阶乘");
    }
    if (static_cast<factorial_type>(n) > kMaxFactorialInput) {
        throw std::overflow_error("factorial: 结果超出 64 位无符号范围（20! 是上限）");
    }
    ArrayStack<factorial_type> pending;
    for (long long i = n; i > 1; --i) {  // 按递归规则压栈
        pending.push(static_cast<factorial_type>(i));
    }
    factorial_type m = 1;  // 递归出口的返回值
    while (auto top = pending.pop()) {   // 出栈即"递归返回"
        m *= *top;
    }
    return m;
}
```

三者的差别不在快慢（显式栈版最慢），而在**数据放在哪**：
前者在运行栈上，受进程栈上限约束；后者在堆上，只受内存约束。
上面那张表量的就是这个差别。

### 原书这三个版本共同的问题：不查溢出

三个版本都是 `long factorial(long n)`，既不检查负数也不检查溢出。
64 位 `long` 装得下的最大阶乘是 20!，从 21 开始：

```text
factorial(20) = 2432902008176640000
factorial(21) = -4249290049419214848      ← 负数
factorial(66) = 0                          ← 零
factorial(-5) = 1                          ← 负数静默当成 0 处理
```

而且这不只是"答案错"——有符号整数溢出在 C++ 里是**未定义行为**：

```text
runtime error: signed integer overflow: 21 * 2432902008176640000
               cannot be represented in type 'long int'
```

本书改用 `std::uint64_t` 并在入口显式判界：超过 20 抛 `std::overflow_error`，
负数抛 `std::invalid_argument`。三个版本在边界上的行为完全一致——
测试要求它们对同一输入给出相同答案，包括同样地抛出异常。

（另有一处书内不一致：算法3.9 写 `s.pop(&tmp)` 传指针，
而同章代码3.1 的栈 ADT 声明的是 `bool pop(T& item)` 传引用，两处配不上。
详见 `code/ch03/recursion_and_stack/legacy.md`。）

### 一个更复杂的例子：背包问题

阶乘只有**一条**递归规则，改写成循环几乎是显然的。原书接着给了一个有**两条**
递归规则的例子——背包问题（更准确地说是子集和判定：能否从若干物品中选出一部分，
使重量之和恰好等于背包承重）。

```cpp file=code/ch03/knapsack/modern.hpp#recursive
/// 【算法3.10】递归解法。两条递归规则、两个递归出口，原书的结构一字未改：
///   出口 1：承重恰为 0 → 有解（什么都不再选）
///   出口 2：承重为负，或承重为正但已无物品可选 → 无解
///   规则 1：选第 n-1 件 → 求解 knap(s - w[n-1], n-1)
///   规则 2：不选第 n-1 件 → 求解 knap(s, n-1)
///
/// 与原书的差别只有两处：
/// 1. **原书直接 `cout << w[n-1]`** 把选中的物品打印出来——算法与终端焊死，
///    调用方拿不到结果，也没法测试。这里把下标收集进 `chosen` 返回。
/// 2. **原书的 `w[]` 是一个从未声明的全局数组**（legacy.md 缺陷 3）。这里作参数传入。
[[nodiscard]] inline std::optional<knapsack_solution> knapsack_recursive(
    int capacity, const std::vector<int>& weights) {
    detail::validate(capacity, weights);
    knapsack_solution chosen;

    // 返回 true 表示 weights[0..n) 中存在一个子集，其和恰为 s
    const auto solve = [&weights, &chosen](auto&& self, int s, std::size_t n) -> bool {
        if (s == 0) {
            return true;  // 递归出口 1
        }
        if (s < 0 || n == 0) {
            return false;  // 递归出口 2
        }
        if (self(self, s - weights[n - 1], n - 1)) {  // 规则 1：选它
            chosen.push_back(n - 1);
            return true;
        }
        return self(self, s, n - 1);  // 规则 2：不选它
    };

    return solve(solve, capacity, weights.size())
               ? std::optional<knapsack_solution>(std::move(chosen))
               : std::nullopt;
}
```

两个递归出口、两条递归规则，本书一字未改。改的是两处：
原书 `w[]` 是一个**从未声明的全局数组**，这里作参数传入；
原书在选中物品时 `cout << w[n-1]` 直接打印，调用方拿不到解、正确性也无从检验，
这里把下标收集起来返回——测试因此可以独立验算：**把返回的下标对应的重量加起来，
必须恰好等于承重**。

#### 把它机械地改写成循环

原书的做法是引入一个显式栈，每帧保存四个域：参数 s 与 n、**返回地址** rd、结果单元 k。
返回地址是关键——它记的是"这一层算完之后该回到哪一步继续"，
正是编译器为你做的那件事。原书用 `goto label0/1/2/3` 表达它。

本书保留这套机制，但把"返回地址"写成栈帧里的一个 `stage` 字段：

```cpp file=code/ch03/knapsack/modern.hpp#explicit-stack
/// 【算法3.11】把上面的递归机械地改写成显式栈驱动。
///
/// 原书用 `goto label0/1/2/3` 表示"执行到哪一步"，本书把同一件事写成栈帧里的
/// 一个 `stage` 字段——**语义完全对应**，只是不用 goto：goto 跳进跳出会让编译器
/// 无法保证局部对象的构造与析构配对，在有 RAII 的 C++ 里不能这么写。
///
///   `Enter`      ↔ label0，递归调用入口：判出口，否则按规则 1 展开
///   `AfterRule1` ↔ label1，规则 1（选第 n-1 件）返回后的处理
///   `AfterRule2` ↔ label2，规则 2（不选第 n-1 件）返回后的处理
///
/// 每一帧存原书说的四个域：参数 s 与 n、返回地址（这里是 stage）、结果单元 k。
[[nodiscard]] inline std::optional<knapsack_solution> knapsack_with_explicit_stack(
    int capacity, const std::vector<int>& weights) {
    detail::validate(capacity, weights);

    enum class Stage { Enter, AfterRule1, AfterRule2 };
    struct Frame {
        int s = 0;
        std::size_t n = 0;
        Stage stage = Stage::Enter;
    };

    ArrayStack<Frame> stack;
    stack.push(Frame{capacity, weights.size(), Stage::Enter});
    knapsack_solution chosen;
    bool child_result = false;   // 下层刚刚返回的结果单元 k

    while (!stack.empty()) {
        Frame frame = *stack.pop();

        if (frame.stage == Stage::Enter) {
            if (frame.s == 0) {            // 递归出口 1
                child_result = true;
                continue;                  // 相当于 goto label3：直接向上返回
            }
            if (frame.s < 0 || frame.n == 0) {   // 递归出口 2
                child_result = false;
                continue;
            }
            frame.stage = Stage::AfterRule1;     // 记下"回来时该走哪一步"
            stack.push(frame);
            stack.push(Frame{frame.s - weights[frame.n - 1], frame.n - 1, Stage::Enter});
            continue;
        }

        if (frame.stage == Stage::AfterRule1) {
            if (child_result) {            // 规则 1 成功：第 n-1 件被选中
                chosen.push_back(frame.n - 1);
                continue;                  // k 已是 true，继续上传
            }
            frame.stage = Stage::AfterRule2;     // 回溯，改用规则 2
            stack.push(frame);
            stack.push(Frame{frame.s, frame.n - 1, Stage::Enter});
            continue;
        }

        // Stage::AfterRule2：规则 2 的结果就是本层的结果，原样上传
    }

    return child_result ? std::optional<knapsack_solution>(std::move(chosen)) : std::nullopt;
}
```

`Enter` / `AfterRule1` / `AfterRule2` 与原书的 label0 / label1 / label2 一一对应。
**不用 goto 的理由是硬的**：goto 跳进跳出会让编译器无法保证局部对象的构造与析构
配对，在有 RAII 的 C++ 里不能这么写。

#### 优化：让每层要记的东西更少

原书接着指出两点可以省：

1. **结果单元 k 可以提到栈外**——一旦某层为真就逐层上传且不再变化，
   一个函数级变量就够了；
2. **参数 n 可以由栈深推出**——每递归一层 n 减 1、栈深加 1，所以 `n = n₀ − 栈深`。

于是栈帧从四个域缩到两个：

```cpp file=code/ch03/knapsack/modern.hpp#optimized
/// 【算法3.12】原书"优化版"的两点观察，本书照单实现：
///
/// 1. **结果单元 k 可以提到栈外**——一旦某层为 true 就逐层上传且不再变化，
///    因此一个函数级变量即可，栈帧里的 `k` 域连同它的反复赋值、进出栈都省掉。
///    （上面那版其实已经这么做了：`child_result` 就在栈外。）
/// 2. **参数 n 可以由栈深推出**——每递归一层 n 减 1、栈深加 1，
///    所以 `n = n0 - 栈深`，栈帧里的 `n` 域也能省掉。
///
/// 于是栈帧从四个域缩到两个（s 与 stage）。这是本节真正的"优化"：
/// 不是让它更快，而是**让每层要记的东西更少**——这正是手工模拟递归的意义。
///
/// 原书这一版另有一处致命问题：它同时把 `stack.top` 当**数据成员**用
/// （`t = stack.top;`）又当**成员函数**用（`stack.top(&tmp);`）。
/// 这在任何一种解释下都编译不过，而且它恰恰依赖代码3.2/3.4 那个 `top` 重名缺陷。
[[nodiscard]] inline std::optional<knapsack_solution> knapsack_optimized(
    int capacity, const std::vector<int>& weights) {
    detail::validate(capacity, weights);

    enum class Stage { Enter, AfterRule1, AfterRule2 };
    struct Frame {
        int s = 0;              // 只剩两个域
        Stage stage = Stage::Enter;
    };

    const std::size_t n0 = weights.size();
    ArrayStack<Frame> stack;
    knapsack_solution chosen;
    bool child_result = false;

    stack.push(Frame{capacity, Stage::Enter});
    std::size_t depth = 1;      // 栈中帧数；当前帧的 n = n0 - (depth - 1)

    while (!stack.empty()) {
        Frame frame = *stack.pop();
        --depth;
        const std::size_t n = n0 - depth;   // 观察 2：n 由栈深推出，不再入栈

        if (frame.stage == Stage::Enter) {
            if (frame.s == 0) { child_result = true; continue; }
            if (frame.s < 0 || n == 0) { child_result = false; continue; }
            frame.stage = Stage::AfterRule1;
            stack.push(frame);
            stack.push(Frame{frame.s - weights[n - 1], Stage::Enter});
            depth += 2;
            continue;
        }

        if (frame.stage == Stage::AfterRule1) {
            if (child_result) { chosen.push_back(n - 1); continue; }
            frame.stage = Stage::AfterRule2;
            stack.push(frame);
            stack.push(Frame{frame.s, Stage::Enter});
            depth += 2;
            continue;
        }
        // AfterRule2：结果原样上传
    }

    return child_result ? std::optional<knapsack_solution>(std::move(chosen)) : std::nullopt;
}
```

**这才是本节真正的"优化"**：不是让它跑得更快，而是让每层要记的东西更少。
手工模拟递归的全部意义就在这里——你必须想清楚"每一层到底需要记住什么"，
而这正是编译器替你想过的那个问题。

> **一句实话**：写这个单元时，第一版显式栈实现我把"返回地址"塞进了位标志里，
> 结果死循环、撞上构建超时；改写成与原书 label 一一对应的状态机之后才一次通过。
> 手工模拟递归确实容易写错——而原书那份用 goto 的版本从未被编译器验证过。
> 算法3.12 里 `stack.top` 同时被当成数据成员（`t = stack.top;`）和成员函数
> （`stack.top(&tmp);`），任何一种解释下都编译不过。

## 与原书的对照

| 原书 | 现在 | 为什么 |
| --- | --- | --- |
| `class Stack<T>` 空基类 | 三条 `static_assert` + `<type_traits>` | 空基类给不了多态，还带来非虚析构的未定义行为；对 `T` 的要求应当是编译期约束 |
| `int mSize / int top / T* st` | `size_t capacity_ / size_t top_index_ / T* data_` | `int` 溢出是未定义行为；`top_index_` 避开与成员函数 `top()` 重名 |
| 只有 `~arrStack()` | 五个特殊成员函数补齐 | 否则一次拷贝就二次释放 |
| `bool pop(T& item)` | `[[nodiscard]] std::optional<T> pop()` | 「有没有值」交给类型系统，忽略返回值会告警 |
| `bool top(T& item)` | `optional<T> top()` 取副本；`const T* peek()` 零拷贝 | 两种正当需求，两种代价，都摆在明面上 |
| `cout << "栈满溢出"` | 不做任何 I/O；越界抛 `std::out_of_range` | 数据结构不该和标准输出耦合；可预期的空状态用 `optional`，真错误才抛异常 |
| `delete[] st; st = newSt;` | 先建后换 + `try/catch`，按**移动赋值是否 noexcept** 决定移动还是拷贝 | 搬迁中途抛异常不破坏原栈（强异常保证）；判据落在实际执行的操作上，不是 `move_if_noexcept` 看的移动构造 |

**刻意没改的**：它仍然是一个手写的、自己管缓冲区的数组栈，缓冲区仍是裸的
`T* data_`，扩容仍是算法3.3 的翻倍策略，`clear()` 仍然只把长度归零、保留已分配容量。
把它换成 `std::stack` 的薄封装固然更短，但这一节要教的正是这些实现细节本身。

完整实现见 `code/ch03/array_stack/modern.hpp`，测试见同目录 `test.cpp`
（58 项断言，覆盖上表每一行；用 `python3 tools/check_code.py` 在
`-Werror` + ASan/UBSan 与 `-O2` 两种构建下各跑一遍）。

## 3.2 队列

队列只允许在一端插入、在另一端删除，元素按先进先出(FIFO)访问。插入端是队尾，删除端是队头。空队列上的提取是可预期状态，返回 `std::nullopt`。

### 3.2.1 队列的抽象数据类型

常用运算是入队 `enqueue`、出队 `dequeue`、读队头，以及判空。顺序实现还需要判满。原书【代码3.13】同样把这些运算写成一个假抽象基类；本书直接定义在 `ArrayQueue` 与 `LinkedQueue` 上。

### 3.2.2 顺序队列

循环队列牺牲一个槽位区分空与满：逻辑容量为 n 时实际申请 n+1 个槽。`front_ == rear_` 为空，`(rear_ + 1) % slots_ == front_` 为满。这正是章首 demo 里「容量 3 时第 4 个入不进去」的原因。

```cpp file=code/ch03/queue/modern.hpp#array-queue
template <typename T>
class ArrayQueue {
public:
    explicit ArrayQueue(std::size_t capacity) : slots_(capacity + 1), data_(slots_ ? new T[slots_] : nullptr) {}
    ArrayQueue(const ArrayQueue& other) : slots_(other.slots_), front_(other.front_), rear_(other.rear_), data_(slots_ ? new T[slots_] : nullptr) { for (std::size_t i = front_; i != rear_; i = (i + 1) % slots_) data_[i] = other.data_[i]; }
    ArrayQueue& operator=(const ArrayQueue& other) { if (this != &other) { ArrayQueue copy(other); swap(copy); } return *this; }
    ArrayQueue(ArrayQueue&& other) noexcept { swap(other); }
    ArrayQueue& operator=(ArrayQueue&& other) noexcept { if (this != &other) { ArrayQueue moved(std::move(other)); swap(moved); } return *this; }
    ~ArrayQueue() { delete[] data_; }
    void swap(ArrayQueue& other) noexcept { using std::swap; swap(slots_, other.slots_); swap(front_, other.front_); swap(rear_, other.rear_); swap(data_, other.data_); }
    [[nodiscard]] bool empty() const noexcept { return front_ == rear_; }
    [[nodiscard]] bool full() const noexcept { return slots_ != 0 && (rear_ + 1) % slots_ == front_; }
    [[nodiscard]] std::size_t size() const noexcept { return rear_ >= front_ ? rear_ - front_ : slots_ - front_ + rear_; }
    [[nodiscard]] bool enqueue(const T& value) { if (full()) return false; data_[rear_] = value; rear_ = (rear_ + 1) % slots_; return true; }
    [[nodiscard]] bool enqueue(T&& value) { if (full()) return false; data_[rear_] = std::move(value); rear_ = (rear_ + 1) % slots_; return true; }
    [[nodiscard]] std::optional<T> dequeue() { if (empty()) return std::nullopt; T value = std::move(data_[front_]); front_ = (front_ + 1) % slots_; return value; }
    [[nodiscard]] const T* front() const noexcept { return empty() ? nullptr : &data_[front_]; }
    void clear() noexcept { front_ = rear_ = 0; }
private: std::size_t slots_{0}, front_{0}, rear_{0}; T* data_{nullptr};
};
```

### 3.2.3 链式队列

链式队列用首尾指针维持 FIFO，入队接在尾、出队摘下头，并具备独立复制所有权。

```text
template <typename T>
class LinkedQueue {
    struct Node { T value; Node* next{nullptr}; template <typename U> explicit Node(U&& value) : value(std::forward<U>(value)) {} };
public:
    LinkedQueue() = default;
    LinkedQueue(const LinkedQueue& other) { for (Node* n = other.front_; n != nullptr; n = n->next) enqueue(n->value); }
    LinkedQueue& operator=(const LinkedQueue& other) { if (this != &other) { LinkedQueue copy(other); swap(copy); } return *this; }
    LinkedQueue(LinkedQueue&& other) noexcept { swap(other); }
    LinkedQueue& operator=(LinkedQueue&& other) noexcept { if (this != &other) { LinkedQueue moved(std::move(other)); swap(moved); } return *this; }
    ~LinkedQueue() { clear(); }
    void swap(LinkedQueue& other) noexcept { using std::swap; swap(front_, other.front_); swap(rear_, other.rear_); swap(size_, other.size_); }
    [[nodiscard]] bool empty() const noexcept { return front_ == nullptr; }
    [[nodiscard]] std::size_t size() const noexcept { return size_; }
    void enqueue(const T& value) { append(new Node(value)); }
    void enqueue(T&& value) { append(new Node(std::move(value))); }
    [[nodiscard]] std::optional<T> dequeue() { if (front_ == nullptr) return std::nullopt; Node* old = front_; front_ = old->next; if (front_ == nullptr) rear_ = nullptr; --size_; T value = std::move(old->value); delete old; return value; }
    [[nodiscard]] const T* front() const noexcept { return front_ == nullptr ? nullptr : &front_->value; }
    void clear() noexcept { while (front_ != nullptr) { Node* old = front_; front_ = old->next; delete old; } rear_ = nullptr; size_ = 0; }
private: void append(Node* node) noexcept { if (rear_ == nullptr) front_ = rear_ = node; else { rear_->next = node; rear_ = node; } ++size_; } Node* front_{nullptr}; Node* rear_{nullptr}; std::size_t size_{0};
};
```

## 3.3 栈与队列的比较

### 3.3.1 顺序栈与链式栈

顺序栈预分配连续缓冲区，扩容要搬迁；链式栈按需分配，没有「栈满」，但每个结点多一个指针。两者的 `push`/`pop` 都是 O(1)。

### 3.3.2 顺序队列与链式队列

两种队列的端点操作都是常数时间。循环数组适合容量上界已知、希望局部性好的场合；链表适合长度变化大、不愿预留空洞槽位的场合。

### 3.3.3 限制存取点的表

栈和队列都是限制存取点的线性表：栈只开一端，队列开两端但方向相反。双端队列同时开放两端，不在本章实现范围内。

## 本章小结

栈只在一端插入删除，后进先出；队列在一端插、另一端删，先进先出。二者都可以顺序或链式实现，端点运算都是常数时间。顺序实现要处理满与空（循环队列牺牲一个槽）；链式实现按需分配，没有预先的容量上限。栈用于递归、括号和表达式；队列用于层次周游和缓冲。

## 习题

### 补充算法设计题（参考课程第 3 章）

1. 只使用两个栈的 `push`、`pop`、`empty`，设计队列的 `enqueue`、`dequeue` 和 `empty`，并分析摊还复杂度。
2. 编号为 `1..n` 的车辆依次进栈，求所有合法出栈序列的数量并证明其为 Catalan 数。
3. 用两个栈设计编辑器的撤销和恢复；说明新操作、撤销、恢复时两个栈如何变化。

1. 循环队列两端都允许插入删除。给出类型定义，以及「从队尾删除」「从队头插入」的算法。
2. 用循环数组表示队列，只设头指针 `front` 和计数器 `count`，不设尾指针。实现判空、入队、出队。
3. 把栈 $S$ 中的元素逆置：分别使用两个额外栈；一个额外队列；一个额外栈加若干非数组变量。
4. 用栈定义队列。
5. 带头结点的循环链表表示队列，只设一个指向队尾的指针。写出初始化、入队、出队。
6. 在长度为 $n$ 的数组里实现两个栈，元素总数不到 $n$ 时都不溢出，且 `push`/`pop` 为 $O(1)$。
7. 编号 1 到 5 的列车顺序进栈式站台，出站顺序有多少种？
8. 证明：用一个栈把 $1,2,\ldots,n$ 变成 $p_1,\ldots,p_n$ 的充要条件是不存在 $i<j<k$ 使 $p_j<p_k<p_i$。
9. 用栈计算后缀表达式 $12\,8\,9\,*\,+$，写出每一步的栈。
10. 用栈把中缀 $a*(b*c-d)+e$ 转成后缀，写出每一步的栈。
11. 按 $GCD(n,m)$ 的递归定义写算法。
12. 写递归和非递归算法，求 $1+1/2-1/3+1/4-\cdots$ 的前 $n$ 项和。

## 上机题

1. 用两个栈模拟队列，实现 `enqueue`、`dequeue`、`queue_empty`。
2. 用数组实现双端队列，两端插入删除都是常数时间。
3. 用辅助队列（两个或一个）使队列元素有序。
4. 把栈 $s_1$ 的元素转到 $s_2$ 并保持原顺序：分别用一个辅助栈；只用非数组变量。
5. 写计算 $fib(n)$ 的递归过程，再用栈改成非递归。
6. 按定义计算 $Ack(m,n)$：写出 $Ack(2,1)$ 的过程，以及非递归算法。

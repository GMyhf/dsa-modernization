# 原书写法 → 问题 → 现代写法：顺序栈

覆盖清单：**代码3.1**（栈的抽象数据类型定义）、**代码3.2**（栈的顺序实现）、
**算法3.3**（改进的进栈操作）。原文见 `dsa_raw.md:1806`、`dsa_raw.md:1841`、`dsa_raw.md:1905`。

> 本文件是「证据」，不是「观点」。下面每条缺陷都附了可复现的命令与真实输出。
> 复核方可以逐条重跑。
>
> 现代化取值遵循 `collab/DECISION_LOG.md` 的 **D-001 风格公约**（人已拍板）：
> C++17、不拿 STL 容器替代手写实现、存储结构属教学内容故用裸指针 + 显式五法则、
> 容器内零 I/O、空状态返回 `optional`、越界与溢出抛标准异常。

## 一、原书清单（已修复 OCR 损伤，逻辑一字未改）

OCR 把 `}` 认成了 `1`/`一`，把 `--` 拆成 `- -`，把 `-1` 写成 `− 1`，把语言标签标成了
```hcl。下面这份只做了这类无争议的还原，**没有动任何逻辑**：

```cpp
template <class T>              // 代码3.1：抽象数据类型
class Stack {
public:
    void clear();
    bool push(const T item);
    bool pop(T & item);
    bool top(T & item);
    bool isEmpty();
    bool isFull();
};

template <class T>              // 代码3.2：顺序实现
class arrStack : public Stack<T> {
private:
    int mSize;                  // 栈中最多可存放的元素个数
    int top;                    // 栈顶位置，应小于 mSize
    T * st;                     // 存放栈元素的数组
public:
    arrStack(int size) { mSize = size; top = -1; st = new T[mSize]; }
    arrStack() { top = -1; }
    ~arrStack() { delete [] st; }
    void clear() { top = -1; }
    bool push(const T item) {
        if (top == mSize - 1) { cout << "栈满溢出" << endl; return false; }
        else { st[++top] = item; return true; }
    }
    bool pop(T & item) {
        if (top == -1) { cout << "栈为空，不能执行出栈操作" << endl; return false; }
        else { item = st[top--]; return true; }
    }
    bool top(T & item) {
        if (top == -1) { cout << "栈为空，不能读取栈顶元素" << endl; return false; }
        else { item = st[top]; return true; }
    }
};

// 算法3.3：改进的进栈操作（扩容版）
bool arrStack<T>::push(const T item) {
    if (top == mSize - 1) {
        T * newSt = new T[mSize * 2];
        for (i = 0; i <= top; i++) newSt[i] = st[i];
        delete [] st;
        st = newSt;
        mSize *= 2;
    }
    st[++top] = item;
    return true;
}
```

## 二、缺陷清单与证据

### 缺陷 1（致命）：代码3.2 按印刷原样**编译不过**

`int top;` 与 `bool top(T & item);` 在同一个类里重名。

```console
$ g++ -std=c++17 -c legacy.cpp
legacy.cpp:65:5: error: ‘bool arrStack<T>::top(T&)’ conflicts with a previous declaration
legacy.cpp:19:9: note: previous declaration ‘int arrStack<T>::top’
   19 |     int top;
```

**这不是 OCR 的锅**——两处名字在书里印得清清楚楚。也就是说这份教材代码
从未被编译器验证过。现代实现里栈顶位置是私有的 `top_index_`，公开接口是 `top()`——D-001 第 4 条把「彻底消除成员变量与成员函数重名」定成了红线，起因就是这一处。

### 缺陷 2（致命）：算法3.3 按印刷原样**编译不过**

扩容循环里的 `i` 从未声明（原书正文的 `for (int i = 0; ...)` 只出现在 push 之外的段落）。

```console
$ g++ -std=c++17 -Wall -Wextra -c alg33.cpp
alg33.cpp:9:18: error: ‘i’ was not declared in this scope
    9 |             for (i = 0; i <= topIdx; i++)
```

### 缺陷 3（未定义行为）：无参构造留下未初始化成员

`arrStack() { top = -1; }` 没给 `mSize` 和 `st` 赋值。之后：

- `push` 读 `mSize`（不确定值）——实测某次运行里 `mSize` 恰好是 0，
  于是这个空栈立刻报「栈满溢出」，**一个元素也存不进去**；
- 析构 `delete [] st` 释放一个不确定指针。

```console
$ ./drv3          # 先让另一个 arrStack 把这段栈内存弄脏，再默认构造一个
栈满溢出
```

值得注意的是 **`-Wall -Wextra -Wpedantic` 一句警告都不给**：编译器不追踪构造函数
是否初始化了全部成员。靠「开警告」发现不了这类问题，只能靠写测试去用它。
现代实现里成员都有默认初始化器，默认构造出来的是一个容量为 0、**可用可析构**的空栈。

### 缺陷 4（未定义行为）：违反三/五法则 → 二次释放

只有析构函数，没有拷贝构造和拷贝赋值。编译器生成的浅拷贝让两个对象持有同一根 `st`：

```console
$ cat drv2.cpp
int main(){ arrStack<int> a(4); a.push(7); arrStack<int> b = a; }
$ g++ -std=c++17 -fsanitize=address -g drv2.cpp -o drv2 && ./drv2
==2331703==ERROR: AddressSanitizer: attempting double-free on 0x502000000010 in thread T0:
    #0 operator delete[](void*)
    #1 arrStack<int>::~arrStack() legacy2_body.h:32
```

对应 `test.cpp::test_copy_is_deep`。

### 缺陷 5：`class Stack` 是个假的抽象基类

成员函数既非纯虚也无定义，析构函数不是 virtual。`arrStack` 继承它不会得到任何
多态能力，反而埋下「通过 `Stack<T>*` 删除派生对象」的未定义行为。
现代实现直接去掉了这层继承。C++17 里表达「T 要满足什么」的直接工具是
`static_assert` + `<type_traits>`：同样是编译期检查、同样不付虚表代价，
错误信息还停在实例化处。见 `modern.hpp` 顶部的三条 `static_assert`。
（C++20 的 concept 更好用，但按 D-001 本项目定在 C++17。）

### 缺陷 6：`bool pop(T & item)` 双通道返回

用出参带值、用 `bool` 带成败。调用方很容易忽略返回值而读到未修改的 `item`。
现代实现返回 `std::optional<T>`，并加 `[[nodiscard]]`：忽略返回值直接编译告警
（本项目 `-Werror`，即编译失败）。

### 缺陷 7：容器里做 I/O

`cout << "栈满溢出"` 把一个数据结构和 `std::cout` 焊死：库里没法用、
中文提示没法本地化、失败路径没法测试。现代实现全程不碰 I/O，
`test.cpp::test_no_console_output` 会把 `cout`/`cerr` 重定向并断言其为空。

### 缺陷 8：`const T item` 按值传参

顶层 `const` 对调用方毫无意义，却强制了一次拷贝；`std::unique_ptr` 这类
move-only 类型根本传不进去。现代实现提供 `push(const T&)` 与 `push(T&&)` 两个重载，
`test.cpp::test_move_only_element` 用 `ArrayStack<std::unique_ptr<int>>` 守住这一点。

### 缺陷 9：`int` 当下标与容量

有符号溢出是未定义行为，与标准库容器的 `size_type` 也不兼容。
现代实现统一用 `std::size_t`，并在翻倍前检查溢出（按 D-001 第 3 条抛 `std::overflow_error`）。

### 缺陷 10：算法3.3 的扩容没有异常安全

原书先 `delete [] st` 再 `st = newSt`。若元素的拷贝赋值在搬迁途中抛异常，
栈就停在一个半旧半新的状态。现代实现在新缓冲区上搬完才换指针（强异常保证），
并用 `std::move_if_noexcept` 决定搬迁时移动还是拷贝；搬迁失败时用 `try/catch`
把新缓冲区收掉再重新抛出——**用裸指针就必须自己写这一段，RAII 版本不用**，
这本身就是这一节值得讲的地方。

这条不是推理，是**故障注入实测**：`test.cpp::test_strong_exception_guarantee_on_growth`
用一个「第 3 次拷贝赋值必抛」的 `Fragile` 类型触发扩容失败，断言长度、容量、
每个元素逐个不变，且栈之后仍可继续使用。把「先建后换」改回「先释放旧的」，
Debug 档 UBSan 立刻报 `reference binding to null pointer`，Release 档直接段错误。

## 三、刻意保留的东西

现代化**不是**把它换成 `std::stack` 的薄封装：

- 仍然是手写的、基于数组的栈，仍然自己管缓冲区；
- 扩容仍是算法3.3 的「满了翻倍」，摊还 O(1) 的教学点原样保留；
- `clear()` 仍然只把长度归零、留着容量复用，与原书语义一致。

## 四、已知欠账

`new T[capacity]` 会把容量内的所有槽位**默认构造**出来，因此 `T` 必须可默认构造
（`modern.hpp` 顶部的 `static_assert` 把这条限制显式写了出来）。
这一点和原书 `new T[mSize]` 完全相同，属于**没有恶化、也没有解决**。
真正的容器做法是申请未初始化存储 + placement new，只在槽位真正被使用时构造对象。
这条记在 `collab/PLAN.md` 的 **T-004**，不在本单元里悄悄带过。

~~另一条小欠账：`top()` 返回副本，对 move-only 元素不可用。~~
**2026-08-12 已销账**：人拍板补充 D-001 第 3b 条，新增
`const T* peek() const noexcept`——零拷贝、空栈返回 `nullptr`、move-only 元素可用，
代价是返回的指针在下一次 `push`/`pop`/`clear` 之后失效。两个接口各司其职，
两种代价都写在了接口注释与书稿正文里。守门用例见 `test.cpp::test_peek_does_not_copy`
（`Counted` 计拷贝次数：peek 必须为 0、top 必须 ≥ 1）。

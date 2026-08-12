# 原书写法 → 问题 → 现代写法：顺序表

覆盖清单：**代码2.1**（线性表抽象数据类型）、**代码2.2**（顺序表类定义）、
**算法2.3**（按值查找）、**算法2.4**（插入）、**算法2.5**（删除）。
原文见 `dsa_raw.md:1187`、`1276`、`1319`、`1359`、`1401`。

> 本文件是「证据」，不是「观点」。每条缺陷都附了可复现的命令与真实输出。
>
> 现代化取值遵循 `collab/DECISION_LOG.md`：D-001 风格公约（C++17、不拿 STL 容器替换、
> 存储结构属教学内容故用裸指针 + 显式五法则、容器内零 I/O、越界抛标准异常）、
> D-005（扩容搬迁判据落在「移动赋值是否 noexcept」）。

## 一、原书清单（已修复 OCR 损伤，逻辑一字未改）

```cpp
template <class T>                  // 代码2.1：线性表抽象数据类型
class List {
    void clear();
    bool isEmpty();
    bool append(const T value);
    bool insert(const int p, const T value);
    bool delete(const int p);
    bool getValue(const int p, T& value);
    bool setValue(const int p, const T value);
    bool getPos(int &p, const T value);
};

template <class T>                  // 代码2.2：顺序表类定义
class arrList : public List<T> {
private:
    T * aList;
    int maxSize;
    int curLen;
    int position;                   // 当前处理位置
public:
    arrList(const int size) { maxSize = size; aList = new T[maxSize]; curLen = position = 0; }
    ~arrList() { delete [] aList; }
    void clear() { delete [] aList; curLen = position = 0; aList = new T[maxSize]; }
    int length();
    bool append(const T value);
    bool insert(const int p, const T value);
    bool delete(const int p);
    bool setValue(const int p, const T value);
    bool getValue(const int p, T& value);
    bool getPos(int &p, const T value);
};

template <class T>                  // 算法2.3：按值查找
bool arrList<T>::getPos(int &p, const T value) {
    int i;
    for (i = 0; i < n; i++)
        if (value == aList[i]) { p = i; return true; }
    return false;
}

template <class T>                  // 算法2.4：插入
bool arrList<T>::insert(const int p, const T value) {
    int i;
    if (curLen >= maxSize) { cout << "The list is overflow" << endl; return false; }
    if (p < 0 || p > curLen) { cout << "Insertion point is illegal" << endl; return false; }
    for (i = curLen; i > p; i--)
        aList[i] = aList[i-1];
    aList[p] = value;
    curLen++;
    return true;
}

template <class T>                  // 算法2.5：删除
bool arrList<T>::delete(const int p) {
    int i;
    if (curLen <= 0) { cout << "No element to delete\n" << endl; return false; }
    if (p < 0 || p > curLen - 1) { cout << "deletion is illegal\n" << endl; return false; }
    for (i = p; i < curLen - 1; i++)
        aList[i] = aList[i+1];
    curLen--;
    return true;
}
```

## 二、缺陷清单与证据

### 缺陷 1（致命）：`delete` 是关键字，不能当成员函数名

代码2.1 与代码2.2 都声明了 `bool delete(const int p);`。

```console
$ g++ -std=c++17 -c ch2_a.cpp
ch2_a.cpp:8:10: error: expected unqualified-id before ‘delete’
    8 |     bool delete(const int p);
      |          ^~~~~~
```

**这不是 OCR 的锅**——`delete` 在书里印得清清楚楚，算法2.5 的定义处也写作
`bool arrList<T>::delete(const int p)`。整章的删除操作都建立在一个编译不过的名字上。
现代实现叫 `remove()`。

### 缺陷 2（致命）：算法2.3 的循环上界 `n` 从未声明

```console
$ g++ -std=c++17 -c ch2_b.cpp
ch2_b.cpp: In member function ‘bool arrList<T>::getPos(int&, T)’:
ch2_b.cpp:8:25: error: ‘n’ was not declared in this scope
    8 |         for (i = 0; i < n; i++)
      |                         ^
```

按上下文应当是 `curLen`。与第 3 章算法3.3 的 `i` 未声明是同一类错误：
这些清单从未被编译器验证过。

### 缺陷 3（致命）：代码2.1 的 `class List` 没写 `public:`

`class` 的默认访问权限是 private，而代码2.1 通篇没有访问说明符——
**这个抽象数据类型的每一个运算都是私有的**，谁也调不到。

```console
$ g++ -std=c++17 -c ch2_c.cpp
ch2_c.cpp: In function ‘int main()’:
ch2_c.cpp:6:34: error: ‘void List<T>::clear() [with T = int]’ is private within this context
    6 | int main() { List<int> l; l.clear(); }
      |                           ~~~~~~~^~
```

对照第 3 章的代码3.1，那里是写了 `public:` 的。同一本书里两处体例不一致，
更能说明这些清单没有被真正编译过。

### 缺陷 4（未定义行为）：违反三/五法则 → 二次释放

`arrList` 有析构函数 `delete[] aList`，却没有拷贝构造与拷贝赋值。
与第 3 章 `arrStack` 完全相同的错误，实测复现见
`../../ch03/array_stack/legacy.md` 缺陷 4（ASan 报 double-free）。
对应用例 `test.cpp::test_copy_is_deep`。

### 缺陷 5：`clear()` 会释放后重新分配，且不是异常安全的

```cpp
void clear() { delete [] aList; curLen = position = 0; aList = new T[maxSize]; }
```

两个问题：一是没必要——把长度归零即可，容量留着复用；二是若 `new` 抛异常，
对象就停在「`aList` 已被释放、`curLen` 已归零」的破碎状态，之后析构再 `delete[]`
一次已释放的指针。现代实现的 `clear()` 是 `noexcept` 的，只把 `size_` 归零。

### 缺陷 6：容器里做 I/O

插入溢出打印 `"The list is overflow"`，位置非法打印 `"Insertion point is illegal"`，
删除空表打印 `"No element to delete\n"`。与第 3 章同一个毛病：数据结构和
`std::cout` 焊死，库里没法用，失败路径没法测。
`test.cpp::test_no_console_output` 重定向 `cout`/`cerr` 并断言其为空。

### 缺陷 7：「出参 + bool」双通道返回

`getPos(int& p, const T value)`、`getValue(const int p, T& value)` 都是这个形状。
调用方忘了检查返回值，读到的就是没被写过的出参。现代实现：
`find()` 返回 `std::optional<size_type>`，`at()` 直接返回引用、越界抛
`std::out_of_range`。

### 缺陷 8：`const T value` 按值传参

与第 3 章缺陷 8 相同：顶层 `const` 对调用方无意义却强制一次拷贝，
`std::unique_ptr` 这类只能移动的类型根本传不进去。
现代实现提供 `const T&` 与 `T&&` 两个重载，`test_move_only_element` 守住这一点。

### 缺陷 9：`int` 当下标与长度

`p < 0` 这类检查正是因为下标可以为负才需要。现代实现统一 `std::size_t`，
「负下标」在类型层面就不存在了，越界检查只剩一条 `index >= size_`。

### 缺陷 10（设计）：`position` 游标住在容器里

代码2.2 有个 `int position` 成员，配合正文提到的 `setPos/setStart/next/prev`
用来「依次处理元素」。把遍历状态放进容器有三个后果：

- `const` 对象没法遍历（游标要改）；
- 两处代码不能同时遍历同一个表；
- 嵌套遍历直接互相踩。

现代实现删掉了这个成员，改为提供 `begin()/end()`（裸指针当迭代器），
遍历状态回到调用方。`test_range_for_and_const_iteration` 三条断言分别对着上面三点。
顺带一提，原书那个 `position` 在展示的所有算法里**一次都没被用到**。

### 缺陷 11：固定容量，满了就拒绝

原书 `insert` 遇到 `curLen >= maxSize` 打印一行然后返回 false。
现代实现按第 3 章算法3.3 的策略自动翻倍，并沿用 D-005 的搬迁判据。
**插入仍是 O(n)**——扩容不改变这一点，顺序表与链表的对比依据没有被动过。

## 三、刻意保留的东西

- 仍然是连续存储、按下标 O(1) 随机存取；
- 插入/删除仍然要搬动 O(n) 个元素，一步没省；
- `clear()` 仍然保留已分配容量（语义与原书一致，只是不再重新分配）。

这三条正是 2.3 节拿顺序表和链表做对比的全部依据，一个都不能优化掉。

## 四、已知欠账

与第 3 章相同：`new T[capacity]` 会默认构造整块槽位，因此 `T` 必须可默认构造
（已写成 `static_assert`）。真正的容器做法是未初始化存储 + placement new，
记在 `collab/PLAN.md` 的 **T-004**。

另：本单元的 `ensure_capacity()` 与第 3 章 `ArrayStack` 的那份是**手写的两份**，
不是共享代码。这是有意的——每章的容器要能独立阅读，那是教学内容本身。
代价是两份可能漂移，对冲手段是两边各有一条守门用例
（`test_growth_moves_when_move_assignment_is_noexcept`），判据变了任一边都会红。

# 原书写法 → 问题 → 现代写法：链表

覆盖清单：**代码2.6**（单链结点）、**代码2.7**（单链表类型）、
**算法2.8**（构造/析构）、**算法2.9**（定位）、**算法2.10**（插入）、
**算法2.11**（删除）、**代码2.12**（双链结点）。原文范围：`dsa_raw.md:1450-1660`。

### 算法2.11 的 OCR 边界（T-008，2026-08-13 定）

清单在 `dsa_raw.md:1595` 开，**没有** `【算法2.11结束】`。
代码围栏从 1597 行的 ` ```c ` 起到 1617 行的闭合围栏；1616 是 `return true;`，
之后没有函数的闭合 `}`（OCR 把若干 `}` 认成了 `1`）。
1619 行起是解释「定位代价为 O(n)」的正文，1621 是图 2.8，1626 是新小节「2.3.2 双链表」。
因此切片收到 **1617 行围栏结束**：之后已不是本算法的语句。此判定不改 `dsa_raw.md`。

> OCR 底稿在这七条中有大量可见损伤（`cosnt`、`InkList`、`1` 代替 `}`、缺失花括号
> 和结束标记）。下文把这类损伤标为 OCR，不把它误说成原书设计错误；能够从清晰原文
> 复现的 C++ 错误和内存所有权问题另列证据。

## 一、逐条核对

| 清单 | 原书要讲什么 | 核对结论 | 现代落点 |
| --- | --- | --- | --- |
| 代码2.6 | `data + next` 自引用单链结点 | `const Link*` 传给 `Link* next` 是 const 错配；构造函数闭合符是 OCR 损伤 | `SinglyLink<T>` |
| 代码2.7 | 头、尾指针和 `setPos` 的单链表接口 | `delete` 是 C++ 关键字；`cosnt`/`public : )ublic :` 是 OCR 损伤 | `LinkedList<T>` |
| 算法2.8 | 带头结点的构造与逐结点析构 | 主体思路正确；模板作用域与花括号在 OCR 中断裂 | 默认构造 + `clear()` / 析构 |
| 算法2.9 | 从头结点循链定位 | `new Link(head->next)` 为每次定位额外分配结点且无释放，是内存泄漏；其余符号损伤属 OCR | `predecessor_at` / `node_at` |
| 算法2.10 | 定位前驱后改两条链接 | 缺失 `}` 是 OCR；非法位置打印 I/O、`bool + cout` 是接口问题 | `insert` |
| 算法2.11 | 摘除结点、删尾时修 `tail` | `delete` 关键字、`NULL:`、缺失 return/花括号都有问题；接口仍耦合 I/O | `remove` |
| 代码2.12 | `data + prev + next` 双链结点 | 构造函数闭合符缺失是 OCR；保留双向链接概念 | `DoublyLink<T>` |

## 二、可复现证据

### 缺陷 1（致命）：`delete` 不能作为成员函数名

代码2.7 与算法2.11 均清晰使用 `delete`。这不是 OCR 才产生的空白或符号问题。

```console
$ g++ -std=c++17 -c delete_keyword.cpp
delete_keyword.cpp:1:49: error: expected member name or ';' after declaration specifiers
template <class T> class lnkList { public: bool delete(const int p); };
                                           ~~~~ ^
```

现代接口命名为 `remove(size_type)`，返回被删除的元素；位置非法是
`std::out_of_range`，不会向标准输出写提示。

### 缺陷 2（致命）：代码2.6 的 const 指针不能赋给可写链接

原文构造函数参数是 `const Link<T>* nextValue`，但 `next` 成员类型是 `Link<T>*`，
随后直接赋值。实例化就会失败：

```console
$ g++ -std=c++17 -c const_pointer.cpp
error: assigning to 'Link<int> *' from 'const Link<int> *' discards qualifiers
```

现代 `SinglyLink` 和 `DoublyLink` 的链接参数均为可写的同型指针；容器自身不暴露其
私有 Node，调用方不能绕过不变量改链接。

### 缺陷 3（内存泄漏）：算法2.9 用 `new` 复制首结点来做定位

原文是 `Link<T>* p = new Link<T>(head->next)`，随后只令 `p = p->next`；最初
分配的结点既不属于链表，也从未 `delete`。每次 `setPos(0)` 都至少泄漏一个结点。
定位只需要借用已有指针，根本不应分配：现代 `predecessor_at` 从嵌入的头结点开始循链。

### 缺陷 4（所有权）：析构有了，复制控制却没有

代码2.7 声明析构函数，算法2.8 逐结点 `delete`，却没有复制构造/拷贝赋值。默认浅拷贝
会让两个链表共同删除同一串结点，产生二次释放。现代实现显式 Rule of Five；复制构造
中途若元素复制抛异常，也在构造函数内 `clear()`，因为未构造完成的对象不会自动调用
自身析构函数。`test_copy_constructor_cleans_partial_chain` 守住这一条。

### 缺陷 5（接口）：容器用 `cout` 报错并以 `bool + 出参` 传回结果

算法2.10/2.11 的非法位置向 `cout` 写文本；代码2.7 的读取和查找又用 `bool` 与出参。
现代实现将按值查找表示为 `optional<size_type>`，按位置访问/插入/删除以
`out_of_range` 报错，容器内完全没有 I/O。

## 三、刻意保留与现代边界

- 保留头结点：空表、首结点插入和删除统一经由一个前驱结点处理。
- 保留尾指针：`append` 在 O(1) 时间接在末尾；删尾后必须回退 `tail_`，测试覆盖该不变量。
- 保留循链定位：`at`、`find`、指定位置插入/删除仍是 O(n)，没有伪装成随机存取。
- 代码2.12 只给出双链结点而没有完整双链表操作；本单元也只实现结点类型，不虚构原书未给的
  双链表算法。完整双链表应另列任务与清单认领。

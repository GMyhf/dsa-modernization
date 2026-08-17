# 原书写法 → 问题 → 现代写法：递归与栈空间

覆盖清单：**算法3.6**（阶乘函数，递归）、**算法3.7**（计算阶乘的主程序）、
**算法3.8**（阶乘的迭代实现）、**算法3.9**（阶乘的一种非递归实现，显式栈）。
原文见 `dsa_raw.md:2238`、`2285`、`2312`、`2329`。

> 本文件是「证据」，不是「观点」。每条都附了可复现的命令与真实输出。

## 一、原书清单（已修复 OCR 损伤，逻辑一字未改）

```cpp
long factorial(long n) {              // 算法3.6：递归
    if (n <= 0) return 1;
    return n * factorial(n - 1);
}

// 算法3.7：计算阶乘的主程序（底稿 2285 行，原样，只修 OCR 拆开的空格）
#include <iostream>
void main( ) {
long x;
cin >> x;
cout << factorial(4) << endl;

long factorial(long n) {              // 算法3.8：迭代
    long m = 1;
    long i;
    if (n > 0)
        for (i = 1; i <= n; i++) m = m * i;
    return m;
}

long factorial(long n) {              // 算法3.9：显式栈
    Stack<long> s;
    long tmp;
    long m = 1;
    while (n > 0) s.push(n--);
    while (s.pop(&tmp)) m *= tmp;
    return m;
}
```

## 二、缺陷清单与证据

### 缺陷 1（未定义行为）：不查溢出，从 21! 起静默给出垃圾

三个版本都用 `long`，都不检查溢出。64 位 `long` 装得下的最大阶乘是 20!：

```console
$ g++ -std=c++17 -O0 fact.cpp -o fact && ./fact
sizeof(long) = 8 字节
  原书 factorial(12) = 479001600
  原书 factorial(20) = 2432902008176640000
  原书 factorial(21) = -4249290049419214848
  原书 factorial(25) = 7034535277573963776
  原书 factorial(66) = 0
```

`factorial(21)` **返回负数**，`factorial(66)` **返回 0**——都没有任何提示。

而且这不只是"答案错"。有符号整数溢出在 C++ 里是**未定义行为**：

```console
$ g++ -std=c++17 -O0 -fsanitize=undefined fact.cpp -o factu && ./factu
fact.cpp:5:74: runtime error: signed integer overflow: 21 * 2432902008176640000
               cannot be represented in type 'long int'
```

现代实现改用 `std::uint64_t` 并在入口显式判界，超过 20 抛 `std::overflow_error`。

### 缺陷 1b（编译期硬伤）：算法3.7 的主程序三处编不过

原书这段只有五行，三处都是编译级问题——`void main`、无 `std::` 限定、以及缺右花括号
（最后一处从底稿看是印刷/扫描时就丢了，与前两处性质不同）。原样喂给编译器：

```text
$ g++ -std=c++17 -c a37.cpp
a37.cpp:3:1: error: '::main' must return 'int'
    3 | void main( ) {
      | ^~~~
a37.cpp: In function 'int main()':
a37.cpp:5:1: error: 'cin' was not declared in this scope; did you mean 'std::cin'?
    5 | cin >> x;
      | ^~~
      | std::cin
```

`void main()` 在 C++ 里从来都不合法（ISO C++ 规定 `main` 返回 `int`），
这一点连 2008 年的编译器也该拒绝；它能在教材里活下来，多半是当年在
某些只做警告的编译器上「跑通了」。

还有一处不是编译错误、但更值得讲：**`cin >> x` 读了 `x`，接着调用的却是
`factorial(4)`**——读进来的值从头到尾没用上。原书正文解释这段时说的是
「主程序通过 factorial(4) 这个语句向阶乘函数的形参 n 提供了实参 4」，
可见 4 是有意写死的，那 `cin >> x` 就是一句不该留在清单里的残留。
现代实现把它改成 `factorial_driver(long long n)`，**参数从哪来由调用方决定**，
主程序不再自己读输入——这样它既可测（`test.cpp` 里直接断言
`factorial_driver(4) == 24`），也不会有「读了不用」这种自相矛盾。

### 缺陷 2：负数静默返回 1

算法3.6 的 `if (n <= 0) return 1;` 把所有负数当成递归出口。
`factorial(-5)` 返回 **1**。算法3.8 的 `if (n > 0)` 同理，算法3.9 的 `while (n > 0)` 亦然。
负数没有阶乘，这是定义域错误，现代实现抛 `std::invalid_argument`。

### 缺陷 3（书内自相矛盾）：`s.pop(&tmp)` 与本书自己的栈 ADT 对不上

算法3.9 写的是 `while (s.pop(&tmp))`——传的是**指针**。
而同一章代码3.1 声明的栈 ADT 是：

```cpp
bool pop(T & item);      // 代码3.1，传引用
```

两处签名对不上，算法3.9 按印刷原样配不上本书自己的栈。
（现代实现用本章的 `ArrayStack::pop()`，返回 `std::optional<T>`。）

### 缺陷 4：`Stack<long> s;` 直接实例化了那个假抽象基类

代码3.1 的 `Stack<T>` 成员函数既非纯虚也无定义（见
`../array_stack/legacy.md` 缺陷 5）。`Stack<long> s;` 声明得出来，
一旦调用 `push`/`pop` 就是链接错误。原书这里想写的应该是 `arrStack<long>`。

## 三、刻意保留的东西

- **递归版仍然递归**。3.1.5 节要教的就是「递归吃运行栈」，改成迭代这一节就没了。
- **显式栈版仍然显式**，而且用的就是本章自己的 `ArrayStack`——
  原书 `Stack<long> s;` 正是这个意思。它比迭代版慢，存在的意义是
  **演示编译系统处理递归的机制**，不是为了性能。
- 三个版本都保留，正因为它们互为参照物：测试要求三者对同一输入给出相同答案。

## 四、本书补充的实测：运行栈到底有多"足够大"

原书正文说递归「需要在内存中开辟一个称为运行栈(runtime stack)的**足够大**的动态区」。
多大算足够大？这可以量出来——数字与结论见书稿 3.1.5 节，
`modern.hpp` 里的 `sum_to_recursive` / `sum_to_with_explicit_stack`
是为此补充的一对函数（不对应原书清单，阶乘在 21 就溢出了，深度走不远，演示不了栈深度）。

**最反直觉的一条**：同一份递归源码，`-O0` 与 `-O1+ASan` 在 50 万层崩溃，
而 `-O2` 把递归整个转成了循环，100 万层照样通过——汇编层面确认
`-O2` 下函数体内**零次自调用**。所以「这段递归会不会爆栈」
不是源码单独决定的，是源码 × 编译器 × 优化档共同决定的。

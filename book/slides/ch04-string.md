---
title: 第4章 字符串
subtitle: 数据结构与算法：Python 讲算法，C++ 讲实现
---

# 第4章 字符串

**一类内容受限的线性表**：每个元素都是字符。

两件事要分开：

- **保存字符串的类**：容量、复制、下标边界
- **在文本里找模式的算法**：比较顺序

本章有一处不是写法问题，是**算法结果错**——原书两个匹配算法返回的位置都差 1。

<!-- 备注
最后一句先埋下，讲到 4.3 时兑现。这是本书在这一章最硬的一个发现，
四组数据对拍证实，值得当场演示。
-->

---

# 4.1 基本概念

- **串**：零个或多个字符组成的有限序列
- **长度**：串中字符的数目；长度为 0 的是**空串**
- **子串**：串中任意个连续字符组成的序列

比较两个串的大小：**从左到右逐字符比**，
第一个不同的字符谁大串就谁大；都相同则短的小。

<!-- 备注
可以问一句："abc" 和 "abcd" 谁大？"Z" 和 "a" 谁大？
后一个引出：比的是字符集里的编码值，不是字母表顺序。
-->

---

# 4.1.1 字符编码

字符串的逻辑元素是“字符”，内存中保存的是**编码单元**。

| 编码 | 一个码点占用 | 关键事实 |
| --- | --- | --- |
| ASCII | 1 字节 | 只覆盖 0–127 |
| UTF-8 | 1–4 字节 | 与 ASCII 兼容，长度可变 |

- C++ 的 `char` 是一个字节，不等于一个人眼字符
- `std::string::size()` 返回**字节数**
- 数 Unicode 字符必须解码，不能按字节下标猜

`u8"..."` 表示 UTF-8 字节序列；接口仍是字符串的查找与拼接。

---

# 4.1.2 编码顺序不等于语言顺序

ASCII 中数字、大写字母、小写字母各自连续递增，
但码元顺序不是中文拼音、重音或大小写折叠后的自然语言顺序。

对 `std::string`，比较的是编码单元字典序：

```text
"123" < "1234" < "23"
```

前缀相同时，较短者更小。比较前必须约定编码与规范化方式。

**C++ 陷阱**：两边都是字符串字面量时，`<` 比的是数组地址；
至少一边先转成 `std::string`，才是内容比较。

---

# 4.2 变长存储：这一节的正题

字符串长度会变。所以要**动态分配、按新长度重新开辟、拷贝、释放**。

![图4.3 创建字符串的示意图](../assets/17558c608a970971.jpg)

所以这里是手写的 `char*` 缓冲区，**不是** `std::string`——
换成 `std::string`，这一节就没了。

---

# 空串也要占一个字节

```cpp file=code/ch04/string_class/teaching.hpp#fn:String
// 空串。**注意它仍然申请了 1 个字节**，里面放一个 '\0'。
// 这样 c_str() 永远返回一个合法的 C 字符串，调用方不必先判空指针。
String() : data_(new char[1]), size_(0) {
    data_[0] = '\0';
}

// 从 C 字符串构造。参数是 `const char*` 而不是原书的 `char*`——
// 原书那个签名让它自己书里的例子 `String s1 = "Hello";` 从 C++11 起就编译不过：
// 字符串字面量的类型是 const char[6]，绑不到 char*。
//
// 这里**故意不加 explicit**，为的就是保住 `String s1 = "Hello";` 这种写法。
String(const char* s) {   // NOLINT(google-explicit-constructor)
    if (s == nullptr) {
        throw std::invalid_argument("String: 不能用空指针构造字符串");
    }
    size_ = std::strlen(s);
    data_ = new char[size_ + 1];
    std::memcpy(data_, s, size_ + 1);   // +1 把结尾那个 '\0' 也带上
}

// 三法则：自己管着 new 出来的缓冲区，拷贝必须自己写。
// 原书正文里描述过赋值时「必须释放 s1 的原有空间」，却没有把拷贝构造和
// 拷贝赋值作为清单给出。只有析构没有这两个，一次 `String b = a;` 就是二次释放。
String(const String& other) : data_(new char[other.size_ + 1]), size_(other.size_) {
    std::memcpy(data_, other.data_, size_ + 1);
}
```

<!-- 备注
两处要讲：

1. 空串仍然申请 1 个字节放 '\0'——这样 c_str() 永远返回合法的 C 字符串，
   调用方不必先判空指针。这是个小设计但省掉了一整类 bug。

2. 参数是 const char* 而不是原书的 char*。原书那个签名让**它自己书里的例子**
   `String s1 = "Hello";` 从 C++11 起编译不过：字符串字面量的类型是
   const char[6]，绑不到 char*。GCC 默认降级为警告，本书 -Werror 下就是错误。
-->

---

# 追加：为什么是 O(n)

```cpp file=code/ch04/string_class/teaching.hpp#fn:append
// 在串尾添加一个字符。
//
// **变长存储的代价在这里看得最清楚**：字符串长度变了，就得重新申请一块、
// 把老内容拷过去、再把老的还回去。所以 append 一个字符是 O(n)，不是 O(1)。
//
// 返回自身引用，于是 `s.append('a').append('b')` 可以连着写；
// 原书【代码4.1】声明的是**按值返回**，调用方从签名看不出它改不改本串。
String& append(char c) {
    char* fresh = new char[size_ + 2];        // 老内容 + 新字符 + '\0'
    std::memcpy(fresh, data_, size_);
    fresh[size_] = c;
    fresh[size_ + 1] = '\0';
    delete[] data_;
    data_ = fresh;
    ++size_;
    return *this;
}
```

**变长存储的代价在这里看得最清楚**：长度变了，就得重新申请一块、
把老内容拷过去、再把老的还回去。

所以追加**一个**字符是 O(n)，不是 O(1)。

<!-- 备注
这就是后面章节讨论「预留容量」的动机——std::string 和 vector 都会多要一些，
把摊还代价压到 O(1)。本书的 String 不做这个优化，为的是让代价看得见。

返回 String& 而不是原书的按值返回：调用方从签名一眼看出它改的是本串，
而且 s.append('a').append('b') 可以连着写。
-->

---

# 抽取子串：原书这里会崩

原书【算法4.5】在起始位置越界时写 `return NULL;`。

返回类型是 `String`，所以 `NULL` **不是空串**——
它先转成 `char*`，再走 `String(char*)` 构造函数，于是 `strlen(nullptr)`。

```text
$ ./s7
准备调用 s.Substr(99, 1)
runtime error: null pointer passed as argument 1, which is declared to never be null
AddressSanitizer:DEADLYSIGNAL
ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000
```

**这段能编译，崩在运行期。**

---

# 现代写法：越界就抛

```cpp file=code/ch04/string_class/teaching.hpp#fn:substr
// 【算法4.5】从 pos 开始取长度至多 len 的子串。
//
// 原书在 `pos >= size` 时 `return NULL;`。那不是「返回空串」——
// NULL 会去走 String(char*) 构造函数，然后 strlen(nullptr) 当场段错误。
// 这里越界就抛异常，让错误停在发生的地方。
// pos == size() 是合法的，得到空串（「从末尾取 0 个字符」）。
String substr(size_type pos, size_type len) const {
    if (pos > size_) {
        throw std::out_of_range("String::substr: 起始位置越界");
    }
    size_type available = size_ - pos;
    size_type take = (len < available) ? len : available;   // 原书的 if (n > left) n = left
    String result;
    char* fresh = new char[take + 1];
    std::memcpy(fresh, data_ + pos, take);
    fresh[take] = '\0';
    delete[] result.data_;
    result.data_ = fresh;
    result.size_ = take;
    return result;
}
```

- 越界抛 `std::out_of_range`，让错误停在**发生的地方**
- `pos == size()` 是**合法**的，得到空串
- `len` 超出剩余长度时**截断**，与原书 `if (n > left) n = left;` 一致

---

# 查找与比较

```cpp file=code/ch04/string_class/teaching.hpp#fn:find
// 【算法4.4】从 start 开始查找字符 c。找到返回下标，没找到返回空 optional。
// 原书用 -1 表示没找到——与「位置 0」只差一个符号，忘了判就会读错位置。
std::optional<size_type> find(char c, size_type start = 0) const {
    for (size_type i = start; i < size_; ++i) {
        if (data_[i] == c) {
            return i;
        }
    }
    return std::nullopt;
}
```

原书用 `-1` 表示没找到——**与「位置 0」只差一个符号**，
调用方漏判就会把「没找到」当成「匹配在开头」。

<!-- 备注
这与第 2 章 getPos 那一处是同一个问题、同一个改法。可以回头指一下。

比较那边也有一处：原书自己实现了一个 strcmp，返回值固定 -1/0/1，
并在正文里说「这与 C/C++ 通常的习惯不一致」——
其实不一致的是原书自己：标准 strcmp 返回的就是差值的符号，
调用方本来就只该看符号。
-->

---

# 4.3 模式匹配

在正文 T 里找模式 P 第一次出现的位置。

![图 4.6 朴素匹配的示例](../assets/a01806ddff22b2f2.jpg)

**朴素做法**：把 P 对齐到 T 的每个位置试一遍，
失配就把 P 整体右移一位，从头再比。

---

# 朴素匹配：手动走一遍

```text
文本 T   a b a b c
模式 P   a b c

对齐 0:  a b a b c      比 a=a, b=b, a!=c   失配, P 右移一位
         a b c
对齐 1:  a b a b c      比 b!=a             失配
           a b c
对齐 2:  a b a b c      比 a=a, b=b, c=c    匹配! 返回 2
             a b c
```

每次失配，**文本指针退回去**从下一个位置重来。
最坏 $O(n \times m)$：T 全是 a、P 是 aaab 时，每次都要比到最后一个。

完整实现见 `code/ch04/pattern_matching/modern.hpp`。

---

# 原书这里算错了

原书【算法4.6】和【算法4.8】匹配成功时返回 `j - pLen + 1`。

**在 0 起始下标下，这个值一律差 1。**

```text
文本    abcddabcababcdaabcababcdaabcabaa
模式    abcdaabcab
正确起始下标  10
原书返回      11
```

四组数据对拍证实。本书返回 `j - pLen`。

<!-- 备注
课程 2021 秋的《教材1-6章勘误表》独立发现了同一处（错误编号 10 和 13），
可以作为佐证提一句：这不是我们看错了。

顺带：原书正文里 next 数组比模式还长一位，与图4.11 自相矛盾。
-->

---

# 有没有更好的办法

```text
文本    a b a b c a b c a c b a b
模式    a b a b c a b c a c b a b
              ^ 在这里失配
```

失配时，**前面那一段是已经比对过的**——它的内容我们完全知道。
朴素做法把这份信息全扔了，退回去从头再比。

**KMP 的想法**：利用失配位置**之前**的信息，直接算出该滑多远，
文本指针**永不回退**。

---

# 特征向量 next

对模式 P 的每个位置 i，问：

> `P[0..i-1]` 这一段里，**最长的「既是前缀又是后缀」的长度**是多少？

```text
P        a  b  a  b  c
下标     0  1  2  3  4
next    -1  0  0  1  2
```

`next[3] = 1`：`P[0..2] = "aba"`，前缀 `a` 和后缀 `a` 相同，长度 1。

<!-- 备注
这是全章最难的一页，值得慢讲。
关键在于让学生理解：next[i] 说的是「失配在 i 时，模式该退到哪个位置继续比」，
而不是「退多少位」。
-->

---

# 计算 next：用模式串跟自己匹配

```text
P      a  b  a  b  c
i      0  1  2  3  4
next  -1  0  0  1  2
```

- `next[2] = 0`：`P[0..1] = "ab"`，没有相同的前后缀
- `next[3] = 1`：`P[0..2] = "aba"`，前缀 a 和后缀 a 相同
- `next[4] = 2`：`P[0..3] = "abab"`，前缀 ab 和后缀 ab 相同

**代价 $O(m)$**：i 只增不减，k 每次至多增 1，
所以总回退次数不超过总前进次数。

<!-- 备注
这段代码本身就是一个 KMP——用模式串跟自己匹配，非常值得指出来。
原书【算法4.7】的 while 条件是 i<m 并在循环里加了 if(i==m) break；
课程勘误表指出应为 i<m-1，那句 break 是多余的。
完整实现见 code/ch04/pattern_matching/modern.hpp。
-->

---

# KMP：失配时模式退，文本不退

![图 4.12 KMP 匹配示例](../assets/4f9aa9617b2ddc56.jpg)

失配在模式的第 j 位时：

```text
朴素   j = 0        文本指针 i 退回到本次对齐的下一位
KMP    j = next[j]  文本指针 i 一步都不退
```

**「文本指针永不回退」是全部的关键**——i 只增不减，所以是 $O(n + m)$。

完整实现见 `code/ch04/pattern_matching/modern.hpp`。

---

# 朴素与 KMP：代价对照

| | 朴素 | KMP |
| --- | --- | --- |
| 预处理 | 无 | $O(m)$ 建 next |
| 最好 | $O(n)$ | $O(n)$ |
| 最坏 | $O(n \times m)$ | $O(n + m)$ |
| 文本指针 | 会回退 | **永不回退** |
| 额外空间 | 无 | $O(m)$ |

**「文本指针永不回退」不只是快**——它让 KMP 能处理**流式**输入：
文本可以边读边匹配，不需要缓存回退。

---

---

# 课堂讲解卡：字符串是带边界的序列

字符串问题先处理编码、长度和边界，再讨论匹配算法。空串、非 ASCII 字符和越界输入不能留到最后才补。

---

# 课堂例题：模式 `ABABC` 在文本中的匹配

朴素算法失配后把文本指针退回；KMP 使用已经匹配的前缀信息，只让模式串退，文本指针不退。
课堂上逐字符标出 `next`，让学生说出每次失配后模式串应退到哪里。

---

---

# 课堂例题答案：`ABABC` 的回退

模式前缀中最长相等真前缀/后缀长度为 `0,0,1,2,0`（按实现约定可能整体右移一格）。失配时文本指针不退，模式串按表回退，因此匹配阶段为线性扫描。

---

# 课末自检

- 字节数、字符数和显示宽度是否混为一谈？
- 子串下标越界时接口给出什么行为？
- `next` 表示的到底是前缀长度还是回退位置？
- 能否说明 KMP 为什么是 O(n+m)？

---

---

# 课末自检参考答案

- 字节数、字符数、显示宽度是不同层次，UTF-8 下不能混用。\n- 子串越界应抛异常或返回明确错误状态。\n- `next` 的含义必须以实现约定为准。\n- KMP 预处理 O(m)，匹配 O(n)，合计 O(n+m)。

---

# 本章小结

- 字符串是元素受限的线性表；本章正题是**变长存储管理**
- 长度一变就要重新申请、拷贝、释放 → 追加一个字符是 O(n)
- 原书三处硬伤：`String(char*)` 让书里自己的例子编译不过；
  `Substr` 越界 `return NULL` 运行期崩；两个匹配算法返回值差 1
- `find` 用 `optional` 而不是 `-1`——与第 2 章同一条口径
- KMP 的核心是**利用失配前已知的信息**，文本指针永不回退
- 代价从 $O(n \times m)$ 降到 $O(n + m)$，换来 $O(m)$ 的 next 表

---

# 上机

```bash
python3 tools/check_code.py code/ch04/pattern_matching
```

- 用图 4.12 那对串跑一遍，确认返回 10 而不是 11
- 把 KMP 失配时的 `j = next[j]` 改成 `j = 0`，看结果和耗时怎么变
- 造一组最坏输入（`aaaa...a` 找 `aaab`），比较两个算法的比较次数

> 测试里有 3000 组随机对拍：朴素和 KMP 必须给出同样的答案。

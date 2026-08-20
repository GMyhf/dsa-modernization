# 第4章 字符串

字符串是字符的有限序列。先区分「保存字符串」的类与「在文本中找模式」的算法：前者处理容量、
复制和下标边界，后者处理比较顺序。朴素匹配在失配后移动模式；KMP 预先计算 `next` 信息，避免
重复比较已经知道相等的前缀。

源码：[字符串类·教学版](../code/ch04/string_class/teaching.hpp)、
[字符串类·工程版](../code/ch04/string_class/modern.hpp)、
[字符串示例](../code/ch04/string_class/demo.cpp)、
[模式匹配](../code/ch04/pattern_matching/modern.hpp)、
[匹配示例](../code/ch04/pattern_matching/demo.cpp)。

## 先跑一遍

```cpp file=code/ch04/string_class/demo.cpp
// 第 4 章「先跑一遍」：用教学版 String 走一遍 append / substr / find。
// 编译运行：
//   g++ -std=c++17 -I code/ch04/string_class code/ch04/string_class/demo.cpp -o demo && ./demo
#include "teaching.hpp"

#include <iostream>

int main() {
    String text = "Hello";                       // 隐式转换：String(const char*)
    text.append(' ').append('C').append('+').append('+');
    std::cout << "拼接后: " << text.c_str() << '\n';

    const String slice = text.substr(6, 3);
    std::cout << "子串: " << slice.c_str() << '\n';

    // find 返回 optional：有值才解引用
    std::cout << "首次出现 C 的下标: ";
    if (const auto found = text.find('C')) {
        std::cout << *found << '\n';
    } else {
        std::cout << "无\n";
    }
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch04/string_class \
    code/ch04/string_class/demo.cpp -o /tmp/str-demo
/tmp/str-demo
```

```console
拼接后: Hello C++
子串: C++
首次出现 C 的下标: 6
```

缓冲区仍是手写的 `char*`，换成 `std::string` 这一节就没了。`find` 找不到时返回空 optional，不用 `-1` 和位置 0 抢同一个数字。

图4.12 自己的那对串，原书两个匹配算法都返回 11，正确答案是 10：

```cpp file=code/ch04/pattern_matching/demo.cpp
#include "modern.hpp"

#include <iostream>

int main() {
    const char* text = "abcddabcababcdaabcababcdaabcabaa";
    const char* pattern = "abcdaabcab";
    const auto naive = dsa::naive_search(text, pattern);
    const auto kmp = dsa::kmp_search(text, pattern);
    std::cout << "图4.12 的串，正确起始下标是 10\n";
    std::cout << "朴素: " << (naive ? static_cast<long>(*naive) : -1) << '\n';
    std::cout << "KMP:  " << (kmp ? static_cast<long>(*kmp) : -1) << '\n';
    std::cout << "原书返回 11，一律差 1\n";
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch04/pattern_matching \
    code/ch04/pattern_matching/demo.cpp -o /tmp/kmp-demo
/tmp/kmp-demo
```

```console
图4.12 的串，正确起始下标是 10
朴素: 10
KMP:  10
原书返回 11，一律差 1
```

> **本文件的地位**：《数据结构与算法》（张铭、王腾蛟、赵海燕，高等教育出版社 2008）
> 第 4 章的现代化重排（4.1 字符串概念、4.2 存储结构与实现、4.3 模式匹配）。
>
> 书里印的每一段 C++ 都来自 `code/ch04/string_class/` 与 `code/ch04/pattern_matching/`，
> 真正编译、真正跑过（`tools/check_doc.py` 的 R3 逐字核对）。原书那份写法错在哪、
> 改了什么、什么刻意没改，逐条记在两个单元各自的 `legacy.md`。
>
> **本章有一处不是写法问题，是算法结果错**：原书两个模式匹配算法返回的位置都差 1。
> 详见 4.3.1 节末。

## 4.1 字符串的基本概念

字符串(string)是由零个或多个字符组成的有限序列，是一种特殊的线性表——
它的每个元素都是字符。串中字符的数目称为串的长度，长度为零的串称为空串。

原书【代码4.1】给出字符串的抽象数据类型。那份声明有三处今天必须改：

1. **类名是小写的 `string`。** 在任何 `using namespace std;` 的翻译单元里，
   它与 `std::string` 构成歧义。原书正文随后又改用大写 `String`，
   同一章里两个名字混用。
2. **`int isEmpty();`** 用 `int` 表达布尔，**`int find(const char c, const int start);`**
   用 `-1` 表示"没找到"——与"位置 0"只差一个符号，调用方漏判就会把
   "没找到"当成"匹配在开头"。
3. **修改器按值返回**：`string append(const char c);`、`string concatenate(const char* s);`
   都返回 `string` 而非引用。代码4.1 只有声明没有函数体，所以不能断定原书会丢结果；
   **能断定的是签名含混**——调用方无从判断 `s.append('x');` 是改了 `s`，
   还是返回了一个新串而 `s` 原封不动。

本书的 `String` 把这三处分别改为：类名大写、`bool empty()`、
`std::optional<size_type> find(...)`、修改器返回 `String&`。

### 4.1.1 字符编码

字符串的逻辑元素是字符，内存中却保存编码单元。ASCII 为 0–127 的单字节编码，UTF-8 则以 1～4 个字节编码一个 Unicode 码点；因此 C++ 的 `char` 是一个字节，不等于一个“人眼看到的字符”。`std::string::size()` 返回字节数，不能用来数 Unicode 字符；需要按 UTF-8 解码或使用明确的 Unicode 库。字面量 `u8"..."` 表示 UTF-8 字节序列，编码本身不会改变字符串查找和拼接的接口语义。

### 4.1.2 字符的编码顺序

ASCII 保留了数字 `0`–`9`、大写字母和小写字母各自连续递增的区间，所以同一区间内可用编码值比较。但这不是通用的语言学排序规则：字符的编码值比较是按码元的字典序，大小写、重音、中文排序都可能需要额外的 locale 或排序键。字符串比较先比较第一个不同的编码单元；若一个是另一个的前缀，较短者更小。因此对 `std::string` 而言 `"123" < "1234" < "23"`，但这不是整数大小比较。比较前必须先约定编码和规范化方式，不能把“字节序”误当成“自然语言顺序”。

**这里有个 C++ 特有的坑，必须点破**：上面那条结论只对 `std::string` 成立。两个**字符串字面量**之间写 `<`，比的是两个数组的**地址**，不是内容：

```text
$ g++ -std=c++17 -Wall -Wextra litcmp.cpp
litcmp.cpp:7:25: warning: comparison between two arrays [-Warray-compare]
    7 |     std::cout << ("123" < "1234") << ' ' << ("1234" < "23") << '\n';
      |                   ~~~~~~^~~~~~~~
$ ./litcmp
true true      # ("123" < std::string("1234"))、(std::string("1234") < "23")
false true     # ("123" < "1234")、("1234" < "23")——第一个的答案反了
```

同一行代码，两边只要有一边是 `std::string`，比的就是内容；两边都是裸字面量，比的就是地址，而地址由编译器摆放决定。所以本书凡是比较字符串，一律先落到 `String`/`std::string` 上，再谈编码顺序。

## 4.2 字符串的存储结构和实现

### 为什么这一节没有 Python 版

本节的对象是一个自管理的字符缓冲区：长度、容量、结尾空字符、深复制和移动后的源对象状态都属于接口契约。Python `str` 是不可变的运行时对象，`list` 也不会让读者实现缓冲区的分配、释放和强异常保证；把它们当作“字符串类的 Python 版”会跳过本节的存储布局和所有权问题。因此算法 4.3 的模式匹配有 Python 版，4.2 的字符串类只保留 C++ 实现。

`String` 采用动态变长的存储结构：内部持有一块以 `'\0'` 结尾的字符数组和当前长度，
构造时按初值长度分配，赋值时按新长度重新分配。这正是本节要教的内容，
所以缓冲区是**裸 `char*`**——换成 `std::string` 这一节就没了。

### 4.2.1 字符串的顺序存储

字符串用一段连续字符数组存储，末尾保留 `\0`，并单独记录不含终止符的长度。
下标访问是 O(1)，插入、拼接和抽取需要复制字符。

### 4.2.2 字符串类 class String 的存储结构

顺序存储只说了「字符放在哪」，还没说「这块内存归谁、什么时候还」。
把它包成一个类之后，这两件事才有着落：类里存一个指向字符数组的指针和一个长度，
构造时申请、析构时释放、拷贝时另开一份——本节的教学内容就是这套**变长存储管理**。

#### 教学版：完整实现

下面是一份**完整的、能直接编译运行的**字符串类。一个文件、一个类。
后面 4.2.2、4.2.3 两节就是把它拆开逐段讲。

```cpp file=code/ch04/string_class/teaching.hpp
// 字符串类 String —— 教学版。原书【代码4.1】【算法4.3】【算法4.4】【算法4.5】。
//
// 一个文件、一个类、能直接编译运行，给「第一次读这一节」的人看。
//
// 本节的教学内容是**字符串的变长存储管理**：动态分配、按长度重新开辟、拷贝与释放。
// 所以这里是手写的 char* 缓冲区，不是 std::string——换成 std::string，这一节就没了。
//
// 与 modern.hpp（工程版）的分工：
//   教学版  三法则（析构 + 拷贝构造 + 拷贝赋值）；
//   工程版  补齐移动语义（移动之后 data_ 为空，读取路径要多一层 raw() 兜底）、
//           比较运算符全家、copy-and-swap。
// 两份都在闸门里真编译真运行。先读这一份，4.2a「进阶（选读）」再读那一份。
#pragma once

#include <cstddef>
#include <cstring>
#include <optional>
#include <stdexcept>

class String {
public:
    using size_type = std::size_t;

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

    ~String() { delete[] data_; }

    // 三法则：自己管着 new 出来的缓冲区，拷贝必须自己写。
    // 原书正文里描述过赋值时「必须释放 s1 的原有空间」，却没有把拷贝构造和
    // 拷贝赋值作为清单给出。只有析构没有这两个，一次 `String b = a;` 就是二次释放。
    String(const String& other) : data_(new char[other.size_ + 1]), size_(other.size_) {
        std::memcpy(data_, other.data_, size_ + 1);
    }

    String& operator=(const String& other) {
        if (this == &other) {
            return *this;
        }
        char* fresh = new char[other.size_ + 1];   // 先备好新的
        std::memcpy(fresh, other.data_, other.size_ + 1);
        delete[] data_;                            // 再释放旧的
        data_ = fresh;
        size_ = other.size_;
        return *this;
    }

    size_type size() const { return size_; }
    size_type length() const { return size_; }
    bool empty() const { return size_ == 0; }
    const char* c_str() const { return data_; }

    void clear() {
        char* fresh = new char[1];
        fresh[0] = '\0';
        delete[] data_;
        data_ = fresh;
        size_ = 0;
    }

    // 按下标取字符。越界抛异常，不是返回一个随便什么值。
    char at(size_type index) const {
        if (index >= size_) {
            throw std::out_of_range("String::at: 下标越界");
        }
        return data_[index];
    }

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

    // 把 s 接在本串后面。同样是「重新申请、拷两段、释放旧的」。
    String& concatenate(const char* s) {
        if (s == nullptr) {
            throw std::invalid_argument("String::concatenate: 空指针");
        }
        size_type extra = std::strlen(s);
        char* fresh = new char[size_ + extra + 1];
        std::memcpy(fresh, data_, size_);
        std::memcpy(fresh + size_, s, extra + 1);
        delete[] data_;
        data_ = fresh;
        size_ += extra;
        return *this;
    }

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

    // 【算法4.3】三路比较：负 / 零 / 正 表示 小于 / 等于 / 大于。
    //
    // 原书自己实现了一个 strcmp，返回值固定为 -1/0/1，并在正文里说
    // 「这与 C/C++ 语言中通常的大小比较习惯不一致」——其实不一致的是原书自己：
    // 标准 strcmp 返回的就是差值的符号，调用方只该看符号，不该看具体数值。
    int compare(const String& other) const {
        return std::strcmp(data_, other.data_);
    }

private:
    char* data_;       // 以 '\0' 结尾的字符数组，永远非空
    size_type size_;   // 字符个数，不含结尾的 '\0'
};

inline bool operator==(const String& a, const String& b) { return a.compare(b) == 0; }
inline bool operator!=(const String& a, const String& b) { return a.compare(b) != 0; }
inline bool operator<(const String& a, const String& b) { return a.compare(b) < 0; }
```

#### 构造与所有权

原书【算法4.4】的构造函数有两处问题。

第一处，`assert(str != '\0')` **本身就编译不过**：`'\0'` 是 `char` 而不是空指针常量，
拿指针与它比较是 ill-formed（`error: ISO C++ forbids comparison between pointer and integer`）。
而且即便改写成 `assert(str != nullptr)` 也是无效断言——`new` 失败时抛
`std::bad_alloc`，从不返回空指针；`assert` 更会在 `NDEBUG` 构建里整个消失。

第二处，参数类型是 `char*`。原书 4.2.2 节自己写下：

> `String s1 = "Hello";` 隐含地调用构造函数 `String::String(char* s)`

而字符串字面量的类型是 `const char[6]`，转成 `char*` 在 C++11 起已被移除。
GCC 默认降级为警告，在本书的 `-Werror` 构建下即是错误。

还有一处原书没有做的检查：构造函数对 `s == nullptr` 毫无防备——
而 4.2.3 节的 `Substr` 恰好会喂给它一个空指针（见 4.2.3 节）。

原书的类里有 `~string()`，却从未把拷贝构造与拷贝赋值作为清单给出
（正文描述过赋值时"必须释放 s1 的原有空间"，但没有代码）。
只要有析构而没有这两个，一次 `String b = a;` 就是二次释放——
与第 2、3 章是同一个错误。

教学版按**三法则**补齐了这两个（析构 + 拷贝构造 + 拷贝赋值），
拷贝赋值走的是「先备好新缓冲区，再释放旧的，最后接管」——顺序反过来，
一旦 `new` 抛异常，对象就停在「指针已释放」的破碎状态。

工程版还多两个**移动**操作，见 4.2a。


### 4.2.3 字符串运算的实现

#### 追加与拼接

`append(char)` 与 `concatenate(const char*)` 的形状是一样的三步：
**重新申请一块、把老内容拷过去、释放旧块**。所以追加一个字符是 O(n)，不是 O(1)——
这就是变长串管理的代价，也是后面章节讨论"预留容量"的动机。

两者都返回 `String&`，于是 `s.append('a').append('b')` 可以连着写，
而且调用方从签名一眼看出它改的是本串。原书【代码4.1】声明的是**按值返回**，
`s.append('x');` 到底改不改 `s`，从签名看不出来。

#### 抽取子串

原书【算法4.5】在起始位置越界时写 `return NULL;`。这里的返回类型是 `String`，
所以 `NULL` 不是"空串"：它先转成 `char*`，再走 `String(char*)` 构造函数，
于是 `strlen(nullptr)`。这段**能编译**，崩在运行期：

```text
$ ./s7
准备调用 s.Substr(99, 1)——原书会走到 return NULL 那一支
runtime error: null pointer passed as argument 1, which is declared to never be null
AddressSanitizer:DEADLYSIGNAL
ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000
```

本书越界就抛 `std::out_of_range`，让错误停在发生的地方，
而不是变成调用方某处的段错误。

`pos == size()` 是**合法**的，得到空串——与"从末尾取 0 个字符"的直觉一致；
`len` 超出剩余长度时**截断**而不报错，与原书的 `if (n > left) n = left;` 语义一致。

#### 查找与比较

对应【算法4.4】的 `find` 返回 `std::optional<size_type>`：找到是下标，没找到是空盒子。
原书用 `-1` 表示没找到——与"位置 0"只差一个符号，调用方漏判就会把
"没找到"当成"匹配在开头"。这与 2.2.2 节 `getPos` 那一处是同一个问题、同一个改法。

关于【算法4.3】：原书自己实现了一个 `strcmp`，固定返回 −1/0/1，
并在正文里说"这与 C/C++ 语言中通常的大小比较习惯(0和非0)不一致"。
其实标准 `strcmp` 返回的就是差值的符号，调用方本来就只该看符号——
不一致的是原书固定返回 ±1 的写法。本书保持标准语义，并据此提供关系运算符。

（另外补一句实测结论：原书那个与标准库同名同签名的 `strcmp` 定义，
既能编译也能链接，不构成冲突——这是我们查过之后否掉的一个猜测，
记在 `legacy.md` 第五节。）

### 4.2a 进阶（选读）：从教学版到工程版

**这一节可以整节跳过。** 工程版在 `code/ch04/string_class/modern.hpp`。
它比教学版多的东西里，只有一件是这一章特有的：**移动之后的那个空壳怎么办。**

```cpp file=code/ch04/string_class/modern.hpp#rule-of-five
// 原书只在正文里描述了赋值时"必须释放 s1 的原有空间(delete [] s1.str)"，
// 却没有把拷贝构造和拷贝赋值作为清单给出。只要有析构函数而没有这两个，
// 一次 `String b = a;` 就是二次释放——与第 2、3 章是同一个错误。
String(const String& other) : data_(new char[other.size_ + 1]), size_(other.size_) {
    // 注意读的是 other.raw() 不是 other.data_：源可能是被移动过的对象（data_ 为空），
    // 从空指针 memcpy 即便长度为 0 也是未定义行为。
    std::memcpy(data_, other.raw(), size_ + 1);
}

String& operator=(const String& other) {
    if (this != &other) {
        String copy(other);  // 拷贝并交换：自赋值安全，且拷贝失败时原对象不受影响
        swap(copy);
    }
    return *this;
}

/// 移动**不分配**：直接接管缓冲区，把对方置为 nullptr。
/// 被移动方仍是一个可用的空串——读取路径统一走 raw()，它在 data_ 为空时返回 ""。
/// （若在这里 new 一块空缓冲区来"修复"对方，移动就不再是 noexcept 的了。）
String(String&& other) noexcept : data_(other.data_), size_(other.size_) {
    other.data_ = nullptr;
    other.size_ = 0;
}

String& operator=(String&& other) noexcept {
    if (this != &other) {
        delete[] data_;
        data_ = other.data_;
        size_ = other.size_;
        other.data_ = nullptr;
        other.size_ = 0;
    }
    return *this;
}

~String() { delete[] data_; }
```

移动操作声明为 `noexcept`，因此**不能在里面分配**——所以被移动方的 `data_`
只能置空，不能塞一块新的 1 字节缓冲区进去。可是教学版承诺过
「`c_str()` 永远不是空指针」，这个承诺不能因为被移动过就作废。

工程版的解法是加一个私有的 `raw()`：`data_` 为空时返回一个静态空串。
读取路径全部走它，于是「被移动之后仍是可用的空串」这个保证**不花任何分配**就成立。
教学版没有移动操作，也就不需要这一层。

其余的差别与前几章相同：`[[nodiscard]]`、`noexcept`、copy-and-swap、
关系运算符全家（`< > <= >=` 都由 `compare()` 派生）。

## 4.3 字符串的模式匹配

给定目标串 T 和模式串 P，模式匹配要回答：P 是否作为 T 的连续子串出现，
若出现，首次出现在哪个位置。

本节的教学内容是**算法本身**，因此下面的实现用 `std::string_view` 接收输入：
不拷贝、不拥有，把注意力留在匹配过程上。字符串容器的实现是 4.2 节的事。

### 4.3.1 朴素的模式匹配算法

朴素算法的思路直白：把模式的首字符对齐目标的每一个位置，逐字符比较；
一旦失配，就把模式整体右移一位，从头再来。

```cpp file=code/ch04/pattern_matching/modern.hpp#naive
/// 朴素模式匹配：返回 pattern 在 text 中首次出现的**起始下标**；没有则 std::nullopt。
///
/// 与原书【算法4.6】的关键差别是返回值：原书写的是 `return (j - pLen + 1);`，
/// 而在 0 起始的下标体系里正确的是 `j - pLen`——**原书这里差了 1**。
/// 用书中自己的例子可以当场看出来：T="abcddabcab..."、P="abcdaabcab" 匹配始于下标 10，
/// 原书返回 11（证据见 legacy.md 缺陷 1）。
///
/// 空模式约定返回 0（与 std::string::find("") 一致）；原书用 assert(m>0) 把它挡在门外，
/// 而 assert 在 NDEBUG 下会被整个编译掉。
[[nodiscard]] inline std::optional<std::size_t> naive_search(std::string_view text,
                                                             std::string_view pattern) {
    const std::size_t n = text.size();
    const std::size_t m = pattern.size();
    if (m == 0) {
        return std::size_t{0};
    }
    if (n < m) {
        return std::nullopt;
    }
    std::size_t i = 0;  // 模式下标
    std::size_t j = 0;  // 目标下标
    while (i < m && j < n) {
        if (text[j] == pattern[i]) {
            ++i;
            ++j;
        } else {
            j = j - i + 1;  // 回退到本趟起点的下一个位置
            i = 0;
        }
    }
    return i >= m ? std::optional<std::size_t>(j - m) : std::nullopt;
}
```

```python file=code/ch04/pattern_matching/modern.py#naive
def naive_search(text: str, pattern: str) -> int | None:
    """朴素匹配：返回首次出现的 0 起始下标。"""
    if not pattern:
        return 0
    i = 0
    j = 0
    while i < len(pattern) and j < len(text):
        if text[j] == pattern[i]:
            i += 1
            j += 1
        else:
            j = j - i + 1
            i = 0
    return j - len(pattern) if i == len(pattern) else None
```

失配时 `j = j - i + 1` 把目标下标退回本趟起点的**下一个**位置——这一步的"+1"是对的，
它表示"换一个起点重来"。最坏情况下，每个起点都要比较到模式末尾才失配
（例如 T = "aaa…a"、P = "aa…ab"），总比较次数为 O(|T|·|P|)。

#### 一处必须指出的错误：原书的返回值差 1

原书【算法4.6】与【算法4.8】在匹配成功时都写：

```text
if (i >= pLen) return (j - pLen + 1);
```

匹配成功时 `j` 已经走到匹配段的**末尾之后**，所以起始位置是 `j - pLen`。
那个 `+1` 是多余的。把原书两段代码照抄进程序，拿标准库的 `find` 做参照：

```text
T=abc                              P=abc          原书朴素=  1  正确答案=  0
T=xabc                             P=abc          原书朴素=  2  正确答案=  1
T=aaab                             P=ab           原书朴素=  3  正确答案=  2
T=abcddabcababcdaabcababcdaabcabaa P=abcdaabcab   原书朴素= 11  正确答案= 10
```

**每一组都恰好多 1**，最后一组正是书中图4.12 自己用来演示 KMP 的那对串。
原书用它逐趟画了匹配过程，却没有给出返回值，这个错误因此在书里没有暴露。

这不是排版或 OCR 造成的，有三重佐证：`j - pLen + 1` 在两个算法里独立印出、写法一致；
同一段代码里的 `j = j - i + 1` 说明作者用的就是 0 起始下标；
而 4.3 节开头的约定更是把这一点写死了——

> P和T的第一个字符都从位置0开始。

本书的实现返回 `j - m`，并且**每一条匹配用例都拿标准库的 `find` 逐个对拍**，
再加 3000 组随机对拍。只断言"找到了"的测试，在原书那份实现下同样全绿——
那样的测试等于没写。

### 4.3.2 字符串的特征向量

KMP 的想法是：失配时不必把模式退回从头开始，因为**已经匹配上的那一段本身携带了信息**。
这句话抽象，用一个例子就清楚了。

设目标串 T = `ababab d`，模式 P = `ababd`，从 T 的位置 0 开始比：

```text
位置:  0 1 2 3 4 5 6
T   :  a b a b a b d
P   :  a b a b d
                ↑ 这里失配（T[4]='a'，P[4]='d'）
```

失配前已经匹配上了 `abab` 这四个字符。**朴素算法**到这里会把模式整体右移一位、从 T[1] 重新
逐字符比起——但这其实浪费了信息：我们已经知道 T[0..3] 就是 `abab`。

KMP 的做法是问：`abab` 的**前缀**和**后缀**里，最长的相同的一段是多少？

```text
abab 的前缀: a, ab, aba
abab 的后缀: b, ab, bab
最长相同的一段: "ab"，长度 2
```

这个 2 的意义是：`abab` 的末尾两个字符 `ab`，恰好也是 `abab` 的开头两个字符。而模式的开头
也是 `ab`。所以模式可以**一次右移 4 − 2 = 2 位**直接对上去，并且移过去之后**前两个字符
不必再比**——它们必然相同：

```text
位置:  0 1 2 3 4 5 6
T   :  a b a b a b d
P   :      a b a b d
             ↑ 这两个已知相同，从这里继续比
```

接着从 P[2] 与 T[4] 往下比，一路比到底，在位置 2 匹配成功。整个过程中 **T 的下标从来没有
往回退过**，这正是 KMP 优于朴素算法的地方：朴素算法最坏要把 T 的每个位置都当一次起点重来。

把「每个位置失配时该退到哪里」对模式的每个位置预先算出来，就是特征向量（next 数组）。
上面这个例子用本章的实现跑出来是：

```text
next(ababd) = -1 0 -1 0 2
kmp_search("abababd", "ababd") = 2
std::string::find 对照         = 2
```

`next[4] = 2` 正是「在 P[4] 处失配时，模式的比较位置退到 2」——和上面手推的一致。
（这里印的是原书【算法4.7】的**优化版** next，所以中间会出现 −1；未优化版本的取值不同，
但失配后的落点等价。）

```cpp file=code/ch04/pattern_matching/modern.hpp#build-next
/// 计算模式的特征向量（next 数组），原书【算法4.7】的"优化版"。
///
/// 与原书的差别只有所有权：原书 `int* findNext(String P)` 用 `new int[m]` 返回裸数组，
/// 而书中**从未展示过与之配对的 delete[]**——每调用一次泄漏一个数组。
/// 计算过程一字未改，包括 `next[i] = next[k]` 这一步优化。
///
/// 空模式返回空向量；原书是 `assert(m > 0)`，而 assert 在 NDEBUG 下会被编译掉，
/// 于是 release 构建里 `new int[0]` 加 `next[0] = -1` 就是一次越界写。
[[nodiscard]] inline std::vector<next_type> build_next(std::string_view pattern) {
    const std::size_t m = pattern.size();
    std::vector<next_type> next(m);
    if (m == 0) {
        return next;
    }
    next_type i = 0;
    next_type k = -1;
    next[0] = -1;
    while (i < static_cast<next_type>(m)) {
        while (k >= 0 && pattern[static_cast<std::size_t>(i)] != pattern[static_cast<std::size_t>(k)]) {
            k = next[static_cast<std::size_t>(k)];  // 沿已算好的特征值回退
        }
        ++i;
        ++k;
        if (i == static_cast<next_type>(m)) {
            break;
        }
        const auto ui = static_cast<std::size_t>(i);
        const auto uk = static_cast<std::size_t>(k);
        // P[i] 与 P[k] 相等时可以直接借用 next[k]，省掉一次注定失败的比较——这就是"优化版"。
        next[ui] = (pattern[ui] == pattern[uk]) ? next[uk] : k;
    }
    return next;
}
```

```python file=code/ch04/pattern_matching/modern.py#build-next
def build_next(pattern: str) -> list[int]:
    """计算原书算法4.7的优化版 next 数组。"""
    if not pattern:
        return []
    next_values = [-1] * len(pattern)
    i = 0
    k = -1
    while i < len(pattern):
        while k >= 0 and pattern[i] != pattern[k]:
            k = next_values[k]
        i += 1
        k += 1
        if i == len(pattern):
            break
        next_values[i] = next_values[k] if pattern[i] == pattern[k] else k
    return next_values
```

`next[i] = next[k]` 那一步是原书所说的"优化"：如果 `P[i]` 与 `P[k]` 相同，
那么用 `k` 作为回退目标必然会再失配一次，不如直接借用 `next[k]` 一步到位。

对模式 `"abcdaabcab"`，算法产生：

| i | 0 | 1 | 2 | 3 | 4 | 5 | 6 | 7 | 8 | 9 |
| --- | --- | --- | --- | --- | --- | --- | --- | --- | --- | --- |
| P[i] | a | b | c | d | a | a | b | c | a | b |
| next[i] | −1 | 0 | 0 | 0 | −1 | 1 | 0 | 0 | 3 | 0 |

这与原书图4.11 最后一行完全一致。

> **注意原书正文与图 4.11 不一致。** 正文写的是
> `next = {-1,0,0,0,0,-1,1,0,0,3,0}`，数一下是 **11 个值**，
> 而模式只有 **10 个字符**。图 4.11 给的是 10 个值，与算法实算的结果相符。
> 正文里多出来的那个 0 是错的。本书的测试逐个比对图 4.11 的十个值，
> 并单独断言"模式只有 10 个字符"，把这处矛盾钉住。

关于所有权：原书 `findNext` 用 `new int[m]` 返回裸数组，而书中调用它的地方
——算法4.8 的 `N` 参数、正文的示例——**一次都没有出现配对的 `delete[]`**。
每匹配一个模式就漏一个数组。本书返回一个拥有所有权的容器，计算过程一字未改。

### 4.3.3 KMP 模式匹配算法

有了特征向量，匹配过程与朴素算法几乎一样，只有失配那一步不同：
不再把模式退回开头，而是退到 `next[i]`。

```cpp file=code/ch04/pattern_matching/modern.hpp#kmp
/// KMP 模式匹配。失配时不再把模式右移一位，而是按特征值决定右移多少。
///
/// 返回值与 naive_search 一致，也修正了原书【算法4.8】同样的差一错误。
/// next 由调用方传入：同一个模式可以只算一次、多次匹配复用——
/// 这正是原书强调的性质，接口把它显式表达出来。
[[nodiscard]] inline std::optional<std::size_t> kmp_search(std::string_view text,
                                                           std::string_view pattern,
                                                           const std::vector<next_type>& next) {
    const std::size_t n = text.size();
    const std::size_t m = pattern.size();
    if (m == 0) {
        return std::size_t{0};
    }
    if (next.size() != m) {
        throw std::invalid_argument("kmp_search: next 数组长度与模式不符");
    }
    if (n < m) {
        return std::nullopt;
    }
    next_type i = 0;    // 模式下标，可以退到 -1
    std::size_t j = 0;  // 目标下标，只增不减
    while (i < static_cast<next_type>(m) && j < n) {
        if (i == -1 || text[j] == pattern[static_cast<std::size_t>(i)]) {
            ++i;
            ++j;
        } else {
            i = next[static_cast<std::size_t>(i)];
        }
    }
    return i >= static_cast<next_type>(m) ? std::optional<std::size_t>(j - m) : std::nullopt;
}

/// 便利重载：模式只用一次时，自己把 next 算掉。
[[nodiscard]] inline std::optional<std::size_t> kmp_search(std::string_view text,
                                                           std::string_view pattern) {
    return kmp_search(text, pattern, build_next(pattern));
}
```

```python file=code/ch04/pattern_matching/modern.py#kmp
def kmp_search(text: str, pattern: str, next_values: list[int] | None = None) -> int | None:
    """KMP 匹配；目标串下标只向前移动。"""
    if not pattern:
        return 0
    if next_values is None:
        next_values = build_next(pattern)
    if len(next_values) != len(pattern):
        raise ValueError("kmp_search: next 数组长度与模式不符")
    i = 0
    j = 0
    while i < len(pattern) and j < len(text):
        if i == -1 or text[j] == pattern[i]:
            i += 1
            j += 1
        else:
            i = next_values[i]
    return j - len(pattern) if i == len(pattern) else None
```

**时间代价的关键在于目标下标 `j` 只增不减。** 循环体里 `++j` 至多执行 |T| 次，
与它同处一条语句的 `++i` 也不超过 |T| 次；能让 `i` 减少的只有 `i = next[i]`，
而 `next[i] < i`，所以每执行一次 `i` 至少减 1；一旦减到 −1，下一步必然进入
`++i, ++j` 分支。因此 `i = next[i]` 的执行次数不超过 `++i, ++j` 的次数加 1，
整个循环体至多执行 2|T| + 1 次——**与目标串长度成线性关系**。

加上计算 next 的 O(|P|)，KMP 整体为 O(|P| + |T|)。而同一个模式的特征向量
只需算一次即可跨多个目标复用，这一点接口里用「next 由调用方传入」显式表达。

本书的测试用朴素算法的最坏情况（T = 20 万个 'a'，P = 1999 个 'a' 加一个 'b'）
验证这条线性性质：KMP 在其上瞬间完成，而失配后不按特征值回退的实现会退化成
O(|T|·|P|)，直接撞上构建闸门的超时。

## 与原书的对照

| 原书 | 现在 | 为什么 |
| --- | --- | --- |
| `return (j - pLen + 1)` | `return j - m` | **原书差 1**，四组数据逐个对拍证实 |
| `int` 返回值，−1 表示没找到 | `std::optional<std::size_t>` | −1 与"位置 0"只差一个符号，漏判就把未匹配当成匹配在开头 |
| `int* findNext()` 返回 `new int[]` | 返回拥有所有权的容器 | 书中从未展示配对的 `delete[]`，每次调用漏一个数组 |
| `assert(m > 0)` | 空模式返回 0 | `assert` 在 NDEBUG 下整个消失，release 构建里是越界写 |
| `assert(next != 0)` | 删除 | `new` 失败抛 `bad_alloc`，从不返回空指针，该断言永远为真 |

**刻意没改的**：朴素匹配仍是回溯式的；next 仍是原书的优化版（图4.11 对的是这一版）；
KMP 的 next 仍由调用方传入并可跨目标复用。这三条是本节全部的教学内容。

完整实现见 `code/ch04/pattern_matching/modern.hpp`，测试见同目录 `test.cpp`
（56 项断言，含 3000 组随机对拍；用 `python3 tools/check_code.py` 在
`-Werror` + ASan/UBSan 与 `-O2` 两种构建下各跑一遍）。

## 本章小结

字符串是元素为字符的线性表。存储要考虑变长：定长数组访问方便，插入删除要搬字符；动态缓冲按长度分配。常用运算是复制、求长、比较、拼接、抽子串、查找字符。模式匹配是子串在主串中的定位：朴素算法直观但会回溯，时间与 $|P|\,|T|$ 成正比；KMP 用模式自身的特征向量避免回溯，时间为 $O(|P|+|T|)$。本书还修正了原书返回值一律差 1 的错误。

## 习题

1. 已知 $s=\texttt{(xyz)+*}$，$t=\texttt{(x+z)*y}$。用连接、抽子串和置换把 $s$ 变成 $t$。
2. 给出使 $s_1+s_2=s_2+s_1$ 成立的所有可能条件（$+$ 为连接）。
3. 统计输入串中各合法字符（A–Z、0–9）的频度。
4. 把长为 $n$ 的串 $S$ 改造成：偶数位字符按原下标从大到小放后半，奇数位从小到大放前半。例如 `ABCDEFGHIJKL` 变成 `ACEGIKLJHFDB`。
5. 判断串是否对称，对称返回 1 否则 0。
6. 求包含在 $s$ 中但不在 $t$ 中的字符构成的新串，以及每个字符在 $s$ 中第一次出现的位置。
7. 线性时间判断 $T$ 是否是 $T'$ 的循环反转（如 `arc` 与 `car`）。
8. 从 $s$ 中删除所有与 $t$ 相同的子串。
9. 求 `BAAABBBAA` 的特征向量，并与目标 `BAAABBBCDDDCCHHHHBBBAAABBBAADD` 匹配，画出过程。

## 上机题

1. 实现 `atoi(x)`：把由数字和可选负号组成的串转成整数。
2. 统计输入串中的整数个数并输出它们（连续数字看成一个整数）。
3. 返回串中最长重复子串及其下标。例如 `abcdacdac` 的最长重复子串是 `cdac`，下标 2。
4. 实现一个简单行编辑器：行插入/删除、当前行指针、分页显示，以及用 KMP（不得调用环境自带查找）做查找替换。

# 第4章 字符串（现代化稿）

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

```cpp file=code/ch04/string_class/modern.hpp#class-head
/// 变长字符串。内部保存一块以 '\0' 结尾的字符数组和当前长度。
///
/// 与原书 String 的差别：构造函数取 const char*（原书取 char*，
/// 使书中自己的例子 `String s1 = "Hello";` 在 C++11 起就是非法转换）；
/// 越界抛 std::out_of_range，而不是 `return NULL` 让调用方拿到一个必然崩溃的对象；
/// 补齐五法则；不做任何 I/O。
class String {
public:
    using size_type = std::size_t;

    /// 空串。注意它仍然持有一块 1 字节的缓冲区，于是 c_str() 永远可用、永不为空指针。
    String() : data_(new char[1]{'\0'}), size_(0) {}

    /// 从 C 字符串构造。故意**不加 explicit**：原书 `String s1 = "Hello";` 这种写法
    /// 是本节的教学用例，保留它；代价是隐式转换，值得知道但这里可以接受。
    String(const char* s) {  // NOLINT(google-explicit-constructor)
        if (s == nullptr) {
            // 原书的 Substr 在越界时 `return NULL`，随后 strlen(nullptr) 当场 SEGV。
            // 这里把它挡在门口，并且说清楚是什么问题。
            throw std::invalid_argument("String: 不能用空指针构造字符串");
        }
        size_ = std::strlen(s);
        data_ = new char[size_ + 1];
        std::memcpy(data_, s, size_ + 1);
    }
```

## 4.2 字符串的存储结构和实现

`String` 采用动态变长的存储结构：内部持有一块以 `'\0'` 结尾的字符数组和当前长度，
构造时按初值长度分配，赋值时按新长度重新分配。这正是本节要教的内容，
所以缓冲区是**裸 `char*`**——换成 `std::string` 这一节就没了。

### 4.2.1 构造与所有权

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

移动操作声明为 `noexcept`，因此**不能在里面分配**。被移动方的指针置空，
读取路径统一走一个私有的 `raw()`：指针为空时返回静态空串。
于是"被移动之后仍是可用的空串"这个保证不花任何分配就成立，
`c_str()` 也永远不会是空指针。

### 4.2.2 追加与拼接

```cpp file=code/ch04/string_class/modern.hpp#append
/// 在串尾添加一个字符，返回自身引用。
///
/// 原书【代码4.1】声明的是 `string append(const char c);`——**按值返回**。
/// 代码4.1 只有声明没有函数体，所以「它到底改不改本串」在书里无从查证；
/// 而这正是问题所在：一个修改器按值返回，调用方无法从签名判断
/// `s.append('x');` 是改了 s 还是返回了一个新串而 s 原封不动。
/// 返回自身引用把这件事说死，同时支持链式调用。
String& append(char c) {
    char* fresh = new char[size_ + 2];
    std::memcpy(fresh, raw(), size_);
    fresh[size_] = c;
    fresh[size_ + 1] = '\0';
    delete[] data_;
    data_ = fresh;
    ++size_;
    return *this;
}

/// 把 s 连接在本串后面。s 为空指针时抛 std::invalid_argument。
String& concatenate(const char* s) {
    if (s == nullptr) {
        throw std::invalid_argument("String::concatenate: 空指针");
    }
    const size_type extra = std::strlen(s);
    char* fresh = new char[size_ + extra + 1];
    std::memcpy(fresh, raw(), size_);
    std::memcpy(fresh + size_, s, extra + 1);
    delete[] data_;
    data_ = fresh;
    size_ += extra;
    return *this;
}

String& operator+=(char c) { return append(c); }
String& operator+=(const String& other) { return concatenate(other.c_str()); }
```

每次追加都重新分配一块、拷贝、释放旧块——这就是变长串管理的代价，
也是后面章节讨论"预留容量"的动机。

### 4.2.3 抽取子串

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

```cpp file=code/ch04/string_class/modern.hpp#substr
/// 从 pos 开始抽取长度至多为 len 的子串。
///
/// 原书【算法4.5】在 `pos >= size` 时 `return NULL;`——那不是"返回空串"，
/// 而是拿 NULL 走 String(char*) 构造函数，接着 strlen(nullptr) 当场崩溃
/// （证据见 legacy.md 缺陷 3）。这里越界就抛 std::out_of_range，
/// 让错误停在发生的地方，而不是变成调用方某处的段错误。
///
/// pos == size() 是合法的，得到空串——与"从末尾取 0 个字符"的直觉一致。
[[nodiscard]] String substr(size_type pos, size_type len) const {
    if (pos > size_) {
        throw std::out_of_range("String::substr: 起始位置越界");
    }
    const size_type available = size_ - pos;
    const size_type take = len < available ? len : available;  // 原书的 if (n > left) n = left
    String result;
    char* fresh = new char[take + 1];
    std::memcpy(fresh, raw() + pos, take);
    fresh[take] = '\0';
    delete[] result.data_;
    result.data_ = fresh;
    result.size_ = take;
    return result;
}
```

`len` 超出剩余长度时**截断**而不报错，与原书的 `if (n > left) n = left;` 语义一致。

### 4.2.4 查找与比较

```cpp file=code/ch04/string_class/modern.hpp#find-compare
/// 从 start 开始查找字符 c，返回下标；没有则 std::nullopt。
/// 原书 `int find(const char c, const int start)` 用 -1 表示没找到，
/// 与"位置 0"只差一个符号。
[[nodiscard]] std::optional<size_type> find(char c, size_type start = 0) const {
    for (size_type i = start; i < size_; ++i) {
        if (raw()[i] == c) {
            return i;
        }
    }
    return std::nullopt;
}

/// 三路比较，负/零/正 表示 小于/等于/大于。
///
/// 原书【算法4.3】自己实现了一个 strcmp，返回值固定为 -1/0/1，
/// 并在正文里指出"这与 C/C++ 语言中通常的大小比较习惯(0和非0)不一致"——
/// 其实不一致的是原书自己：标准 strcmp 返回的就是差值的符号，
/// 调用方只该看符号，不该看具体数值。这里保持标准语义。
[[nodiscard]] int compare(const String& other) const noexcept {
    return std::strcmp(raw(), other.raw());
}
```

关于【算法4.3】：原书自己实现了一个 `strcmp`，固定返回 −1/0/1，
并在正文里说"这与 C/C++ 语言中通常的大小比较习惯(0和非0)不一致"。
其实标准 `strcmp` 返回的就是差值的符号，调用方本来就只该看符号——
不一致的是原书固定返回 ±1 的写法。本书保持标准语义，并据此提供关系运算符。

（另外补一句实测结论：原书那个与标准库同名同签名的 `strcmp` 定义，
既能编译也能链接，不构成冲突——这是我们查过之后否掉的一个猜测，
记在 `legacy.md` 第五节。）

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

KMP 的想法是：失配时不必把模式退回从头开始，因为已经匹配上的那一段
本身携带了信息——它的**最长相同前后缀**长度决定了模式可以直接右移多少。
把这个信息对模式的每个位置预先算出来，就是特征向量（next 数组）。

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

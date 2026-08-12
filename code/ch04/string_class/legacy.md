# 原书写法 → 问题 → 现代写法：字符串类 String

覆盖清单：**代码4.1**（字符串抽象数据类型）、**算法4.3**（标准串比较运算）、
**算法4.4**（构造函数）、**算法4.5**（抽取子串）。
原文见 `dsa_raw.md:2982`、`3147`、`3172`、`3188`。

> 本文件是「证据」，不是「观点」。每条缺陷都附了可复现的命令与真实输出。
>
> **本轮有两条我原先的猜测被证据否掉了，都写在下面第五节**，
> 免得后来人再猜一遍。

## 一、原书清单（已修复 OCR 损伤，逻辑一字未改）

```cpp
class string {                                  // 代码4.1：抽象数据类型
private:
    …                                           // 字符串的数据表示，例如 char * str;
    …                                           // 串的当前长度，例如 int size;
public:
    string();
    string(char * s);
    ~string();
    int length();
    int isEmpty();
    void clear();
    string append(const char c);
    string concatenate(const char * s);
    string copy(const char * s);
    string insert(const char c, const int index);
    int find(const char c, const int start);
    string substr(const int s, const int len);
};

int strcmp(const char * s1, const char * s2) {  // 算法4.3
    int i = 0;
    while (s2[i] != '\0' || s1[i] != '\0') {
        if (s1[i] > s2[i]) return 1;
        else if (s1[i] < s2[i]) return -1;
        i++;
        if (s1[i] == '\0' && s2[i] != '\0') return -1;
        else if (s2[i] == '\0' && s1[i] != '\0') return 1;
    }
    return 0;
}

String::String(char * s) {                      // 算法4.4
    size = strlen(s);
    str = new char[size + 1];
    assert(str != '\0');
    strcpy(str, s);
}

String String::Substr(int pos, int n) {         // 算法4.5
    int i;
    int left = size - pos;
    String tmp;
    char *p, *q;
    if (pos >= size) return NULL;
    if (n > left) n = left;
    delete [] tmp.str;
    tmp.str = new char[n + 1];
    assert(tmp.str != NULL);
    p = tmp.str;
    q = &str[pos];
    for (i = 0; i < n; i++) *p++ = *q++;
    *p = '\0';
    tmp.size = n;
    return tmp;
}
```

## 二、缺陷清单与证据

### 缺陷 1（致命）：`assert(str != '\0')` 本身编译不过

`'\0'` 在 C++ 里是 `char`，不是空指针常量。拿指针和它比较是 ill-formed：

```console
$ g++ -std=c++17 -c s1.cpp
s1.cpp: In constructor ‘String::String(char*)’:
s1.cpp:10:20: error: ISO C++ forbids comparison between pointer and integer [-fpermissive]
   10 |         assert(str != '\0');
```

而且**即使写成 `assert(str != nullptr)` 也是无效的断言**：`new` 分配失败时抛
`std::bad_alloc`，从不返回空指针，这条断言永远为真。算法4.5 里的
`assert(tmp.str != NULL)` 同理。

再加一层：`assert` 在定义了 `NDEBUG` 的构建里被整个编译掉，
所以它连"调试期兜底"都算不上。

### 缺陷 2（致命）：`String(char* s)` 让书中自己的例子编译不过

原书 4.2.2 节自己写：

> `String s1 = "Hello";`
> 隐含地调用构造函数 `String::String(char* s)`

字符串字面量的类型是 `const char[6]`。把它转成 `char*` 在 C++11 起就被移除了。
GCC 默认降级为警告，但本项目按 D-001 开 `-Werror`：

```console
$ g++ -std=c++17 -Wall -Wextra -Wpedantic -Werror -c s4.cpp
s4.cpp: In function ‘int main()’:
s4.cpp:8:26: error: ISO C++ forbids converting a string constant to ‘char*’
             [-Werror=write-strings]
    8 | int main() { String s1 = "Hello"; }
```

现代实现取 `const char*`。顺带一提，原书的构造函数**没有对 `s == nullptr` 做任何检查**，
而缺陷 3 恰好会喂给它一个空指针。

### 缺陷 3（未定义行为）：算法4.5 越界时 `return NULL`

`String Substr(int pos, int n)` 的返回类型是 `String`，而 `return NULL;`
不是"返回空串"——`NULL` 先转成 `char*`，再走 `String(char*)` 构造函数，
于是 `strlen(nullptr)`。这段**能编译**，崩在运行期：

```console
$ g++ -std=c++17 -g -fsanitize=address,undefined s7.cpp -o s7 && ./s7
准备调用 s.Substr(99, 1)——原书会走到 return NULL 那一支
s7.cpp:7:36: runtime error: null pointer passed as argument 1, which is declared to never be null
AddressSanitizer:DEADLYSIGNAL
==2525089==ERROR: AddressSanitizer: SEGV on unknown address 0x000000000000
==2525089==The signal is caused by a READ memory access.
```

现代实现在 `pos > size()` 时抛 `std::out_of_range`，让错误停在发生的地方；
`pos == size()` 合法，得到空串。

### 缺陷 4（未定义行为）：有析构却没有拷贝构造/拷贝赋值

原书 4.2.2 节的正文详细描述了 `s1 = s2` 要"释放 s1 的原有空间(delete [] s1.str)"
再重新分配——**但这个赋值运算符从未作为清单给出**，拷贝构造更是只字未提。
类里有 `~string()` 而没有这两个，一次 `String b = a;` 就是二次释放。
与第 2、3 章 `arrList`/`arrStack` 是同一个错误。

变异验证：把现代实现的拷贝构造改回抄指针，ASan 立刻报
`heap-use-after-free`。

### 缺陷 5（接口形状）：修改器按值返回

代码4.1 声明 `string append(const char c);`、`string concatenate(const char * s);`、
`string insert(const char c, const int index);`——都是**按值返回**。

代码4.1 只有声明没有函数体，所以**不能断言"原书会丢结果"**。
能断言的是签名含混：调用方无从判断 `s.append('x');` 是改了 `s`，
还是返回了一个新串而 `s` 原封不动。现代实现返回 `String&`，把语义钉死，
并支持链式调用。

### 缺陷 6（接口形状）：用 `int` 和 `-1` 表达"没有"

`int find(const char c, const int start)` 用 `-1` 表示没找到，与"位置 0"
只差一个符号；`int isEmpty()` 用 `int` 表达布尔。现代实现分别用
`std::optional<size_type>` 与 `bool`。

### 缺陷 7：`class string` 这个名字

代码4.1 把类命名为小写 `string`。在任何 `using namespace std;` 的翻译单元里，
它与 `std::string` 构成歧义。原书正文随后改用大写 `String`，
两个名字在同一章里混用。

## 三、刻意保留的东西

- 仍然是**裸 `char*` 加显式五法则**：本节要教的就是变长串的存储管理
  （动态分配、按长度重新开辟、拷贝与释放），换成 `std::string` 这一节就没了；
- 缓冲区仍然以 `'\0'` 结尾，`c_str()` 可以直接交给 C 接口；
- `substr` 的 `len` 超出剩余长度时**截断**而非报错，与原书的
  `if (n > left) n = left;` 语义一致。

## 四、一处设计取舍：被移动之后是什么状态

移动构造/移动赋值声明为 `noexcept`，因此**不能在里面分配**。
被移动方的 `data_` 置空，读取路径统一走私有的 `raw()`——
`data_` 为空时返回静态空串 `""`。于是"被移动方仍是可用的空串"这个保证
不需要任何分配就能成立，`c_str()` 也永远不是空指针。

代价是所有读取 `data_` 的地方都必须走 `raw()`。写这个单元时就踩过一次：
拷贝构造里原本写的是 `memcpy(data_, other.data_, ...)`，
而 `other` 可能是被移动过的对象——从空指针 `memcpy` 即使长度为 0 也是 UB。

## 五、两条被证据否掉的猜测（原样记下，免得后来人再猜）

1. **「算法4.3 定义的 `strcmp` 与标准库同名同签名，会冲突」——不成立。**
   实测既能编译也能链接：

   ```console
   $ g++ -std=c++17 s6.cpp -o /dev/null   # 同时 #include <string.h> 并定义 strcmp
   退出=0
   ```

   所以这条没有写进缺陷清单。原书正文提到"返回值(0,正,负)与 C/C++ 通常的
   大小比较习惯(0和非0)不一致"——实际上标准 `strcmp` 返回的就是符号，
   调用方本来就只该看符号，不一致的是原书自己固定返回 ±1 的写法。

2. **「`String s1 = "Hello";` 编译不过」——说法过强。**
   它在 C++11 起确实是非法转换，但 GCC 默认只给警告；只有在
   `-Werror`（本项目的设置）下才是错误。缺陷 2 已按这个口径改写。

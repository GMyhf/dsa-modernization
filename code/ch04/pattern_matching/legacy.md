# 原书写法 → 问题 → 现代写法：字符串模式匹配

覆盖清单：**算法4.6**（朴素模式匹配）、**算法4.7**（计算特征向量，优化版）、
**算法4.8**（KMP 模式匹配）。原文见 `dsa_raw.md:3260`、`3389`、`3426`。

> 本文件是「证据」，不是「观点」。下面每条都附了可复现的程序与真实输出。
>
> 本单元最重的一条不是写法问题，是**算法结果错**：原书两个匹配算法返回的位置一律差 1。

## 一、原书清单（已修复 OCR 损伤，逻辑一字未改）

OCR 把 `}` 认成 `1`、把 `||` 认成 `∥`、丢了若干花括号。下面这份只做了这类还原：

```cpp
int NaiveStrMatching(const String& T, const String& P) {   // 算法4.6
    int i = 0;                       // 模式的下标变量
    int j = 0;                       // 目标的下标变量
    int pLen = P.length();
    int tLen = T.length();
    if (tLen < pLen) return (-1);
    while (i < pLen && j < tLen) {
        if (T[j] == P[i]) { i++; j++; }
        else { j = j - i + 1; i = 0; }
    }
    if (i >= pLen) return (j - pLen + 1);
    else return (-1);
}

int* findNext(String P) {                                   // 算法4.7
    int i = 0;
    int k = -1;
    int m = P.length();
    assert(m > 0);
    int* next = new int[m];
    assert(next != 0);
    next[0] = -1;
    while (i < m) {
        while (k >= 0 && P[i] != P[k]) k = next[k];
        i++; k++;
        if (i == m) break;
        if (P[i] == P[k]) next[i] = next[k];
        else next[i] = k;
    }
    return next;
}

int KMPStrMatching(const String& T, const String& P, int* N) {   // 算法4.8
    int i = 0, j = 0;
    int pLen = P.length(), tLen = T.length();
    if (tLen < pLen) return (-1);
    while (i < pLen && j < tLen) {
        if (i == -1 || T[j] == P[i]) { i++; j++; }
        else i = N[i];
    }
    if (i >= pLen) return (j - pLen + 1);
    else return (-1);
}
```

## 二、缺陷清单与证据

### 缺陷 1（算法错）：两个匹配算法返回的位置都差 1

`return (j - pLen + 1);`——在 0 起始的下标体系里，匹配成功时 `j` 已经走到匹配段的
**末尾之后**，起始位置应当是 `j - pLen`。加的那个 1 是多余的。

把上面三段照抄进一个程序，拿标准库 `std::string::find` 做参照物：

```console
$ g++ -std=c++17 -fsanitize=address,undefined ch4.cpp -o ch4 && ./ch4
T=abc                              P=abc          原书朴素=  1 原书KMP=  1 正确答案=  0
T=xabc                             P=abc          原书朴素=  2 原书KMP=  2 正确答案=  1
T=aaab                             P=ab           原书朴素=  3 原书KMP=  3 正确答案=  2
T=abcddabcababcdaabcababcdaabcabaa P=abcdaabcab   原书朴素= 11 原书KMP= 11 正确答案= 10
```

**每一组都恰好多 1。** 最后一组正是书中图4.12 自己用的那对串——
原书用它演示 KMP 的匹配过程，却没有给出返回值，于是这个错误在书里没有暴露。

这不是 OCR：`j - pLen + 1` 在算法4.6 与算法4.8 两处独立印出，写法一致。
而同一段代码里的回溯语句 `j = j - i + 1`（这一句是对的）恰恰证明了作者用的就是
0 起始下标——`+1` 在那里是"下一个起始位置"，在返回值里却成了偏移。

现代实现返回 `j - m`，并且**所有匹配用例都拿标准库的 `find` 逐个对拍**，
另加 3000 组随机对拍。只断言「找到了」的测试在原书那份实现下同样全绿，等于没测。

### 缺陷 2（资源）：`findNext` 返回裸数组，书中从未展示配对的 `delete[]`

`int* next = new int[m]; ... return next;`——所有权交给了调用方，
而书里调用它的地方（算法4.8 的 `N` 参数、正文示例）**一次都没有 `delete[]`**。
每匹配一个模式就漏一个数组。

现代实现返回 `std::vector<std::ptrdiff_t>`。这是本单元唯一一处
D-001 §2 的豁免（记在 `unit.json` 的 `d001_exceptions`，附理由）：
本节的教学内容是匹配算法，不是容器；容器是 4.2 节的事。
**next 的计算过程一字未改**，包括 `next[i] = next[k]` 那步优化。

### 缺陷 3：用 `assert` 做输入校验，release 构建里整个消失

`assert(m > 0)` 与 `assert(next != 0)` 有两个问题：

- `assert` 在定义了 `NDEBUG` 时被整个编译掉。于是 release 构建里传入空模式，
  `new int[0]` 之后 `next[0] = -1` 就是一次**堆越界写**；
- `assert(next != 0)` 本身是无效的：`new` 失败时抛 `std::bad_alloc`，从不返回空指针。
  这条断言永远为真。

现代实现把空模式当成合法输入（返回 0，与 `std::string::find("")` 一致），
`next` 与模式长度不配套时抛 `std::invalid_argument`。

### 缺陷 4（书内自相矛盾）：正文的 next 数组比模式还长一位

正文写：

> 最后一行则给出了最终的计算结果：next = {-1,0,0,0,0,-1,1,0,0,3,0}。

数一下是 **11 个值**，而模式 `"abcdaabcab"` 只有 **10 个字符**。
同一页的图4.11 最后一行给的是 `{-1,0,0,0,-1,1,0,0,3,0}`——10 个值。
按原书算法4.7 实算：

```console
书中示例 P="abcdaabcab" 的 next 数组（原书优化版算法算出）：
  {-1,0,0,0,-1,1,0,0,3,0}
```

**算法站在图这一边，正文那个多出来的 0 是错的。**
（这一个 0 是印刷错误还是 OCR 多插的，从 OCR 文本本身分辨不出，
只能确定正文与图 + 算法三者不能同时成立。）

`test.cpp::test_next_matches_the_book_figure` 逐个比对图4.11 的十个值，
并单独断言"模式只有 10 个字符"，把这处矛盾钉在测试里。

### 缺陷 5：接口形状

`int` 返回值用 `-1` 表示"没找到"，与"位置 0"只差一个符号；调用方漏判就会把
未匹配当成匹配在开头。现代实现返回 `std::optional<std::size_t>`，
并加 `[[nodiscard]]`。

## 三、刻意保留的东西

- 朴素匹配仍然是**回溯式**的：失配就把目标下标退回本趟起点的下一位。
  那正是它与 KMP 的全部差别，也是复杂度分析的依据。
- next 的计算仍是原书的**优化版**（`P[i] == P[k]` 时借用 `next[k]`），
  没有换成教科书里更常见的未优化版本——图4.11 的逐行推演对的是这一版。
- KMP 的 `next` 仍由调用方传入并可跨目标复用，这是原书强调的性质。

## 四、这一单元没做的

原书 4.3.1 节的匹配是定义在自家 `String` 类上的（`const String& T`）。
本单元用 `std::string_view` 接收输入，因为本节要教的是算法。
`String` 类本身（代码4.1、算法4.3–4.5）属于 4.2 节，另立单元
`code/ch04/string_class`，尚未开工。

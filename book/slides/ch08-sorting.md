---
title: 第8章 内排序
subtitle: 现代 C++ 数据结构教程
---

# 第8章 内排序

**把一组记录按关键码重排成有序序列。**

- 插入类：直接插入、Shell
- 选择类：直接选择、**堆排序**
- 交换类：冒泡、**快速排序**
- 归并类：两路归并
- 分配类：桶式、基数

一条主线：**代价从 $O(n^2)$ 到 $O(n\log n)$，靠的是什么。**

---

# 8.1 基本概念

- **稳定性**：关键码相同的记录，排完之后相对次序不变 → 稳定
- **就地**：额外空间 $O(1)$
- **比较模型**：只能通过「两两比较」判断次序

**稳定性不是可有可无**：按多个字段排序时，
先排次要字段、再用**稳定**算法排主要字段，就能得到「主序内按次序」的结果。

<!-- 备注
举个例子：先按成绩排，再按班级用稳定排序排，结果就是「班级内按成绩」。
用不稳定的排序做这件事就得不到想要的结果。
-->

---

# 8.2 插入排序

![图 8.1 插入排序](../assets/5493e0dcc0da4096.jpg)

```cpp file=code/ch08/sorting/modern.hpp#insertion
// 算法8.1：直接插入排序。相等元素不越过彼此，故稳定。
inline void insertion_sort(std::vector<int>& values) {
    for (std::size_t index = 1; index < values.size(); ++index) {
        const int value = values[index];
        std::size_t hole = index;
        while (hole != 0 && value < values[hole - 1]) {
            values[hole] = values[hole - 1];
            --hole;
        }
        values[hole] = value;
    }
}
```

<!-- 备注
形象说法：像整理手里的扑克牌，抓一张就往左边已经排好的那段里插。

最好情况：输入已有序，每张牌比一次就位，**O(n)**。
最坏情况：输入逆序，第 i 张要比 i 次，O(n^2)。
所以说「插入排序是 O(n^2)」对，说「是 Θ(n^2)」就错了。
-->

---

# 插入排序的两个优点

- **稳定**：只在严格小于时才往前挪
- **对近乎有序的输入极快**：$O(n + d)$，d 是逆序对数

所以它常被用作**其它算法的收尾**——快排/归并递归到小区间时改用插入排序。

**n 小的时候 $O(n^2)$ 完全可能比 $O(n\log n)$ 快**：常数小、没有递归开销、缓存友好。

<!-- 备注
这是第 1 章「渐进分析不是全部」的一个实例，可以回头指一下。
标准库的 sort 内部就是这么做的（introsort：快排 + 堆排 + 插入排序）。
-->

---

# 8.2.2 Shell 排序

![图 8.2 Shell 排序](../assets/9d5229db93db8c45.jpg)

先按**大增量**分组做插入排序，再逐步缩小增量，最后增量为 1。

**为什么快**：大增量时每组元素少、几乎有序；
小增量时整体已经接近有序，而插入排序恰好对这种输入极快。

代价与增量序列有关，「每次除以 2」约 $O(n^{1.5})$。**不稳定**。

---

# 8.3.2 堆排序

![图 8.4 堆排序](../assets/7f0370f58c7602be.jpg)

```cpp file=code/ch08/sorting/modern.hpp#heap
// 算法8.4：手写最大堆筛选与堆排序，不委托 std::make_heap/sort_heap。
inline void sift_down(std::vector<int>& values, std::size_t root, std::size_t count) {
    while (root * 2 + 1 < count) {
        std::size_t child = root * 2 + 1;
        if (child + 1 < count && values[child] < values[child + 1]) ++child;
        if (values[root] >= values[child]) return;
        using std::swap;
        swap(values[root], values[child]);
        root = child;
    }
}

inline void heap_sort(std::vector<int>& values) {
    for (std::size_t root = values.size() / 2; root != 0; --root) {
        sift_down(values, root - 1, values.size());
    }
    for (std::size_t end = values.size(); end > 1; --end) {
        using std::swap;
        swap(values[0], values[end - 1]);
        sift_down(values, 0, end - 1);
    }
}
```

<!-- 备注
两步：先把整个数组建成**最大**堆，再反复「把根和末尾交换、堆长度减一、下沉」。

关键点：**建堆是 O(n)**（第 5 章证过），所以总代价 O(n log n) 里那个 log n
全部来自后面 n 次取最大值。

优点：就地、最坏也是 O(n log n)。缺点：不稳定，而且缓存局部性差
（下标跳来跳去），实际常数往往比快排大。
-->

---

# 8.4.2 快速排序

![图 8.6 快速排序图示](../assets/8585d3b42b280664.jpg)

选一个**轴值**，把序列分成「小于它的」和「大于它的」两段，再各自递归。

---

# 快排的实现

```cpp file=code/ch08/sorting/modern.hpp#quick
inline std::size_t partition(std::vector<int>& values, std::size_t first, std::size_t last) {
    const int pivot = values[last - 1];
    std::size_t boundary = first;
    for (std::size_t index = first; index + 1 < last; ++index) {
        if (values[index] < pivot) {
            using std::swap;
            swap(values[boundary], values[index]);
            ++boundary;
        }
    }
    using std::swap;
    swap(values[boundary], values[last - 1]);
    return boundary;
}

inline void quick_sort_range(std::vector<int>& values, std::size_t first, std::size_t last) {
    if (last - first < 2) return;
    const std::size_t middle = partition(values, first, last);
    quick_sort_range(values, first, middle);
    quick_sort_range(values, middle + 1, last);
}

// 算法8.6：手写快排。
inline void quick_sort(std::vector<int>& values) { quick_sort_range(values, 0, values.size()); }
```

<!-- 备注
分割（partition）是全部难点。这里用的是最直白的写法：
boundary 左边全是小于轴值的，扫一遍把小的换过去，最后把轴值换到分界点。

平均 O(n log n)，最坏 O(n^2)——输入已经有序而轴值取末尾时，
每次只分出一个元素，递归深度 n。
-->

---

# 快排最坏情况怎么办

| 办法 | 效果 |
| --- | --- |
| 三者取中（首、中、尾） | 避开「已排序」这个最常见的坏输入 |
| 随机轴值 | 让坏输入无法被构造出来 |
| 小区间改插入排序 | 省掉递归开销，常数变小 |
| 递归改成对**短**的那半递归 | 栈深度降到 $O(\log n)$ |

**最后一条常被忽略**：不做的话，最坏情况下递归深度是 $n$，
在第 3 章那张表里就是段错误。

---

# 8.5 归并排序

![图 8.8 归并排序](../assets/3599737381e2502e.jpg)

```cpp file=code/ch08/sorting/modern.hpp#merge
inline void merge_ranges(std::vector<int>& values, std::vector<int>& buffer,
                         std::size_t first, std::size_t middle, std::size_t last) {
    std::size_t left = first;
    std::size_t right = middle;
    std::size_t output = first;
    while (left < middle && right < last) {
        buffer[output++] = values[right] < values[left] ? values[right++] : values[left++];
    }
    while (left < middle) buffer[output++] = values[left++];
    while (right < last) buffer[output++] = values[right++];
    for (std::size_t index = first; index < last; ++index) values[index] = buffer[index];
}

inline void merge_sort_range(std::vector<int>& values, std::vector<int>& buffer,
                             std::size_t first, std::size_t last) {
    if (last - first < 2) return;
    const std::size_t middle = first + (last - first) / 2;
    merge_sort_range(values, buffer, first, middle);
    merge_sort_range(values, buffer, middle, last);
    merge_ranges(values, buffer, first, middle, last);
}

// 算法8.8：两路归并排序。
inline void merge_sort(std::vector<int>& values) {
    std::vector<int> buffer(values.size());
    merge_sort_range(values, buffer, 0, values.size());
}
```

<!-- 备注
分治的另一种切法：快排是「先分好再递归」，归并是「先递归再合并」。

归并的两个特点：
1. **稳定**（合并时相等取左边的）；
2. **最坏也是 O(n log n)**，没有快排那种退化。
代价是需要 O(n) 的额外空间——这是它不如快排流行的主要原因。

归并还有一个别处没有的好处：它天然适合**外排序**，因为只需要顺序读写。
第 9 章全靠它。
-->

---

# 8.6 分配排序：跳出比较模型

**比较排序的下界是 $\Omega(n\log n)$**——

> n 个元素有 $n!$ 种排列，每次比较至多把可能性减半，
> 所以至少要 $\log_2(n!) = \Omega(n\log n)$ 次比较。

想更快，就只能**不靠比较**：直接用关键码的值去算位置。

---

# 桶式排序与基数排序

![图 8.10 基数排序](../assets/916c1027b215b0e1.jpg)

```cpp file=code/ch08/sorting/modern.hpp#radix
// 算法8.11：LSD 基数排序。翻转符号位使二补码有符号 int 按无符号序排序。
inline void radix_sort(std::vector<int>& values) {
    std::vector<int> buffer(values.size());
    for (unsigned shift = 0; shift < 32; shift += 8) {
        std::size_t counts[256]{};
        for (int value : values) {
            const auto key = static_cast<std::uint32_t>(value) ^ 0x80000000U;
            ++counts[(key >> shift) & 0xffU];
        }
        std::size_t offset = 0;
        for (std::size_t& count : counts) { const std::size_t old = count; count = offset; offset += old; }
        for (int value : values) {
            const auto key = static_cast<std::uint32_t>(value) ^ 0x80000000U;
            buffer[counts[(key >> shift) & 0xffU]++] = value;
        }
        values.swap(buffer);
    }
}
```

<!-- 备注
桶式排序：关键码范围有限时，开一排桶，直接扔进去，O(n + r)。

基数排序：把关键码按位拆开，**从低位到高位**依次做桶式排序。
d 位、每位 r 个值时是 O(d(n + r))。

为什么必须从低位开始？因为每一轮的桶式排序必须是**稳定**的，
低位的结果才能被高位的排序保留下来。反过来做会把低位的功夫全废掉。
这一点值得当场问学生。
-->

---

# 8.7 各算法对照

| 算法 | 平均 | 最坏 | 额外空间 | 稳定 |
| --- | --- | --- | --- | --- |
| 直接插入 | $O(n^2)$ | $O(n^2)$ | $O(1)$ | 是 |
| Shell | $O(n^{1.5})$ | 与增量有关 | $O(1)$ | 否 |
| 直接选择 | $O(n^2)$ | $O(n^2)$ | $O(1)$ | 否 |
| 堆排序 | $O(n\log n)$ | $O(n\log n)$ | $O(1)$ | 否 |
| 快速排序 | $O(n\log n)$ | $O(n^2)$ | $O(\log n)$ | 否 |
| 归并排序 | $O(n\log n)$ | $O(n\log n)$ | $O(n)$ | **是** |
| 基数排序 | $O(d(n+r))$ | $O(d(n+r))$ | $O(n+r)$ | **是** |

---

# 怎么选

- **一般情况**：快排（三者取中 + 小区间插入排序）
- **要稳定**：归并
- **最坏也不能退化**（实时系统）：堆排序
- **数据近乎有序**：插入排序
- **关键码是小范围整数**：桶式 / 基数
- **数据放不进内存**：第 9 章的外排序

**没有一个算法在所有维度上都最好**——这是本章真正要传达的。

---

# 本章小结

- 稳定性在**多字段排序**时是刚需，不是可有可无
- 插入排序对近乎有序的输入是 $O(n)$，所以常被用作收尾
- 堆排序靠第 5 章的堆；**建堆 $O(n)$**，总代价里的 $\log n$ 来自取最大值
- 快排的最坏是 $O(n^2)$，靠**取轴策略**和**对短半边递归**避开
- 归并稳定、最坏有保证，代价是 $O(n)$ 额外空间
- 比较模型的下界是 $\Omega(n\log n)$；要更快只能**不比较**
- 基数排序必须**从低位到高位**，且每轮必须稳定

---

# 上机

```bash
python3 tools/check_code.py code/ch08/sorting
```

- 用随机、已排序、逆序、全相等、含负数五组输入对拍所有算法
- 把基数排序改成从**高位**到低位，看结果错在哪
- 给快排喂一个已排序的输入，量一下递归深度

> 测试里所有排序算法**互相对拍**：同一组输入必须给出同样的结果，
> 稳定的那几个还要额外验证稳定性。

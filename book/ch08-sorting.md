# 第8章 内部排序

排序把一个序列重排为非递减顺序。阅读时同时问四个问题：比较还是分配？是否稳定？额外空间多少？最好、平均、最坏时间是多少？

源码：[全部手写排序](../code/ch08/sorting/modern.hpp)、
[可运行示例](../code/ch08/sorting/demo.cpp)、
[对拍测试](../code/ch08/sorting/test.cpp)。

## 8.1 先把题目说清楚

| 方法 | 平均时间 | 稳定性 | 主要特点 |
| --- | --- | --- | --- |
| 直接插入 | O(n²) | 稳定 | 小规模、近乎有序时简单有效 |
| 冒泡 | O(n²) | 稳定 | 教学直观，通常不作为通用选择 |
| 选择 | O(n²) | 不稳定 | 交换次数少 |
| 堆排序 | O(n log n) | 不稳定 | 最坏情况也有保证，原地排序 |
| 快速排序 | O(n log n) 平均 | 不稳定 | 实务常用，坏划分时会退化 |
| 归并排序 | O(n log n) | 稳定 | 需要 O(n) 辅助空间 |
| 计数/基数 | 与值域或位数有关 | 可稳定 | 适合整数键，不是比较排序 |

「稳定」指相等键在排序前后的相对顺序不变。例如记录按成绩排序时，两名同分学生仍按原来的姓名顺序出现。本章所有实现接受有符号整数，测试含重复和负数。没有用 `std::sort` / `std::make_heap` 替代手写算法。

## 8.2 如何调用

```cpp file=code/ch08/sorting/demo.cpp
#include "modern.hpp"

#include <iostream>
#include <vector>

namespace {
void print(const char* name, const std::vector<int>& values) {
    std::cout << name;
    for (int value : values) {
        std::cout << ' ' << value;
    }
    std::cout << '\n';
}
}  // namespace

int main() {
    const std::vector<int> raw{3, -2, 7, 3, 0, -2, 9, 1};

    auto insertion = raw;
    dsa::sorting::insertion_sort(insertion);
    print("插入:", insertion);

    auto heap = raw;
    dsa::sorting::heap_sort(heap);
    print("堆排:", heap);

    auto quick = raw;
    dsa::sorting::quick_sort(quick);
    print("快排:", quick);

    auto radix = raw;
    dsa::sorting::radix_sort(radix);
    print("基数:", radix);
}
```

```bash
c++ -std=c++17 -Wall -Wextra -Werror -Icode/ch08/sorting \
    code/ch08/sorting/demo.cpp -o /tmp/sort-demo
/tmp/sort-demo
```

```console
插入: -2 -2 0 1 3 3 7 9
堆排: -2 -2 0 1 3 3 7 9
快排: -2 -2 0 1 3 3 7 9
基数: -2 -2 0 1 3 3 7 9
```

四种算法交出同一条非递减序列。插入排序保持两个 `-2`、两个 `3` 的相对次序；堆排和快排不保证这一点。

## 8.3 再读实现

直接插入把 `values[index]` 抽出来，向前挪动所有比它大的元素，把空位留给它。相等元素不越过彼此，所以稳定。

堆排序先把数组建成最大堆，再反复把堆顶与堆尾交换并缩小堆。`sift_down` 必须比较左右两个孩子，漏掉右孩子会交出未排序数组——测试里有一条专门守这个。

快排选区间末元素为枢轴，把更小的元素换到左侧，再递归两边。全相等的输入必须也能结束，否则分区不推进就会无限递归。

基数排序把有符号 `int` 的符号位翻转后，按字节做 4 趟计数收集。否则负数会按无符号序排到最大。

## 8.4 现代实现

直接插入：

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

堆排序：

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

快速排序：

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

基数排序：

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

其余算法（Shell、选择、冒泡、归并、计数、索引排序）在同一文件里，接口相同：`void sort(std::vector<int>&)`。

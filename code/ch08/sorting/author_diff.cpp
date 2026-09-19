// author_diff.cpp —— 与作者代码包对拍（由 tools/authorsrc.py --check 编译运行，不归 check_code 管）
//
// 作者代码包 site_visit/DSCode_ZWZ200806_CPP/ch08_Sort 里每种排序都是一个独立程序，
// 共用一个计时驱动 SortMain.h。那个驱动的排序调用被作者注释掉了：
//     //sort<int>(&array[i], listsize);
// 于是包里的 12 个排序程序跑起来只生成 1000 万个随机数、给空循环计时——**作者的排序从没在
// 那个驱动里真正跑过**。这里把它们逐个拿出来，与本单元的实现、与 std::sort 在同一批输入上对拍。
//
// 每种算法各自是一个 .cpp（各带一个 sort 模板和一个 main），不能直接进同一个翻译单元：
// 除 QuickSort.cpp 外都包进各自的命名空间；QuickSort.cpp 里写的是 `::SelectPivot`，
// 只能留在全局。main 统一改名，免得与本文件的 main 冲突。
#include <algorithm>
#include <cstdio>
#include <cstdlib>
#include <ctime>
#include <iostream>
#include <limits>
#include <random>
#include <stdlib.h>
#include <string.h>
#include <string>
#include <time.h>
#include <vector>

#include "modern.hpp"

// 下面几处前置声明同一个原因：作者的排序都是「先调用、后定义」（ModInsSort、Partition、Merge、
// ModMerge、AdjustRecord），实参是 int*，ADL 找不到，按两阶段查找的标准在模板定义处必须可见。
// clang 照标准报错，g++ 放行——作者包自己的这些程序因此只在 g++/VC6 下编译得过（见 programs 的
// clang 基线）。对拍这里只补声明，不改作者代码。（2026-09-18 Codex 在 macOS clang 上复核 T-079 撞出）
template <class Record>
int Partition(Record Array[], int left, int right);
#define main author_main_quick
#include "QuickSort/QuickSort.cpp"
#undef main

// 预处理器不能在宏里 #include，只好逐个手写
#define main author_main_ins
namespace a_ins {
#include "InsSort.cpp"
}
#undef main
#define main author_main_shell
namespace a_shell {
template <class Record>
void ModInsSort(Record Array[], int n, int delta);
#include "ShellSort/ShSort2.cpp"
}
#undef main
#define main author_main_sel
namespace a_sel {
#include "SelSort.cpp"
}
#undef main
#define main author_main_heap
namespace a_heap {
#include "HeapSort/HeapSort.cpp"
}
#undef main
#define main author_main_bub
namespace a_bub {
#include "BubSort.cpp"
}
#undef main
#define main author_main_modquick
namespace a_modquick {
#include "QuickSort/ModQuickSort.cpp"
}
#undef main
#define main author_main_merge
namespace a_merge {
template <class Record>
void Merge(Record Array[], Record TempArray[], int left, int right, int middle);
#include "MergeSort/MergeSort.cpp"
}
#undef main
#define main author_main_modmerge
namespace a_modmerge {
template <class Record>
void ModMerge(Record Array[], Record TempArray[], int left, int right, int middle);
#include "MergeSort/ModMergeSort.cpp"
}
#undef main
#define main author_main_bucket
namespace a_bucket {
#include "BucketSort.cpp"
}
#undef main
#define main author_main_radix
namespace a_radix {
#include "RadixSort/RadixSort_Bin.cpp"
}
#undef main
#define main author_main_index
namespace a_index {
template <class Record>
void AdjustRecord(Record Array[], int IndexArray[], int n);
#include "AddSort_Insert.cpp"
}
#undef main

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::cerr << "  ✗ " << what << "\n";
    }
}

using Vec = std::vector<int>;
using AuthorFn = void (*)(int*, int);
using OursFn = void (*)(Vec&);

struct Pair {
    const char* name;   // 原书清单号
    AuthorFn author;
    OursFn ours;
};

// 作者的入口签名五花八门，这里统一成 (int*, int)
void author_insert(int* a, int n) { a_ins::InsertSort(a, n); }
void author_shell(int* a, int n) { a_shell::ShellSort(a, n); }
void author_select(int* a, int n) { a_sel::SelectSort(a, n); }
// 作者包的 MaxHeap::SiftDown（HeapSort/MaxHeap.h:76）判右孩子写的是
//     if ((j < CurrentSize) && (heapArray[j] < heapArray[j+1]))
// 应为 j < CurrentSize-1（第 5 章 MinHeap 与原书代码5.11 都写对了）。最后一个父结点只有左孩子时
// ——n 为偶数——建堆会读 heapArray[n]，越界一格；那一格的值若更大，还会被换进堆里、再把 temp 写到
// heapArray[n]。原书没有印 MaxHeap（算法8.4 只印了调用它的 sort），所以这是**包里独有**的缺陷。
// 对拍时给数组尾部垫一格 INT_MIN：越界读落在自己的内存里、且永远比不过真数据，算法其余部分照常检验。
void author_heap(int* a, int n) {
    std::vector<int> padded(a, a + n);
    padded.push_back(std::numeric_limits<int>::min());
    a_heap::sort(padded.data(), n);
    std::copy(padded.begin(), padded.begin() + n, a);
}
// 作者包的 BubbleSort（BubSort.cpp:10）内层循环写 for (j=n-1; j>=i; j--)：i=0 时 j 走到 0，
// 比较 Array[0] < Array[-1]——越界一格读，若成立还会把 Array[-1] 换进来、把 Array[0] 写出去。
// **原书印的是 j > i，是对的**（dsa_raw.md 算法8.5）：这一处是印刷版比代码包更正确。
// 对拍时在数组前垫一格 INT_MIN，越界读落在自己的内存里且比较永远不成立。
void author_bubble(int* a, int n) {
    std::vector<int> padded(1, std::numeric_limits<int>::min());
    padded.insert(padded.end(), a, a + n);
    a_bub::BubbleSort(padded.data() + 1, n);
    std::copy(padded.begin() + 1, padded.end(), a);
}
void author_quick(int* a, int n) { QuickSort(a, 0, n - 1); }
// 算法8.7 的 ModQuickSort 只处理长于 THRESHOLD 的子串，短子串留给最后一趟插入排序——
// 作者包里那一趟在 sort() 包装里（与原书算法8.7 末尾的 Quicksort 包装一致），所以调包装。
void author_modquick(int* a, int n) { a_modquick::sort(a, n); }
void author_merge(int* a, int n) {
    Vec tmp(static_cast<std::size_t>(n) + 1);
    a_merge::MergeSort(a, tmp.data(), 0, n - 1);
}
void author_modmerge(int* a, int n) {
    Vec tmp(static_cast<std::size_t>(n) + 1);
    a_modmerge::ModMergeSort(a, tmp.data(), 0, n - 1);
}
constexpr int bucket_max = 100;  // 桶排序的值域上界：对拍用 [0, bucket_max)
void author_bucket(int* a, int n) { a_bucket::BucketSort(a, n, bucket_max); }
void author_radix(int* a, int n) { a_radix::RadixSort(a, n, DStep, radix); }  // 16 进制 4 趟：值须 < 65536

Vec sorted_copy(Vec v) {
    std::sort(v.begin(), v.end());
    return v;
}

Vec run_author(AuthorFn fn, Vec v) {
    fn(v.data(), static_cast<int>(v.size()));
    return v;
}

std::vector<Vec> inputs(int upper) {
    std::vector<Vec> out = {{}, {7}, {2, 1}, {1, 2}, {3, 3, 3},
                            {29, 25, 34, 64, 34, 12, 32, 45}};  // SortMain.h 注释里作者自己的小数据
    std::mt19937 rng(20080601);
    for (int n : {3, 5, 16, 27, 28, 29, 57, 100, 1000}) {  // 28/29 跨过作者的 THRESHOLD
        std::uniform_int_distribution<int> dist(0, upper - 1);
        Vec random(static_cast<std::size_t>(n));
        for (int& x : random) x = dist(rng);
        out.push_back(random);
        Vec ascending = sorted_copy(random);
        out.push_back(ascending);
        out.push_back(Vec(ascending.rbegin(), ascending.rend()));
        out.push_back(Vec(static_cast<std::size_t>(n), upper / 2));
    }
    return out;
}

}  // namespace

int main() {
    const Pair general[] = {
        {"算法8.1 插入排序", author_insert, dsa::sorting::insertion_sort},
        {"算法8.2 Shell 排序", author_shell, dsa::sorting::shell_sort},
        {"算法8.3 直接选择排序", author_select, dsa::sorting::selection_sort},
        {"算法8.4 堆排序", author_heap, dsa::sorting::heap_sort},
        {"算法8.5 冒泡排序", author_bubble, dsa::sorting::bubble_sort},
        {"算法8.6 快速排序", author_quick, dsa::sorting::quick_sort},
        {"算法8.7 优化的快速排序", author_modquick, dsa::sorting::quick_sort_optimized},
        {"算法8.8 两路归并排序", author_merge, dsa::sorting::merge_sort},
        {"算法8.9 优化的两路归并排序", author_modmerge, dsa::sorting::merge_sort_optimized},
        {"算法8.11 基数排序", author_radix, dsa::sorting::radix_sort},
    };
    int cases = 0;
    for (const Pair& p : general) {
        for (const Vec& in : inputs(32003)) {  // 32003：SortMain.h 生成随机数的上界
            Vec expected = sorted_copy(in);
            Vec ours = in;
            p.ours(ours);
            expect(run_author(p.author, in) == expected,
                   std::string(p.name) + "：作者版在 n=" + std::to_string(in.size()) + " 上没排对");
            expect(ours == expected,
                   std::string(p.name) + "：本单元实现在 n=" + std::to_string(in.size()) + " 上没排对");
            ++cases;
        }
    }
    // 作者 MaxHeap 越界那一格若比真数据大，结果就被污染：垫 INT_MAX 而不是 INT_MIN，
    // 偶数长度 {1, 2} 排出来不再是 {1, 2}。这条断言是**缺陷的证据**：包若修好了，它会红，
    // 提醒去改 collab/authorsrc.json 与 book/考场代码包.md 里的说法。
    {
        std::vector<int> padded = {1, 2, std::numeric_limits<int>::max()};
        a_heap::sort(padded.data(), 2);
        expect(!(padded[0] == 1 && padded[1] == 2),
               "作者 MaxHeap::SiftDown 的越界读应当可复现（数组尾后一格放大数，结果被污染）");
        ++cases;
    }
    // 同理，作者 BubbleSort 的 Array[-1] 若比 Array[0] 大，就被换进结果。
    {
        std::vector<int> padded = {std::numeric_limits<int>::max(), 1, 2};
        a_bub::BubbleSort(padded.data() + 1, 2);
        expect(!(padded[1] == 1 && padded[2] == 2),
               "作者 BubbleSort 的 j>=i 越界应当可复现（数组前一格放大数，结果被污染）");
        ++cases;
    }
    for (const Vec& in : inputs(bucket_max)) {
        Vec ours = in;
        dsa::sorting::counting_sort(ours);
        expect(run_author(author_bucket, in) == sorted_copy(in), "算法8.10 桶式排序：作者版没排对");
        expect(ours == sorted_copy(in), "算法8.10 桶式排序：本单元实现没排对");
        ++cases;
    }
    // 算法8.14/8.15：索引排序。作者的 IndexSort 排完索引就调 AdjustRecord 就地整理，
    // 所以直接比整理后的数组；本单元把两步拆开（insertion_index_sort + adjust_by_index）。
    for (const Vec& in : inputs(32003)) {
        Vec author = in;
        Vec index(in.size());
        a_index::IndexSort(author.data(), index.data(), static_cast<int>(in.size()));
        Vec ours = in;
        auto idx = dsa::sorting::insertion_index_sort(ours);
        dsa::sorting::adjust_by_index(ours, idx);
        expect(author == sorted_copy(in), "算法8.14/8.15 索引排序：作者版没排对");
        expect(ours == sorted_copy(in), "算法8.14/8.15 索引排序：本单元实现没排对");
        ++cases;
    }
    if (failures) {
        std::cerr << "❌ 排序对拍：" << failures << " 处不一致\n";
        return 1;
    }
    std::cout << "✅ 排序对拍：12 种算法 × 各自的输入，共 " << cases
              << " 组，作者版、本单元实现与 std::sort 三方一致\n";
    return 0;
}

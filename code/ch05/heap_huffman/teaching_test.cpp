// 教学版最小堆与 Huffman 树的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
#include "teaching.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const char* what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("  FAIL: %s\n", what);
    }
}

// ---- 最小堆 ---------------------------------------------------------------

// 堆的唯一承诺：**逐个取出来是升序**。这一条覆盖了上浮和下沉两条路径。
// 变异：sift_down 里改成和较大的孩子交换 → 这里会红。
void test_heap_pops_in_ascending_order() {
    MinHeap<int> heap;
    int input[] = {5, 1, 9, 3, 7, 2, 8, 6, 4, 0};
    for (int x : input) {
        heap.insert(x);
    }
    check(heap.size() == 10, "插入 10 个之后 size==10");
    bool ascending = true;
    for (int expected = 0; expected < 10; ++expected) {
        auto got = heap.remove_min();
        if (!got.has_value() || got.value() != expected) {
            ascending = false;
        }
    }
    check(ascending, "10 个元素按 0..9 升序取出");
    check(heap.empty(), "取完之后堆空");
}

void test_heap_empty_returns_nullopt() {
    MinHeap<int> heap;
    check(heap.empty(), "新建的堆是空的");
    check(heap.size() == 0, "空堆 size 为 0");
    check(!heap.remove_min().has_value(), "空堆 remove_min 返回 nullopt");
}

// 只有一个元素时，remove_min 之后不能再去 sift_down（数组已经空了）。
void test_heap_single_element() {
    MinHeap<int> heap;
    heap.insert(42);
    check(heap.size() == 1, "插入一个之后 size==1");
    check(heap.remove_min() == 42, "取出唯一的那个");
    check(heap.empty(), "取完之后空");
    check(!heap.remove_min().has_value(), "再取返回 nullopt");
}

// 重复元素不能丢也不能多。
void test_heap_handles_duplicates() {
    MinHeap<int> heap;
    for (int i = 0; i < 5; ++i) {
        heap.insert(7);
    }
    check(heap.size() == 5, "5 个相同的元素都在");
    int count = 0;
    while (auto got = heap.remove_min()) {
        if (got.value() == 7) ++count;
    }
    check(count == 5, "5 个 7 一个不少地取出来");
}

// 扩容：初始容量之外继续插入，顺序仍然正确。
// 变异：grow() 里把 delete[] 挪到拷贝循环之前 → ASan 报 heap-use-after-free。
void test_heap_growth_preserves_order() {
    MinHeap<int> heap(1);
    for (int i = 500; i >= 1; --i) {
        heap.insert(i);
    }
    check(heap.size() == 500, "插入 500 个（触发多次扩容）");
    bool ascending = true;
    for (int expected = 1; expected <= 500; ++expected) {
        auto got = heap.remove_min();
        if (!got.has_value() || got.value() != expected) {
            ascending = false;
        }
    }
    check(ascending, "扩容之后 500 个元素仍然按升序取出");
}

// 插入与取出交错，堆序必须一直成立。
void test_heap_interleaved_operations() {
    MinHeap<int> heap;
    heap.insert(5);
    heap.insert(3);
    check(heap.remove_min() == 3, "先取到 3");
    heap.insert(1);
    heap.insert(4);
    check(heap.remove_min() == 1, "再取到 1");
    check(heap.remove_min() == 4, "然后是 4");
    check(heap.remove_min() == 5, "最后是 5");
    check(heap.empty(), "交错操作之后堆空");
}

// 变异实测：删掉拷贝构造，`MinHeap<int> b = a;` 在 -Werror 下先撞
// -Wdeprecated-copy 编译即红；放行后 ASan 报 attempting double-free。
void test_heap_copy_is_deep() {
    MinHeap<int> a;
    for (int x : {5, 1, 3}) {
        a.insert(x);
    }
    MinHeap<int> b = a;
    check(b.size() == 3, "副本长度一致");
    check(b.remove_min() == 1, "副本内容一致");
    check(a.size() == 3, "取副本不影响原堆");
    check(a.remove_min() == 1, "原堆内容没被动过");
}

void test_heap_copy_assignment_is_deep() {
    MinHeap<int> a;
    for (int x : {5, 1, 3}) {
        a.insert(x);
    }
    MinHeap<int> b(2);
    b.insert(99);
    b = a;
    check(b.size() == 3, "赋值后长度取自右边");
    check(b.remove_min() == 1, "赋值后内容取自右边");
    check(a.size() == 3, "赋值不改动右边");
}

void test_heap_self_assignment_is_safe() {
    MinHeap<int> heap;
    for (int x : {5, 1, 3}) {
        heap.insert(x);
    }
    heap = heap;
    check(heap.size() == 3, "自赋值后长度不变");
    check(heap.remove_min() == 1, "自赋值后内容不变");
}

// ---- Huffman 树 -----------------------------------------------------------

// 原书的例子：权 2、3、4、7。
// 合并过程：2+3=5 → 4+5=9 → 7+9=16，根权 16。
void test_huffman_total_weight() {
    int weights[] = {2, 3, 4, 7};
    HuffmanTree tree(weights, 4);
    check(tree.total_weight() == 16, "根的权是所有叶子权之和 = 16");
    check(tree.root() != nullptr, "非空输入建出了一棵树");
}

// Huffman 的意义在于 WPL 最小。手算：2 和 3 在第 3 层、4 在第 2 层、7 在第 1 层，
// WPL = 2*3 + 3*3 + 4*2 + 7*1 = 6 + 9 + 8 + 7 = 30。
// 变异：合并时取「最大的两个」而不是最小的两个 → WPL 变大，这里会红。
void test_huffman_wpl_is_minimal() {
    int weights[] = {2, 3, 4, 7};
    HuffmanTree tree(weights, 4);
    check(tree.weighted_path_length() == 30, "带权路径长度是 30");

    // 权全相等时，树是平衡的：4 个权 1 的叶子各在第 2 层，WPL = 4*2 = 8
    int equal_weights[] = {1, 1, 1, 1};
    HuffmanTree balanced(equal_weights, 4);
    check(balanced.total_weight() == 4, "四个 1 的总权是 4");
    check(balanced.weighted_path_length() == 8, "全等权时 WPL = 4*2 = 8");
}

void test_huffman_edge_cases() {
    HuffmanTree empty_tree(nullptr, 0);
    check(empty_tree.total_weight() == 0, "空输入得到空树，总权 0");
    check(empty_tree.root() == nullptr, "空树的根是 nullptr");
    check(empty_tree.weighted_path_length() == 0, "空树 WPL 为 0");

    int one[] = {5};
    HuffmanTree single(one, 1);
    check(single.total_weight() == 5, "单个权重的树，根权就是它");
    check(single.weighted_path_length() == 0, "只有一个叶子且它就是根，深度 0，WPL 为 0");

    int two[] = {1, 2};
    HuffmanTree pair(two, 2);
    check(pair.total_weight() == 3, "两个权重合并成 3");
    check(pair.weighted_path_length() == 3, "两个叶子各在第 1 层，WPL = 1+2 = 3");
}

void test_huffman_rejects_bad_input() {
    bool thrown = false;
    try {
        HuffmanTree tree(nullptr, 3);       // count 非零却没有数组
        (void)tree;
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "count 非零而 weights 为空指针时抛 invalid_argument");

    thrown = false;
    try {
        int bad[] = {1, -2, 3};
        HuffmanTree tree(bad, 3);
        (void)tree;
    } catch (const std::invalid_argument&) {
        thrown = true;
    }
    check(thrown, "负权抛 invalid_argument");
}

// D-001 第 3 条红线：容器内零 I/O。
void test_no_console_output() {
    std::ostringstream out, err;
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(err.rdbuf());
    {
        MinHeap<int> heap(1);
        for (int x : {3, 1, 2}) {
            heap.insert(x);
        }
        (void)heap.remove_min();
        (void)heap.remove_min();
        (void)heap.remove_min();
        (void)heap.remove_min();            // 空堆再取：原书在这里打印
        MinHeap<int> copy = heap;
        copy = heap;

        int weights[] = {2, 3, 4, 7};
        HuffmanTree tree(weights, 4);
        (void)tree.total_weight();
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(out.str().empty(), "堆与 Huffman 树没有往 stdout 打任何东西");
    check(err.str().empty(), "堆与 Huffman 树没有往 stderr 打任何东西");
}

}  // namespace

int main() {
    test_heap_pops_in_ascending_order();
    test_heap_empty_returns_nullopt();
    test_heap_single_element();
    test_heap_handles_duplicates();
    test_heap_growth_preserves_order();
    test_heap_interleaved_operations();
    test_heap_copy_is_deep();
    test_heap_copy_assignment_is_deep();
    test_heap_self_assignment_is_safe();

    test_huffman_total_weight();
    test_huffman_wpl_is_minimal();
    test_huffman_edge_cases();
    test_huffman_rejects_bad_input();

    test_no_console_output();

    std::printf("HeapHuffman(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

// 教学版最小堆与 Huffman 树的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
#include "teaching.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

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
    // 自赋值写成「先取引用别名再赋值」，而不是 `heap = heap;`：
    // clang 的 -Wself-assign-overloaded 会拒绝后者，而闸门开着 -Werror，
    // 于是整套教学版测试在 clang 上根本编不过（2026-08-17 Codex 在 macOS 上撞到）。
    // 运行时语义没变：还是同一个对象赋给它自己。
    auto& same = heap;
    heap = same;
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


// ---- Huffman 编码与译码 ---------------------------------------------------

// 原书 4.567 节那个例子：电文 abbaaadc。
// 定长编码要 2 位一个字符、共 16 位；Huffman 按频率给短码，应当更短。
void test_huffman_encode_is_shorter_than_fixed_length() {
    const char symbols[] = {'a', 'b', 'c', 'd'};
    const int weights[] = {4, 2, 1, 1};        // abbaaadc 里 a×4 b×2 c×1 d×1
    HuffmanTree tree(symbols, weights, 4);

    auto bits = tree.encode("abbaaadc");
    check(bits.has_value(), "8 个字符全在树里，编码成功");
    check(bits->size() == 14, "Huffman 编码 14 位，比定长的 16 位短");
    check(tree.weighted_path_length() == 14, "编码总长恰好等于 WPL——这不是巧合");
}

// **前缀码**：任何字符的编码都不是另一个字符编码的前缀。
// 这是能译码的前提，也是「字符只住在叶子上」的直接推论。
void test_huffman_codes_are_prefix_free() {
    const char symbols[] = {'a', 'b', 'c', 'd'};
    const int weights[] = {4, 2, 1, 1};
    HuffmanTree tree(symbols, weights, 4);

    std::string codes[4];
    bool all_found = true;
    for (int i = 0; i < 4; ++i) {
        auto c = tree.code_of(symbols[i]);
        if (!c) { all_found = false; } else { codes[i] = *c; }
    }
    check(all_found, "四个字符都查得到编码");

    bool prefix_free = true;
    for (int i = 0; i < 4; ++i) {
        for (int j = 0; j < 4; ++j) {
            if (i != j && codes[j].compare(0, codes[i].size(), codes[i]) == 0) {
                prefix_free = false;
            }
        }
    }
    check(prefix_free, "没有任何一个编码是另一个的前缀");

    // 权大的字符编码应当不比权小的长
    check(tree.code_of('a')->size() <= tree.code_of('c')->size(),
          "权 4 的 a 编码不比权 1 的 c 长");
    check(!tree.code_of('z').has_value(), "树里没有的字符返回 nullopt");
}

// 译码用的是**同一棵树**：读 0 走左、读 1 走右，到叶子就吐一个字符再回根。
// 变异：把 decode 里到叶子后不 `current = root_` → 第二个字符起全错，这里会红。
void test_huffman_round_trip() {
    const char symbols[] = {'a', 'b', 'c', 'd'};
    const int weights[] = {4, 2, 1, 1};
    HuffmanTree tree(symbols, weights, 4);

    const std::string text = "abbaaadc";
    auto bits = tree.encode(text);
    check(bits.has_value(), "编码成功");
    auto back = tree.decode(*bits);
    check(back.has_value(), "译码成功");
    check(*back == text, "编码再译码，回到原文");

    auto empty_bits = tree.encode("");
    check(empty_bits.has_value() && empty_bits->empty(), "空文本编成空比特串");
    auto empty_text = tree.decode("");
    check(empty_text.has_value() && empty_text->empty(), "空比特串译回空文本");
}

void test_huffman_decode_rejects_bad_input() {
    const char symbols[] = {'a', 'b', 'c', 'd'};
    const int weights[] = {4, 2, 1, 1};
    HuffmanTree tree(symbols, weights, 4);

    auto bits = tree.encode("abc");
    check(bits.has_value(), "先编一段出来");

    // 砍掉最后一位：走到一半没比特了，串不完整
    std::string truncated = bits->substr(0, bits->size() - 1);
    check(!tree.decode(truncated).has_value(), "半截比特串译码失败，而不是吐出半个字符");

    check(!tree.decode("012").has_value(), "出现非 0/1 字符时译码失败");
}

// 单字符的树是真实边界：从根到叶的路径是空串，那样的编码没法传。
void test_huffman_single_symbol() {
    const char symbols[] = {'x'};
    const int weights[] = {7};
    HuffmanTree tree(symbols, weights, 1);

    auto code = tree.code_of('x');
    check(code.has_value() && *code == "0", "只有一个字符时约定编码为 \"0\"，不是空串");
    auto bits = tree.encode("xxx");
    check(bits.has_value() && *bits == "000", "三个 x 编成 000");
    auto back = tree.decode("000");
    check(back.has_value() && *back == "xxx", "000 译回 xxx");
    check(!tree.decode("001").has_value(), "单字符树里出现 1 是非法的");
}

// 不带字符建的树只能算 WPL，不能编码——接口如实反映这一点。
void test_huffman_without_symbols_cannot_encode() {
    const int weights[] = {2, 3, 4, 7};
    HuffmanTree tree(weights, 4);
    check(tree.total_weight() == 16, "不带字符也能建树、算总权");
    check(!tree.code_of('a').has_value(), "没给字符，查不到任何字符的编码");
}

// 全等权 → 树是平衡的，8 个字符各得 3 位：**频率都一样时 Huffman 退化成定长编码**，
// 一位都省不下来。压缩的收益完全来自频率的不均匀。
//
// 这组输入还有一个用处：它是唯一能分辨「find_code 忘了回溯」的形状。
// 平衡树的左子树是**内部结点**，找右边的符号时会先在左边整棵失败；
// 不 pop_back 就把失败路径的残留带进了结果——实测 b 的编码从 111 变成 0011011。
// 偏斜的树（比如上面 2/3/4/7 那棵）左孩子都是叶子，失败时不入栈，反而看不出来。
void test_huffman_balanced_tree_gives_fixed_length_codes() {
    const char symbols[] = {'a', 'b', 'c', 'd', 'e', 'f', 'g', 'h'};
    const int weights[] = {1, 1, 1, 1, 1, 1, 1, 1};
    HuffmanTree tree(symbols, weights, 8);

    bool all_three_bits = true;
    for (char c : symbols) {
        auto code = tree.code_of(c);
        if (!code || code->size() != 3) {
            all_three_bits = false;
        }
    }
    check(all_three_bits, "8 个等权字符各得 3 位——全等权时 Huffman 就是定长编码");
    check(tree.weighted_path_length() == 24, "WPL = 8 x 3 = 24");

    const std::string text = "abcdefgh";
    auto bits = tree.encode(text);
    check(bits.has_value() && bits->size() == 24, "8 个字符编成 24 位");
    auto back = tree.decode(*bits);
    check(back.has_value() && *back == text, "八个字符全部编码再译码，一个不差");
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
    test_huffman_encode_is_shorter_than_fixed_length();
    test_huffman_codes_are_prefix_free();
    test_huffman_round_trip();
    test_huffman_decode_rejects_bad_input();
    test_huffman_single_symbol();
    test_huffman_without_symbols_cannot_encode();
    test_huffman_balanced_tree_gives_fixed_length_codes();

    test_no_console_output();

    std::printf("HeapHuffman(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

#include "modern.hpp"

#include "support/fault_injection.hpp"

#include <cstdio>
#include <limits>
#include <stdexcept>
#include <utility>

namespace {
int checks = 0, failures = 0;
// 与 binary_tree 及其余五个单元统一：断言必须带描述，否则失败时看不出是哪一条。
void check(bool ok, const char* what) {
    ++checks;
    if (!ok) {
        ++failures;
        std::printf("  FAIL: %s\n", what);
    }
}
}  // namespace


// 复核补充（Claude，2026-08-12）：MinHeap 拷贝构造失败时必须清理已分配的缓冲区。
//
// 这条路径此前**没有任何用例走到**——把 `catch (...) { delete[] data_; throw; }`
// 里的 delete[] 去掉，闸门照样全绿。用一个「移动 noexcept、拷贝会抛」的类型覆盖它：
// 共享探针里的 Fragile 在 MinHeap 上根本实例化不了（它的移动赋值不是 noexcept），
// 所以需要 NothrowMoveThrowingCopy 这个形状。
void test_copy_constructor_cleans_up_on_throw() {
    using Probe = dsa::testing::NothrowMoveThrowingCopy;
    dsa::MinHeap<Probe> source;
    for (int i = 5; i >= 1; --i) {
        source.insert(Probe(i));
    }

    Probe::reset(3);  // 拷贝第 3 个元素时抛
    bool threw = false;
    try {
        dsa::MinHeap<Probe> copy(source);  // 若 catch 里漏了 delete[]，这里泄漏整块缓冲区
        (void)copy;
    } catch (const std::runtime_error&) {
        threw = true;
    }
    Probe::reset();

    check(threw, "拷贝构造中途的异常如实抛出");
    // 源堆必须完好——拷贝失败不该影响被拷贝方
    check(source.size() == 5, "拷贝失败后源堆长度不变");
    auto smallest = source.remove_min();
    check(smallest.has_value() && smallest->v == 1, "拷贝失败后源堆内容完好");
}

int main() {
    test_copy_constructor_cleans_up_on_throw();
    dsa::MinHeap<int> heap;
    for (int value : {5, 1, 4, 2, 3}) heap.insert(value);
    dsa::MinHeap<int> copy = heap;
    dsa::MinHeap<int>& alias = copy;
    copy = alias;
    for (int expected = 1; expected <= 5; ++expected) {
        check(heap.remove_min() == expected, "最小元素按序弹出");
        check(copy.remove_min() == expected, "副本独立且顺序一致");
    }
    check(!heap.remove_min(), "空堆 remove_min 返回 nullopt");
    dsa::MinHeap<int> moved = std::move(copy);
    check(copy.empty() && moved.empty(), "移动后被移动方为空");

    int weights[] = {5, 7, 10, 15, 20, 45};
    dsa::HuffmanTree tree(weights, 6);
    check(tree.total_weight() == 102, "Huffman 树总权重 = 各叶权重之和（5+7+10+15+20+45）");
    dsa::HuffmanTree single(weights, 1);
    check(single.total_weight() == 5, "单叶 Huffman 树的总权重就是该叶权重");
    dsa::HuffmanTree empty(nullptr, 0);
    check(empty.total_weight() == 0, "空 Huffman 树总权重为 0");
    bool rejected_null = false;
    try { dsa::HuffmanTree invalid(nullptr, 1); }
    catch (const std::invalid_argument&) { rejected_null = true; }
    check(rejected_null, "权重数组为空指针但个数非零时抛 invalid_argument");
    int negative[] = {-1};
    bool rejected_negative = false;
    try { dsa::HuffmanTree invalid(negative, 1); }
    catch (const std::invalid_argument&) { rejected_negative = true; }
    check(rejected_negative, "负权重被拒绝（invalid_argument）");
    int overflowing[] = {std::numeric_limits<int>::max(), 1};
    bool rejected_overflow = false;
    try { dsa::HuffmanTree invalid(overflowing, 2); }
    catch (const std::overflow_error&) { rejected_overflow = true; }
    check(rejected_overflow, "权重相加溢出被拒绝（overflow_error）");

    std::printf("HeapHuffman: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

#include "modern.hpp"

#include "support/fault_injection.hpp"
#include "teaching.hpp"   // 交叉核对：教学版 HuffmanTree::weighted_path_length 真建树求 WPL

#include <algorithm>
#include <cstdint>
#include <cstdio>
#include <functional>
#include <limits>
#include <stdexcept>
#include <utility>
#include <vector>

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

namespace {
std::uint64_t lcg_state = 0x2545F4914F6CDD1DULL;
std::uint64_t next_random() { lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL; return lcg_state >> 33; }

void test_heap_with_comparator() {
    dsa::MinHeap<int, std::greater<int>> max_heap;
    check(max_heap.peek() == nullptr, "空堆 peek 为 nullptr");
    for (int value : {3, 9, 1, 7, 9}) max_heap.insert(value);
    check(max_heap.peek() != nullptr && *max_heap.peek() == 9, "MinHeap<T, std::greater<T>> 堆顶是最大元");
    std::vector<int> out;
    while (auto value = max_heap.remove_min()) out.push_back(*value);
    check(out == std::vector<int>({9, 9, 7, 3, 1}), "比较器为 greater 时按从大到小弹出");
}

void test_running_median() {
    dsa::RunningMedian<int> empty;
    check(!empty.median().has_value() && empty.size() == 0, "对顶堆：空时中位数为 nullopt");

    bool all_match = true;
    for (int round = 0; round < 60; ++round) {
        dsa::RunningMedian<int> running;
        std::vector<int> seen;
        const int n = 1 + static_cast<int>(next_random() % 200);
        const int range = 1 + static_cast<int>(next_random() % 20);   // 小值域：大量重复
        for (int i = 0; i < n; ++i) {
            const int value = static_cast<int>(next_random() % static_cast<std::uint64_t>(range)) - range / 2;
            running.insert(value);
            seen.push_back(value);
            std::vector<int> sorted = seen;
            std::sort(sorted.begin(), sorted.end());
            const int lower_median = sorted[(sorted.size() - 1) / 2];
            all_match = all_match && running.median() == lower_median && running.size() == seen.size();
        }
    }
    check(all_match, "对顶堆：随机含重复序列，每次插入后都等于排序后的下中位数");

    dsa::RunningMedian<int> ascending;
    bool ascending_ok = true;
    for (int i = 1; i <= 1000; ++i) { ascending.insert(i); ascending_ok = ascending_ok && ascending.median() == (i + 1) / 2; }
    check(ascending_ok, "对顶堆：升序插入（全进大半边）仍每步正确");
    dsa::RunningMedian<int> descending;
    bool descending_ok = true;
    for (int i = 1000; i >= 1; --i) { descending.insert(i); const int k = 1001 - i; descending_ok = descending_ok && descending.median() == 1000 - k / 2; }
    check(descending_ok, "对顶堆：降序插入（全进小半边）仍每步正确");
}

std::uint64_t teaching_wpl(const std::vector<int>& weights) {
    const HuffmanTree tree(weights.data(), weights.size());
    return static_cast<std::uint64_t>(tree.weighted_path_length());
}

void test_huffman_wpl_batched() {
    check(dsa::huffman_wpl_batched(nullptr, 0) == 0, "批量 Huffman：空输入 WPL 为 0");
    const dsa::WeightGroup lone[] = {{12345, 1}};
    check(dsa::huffman_wpl_batched(lone, 1) == 0, "批量 Huffman：只有一个叶子时 WPL 为 0");
    const dsa::WeightGroup book[] = {{7, 1}, {2, 1}, {4, 1}, {3, 1}};
    check(dsa::huffman_wpl_batched(book, 4) == 30, "批量 Huffman：原书权 2、3、4、7 的 WPL = 30");
    const dsa::WeightGroup text[] = {{1, 2}, {2, 1}, {4, 1}, {9, 0}};
    check(dsa::huffman_wpl_batched(text, 4) == 14, "批量 Huffman：abbaaadc 的频率组 WPL = 14（count 为 0 的组被忽略）");

    bool all_match = true;
    for (int round = 0; round < 300; ++round) {
        const std::size_t k = 1 + next_random() % 8;
        std::vector<dsa::WeightGroup> groups;
        std::vector<int> expanded;
        for (std::size_t i = 0; i < k; ++i) {
            const std::uint64_t weight = next_random() % 30;          // 含 0 权与重复权
            const std::uint64_t count = next_random() % 9;            // 含 count 0
            groups.push_back({weight, count});
            for (std::uint64_t c = 0; c < count; ++c) expanded.push_back(static_cast<int>(weight));
        }
        all_match = all_match && dsa::huffman_wpl_batched(groups.data(), groups.size()) == teaching_wpl(expanded);
    }
    check(all_match, "批量 Huffman：300 组随机小规模输入，展开成单个叶子后与教学版真建树的 WPL 一致");

    // 等权 n 个叶子：最优是尽量满的树，WPL = n·L + 2(n − 2^L)，L = ⌊log2 n⌋。
    const auto equal_weights = [](std::uint64_t n) {
        std::uint64_t level = 0;
        while ((std::uint64_t{1} << (level + 1)) <= n) ++level;
        return n * level + 2 * (n - (std::uint64_t{1} << level));
    };
    const dsa::WeightGroup billion[] = {{1, 1000000000ULL}};
    check(dsa::huffman_wpl_batched(billion, 1) == equal_weights(1000000000ULL), "批量 Huffman：1e9 个等权叶子与闭式公式一致（不建结点）");
    const dsa::WeightGroup power[] = {{1, std::uint64_t{1} << 30}};
    check(dsa::huffman_wpl_batched(power, 1) == (std::uint64_t{1} << 30) * 30, "批量 Huffman：2^30 个等权叶子是满二叉树，WPL = 30·2^30");
    const dsa::WeightGroup mixed[] = {{3, 1000000000ULL}, {1000000, 999999999ULL}, {1, 7}, {5, 1}};
    std::uint64_t mixed_wpl = dsa::huffman_wpl_batched(mixed, 4);
    check(mixed_wpl > 0, "批量 Huffman：多组 1e9 级 count 快速算完");

    const dsa::WeightGroup overflowing[] = {{std::uint64_t{1} << 63, 2}};
    bool overflow = false;
    try { (void)dsa::huffman_wpl_batched(overflowing, 1); } catch (const std::overflow_error&) { overflow = true; }
    check(overflow, "批量 Huffman：合并权超出 uint64 抛 overflow_error，不回绕");
    bool null_rejected = false;
    try { (void)dsa::huffman_wpl_batched(nullptr, 2); } catch (const std::invalid_argument&) { null_rejected = true; }
    check(null_rejected, "批量 Huffman：非空输入给空指针抛 invalid_argument");
}
}  // namespace

int main() {
    test_copy_constructor_cleans_up_on_throw();
    test_heap_with_comparator();
    test_running_median();
    test_huffman_wpl_batched();
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

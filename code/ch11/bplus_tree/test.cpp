#include "modern.hpp"

#include <cstdio>
#include <random>
#include <set>
#include <stdexcept>
#include <string>
#include <vector>

namespace {
int checks = 0;
int failures = 0;
void check(bool condition, const char* name) {
    ++checks;
    if (!condition) {
        ++failures;
        std::printf("  FAIL: %s\n", name);
    }
}

using dsa::index::BPlusTree;
using Rows = std::vector<BPlusTree::Row>;

Rows rows(const std::vector<int>& keys) {
    Rows out;
    out.reserve(keys.size());
    for (const int key : keys) {
        out.emplace_back(key, "r" + std::to_string(key));
    }
    return out;
}

template <typename F>
bool throws_invalid(F&& f) {
    try {
        f();
    } catch (const std::invalid_argument&) {
        return true;
    }
    return false;
}

const char* const kFig11_9 =
    "[40,70,90] / [20,30,40] [50,60,70] [80,90] / "
    "[5,10,20] [25,30] [35,40] [45,48,50] [55,60] [65,70] [75,80] [85,90]";

/// 原书图 11.9 的 3 阶 B+ 树，叶 e～l、内部结点 b、c、d、根 a，原样搬进程序。
/// 叶 f 按勘误E48 用 (25,30)，扫描件印作 (5,30)。
BPlusTree fig_11_9() {
    return BPlusTree::assemble(
        3,
        {rows({5, 10, 20}), rows({25, 30}), rows({35, 40}), rows({45, 48, 50}), rows({55, 60}),
         rows({65, 70}), rows({75, 80}), rows({85, 90})},
        {{3, 3, 2}, {3}});
}

void test_fig_11_9_is_a_valid_tree_of_the_book_definition() {
    const BPlusTree tree = fig_11_9();
    check(tree.to_string() == kFig11_9, "图 11.9 原样装出");
    check(tree.validate(), "图 11.9 满足原书 11.4.2 的定义");
    check(tree.size() == 18 && tree.height() == 3 && tree.leaf_count() == 8,
          "图 11.9 18 个关键码、3 层、8 个叶");
    // 勘误E48：扫描件把叶 f 印成 (5,30)。5 不大于左边那棵子树的路标 20，不是合法的 B+ 树。
    check(throws_invalid([] {
              (void)BPlusTree::assemble(
                  3,
                  {rows({5, 10, 20}), rows({5, 30}), rows({35, 40}), rows({45, 48, 50}),
                   rows({55, 60}), rows({65, 70}), rows({75, 80}), rows({85, 90})},
                  {{3, 3, 2}, {3}});
          }),
          "勘误E48 印作 (5,30) 的叶 f 被 validate 拒绝");
    // 叶最多 m 个关键码：3 阶的叶放 4 个就超了。
    check(throws_invalid([] {
              (void)BPlusTree::assemble(3, {rows({1, 2, 3, 4}), rows({5, 6})}, {{2}});
          }),
          "11.4.2 叶超过 m 个关键码被拒绝");
    // 除根外的叶至少 ⌈m/2⌉ 个关键码。
    check(throws_invalid([] {
              (void)BPlusTree::assemble(3, {rows({1, 2}), rows({3})}, {{2}});
          }),
          "11.4.2 叶低于 ⌈m/2⌉ 个关键码被拒绝");
    // 根若是内部结点，至少两个孩子。
    check(throws_invalid([] { (void)BPlusTree::assemble(3, {rows({1, 2})}, {{1}}); }),
          "11.4.2 根只有一个孩子被拒绝");
}

void test_insert_15_reproduces_fig_11_10() {
    BPlusTree tree = fig_11_9();
    tree.reset_counters();
    check(tree.insert(15, "r15"), "图 11.10 插入 15 是新增");
    check(tree.to_string() ==
              "[40,90] / [20,40] [70,90] / [10,20] [30,40] [50,60,70] [80,90] / "
              "[5,10] [15,20] [25,30] [35,40] [45,48,50] [55,60] [65,70] [75,80] [85,90]",
          "图 11.10 叶 e、内部结点 b、根 a 相继分裂，树增高一层");
    check(tree.height() == 4, "图 11.10 树高 4");
    check(tree.validate(), "图 11.10 结构合法");
    check(tree.page_reads() == 3, "图 11.10 读盘 3 次（a, b, e）");
    check(tree.page_writes() == 7, "图 11.10 写盘 7 次（e, e', b, b', a, a', t）");
    // 习题 5：再插入 47，叶 h 与内部结点 c 分裂，a' 还有空位，没有裂到根。
    tree.reset_counters();
    tree.insert(47, "r47");
    check(tree.to_string() ==
              "[40,90] / [20,40] [50,70,90] / [10,20] [30,40] [47,50] [60,70] [80,90] / "
              "[5,10] [15,20] [25,30] [35,40] [45,47] [48,50] [55,60] [65,70] [75,80] [85,90]",
          "习题 5 插入 47：叶裂、c 裂，a' 收下后停住");
    check(tree.page_reads() == 4 && tree.page_writes() == 5, "习题 5 读 t, a', c, h；写 h, h', c, c', a'");
    check(tree.validate(), "习题 5 结构合法");
    // 叶分裂只复写：路标 10、20 都还在叶上。
    check(tree.find(10).has_value() && tree.find(20).has_value(), "11.4.2 分裂后的路标在叶上仍保留");
}

void test_erase_75_reproduces_fig_11_11() {
    BPlusTree tree = fig_11_9();
    tree.reset_counters();
    check(tree.erase(75), "图 11.11 删除 75");
    check(tree.to_string() ==
              "[40,60,90] / [20,30,40] [50,60] [70,90] / "
              "[5,10,20] [25,30] [35,40] [45,48,50] [55,60] [65,70] [80,85,90]",
          "图 11.11 叶 k 与右邻 l 合并，d 向左邻 c 借来 j，根的路标 70 改成 60");
    check(tree.validate(), "图 11.11 结构合法");
    check(tree.page_reads() == 5, "图 11.11 读盘 5 次（a, d, k, l, c）");
    check(tree.page_writes() == 4, "图 11.11 写盘 4 次（k, d, c, a），释放 l 不写");
}

void test_mixed_tree_fig_11_12_to_11_14() {
    // 混合型 B+ 树：内部结点阶 4、叶阶 5。勘误E55：原书图 11.12～11.14 按「k 个孩子 k−1 个分界码、
    // 取右子树最小码」画，与 11.4.2 定义⑤不同；按定义⑤装进来，路标取值不同，但每一步的形状逐结点相同。
    BPlusTree tree = BPlusTree::assemble(
        4, 5,
        {rows({10, 12, 15}), rows({18, 19, 20, 21, 23}), rows({25, 30, 31}), rows({33, 36, 37, 39}),
         rows({40, 45, 47}), rows({48, 50, 52})},
        {{4, 2}, {2}});
    check(tree.to_string() ==
              "[39,52] / [15,23,31,39] [47,52] / "
              "[10,12,15] [18,19,20,21,23] [25,30,31] [33,36,37,39] [40,45,47] [48,50,52]",
          "图 11.12 混合型 B+ 树按定义⑤装出（叶 e 满 5 个，b 满 4 个孩子）");
    tree.insert(22, "r22");
    check(tree.to_string() ==
              "[23,39,52] / [15,20,23] [31,39] [47,52] / "
              "[10,12,15] [18,19,20] [21,22,23] [25,30,31] [33,36,37,39] [40,45,47] [48,50,52]",
          "图 11.13 插入 22：叶 e 分 3 + 3，b 有 5 个孩子分 3 + 2，与图同形");
    tree.erase(40);
    check(tree.to_string() ==
              "[23,52] / [15,20,23] [31,39,52] / "
              "[10,12,15] [18,19,20] [21,22,23] [25,30,31] [33,36,37,39] [45,47,48,50,52]",
          "图 11.14 删除 40：h 与 i 合并，c 只剩一个孩子并进 b'，与图同形");
    check(tree.validate(), "图 11.14 结构合法");
    check(throws_invalid([] { BPlusTree bad(4, 1); }), "11.4.2 叶阶小于 2 被拒绝");
}

void test_final_review_11_17() {
    // 期末复习题库 11-17：5 阶 B+ 树依次插入 10..90。
    BPlusTree tree(5);
    for (int key = 10; key <= 90; key += 10) {
        tree.insert(key, "r" + std::to_string(key));
    }
    check(tree.to_string() == "[30,60,90] / [10,20,30] [40,50,60] [70,80,90]",
          "期末 11-17 叶放进第 6 个才分裂，3 + 3");
    check(tree.validate(), "期末 11-17 结构合法");
}

void test_split_keeps_the_larger_half_on_the_left() {
    // m+1 是奇数时（m 为偶数）两半不一样大：左半 ⌈(m+1)/2⌉ 项，右半其余，两半都不低于 ⌈m/2⌉。
    BPlusTree tree(4);
    for (int key = 1; key <= 5; ++key) {
        tree.insert(key, "r" + std::to_string(key));
    }
    check(tree.to_string() == "[3,5] / [1,2,3] [4,5]", "11.4.2 4 阶叶放进第 5 个：分成 3 + 2");
}

void test_root_collapse_frees_the_old_root() {
    // [2,4] / [1,2] [3,4] 删 1：叶下溢、右邻借不出 → 合并；根只剩一个孩子 → 删去根，树矮一层。
    BPlusTree tree = BPlusTree::bulk_load(3, rows({1, 2, 3, 4}), 2);
    tree.reset_counters();
    check(tree.erase(1), "11.4.2 删除 1");
    check(tree.to_string() == "[2,3,4]", "11.4.2 合并后根只剩一个孩子，孩子当新根");
    check(tree.page_reads() == 3, "11.4.2 读根、叶、右邻");
    check(tree.page_writes() == 1, "11.4.2 只写合并后的叶；旧根与右邻释放，不写");

    // 删 4：下溢的叶并进左邻，它自己那一页释放——刚改过也不写回。
    BPlusTree other = BPlusTree::bulk_load(3, rows({1, 2, 3, 4}), 2);
    other.reset_counters();
    check(other.erase(4) && other.to_string() == "[1,2,3]", "11.4.2 下溢的叶并进左邻");
    check(other.page_writes() == 1, "11.4.2 被并掉的那一页即使刚改过也不写回");
}

void test_new_maximum_updates_the_right_spine() {
    BPlusTree tree = fig_11_9();
    tree.reset_counters();
    tree.insert(95, "r95");
    check(tree.to_string() ==
              "[40,70,95] / [20,30,40] [50,60,70] [80,95] / "
              "[5,10,20] [25,30] [35,40] [45,48,50] [55,60] [65,70] [75,80] [85,90,95]",
          "11.4.2 比全树都大的关键码：最右路径上的路标都改成它");
    check(tree.validate(), "11.4.2 新最大码插入后结构合法");
    check(tree.page_reads() == 3 && tree.page_writes() == 3, "11.4.2 读 a, d, l；写 a, d, l");
}

void test_erase_without_underflow_leaves_the_bound_alone() {
    BPlusTree tree = fig_11_9();
    tree.reset_counters();
    check(tree.erase(50), "11.4.2 删除 50");
    // 叶 h 剩 (45,48) 没下溢；c 里的 50 留着当上界，原书：不必对应一个真实存在的键。
    check(tree.to_string() ==
              "[40,70,90] / [20,30,40] [50,60,70] [80,90] / "
              "[5,10,20] [25,30] [35,40] [45,48] [55,60] [65,70] [75,80] [85,90]",
          "11.4.2 没下溢时上层路标不动");
    check(tree.validate(), "11.4.2 过期的上界仍是合法路标");
    check(tree.page_reads() == 3 && tree.page_writes() == 1, "11.4.2 只写回叶 h");
    check(!tree.find(50).has_value(), "11.4.2 50 已删掉");
    tree.insert(49, "r49");
    check(tree.find(49).has_value() && tree.validate(), "11.4.2 49 仍按上界 50 进叶 h");
}

void test_bulk_load() {
    const BPlusTree tree = BPlusTree::bulk_load(3, rows({10, 20, 30, 50, 70, 90}), 2);
    check(tree.to_string() == "[20,50,90] / [10,20] [30,50] [70,90]", "11.2 批量装入：按页填满、自底向上");
    check(tree.validate(), "11.2 装入后结构合法");
    check(tree.find(50).has_value() && *tree.find(50) == "r50", "11.2 点查询");
    check(!tree.find(60).has_value(), "11.2 查不到返回 nullopt");

    // 最后一页不够下限：匀不开就并进前一页，匀得开就匀。
    check(BPlusTree::bulk_load(3, rows({1, 2, 3, 4, 5}), 2).to_string() == "[2,5] / [1,2] [3,4,5]",
          "11.2 末页 1 个并进前一页");
    check(BPlusTree::bulk_load(5, rows({1, 2, 3, 4, 5, 6, 7}), 5).to_string() ==
              "[4,7] / [1,2,3,4] [5,6,7]",
          "11.2 末页 2 个从前一页匀成 4 + 3");
    for (std::size_t n = 0; n <= 60; ++n) {
        std::vector<int> keys;
        for (std::size_t i = 0; i < n; ++i) {
            keys.push_back(static_cast<int>(i));
        }
        for (const std::size_t order : {3, 4, 5}) {
            for (std::size_t per = (order + 1) / 2; per <= order; ++per) {
                const BPlusTree loaded = BPlusTree::bulk_load(order, rows(keys), per);
                check(loaded.validate() && loaded.size() == n, "11.2 各种规模、阶数、装填量都合法");
            }
        }
    }
}

void test_range_scan_walks_the_leaf_chain() {
    BPlusTree tree = fig_11_9();
    std::vector<int> keys;
    for (const auto& row : tree.range(33, 62)) {
        keys.push_back(row.first);
    }
    check(keys == std::vector<int>({35, 40, 45, 48, 50, 55, 60}), "11.4 范围扫描 33..62");
    check(tree.range(0, 200).size() == 18, "11.4 全范围");
    check(tree.range(41, 44).empty(), "11.4 区间内没有关键码");
    check(tree.range(80, 10).empty(), "11.4 下限大于上限");
    check(tree.range(90, 90).size() == 1, "11.4 单点区间");
    check(tree.range(91, 200).empty(), "11.4 下限比全树都大");

    // 范围扫描的价值：找到起点之后横着走，不再爬回内部结点。
    tree.reset_counters();
    const auto wide = tree.range(0, 200);
    const std::size_t scan_reads = tree.page_reads();
    check(scan_reads == 2 + 8, "11.4 全范围扫描：下降 2 页 + 8 个叶");
    tree.reset_counters();
    for (const auto& row : wide) {
        (void)tree.find(row.first);
    }
    check(scan_reads < tree.page_reads(), "11.4 一次范围扫描比逐个点查省页");
}

void test_page_counters_for_lookups() {
    BPlusTree tree = fig_11_9();
    tree.reset_counters();
    (void)tree.find(48);
    check(tree.page_reads() == 3 && tree.page_writes() == 0, "11.4 三层树点查询读 3 页、不写");
    tree.reset_counters();
    (void)tree.find(999);
    check(tree.page_reads() == 3, "11.4 查不到也一样走到叶");
}

void test_erase_down_to_empty() {
    BPlusTree tree = fig_11_9();
    check(!tree.erase(999), "11.4 删不存在（比全树都大）的关键码返回 false");
    check(!tree.erase(42), "11.4 删不存在的关键码返回 false");
    for (const int key : {75, 15, 40, 5, 90, 48, 25, 60, 85, 10, 70, 35, 55, 20, 80, 45, 30, 65, 50}) {
        const bool present = key != 15;
        check(tree.erase(key) == present, "11.4 逐个删除");
        check(tree.validate(), "11.4 每一步删除后结构合法");
    }
    check(tree.size() == 0 && tree.height() == 1, "11.4 删空后树塌回一层");
    check(tree.range(0, 100).empty() && !tree.find(10).has_value(), "11.4 空树查不到");
    check(tree.insert(7, "r7") && tree.validate(), "11.4 删空后还能再插");
}

void test_leaf_chain_survives_merges() {
    BPlusTree tree(3);
    for (int key = 1; key <= 20; ++key) {
        tree.insert(key, "r" + std::to_string(key));
    }
    check(tree.validate(), "11.4 顺序插入 20 个关键码");
    for (int key = 1; key <= 20; key += 2) {
        tree.erase(key);
    }
    check(tree.validate(), "11.4 删掉一半之后结构合法");
    const auto all = tree.range(0, 100);
    check(all.size() == 10, "11.4 合并后叶链没断，范围扫描仍然完整");
    for (std::size_t i = 1; i < all.size(); ++i) {
        check(all[i - 1].first < all[i].first, "11.4 叶链保持升序");
    }
}

void test_overwrite_and_argument_checks() {
    BPlusTree tree(3);
    check(tree.insert(1, "a"), "11.4 新关键码返回 true");
    tree.reset_counters();
    check(!tree.insert(1, "b"), "11.4 覆盖已有关键码返回 false");
    check(tree.page_writes() == 1, "11.4 覆盖只写回一个叶");
    check(tree.size() == 1 && *tree.find(1) == "b", "11.4 覆盖后取到新值");

    check(throws_invalid([] { BPlusTree bad(2); }), "11.4 阶数小于 3 被拒绝");
    check(throws_invalid([] { (void)BPlusTree::bulk_load(3, rows({5, 5}), 2); }),
          "11.2 批量装入要求关键码严格递增");
    check(throws_invalid([] { (void)BPlusTree::bulk_load(3, rows({1, 2, 3}), 4); }),
          "11.2 每页装入量超过 m 被拒绝");
    check(throws_invalid([] { (void)BPlusTree::bulk_load(5, rows({1, 2, 3}), 2); }),
          "11.2 每页装入量低于 ⌈m/2⌉ 被拒绝");
    check(BPlusTree::bulk_load(3, {}, 2).size() == 0, "11.2 空输入装入空树");
    check(throws_invalid([] { (void)BPlusTree::assemble(3, {rows({1, 2}), rows({3, 4})}, {{3}}); }),
          "11.4 布局和下层结点数对不上被拒绝");
}

void test_random_operations_keep_the_invariants() {
    // 固定种子：失败可复现。每一步之后都查结构不变量，结束时内容也要对得上。
    std::mt19937 rng(20261120);
    for (const std::size_t order : {3, 4, 5, 6}) {
        BPlusTree tree(order);
        std::set<int> mirror;
        std::uniform_int_distribution<int> keys(0, 400);
        bool valid_throughout = true;
        bool results_match = true;
        for (int step = 0; step < 3000; ++step) {
            const int key = keys(rng);
            if ((step % 3) == 2) {
                results_match = results_match && tree.erase(key) == (mirror.erase(key) == 1);
            } else {
                results_match =
                    results_match && tree.insert(key, std::to_string(key)) == mirror.insert(key).second;
            }
            valid_throughout = valid_throughout && tree.validate();
        }
        check(results_match, "11.4 插删结果与 std::set 一致");
        check(valid_throughout, "11.4 每一步之后结构都合法");
        check(tree.size() == mirror.size(), "11.4 随机操作后计数一致");

        std::vector<int> scanned;
        for (const auto& row : tree.range(-1, 1000)) {
            scanned.push_back(row.first);
        }
        check(scanned == std::vector<int>(mirror.begin(), mirror.end()), "11.4 叶链扫描等于有序集合");

        bool all_found = true;
        for (const int key : mirror) {
            all_found = all_found && tree.find(key).has_value();
        }
        check(all_found, "11.4 集合里的关键码全部查得到");
    }
}
}  // namespace

int main() {
    test_fig_11_9_is_a_valid_tree_of_the_book_definition();
    test_insert_15_reproduces_fig_11_10();
    test_erase_75_reproduces_fig_11_11();
    test_mixed_tree_fig_11_12_to_11_14();
    test_final_review_11_17();
    test_split_keeps_the_larger_half_on_the_left();
    test_root_collapse_frees_the_old_root();
    test_new_maximum_updates_the_right_spine();
    test_erase_without_underflow_leaves_the_bound_alone();
    test_bulk_load();
    test_range_scan_walks_the_leaf_chain();
    test_page_counters_for_lookups();
    test_erase_down_to_empty();
    test_leaf_chain_survives_merges();
    test_overwrite_and_argument_checks();
    test_random_operations_keep_the_invariants();
    std::printf("BPlusTree: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

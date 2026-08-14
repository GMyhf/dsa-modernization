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

std::vector<std::pair<int, std::string>> rows(const std::vector<int>& keys) {
    std::vector<std::pair<int, std::string>> out;
    out.reserve(keys.size());
    for (const int key : keys) {
        out.emplace_back(key, "r" + std::to_string(key));
    }
    return out;
}

/// 11.2 静态多分树：按页装满、自底向上建层。得到的正是 11.4 图里那棵 3 阶 B+ 树。
BPlusTree book_tree() {
    return BPlusTree::bulk_load(3, rows({10, 20, 30, 50, 70, 90}), 2);
}

void test_bulk_load_matches_the_book_figure() {
    const BPlusTree tree = book_tree();
    check(tree.to_string() == "[30,70] / [10,20] [30,50] [70,90]",
          "11.4 批量装入得到书上那棵树");
    check(tree.validate(), "11.4 装入后结构合法");
    check(tree.size() == 6 && tree.height() == 2 && tree.leaf_count() == 3,
          "11.4 6 个关键码、2 层、3 个叶");
    check(tree.find(50).has_value() && *tree.find(50) == "r50", "11.4 点查询");
    check(!tree.find(60).has_value(), "11.4 查不到返回 nullopt");
}

void test_insert_60_splits_all_the_way_to_the_root() {
    // 原书 11.4.1 的分裂规则：取中位数作分界。这一段逐字对应 book/ch11-index.md 的例子。
    BPlusTree tree = book_tree();
    tree.insert(60, "r60");
    check(tree.to_string() == "[50] / [30] [70] / [10,20] [30] [50,60] [70,90]",
          "11.4 插入 60：叶裂 → 根裂 → 树增高一层");
    check(tree.height() == 3, "11.4 插入 60 后树高 3");
    check(tree.validate(), "11.4 插入 60 后结构合法");
    // 叶分裂是复写：50 上推之后，叶上仍然留着 50。
    check(tree.find(50).has_value(), "11.4 叶分裂的分界码在叶上仍保留");

    // 习题 5：再插入 65，这次为什么没有裂到根。
    tree.insert(65, "r65");
    check(tree.to_string() == "[50] / [30] [60,70] / [10,20] [30] [50] [60,65] [70,90]",
          "11.4 插入 65：叶裂但父结点没满，不再上传");
    check(tree.height() == 3, "11.4 插入 65 树高不变");
    check(tree.validate(), "11.4 插入 65 后结构合法");
}

void test_range_scan_walks_the_leaf_chain() {
    BPlusTree tree = book_tree();
    tree.insert(60, "r60");
    const auto scan = tree.range(35, 80);
    std::vector<int> keys;
    for (const auto& row : scan) {
        keys.push_back(row.first);
    }
    check(keys == std::vector<int>({50, 60, 70}), "11.4 范围扫描 35..80");
    check(scan.front().second == "r50", "11.4 范围扫描带回记录");

    check(tree.range(0, 200).size() == 7, "11.4 全范围");
    check(tree.range(31, 49).empty(), "11.4 区间内没有关键码");
    check(tree.range(80, 10).empty(), "11.4 下限大于上限");
    check(tree.range(90, 90).size() == 1, "11.4 单点区间");

    // 范围扫描的价值：找到起点之后横着走，不再爬回内部结点。
    tree.reset_counters();
    const auto wide = tree.range(0, 200);
    const std::size_t scan_reads = tree.page_reads();
    tree.reset_counters();
    for (const auto& row : wide) {
        (void)tree.find(row.first);
    }
    check(scan_reads < tree.page_reads(), "11.4 一次范围扫描比逐个点查省页");
}

void test_page_counters() {
    BPlusTree tree = book_tree();
    tree.reset_counters();
    (void)tree.find(50);
    check(tree.page_reads() == 2, "11.4 两层树的点查询读两页（根 + 叶）");
    check(tree.page_writes() == 0, "11.4 查询不写页");

    tree.reset_counters();
    (void)tree.find(999);
    check(tree.page_reads() == 2, "11.4 查不到也一样读两页");
}

void test_erase_borrows_then_merges() {
    BPlusTree tree = book_tree();
    check(tree.erase(10), "11.4 删掉存在的关键码");
    check(!tree.erase(10), "11.4 重复删除返回 false");
    check(!tree.erase(999), "11.4 删不存在的关键码返回 false");
    check(tree.size() == 5, "11.4 删除后计数下降");
    check(tree.validate(), "11.4 借位/合并后结构仍合法");

    // 一路删空：每一步都必须保持结构合法，叶链也不能断。
    BPlusTree shrinking = book_tree();
    shrinking.insert(60, "r60");
    for (const int key : {30, 70, 90, 50, 60, 10, 20}) {
        check(shrinking.erase(key), "11.4 逐个删除");
        check(shrinking.validate(), "11.4 每一步删除后结构合法");
    }
    check(shrinking.size() == 0 && shrinking.height() == 1, "11.4 删空后树塌回一层");
    check(shrinking.range(0, 100).empty(), "11.4 空树的范围扫描");
    check(!shrinking.find(10).has_value(), "11.4 空树查不到");
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
    // 合并时如果忘了接叶链，范围扫描会漏掉后面的叶。
    const auto all = tree.range(0, 100);
    check(all.size() == 10, "11.4 合并后叶链没断，范围扫描仍然完整");
    for (std::size_t i = 1; i < all.size(); ++i) {
        check(all[i - 1].first < all[i].first, "11.4 叶链保持升序");
    }
}

void test_overwrite_and_argument_checks() {
    BPlusTree tree(3);
    check(tree.insert(1, "a"), "11.4 新关键码返回 true");
    check(!tree.insert(1, "b"), "11.4 覆盖已有关键码返回 false");
    check(tree.size() == 1 && *tree.find(1) == "b", "11.4 覆盖后取到新值");

    bool threw = false;
    try {
        BPlusTree bad(2);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "11.4 阶数小于 3 被拒绝");

    threw = false;
    try {
        (void)BPlusTree::bulk_load(3, rows({5, 5}), 2);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "11.2 批量装入要求关键码严格递增");

    threw = false;
    try {
        (void)BPlusTree::bulk_load(3, rows({1, 2, 3}), 9);
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "11.2 每页装入量超过上限被拒绝");

    check(BPlusTree::bulk_load(3, {}, 1).size() == 0, "11.2 空输入装入空树");
}

void test_random_operations_keep_the_invariants() {
    // 固定种子：失败可复现。随机插删之后，结构不变量和内容都要对得上。
    std::mt19937 rng(20260814);
    for (const std::size_t order : {3, 4, 6}) {
        BPlusTree tree(order);
        std::set<int> mirror;
        std::uniform_int_distribution<int> keys(0, 400);
        for (int step = 0; step < 3000; ++step) {
            const int key = keys(rng);
            if ((step % 3) == 2) {
                check(tree.erase(key) == (mirror.erase(key) == 1), "11.4 删除结果与参照一致");
            } else {
                check(tree.insert(key, std::to_string(key)) == mirror.insert(key).second,
                      "11.4 插入结果与参照一致");
            }
        }
        check(tree.validate(), "11.4 随机操作后结构合法");
        check(tree.size() == mirror.size(), "11.4 随机操作后计数一致");

        std::vector<int> scanned;
        for (const auto& row : tree.range(-1, 1000)) {
            scanned.push_back(row.first);
        }
        check(scanned == std::vector<int>(mirror.begin(), mirror.end()),
              "11.4 叶链扫描等于有序集合");

        bool all_found = true;
        for (const int key : mirror) {
            all_found = all_found && tree.find(key).has_value();
        }
        check(all_found, "11.4 集合里的关键码全部查得到");
    }
}
}  // namespace

int main() {
    test_bulk_load_matches_the_book_figure();
    test_insert_60_splits_all_the_way_to_the_root();
    test_range_scan_walks_the_leaf_chain();
    test_page_counters();
    test_erase_borrows_then_merges();
    test_leaf_chain_survives_merges();
    test_overwrite_and_argument_checks();
    test_random_operations_keep_the_invariants();
    std::printf("BPlusTree: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

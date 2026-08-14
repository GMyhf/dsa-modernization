#include "modern.hpp"

#include <cmath>
#include <cstdio>
#include <random>
#include <set>
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

using dsa::advanced::AvlTree;
using dsa::advanced::SplayTree;

template <typename Tree>
std::vector<int> keys_in_order(const Tree& tree) {
    std::vector<int> out;
    tree.inorder([&out](int key) { out.push_back(key); });
    return out;
}

bool strictly_increasing(const std::vector<int>& values) {
    for (std::size_t i = 1; i < values.size(); ++i) {
        if (values[i - 1] >= values[i]) {
            return false;
        }
    }
    return true;
}

AvlTree build(const std::vector<int>& keys) {
    AvlTree tree;
    for (const int key : keys) {
        tree.insert(key);
    }
    return tree;
}

/// 四种旋转各来一次。三个键的最小例子里，无论按什么次序插入，
/// **平衡之后的树高都必须是 2**——旋转没做或做错了，就会留下高度 3 的一条链。
void test_four_rotations() {
    const std::vector<std::pair<const char*, std::vector<int>>> cases{
        {"LL：3,2,1 在 3 上右旋", {3, 2, 1}},
        {"RR：1,2,3 在 1 上左旋", {1, 2, 3}},
        {"LR：3,1,2 先左旋再右旋", {3, 1, 2}},
        {"RL：1,3,2 先右旋再左旋", {1, 3, 2}},
    };
    for (const auto& [name, keys] : cases) {
        const AvlTree tree = build(keys);
        check(tree.height() == 2, name);
        // 旋转改变树形，但中序序列必须一字不差——这是检验旋转的最强不变量。
        check(keys_in_order(tree) == std::vector<int>({1, 2, 3}), "12.4.2 旋转不改变中序序列");
    }

    // 只插入不旋转的话，7 个升序键会退化成高度 7 的一条链。
    const AvlTree ascending = build({1, 2, 3, 4, 5, 6, 7});
    check(ascending.height() == 3, "12.4.2 升序插入 7 个键后树高是 3，不是 7");
    check(keys_in_order(ascending) == std::vector<int>({1, 2, 3, 4, 5, 6, 7}),
          "12.4.2 升序插入后的中序序列");
}

void test_erase_rebalances() {
    AvlTree tree = build({5, 3, 8, 2, 4, 7, 9, 1});
    check(tree.size() == 8 && tree.height() == 4, "12.4.2 删除前的规模与高度");

    tree.erase(7);
    tree.erase(9);
    check(!tree.contains(7) && !tree.contains(9), "12.4.2 删掉的键查不到");
    check(tree.contains(8) && tree.contains(1), "12.4.2 其余键还在");
    check(strictly_increasing(keys_in_order(tree)), "12.4.2 删除后中序仍严格升序");
    check(tree.size() == 6, "12.4.2 删除后计数下降");

    // 删除必须触发再平衡：右边被删空之后左边不能吊着一条长链。
    check(tree.height() <= 3, "12.4.2 删除后重新平衡");

    tree.erase(12345);
    check(tree.size() == 6, "12.4.2 删除不存在的键不改变任何东西");

    // 两个孩子都在的删除：用右子树最小键顶上。
    AvlTree both = build({2, 1, 3});
    both.erase(2);
    check(keys_in_order(both) == std::vector<int>({1, 3}), "12.4.2 删掉有两个孩子的结点");
    check(both.contains(1) && both.contains(3) && !both.contains(2), "12.4.2 顶替后的查找");
}

void test_duplicates_and_empty() {
    AvlTree tree;
    check(tree.height() == 0 && tree.size() == 0 && !tree.contains(1), "12.4.2 空树");
    check(keys_in_order(tree).empty(), "12.4.2 空树的中序序列为空");

    tree.insert(5);
    tree.insert(5);
    check(tree.size() == 1, "12.4.2 重复键不重复插入");
    tree.erase(5);
    check(tree.size() == 0 && tree.height() == 0 && !tree.contains(5), "12.4.2 删空之后回到空树");
}

/// AVL 的高度上界：$h \le 1.4405\log_2(n+2)-0.3277$。
/// 这条不等式是 AVL 之所以是 AVL 的原因——只要旋转有一处漏了，随机数据下它就会被顶破。
void test_height_bound_and_set_agreement() {
    std::mt19937 rng(20260814);
    std::uniform_int_distribution<int> key(0, 200);
    std::uniform_int_distribution<int> action(0, 2);

    int mismatched = 0;
    int over_bound = 0;
    for (int round = 0; round < 60; ++round) {
        AvlTree tree;
        std::set<int> mirror;
        for (int step = 0; step < 300; ++step) {
            const int k = key(rng);
            if (action(rng) == 2) {
                tree.erase(k);
                mirror.erase(k);
            } else {
                tree.insert(k);
                mirror.insert(k);
            }
        }
        for (int k = 0; k <= 200; ++k) {
            if (tree.contains(k) != (mirror.count(k) > 0)) {
                ++mismatched;
            }
        }
        if (keys_in_order(tree) != std::vector<int>(mirror.begin(), mirror.end())) {
            ++mismatched;
        }
        if (tree.size() != mirror.size()) {
            ++mismatched;
        }
        const double n = static_cast<double>(mirror.size());
        if (n > 0 && tree.height() > 1.4405 * std::log2(n + 2.0) - 0.3277) {
            ++over_bound;
        }
    }
    check(mismatched == 0, "12.4.2 随机插删后与 std::set 逐项一致（含中序序列与计数）");
    check(over_bound == 0, "12.4.2 随机插删后始终守住 AVL 的高度上界");
}

void test_splay_moves_the_hit_to_the_root() {
    SplayTree tree;
    for (const int key : {5, 3, 7, 2, 4, 6, 8}) {
        tree.insert(key);
    }
    check(tree.size() == 7, "12.4.3 插入 7 个键");
    check(strictly_increasing(keys_in_order(tree)), "12.4.3 伸展树的中序也严格升序");

    check(tree.contains(2) && tree.root_key() == 2, "12.4.3 命中的键被旋到根");
    check(tree.contains(8) && tree.root_key() == 8, "12.4.3 再访问一个，它也到根");
    check(tree.contains(2) && tree.root_key() == 2, "12.4.3 刚访问过的键再访问仍在根");
    check(strictly_increasing(keys_in_order(tree)), "12.4.3 反复伸展不破坏中序序列");

    check(!tree.contains(100), "12.4.3 查不到的键返回 false");
    check(strictly_increasing(keys_in_order(tree)), "12.4.3 未命中的伸展同样不破坏中序");

    SplayTree empty;
    check(empty.empty() && !empty.contains(1) && empty.size() == 0, "12.4.3 空伸展树");
    empty.insert(1);
    empty.insert(1);
    check(empty.size() == 1 && empty.root_key() == 1, "12.4.3 重复插入不增加结点");
}

void test_splay_matches_a_set() {
    std::mt19937 rng(31415);
    std::uniform_int_distribution<int> key(0, 150);
    int mismatched = 0;
    for (int round = 0; round < 40; ++round) {
        SplayTree tree;
        std::set<int> mirror;
        for (int step = 0; step < 200; ++step) {
            const int k = key(rng);
            tree.insert(k);
            mirror.insert(k);
        }
        for (int k = 0; k <= 150; ++k) {
            if (tree.contains(k) != (mirror.count(k) > 0)) {
                ++mismatched;
            }
        }
        // 查找会改变树形，查完之后中序序列仍必须与集合一致。
        if (keys_in_order(tree) != std::vector<int>(mirror.begin(), mirror.end())) {
            ++mismatched;
        }
    }
    check(mismatched == 0, "12.4.3 随机数据下与 std::set 一致，且伸展不改变中序序列");
}
}  // namespace

int main() {
    test_four_rotations();
    test_erase_rebalances();
    test_duplicates_and_empty();
    test_height_bound_and_set_agreement();
    test_splay_moves_the_hit_to_the_root();
    test_splay_matches_a_set();
    std::printf("BalancedTrees: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

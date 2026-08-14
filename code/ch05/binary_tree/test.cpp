#include "modern.hpp"

#include <cstdio>
#include <memory>
#include <stdexcept>
#include <utility>
#include <vector>

namespace {
int checks = 0, failures = 0;
void check(bool ok, const char* what) { ++checks; if (!ok) { ++failures; std::printf("  FAIL: %s\n", what); } }

dsa::BinaryTree<int> sample_tree() {
    dsa::BinaryTree<int> left; left.create_tree(2); left.root()->left = new dsa::BinaryTree<int>::Node(4); left.root()->right = new dsa::BinaryTree<int>::Node(5);
    dsa::BinaryTree<int> right; right.create_tree(3); right.root()->right = new dsa::BinaryTree<int>::Node(6);
    dsa::BinaryTree<int> tree; tree.create_tree(1, std::move(left), std::move(right)); return tree;
}

void test_traversals_and_parent() {
    const auto tree = sample_tree();
    std::vector<int> preorder, inorder, postorder, iterative_preorder, iterative_inorder, iterative_postorder, level;
    tree.preorder([&](int value) { preorder.push_back(value); });
    tree.inorder([&](int value) { inorder.push_back(value); });
    tree.postorder([&](int value) { postorder.push_back(value); });
    tree.preorder_iterative([&](int value) { iterative_preorder.push_back(value); });
    tree.inorder_iterative([&](int value) { iterative_inorder.push_back(value); });
    tree.postorder_iterative([&](int value) { iterative_postorder.push_back(value); });
    tree.level_order([&](int value) { level.push_back(value); });
    check(preorder == std::vector<int>({1,2,4,5,3,6}), "算法5.3 前序递归周游");
    check(inorder == std::vector<int>({4,2,5,1,3,6}), "算法5.3 中序递归周游");
    check(postorder == std::vector<int>({4,5,2,6,3,1}), "算法5.3 后序递归周游");
    check(iterative_preorder == preorder, "算法5.4 非递归前序周游与递归版一致");
    check(iterative_inorder == inorder, "算法5.5 非递归中序周游与递归版一致");
    check(iterative_postorder == postorder, "算法5.6 非递归后序周游与递归版一致");
    check(level == std::vector<int>({1,2,3,4,5,6}), "算法5.7 层次周游");
    check(tree.parent_of(tree.root()->left->right) == tree.root()->left, "代码5.8 能找到父结点");
    check(tree.parent_of(tree.root()) == nullptr, "勘误R15 算法5.x：根结点没有父结点，失败分支返回空而不是掉出函数");
}

void test_tree_ownership_and_rule_of_five() {
    auto source = sample_tree();
    auto copy = source; copy.root()->left->value = 20;
    check(source.root()->left->value == 2 && copy.root()->left->value == 20, "深拷贝不共享子树");
    dsa::BinaryTree<int> assigned; assigned = source; dsa::BinaryTree<int>& alias = assigned; assigned = alias;
    check(assigned.root()->right->right->value == 6, "拷贝赋值和自赋值安全");
    auto moved = std::move(copy);
    check(copy.empty() && moved.root()->left->value == 20, "移动构造转移根所有权");
    assigned = std::move(moved);
    check(moved.empty() && assigned.root()->left->value == 20, "移动赋值先释放旧树再接管新树");
    assigned.make_empty();
    check(assigned.empty(), "make_empty 后为空树");
}

struct Life {
    int value{0}; inline static int live = 0; inline static int copies = 0; inline static int throw_at = 0;
    Life() { ++live; } explicit Life(int v) : value(v) { ++live; }
    Life(const Life& other) : value(other.value) { if (throw_at && ++copies == throw_at) throw std::runtime_error("copy"); ++live; }
    Life(Life&& other) noexcept : value(other.value) { ++live; }
    ~Life() { --live; }
    static void reset(int at = 0) { copies = 0; throw_at = at; }
};
void test_partial_clone_is_cleaned() {
    Life::reset();
    { dsa::BinaryTree<Life> leaf; leaf.create_tree(Life(2)); dsa::BinaryTree<Life> tree; tree.create_tree(Life(1), std::move(leaf)); const int before = Life::live; Life::reset(2); bool threw = false; try { dsa::BinaryTree<Life> copy(tree); } catch (const std::runtime_error&) { threw = true; } Life::reset(); check(threw && Life::live == before, "复制半树失败时已分配结点全部回收"); }
    check(Life::live == 0, "树析构后不遗留元素对象");
}

void test_bst_insert_remove_contract() {
    dsa::BinarySearchTree<int> tree;
    for (int value : {50,19,35,55,20,5,100,52,88,53,92}) check(tree.insert(value), "算法5.9 插入唯一键");
    check(!tree.insert(55), "重复键插入返回 false");
    check(tree.contains(53) && !tree.contains(54), "BST 检索沿比较路径工作");
    check(tree.remove(52), "算法5.10 删除无左子树结点");
    check(tree.remove(55), "算法5.10 删除有左子树结点并以前驱替换");
    check(!tree.remove(999), "删除不存在键返回 false，不抛异常（D-001 §3c）");
    dsa::BinarySearchTree<int>& alias = tree;
    tree = alias;
    check(tree.contains(53) && tree.contains(100), "BST 拷贝赋值自赋值安全");
    std::vector<int> ordered; tree.inorder([&](int value) { ordered.push_back(value); });
    check(ordered == std::vector<int>({5,19,20,35,50,53,88,92,100}), "删除后中序序列仍严格有序");
}


/// 退化成链的树：析构与深拷贝都不能靠调用栈递归，否则一定压穿。
///
/// 规模 100 万是**量出来的**，不是拍的：本机 8 MB 栈、闸门的 ASan 档下，
/// 递归版 clone 在 40 万就段错误、递归版 destroy 在 100 万段错误。
/// 所以只要有人把 destroy/clone 改回递归，这个用例必然崩——它是这条修复的看门人。
/// 复现方法与完整数字见 collab/UNVERIFIED-RISKS.md。
void test_degenerate_chain_does_not_blow_the_stack() {
    constexpr int kDepth = 1000000;

    dsa::BinaryTree<int> chain;                 // 自底向上造纯左链，构造本身不递归
    for (int i = 0; i < kDepth; ++i) {
        dsa::BinaryTree<int> parent;
        parent.create_tree(i, std::move(chain), dsa::BinaryTree<int>{});
        chain = std::move(parent);
    }
    check(!chain.empty(), "百万深左链建成");
    check(chain.root()->value == kDepth - 1, "链顶是最后放进去的值");

    {
        const dsa::BinaryTree<int> copy = chain;   // 迭代 clone：显式栈在堆上
        check(!copy.empty() && copy.root()->value == kDepth - 1, "百万深左链可深拷贝");
        check(copy.root() != chain.root(), "深拷贝不是共享同一批结点");
    }                                              // 迭代 destroy：右旋拉直后逐个删

    chain.make_empty();
    check(chain.empty(), "百万深左链可析构");
}
}  // namespace

int main() {
    test_traversals_and_parent(); test_tree_ownership_and_rule_of_five(); test_partial_clone_is_cleaned(); test_bst_insert_remove_contract(); test_degenerate_chain_does_not_blow_the_stack();
    std::printf("BinaryTree: %d 项断言，%d 失败\n", checks, failures); return failures == 0 ? 0 : 1;
}

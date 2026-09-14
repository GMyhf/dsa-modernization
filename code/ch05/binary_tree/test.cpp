#include "modern.hpp"

#include <cstdio>
#include <cstdint>
#include <memory>
#include <stdexcept>
#include <string>
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
bool operator<(const Life& a, const Life& b) noexcept { return a.value < b.value; }
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

// ---- 由两种周游序列重建二叉树 ----------------------------------------------
struct Traversals { std::vector<int> pre, in, post, level; };
Traversals traversals_of(const dsa::BinaryTree<int>& tree) {
    Traversals t;   // 深链也要能取序列：用非递归周游与层次周游
    tree.preorder_iterative([&](int v) { t.pre.push_back(v); });
    tree.inorder_iterative([&](int v) { t.in.push_back(v); });
    tree.postorder_iterative([&](int v) { t.post.push_back(v); });
    tree.level_order([&](int v) { t.level.push_back(v); });
    return t;
}

std::uint64_t lcg_state = 88172645463325252ULL;
std::uint64_t next_random() { lcg_state = lcg_state * 6364136223846793005ULL + 1442695040888963407ULL; return lcg_state >> 33; }

/// 随机形状、随机（互不相同的）键：键是 0..n-1 洗牌后的前 n 个，挂接位置随机。
dsa::BinaryTree<int> random_tree(int n) {
    std::vector<int> keys(static_cast<std::size_t>(n));
    for (int i = 0; i < n; ++i) keys[static_cast<std::size_t>(i)] = i * 7 - 100;   // 含负数
    for (std::size_t i = keys.size(); i > 1; --i) std::swap(keys[i - 1], keys[next_random() % i]);
    dsa::BinaryTree<int> tree;
    if (n == 0) return tree;
    tree.create_tree(keys[0]);
    for (std::size_t i = 1; i < keys.size(); ++i) {
        auto* node = tree.root();
        for (;;) {
            auto*& link = (next_random() & 1) ? node->left : node->right;
            if (link == nullptr) { link = new dsa::BinaryTree<int>::Node(keys[i]); break; }
            node = link;
        }
    }
    return tree;
}

bool throws_invalid(const std::vector<int>& a, const std::vector<int>& b, bool postorder) {
    try {
        if (postorder) (void)dsa::BinaryTree<int>::from_inorder_postorder(a.data(), b.data(), a.size());
        else (void)dsa::BinaryTree<int>::from_preorder_inorder(a.data(), b.data(), a.size());
    } catch (const std::invalid_argument&) { return true; }
    return false;
}

// 合法输入上的重建一律经这两个包装调用：实现若回归（左右子树大小算反、取根位置对调），
// 合法序列会被误判为不自洽而抛 invalid_argument——折成空值，交给下面具名的 check 去红，
// 而不是让未捕获异常 terminate 整个进程（那样只知道「挂了」，不知道是哪条用例、为什么）。
std::optional<dsa::BinaryTree<int>> rebuild_post(const std::vector<int>& in, const std::vector<int>& post) {
    try { return dsa::BinaryTree<int>::from_inorder_postorder(in.data(), post.data(), in.size()); }
    catch (const std::invalid_argument&) { return std::nullopt; }
}
std::optional<dsa::BinaryTree<int>> rebuild_pre(const std::vector<int>& pre, const std::vector<int>& in) {
    try { return dsa::BinaryTree<int>::from_preorder_inorder(pre.data(), in.data(), in.size()); }
    catch (const std::invalid_argument&) { return std::nullopt; }
}

void test_rebuild_random_round_trip() {
    bool post_ok = true, pre_ok = true, post_accepted = true, pre_accepted = true;
    for (int round = 0; round < 400; ++round) {
        const int n = static_cast<int>(next_random() % 51);
        const auto original = random_tree(n);
        const Traversals want = traversals_of(original);
        const auto from_post = rebuild_post(want.in, want.post);
        post_accepted = post_accepted && from_post.has_value();
        if (from_post) {
            const Traversals got_post = traversals_of(*from_post);
            post_ok = post_ok && got_post.pre == want.pre && got_post.in == want.in && got_post.post == want.post && got_post.level == want.level;
        }
        const auto from_pre = rebuild_pre(want.pre, want.in);
        pre_accepted = pre_accepted && from_pre.has_value();
        if (from_pre) {
            const Traversals got_pre = traversals_of(*from_pre);
            pre_ok = pre_ok && got_pre.pre == want.pre && got_pre.in == want.in && got_pre.post == want.post && got_pre.level == want.level;
        }
    }
    check(post_accepted, "重建：400 棵随机树的合法中序+后序全部被接受（没有被误判为不自洽）");
    check(pre_accepted, "重建：400 棵随机树的合法前序+中序全部被接受（没有被误判为不自洽）");
    check(post_accepted && post_ok, "重建：400 棵随机树（n≤50）中序+后序重建后四种周游序列（含层次）全一致");
    check(pre_accepted && pre_ok, "重建：400 棵随机树（n≤50）前序+中序重建后四种周游序列（含层次）全一致");
}

void test_rebuild_small_cases() {
    const auto empty = dsa::BinaryTree<int>::from_inorder_postorder(nullptr, nullptr, 0);
    check(empty.empty(), "重建：空序列得空树（count 为 0 时允许空指针）");
    const auto single = rebuild_pre({42}, {42});
    check(single && !single->empty() && single->root()->value == 42 && single->root()->left == nullptr && single->root()->right == nullptr,
          "重建：单结点");
    // 图 5.5：中序 DBGEACHFI，后序 DGEBHIFCA → 前序 ABDEGCFHI
    const std::vector<int> in{'D','B','G','E','A','C','H','F','I'}, post{'D','G','E','B','H','I','F','C','A'};
    const auto fig = rebuild_post(in, post);
    check(fig.has_value(), "重建：图 5.5 的合法中序+后序被接受");
    check(fig && traversals_of(*fig).pre == std::vector<int>({'A','B','D','E','G','C','F','H','I'}), "重建：图 5.5 由中序+后序得前序 ABDEGCFHI");
    check(fig && traversals_of(*fig).level == std::vector<int>({'A','B','C','D','E','F','G','H','I'}), "重建：图 5.5 形状正确（层次序列）");
}

void test_rebuild_rejects_inconsistent() {
    check(throws_invalid({1, 2, 3}, {1, 2, 4}, true), "重建拒绝：后序出现中序没有的键");
    check(throws_invalid({1, 1}, {1, 1}, true), "重建拒绝：中序有重复键");
    {   // 重复键即使不单独查也会因「根落在区间外」被拒（抽屉原理），单独查是为了报错说对原因
        std::string message;
        const std::vector<int> dup{1, 2, 1};
        try { (void)dsa::BinaryTree<int>::from_inorder_postorder(dup.data(), dup.data(), dup.size()); }
        catch (const std::invalid_argument& e) { message = e.what(); }
        check(message.find("duplicate") != std::string::npos, "重建拒绝：重复键的报错指明 duplicate，而不是笼统的区间错误");
    }
    check(throws_invalid({1, 2}, {1, 1}, true), "重建拒绝：后序有重复键（同集合不同多重集）");
    check(throws_invalid({1, 2, 3}, {3, 1, 2}, true), "重建拒绝：中序 123、后序 312 不对应任何二叉树");
    check(throws_invalid({2, 3, 1}, {1, 2, 3}, false), "重建拒绝：前序 231、中序 123 不对应任何二叉树");
    check(throws_invalid({3, 1, 4, 2}, {1, 2, 3, 4}, false), "重建拒绝：前序 3142、中序 1234——键都对，但 4 落在左子树的中序区间之外");
    check(!throws_invalid({2, 1, 3, 4}, {1, 2, 3, 4}, false), "重建：合法的前序 2134、中序 1234 不被误拒");
    bool null_rejected = false;
    try { (void)dsa::BinaryTree<int>::from_inorder_postorder(nullptr, nullptr, 3); } catch (const std::invalid_argument&) { null_rejected = true; }
    check(null_rejected, "重建拒绝：非空序列给空指针");

    // 失败时半成品树必须回收：用 Life 计活对象数（LeakSanitizer 也会看着）
    Life::reset();
    {
        std::vector<Life> in, bad;
        for (int v : {1, 2, 3, 4, 5}) in.emplace_back(v);
        for (int v : {1, 2, 9, 4, 5}) bad.emplace_back(v);
        const int before = Life::live;
        bool threw = false;
        try { (void)dsa::BinaryTree<Life>::from_inorder_postorder(in.data(), bad.data(), in.size()); }
        catch (const std::invalid_argument&) { threw = true; }
        check(threw && Life::live == before, "重建中途发现不自洽：已建结点全部回收");
    }
}

/// 纯左链：深度 = 结点数。重建不递归，所以深度不受调用栈限制；这里取 20 万。
void test_rebuild_left_chain() {
    constexpr int kDepth = 200000;
    std::vector<int> in(kDepth), post(kDepth);
    for (int i = 0; i < kDepth; ++i) { in[static_cast<std::size_t>(i)] = i; post[static_cast<std::size_t>(i)] = i; }   // 左链：中序与后序都是自底向上
    const auto rebuilt = rebuild_post(in, post);
    check(rebuilt.has_value(), "重建：20 万深纯左链的合法中序+后序被接受");
    if (!rebuilt) return;
    const auto& chain = *rebuilt;
    std::size_t depth = 0; bool only_left = true;
    for (const auto* node = chain.root(); node != nullptr; node = node->left) { ++depth; only_left = only_left && node->right == nullptr; }
    check(depth == static_cast<std::size_t>(kDepth) && only_left, "重建：20 万深纯左链不压穿调用栈且形状正确");
    std::vector<int> pre;
    chain.preorder_iterative([&](int v) { pre.push_back(v); });
    check(pre.size() == in.size() && pre.front() == kDepth - 1 && pre.back() == 0, "重建：左链的前序是自顶向下");
}
}  // namespace

int main() {
    test_traversals_and_parent(); test_tree_ownership_and_rule_of_five(); test_partial_clone_is_cleaned(); test_bst_insert_remove_contract(); test_degenerate_chain_does_not_blow_the_stack();
    test_rebuild_random_round_trip(); test_rebuild_small_cases(); test_rebuild_rejects_inconsistent(); test_rebuild_left_chain();
    std::printf("BinaryTree: %d 项断言，%d 失败\n", checks, failures); return failures == 0 ? 0 : 1;
}

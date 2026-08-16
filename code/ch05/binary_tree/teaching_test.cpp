// 教学版二叉树与二叉搜索树的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
#include "teaching.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
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

// 把周游结果拼成一个字符串，方便一条断言对完整次序。
std::string collect(void (*build)(BinaryTree<char>&), int which) {
    BinaryTree<char> tree;
    build(tree);
    std::string out;
    auto visit = [&out](char c) { out.push_back(c); };
    if (which == 0) tree.preorder(visit);
    else if (which == 1) tree.inorder(visit);
    else if (which == 2) tree.postorder(visit);
    else tree.level_order(visit);
    return out;
}

// 样例树（注意 ASCII 图不能画在 // 注释里——行末的反斜杠会被当成续行）：
//
//   A 的左孩子是 B、右孩子是 C；
//   B 的左右孩子是 D 和 E；
//   C 只有右孩子 F。
void build_sample(BinaryTree<char>& tree) {
    BinaryTree<char> d, e, f, b, c;
    d.create_leaf('D');
    e.create_leaf('E');
    f.create_leaf('F');
    BinaryTree<char> empty_left;
    b.create_tree('B', d, e);
    c.create_tree('C', empty_left, f);
    tree.create_tree('A', b, c);
}

// 【算法5.3】三种深度优先次序。三个函数只差 visit 那一行的位置，
// 所以任何一处放错，这里就有一条对不上。
void test_depth_first_orders() {
    check(collect(build_sample, 0) == "ABDECF", "前序（根左右）是 ABDECF");
    check(collect(build_sample, 1) == "DBEACF", "中序（左根右）是 DBEACF");
    check(collect(build_sample, 2) == "DEBFCA", "后序（左右根）是 DEBFCA");
}

// 【算法5.7】层次周游：一层一层从左到右。
// 变异：把入队顺序改成先右后左 → 这里会红。
void test_level_order() {
    check(collect(build_sample, 3) == "ABCDEF", "层次周游是 ABCDEF");
}

void test_size_and_height() {
    BinaryTree<char> tree;
    build_sample(tree);
    check(tree.size() == 6, "样例树 6 个结点");
    check(tree.height() == 3, "样例树高度 3");

    BinaryTree<char> empty_tree;
    check(empty_tree.empty(), "空树 empty 为真");
    check(empty_tree.size() == 0, "空树 0 个结点");
    check(empty_tree.height() == 0, "空树高度 0");

    std::string out;
    empty_tree.preorder([&out](char c) { out.push_back(c); });
    check(out.empty(), "空树周游什么都不访问，也不崩");
}

// create_tree 必须**转移**子树的所有权：传进来的两棵树随即变空。
// 变异：不把 left.root_/right.root_ 置空 → 同一批结点被删两次，ASan 报 double-free。
void test_create_tree_takes_ownership() {
    BinaryTree<char> left, right, tree;
    left.create_leaf('L');
    right.create_leaf('R');
    tree.create_tree('P', left, right);
    check(left.empty(), "子树的所有权已转移，left 变空");
    check(right.empty(), "right 也变空");
    check(tree.size() == 3, "新树有 3 个结点");
}

// 原书只有析构没有拷贝构造 → 两棵树共享同一批结点。
// 变异实测：删掉拷贝构造，`BinaryTree<char> copy = tree;` 在 -Werror 下先撞
// -Wdeprecated-copy 编译即红；放行后 ASan 报 attempting double-free。
void test_copy_is_deep() {
    BinaryTree<char> tree;
    build_sample(tree);
    BinaryTree<char> copy = tree;
    check(copy.size() == 6, "副本结点数一致");

    std::string out;
    copy.preorder([&out](char c) { out.push_back(c); });
    check(out == "ABDECF", "副本形状一致");
    check(copy.root() != tree.root(), "副本持有自己的结点，不是同一根指针");

    tree.clear();
    check(tree.empty(), "原树已清空");
    check(copy.size() == 6, "清空原树不影响副本——说明是深拷贝");
}

void test_copy_assignment_is_deep() {
    BinaryTree<char> tree;
    build_sample(tree);
    BinaryTree<char> other;
    other.create_leaf('Z');
    other = tree;                    // other 原来那个结点必须被释放掉
    check(other.size() == 6, "赋值后结点数取自右边");
    tree.clear();
    check(other.size() == 6, "清空右边不影响左边");
}

void test_self_assignment_is_safe() {
    BinaryTree<char> tree;
    build_sample(tree);
    tree = tree;
    check(tree.size() == 6, "自赋值后结点数不变");
    std::string out;
    tree.inorder([&out](char c) { out.push_back(c); });
    check(out == "DBEACF", "自赋值后形状不变");
}

// 【代码5.8】释放必须是后序：先删两个孩子再删自己。
// 变异：改成 `delete node; destroy(node->left); ...` → ASan 报 heap-use-after-free。
void test_clear_releases_everything() {
    BinaryTree<char> tree;
    build_sample(tree);
    tree.clear();
    check(tree.empty(), "clear 之后是空树");
    check(tree.size() == 0, "clear 之后 0 个结点");
    tree.create_leaf('X');
    check(tree.size() == 1, "clear 之后还能重新建树");
}

// ---- 二叉搜索树 -----------------------------------------------------------

// 【算法5.9】插入唯一键；重复键返回 false，不是错误。
void test_bst_insert_and_contains() {
    BinarySearchTree<int> bst;
    check(bst.empty(), "新建的 BST 是空的");
    check(bst.insert(5), "插入 5 成功");
    check(bst.insert(3), "插入 3 成功");
    check(bst.insert(8), "插入 8 成功");
    check(!bst.insert(5), "重复键返回 false");
    check(bst.contains(3), "找得到 3");
    check(bst.contains(8), "找得到 8");
    check(!bst.contains(99), "找不到 99");
    check(!bst.empty(), "非空 BST empty 为假");
}

// 「左小右大」的直接推论：中序周游得到升序。
// 变异：insert 里把左右分支写反 → 这条立刻红。
void test_bst_inorder_is_sorted() {
    BinarySearchTree<int> bst;
    int input[] = {50, 30, 70, 20, 40, 60, 80, 35};
    for (int x : input) {
        (void)bst.insert(x);
    }
    int previous = -1;
    bool sorted = true;
    int count = 0;
    bst.inorder([&](int x) {
        if (x <= previous) sorted = false;
        previous = x;
        ++count;
    });
    check(count == 8, "8 个键都在树里");
    check(sorted, "中序周游得到严格升序");
    check(previous == 80, "最后一个是最大的 80");
}

// 【算法5.10】删除的三种情形：叶子、只有一个孩子、有两个孩子。
// 第三种是难点：用**中序前驱**顶替，顶替之后「左小右大」必须仍然成立。
void test_bst_remove_all_three_cases() {
    BinarySearchTree<int> bst;
    int input[] = {50, 30, 70, 20, 40, 60, 80, 35, 45};
    for (int x : input) {
        (void)bst.insert(x);
    }

    check(bst.remove(20), "删叶子 20");
    check(!bst.contains(20), "20 没了");

    check(bst.remove(60), "删只有一个孩子的 70 的左孩子 60");
    check(!bst.contains(60), "60 没了");

    check(bst.remove(30), "删有两个孩子的 30");
    check(!bst.contains(30), "30 没了");

    // 删完之后仍然是一棵合法的 BST：中序还得是升序，其余键一个不少
    int previous = -1;
    bool sorted = true;
    int count = 0;
    bst.inorder([&](int x) {
        if (x <= previous) sorted = false;
        previous = x;
        ++count;
    });
    check(count == 6, "删掉 3 个之后还剩 6 个");
    check(sorted, "三次删除之后中序仍是升序——「左小右大」没被破坏");
    for (int x : {35, 40, 45, 50, 70, 80}) {
        check(bst.contains(x), "剩下的键都还在");
    }
}

// 两个孩子的删除里还藏着一步：**中序前驱自己可能还有一个左孩子**，
// 顶替之前必须先把它接走。这棵树是专门为这一步搭的——
// 删 50 时前驱是 45，而 45 有个左孩子 42。
// 变异：去掉 `*predecessor_link = replacement->left;` → 40 的右链仍指向 45，
//       而 45 已经成了根，树上出现环；clear() 时 ASan 当场报错。
void test_bst_remove_when_predecessor_has_a_left_child() {
    BinarySearchTree<int> bst;
    int input[] = {50, 30, 70, 40, 35, 45, 42};
    for (int x : input) {
        (void)bst.insert(x);
    }
    check(bst.remove(50), "删根结点 50（两个孩子，前驱 45 自己还有左孩子 42）");

    int previous = -1;
    bool sorted = true;
    int count = 0;
    bst.inorder([&](int x) {
        if (x <= previous) sorted = false;
        previous = x;
        ++count;
    });
    check(count == 6, "删掉 1 个之后剩 6 个，42 没有走丢");
    check(sorted, "中序仍是升序，树上没有出现环");
    for (int x : {30, 35, 40, 42, 45, 70}) {
        check(bst.contains(x), "剩下的键都还在（含前驱的左孩子 42）");
    }
    check(!bst.contains(50), "50 确实没了");
}

void test_bst_remove_root_and_missing_key() {
    BinarySearchTree<int> bst;
    check(!bst.remove(1), "空树上删除返回 false，不是错误");
    (void)bst.insert(10);
    check(!bst.remove(99), "删不存在的键返回 false");
    check(bst.remove(10), "删根结点");
    check(bst.empty(), "删完唯一的结点后树空了");
    check(!bst.contains(10), "10 确实没了");
    check(bst.insert(10), "空掉之后还能重新插入");
}

void test_bst_copy_is_deep() {
    BinarySearchTree<int> a;
    for (int x : {5, 3, 8, 1}) {
        (void)a.insert(x);
    }
    BinarySearchTree<int> b = a;
    check(b.contains(3) && b.contains(8), "副本内容一致");
    a.clear();
    check(b.contains(3), "清空原树不影响副本");
    check(a.empty(), "原树确实空了");

    BinarySearchTree<int> c;
    (void)c.insert(999);
    c = b;                            // c 原来那个结点必须被释放掉
    check(c.contains(1) && !c.contains(999), "赋值后内容取自右边");
}

void test_bst_self_assignment_is_safe() {
    BinarySearchTree<int> bst;
    for (int x : {5, 3, 8}) {
        (void)bst.insert(x);
    }
    bst = bst;
    check(bst.contains(3) && bst.contains(5) && bst.contains(8), "自赋值后内容不变");
}

// D-001 第 3 条红线：容器内零 I/O。
void test_no_console_output() {
    std::ostringstream out, err;
    std::streambuf* old_out = std::cout.rdbuf(out.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(err.rdbuf());
    {
        BinaryTree<char> tree;
        build_sample(tree);
        tree.preorder([](char) {});
        tree.level_order([](char) {});
        BinaryTree<char> copy = tree;
        copy = tree;
        tree.clear();

        BinarySearchTree<int> bst;
        (void)bst.insert(1);
        (void)bst.insert(1);
        (void)bst.remove(99);
        (void)bst.contains(99);
        BinarySearchTree<int> bcopy = bst;
        bcopy = bst;
    }
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(out.str().empty(), "树没有往 stdout 打任何东西");
    check(err.str().empty(), "树没有往 stderr 打任何东西");
}

}  // namespace

int main() {
    test_depth_first_orders();
    test_level_order();
    test_size_and_height();
    test_create_tree_takes_ownership();
    test_copy_is_deep();
    test_copy_assignment_is_deep();
    test_self_assignment_is_safe();
    test_clear_releases_everything();

    test_bst_insert_and_contains();
    test_bst_inorder_is_sorted();
    test_bst_remove_all_three_cases();
    test_bst_remove_when_predecessor_has_a_left_child();
    test_bst_remove_root_and_missing_key();
    test_bst_copy_is_deep();
    test_bst_self_assignment_is_safe();

    test_no_console_output();

    std::printf("BinaryTree(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

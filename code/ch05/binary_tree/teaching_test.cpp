// 教学版二叉树与二叉搜索树的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 标准和 test.cpp 一样：**把实现退回原书的写法，这里必须有一条会红**。
#include "teaching.hpp"

#include <cstdio>
#include <cstdlib>
#include <iostream>
#include <new>
#include <sstream>
#include <stdexcept>
#include <string>

// 分配失败注入：替换全局 operator new / delete，数一数还有几次 new 没还，
// 并能让第 N 次 new 抛 bad_alloc。只为显式栈版的一条用例服务
// （Codex 复核 T-084 抓出：new Frame / new Node 失败时，栈里剩下的帧没人还）。
// 不靠 LeakSanitizer：泄漏的帧常被旧栈上残留的指针「看见」，LSan 会漏报。
static long g_live_allocations = 0;
static long g_allocations_until_failure = -1;   // -1：永不失败；0：下一次 new 就抛

void* operator new(std::size_t size) {
    if (g_allocations_until_failure == 0) {
        throw std::bad_alloc();
    }
    if (g_allocations_until_failure > 0) {
        --g_allocations_until_failure;
    }
    void* p = std::malloc(size == 0 ? 1 : size);
    if (p == nullptr) {
        throw std::bad_alloc();
    }
    ++g_live_allocations;
    return p;
}

void operator delete(void* p) noexcept {
    if (p != nullptr) {
        --g_live_allocations;
        std::free(p);
    }
}

void operator delete(void* p, std::size_t) noexcept { operator delete(p); }

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
    // 自赋值写成「先取引用别名再赋值」，而不是 `tree = tree;`：
    // clang 的 -Wself-assign-overloaded 会拒绝后者，而闸门开着 -Werror，
    // 于是整套教学版测试在 clang 上根本编不过（2026-08-17 Codex 在 macOS 上撞到）。
    // 运行时语义没变：还是同一个对象赋给它自己。
    auto& same = tree;
    tree = same;
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
    // 自赋值写成「先取引用别名再赋值」，而不是 `bst = bst;`：
    // clang 的 -Wself-assign-overloaded 会拒绝后者，而闸门开着 -Werror，
    // 于是整套教学版测试在 clang 上根本编不过（2026-08-17 Codex 在 macOS 上撞到）。
    // 运行时语义没变：还是同一个对象赋给它自己。
    auto& same = bst;
    bst = same;
    check(bst.contains(3) && bst.contains(5) && bst.contains(8), "自赋值后内容不变");
}

// ---- 由中序 + 后序重建（5.2.2） ----

// 四种周游拼成一个串（中间用 / 隔开）：四种全对上，形状才算一样。
std::string all_orders(const BinaryTree<char>& tree) {
    std::string out;
    auto visit = [&out](char c) { out.push_back(c); };
    tree.preorder(visit);
    out.push_back('/');
    tree.inorder(visit);
    out.push_back('/');
    tree.postorder(visit);
    out.push_back('/');
    tree.level_order(visit);
    return out;
}

// 重建；两个序列对不上（抛 invalid_argument）时返回 false。
// 合法输入上实现若回归，会被误判成不自洽而抛出——折成 false 交给具名的 check 去红，
// 不让未捕获异常直接 terminate 整个进程。
bool rebuild(BinaryTree<char>& tree, const std::string& in, const std::string& post) {
    try {
        tree.build_from_inorder_postorder(in.data(), post.data(), in.size());
    } catch (const std::invalid_argument&) {
        return false;
    }
    return true;
}

// 显式栈版的同一个包装。
bool rebuild_iterative(BinaryTree<char>& tree, const std::string& in, const std::string& post) {
    try {
        tree.build_from_inorder_postorder_iterative(in.data(), post.data(), in.size());
    } catch (const std::invalid_argument&) {
        return false;
    }
    return true;
}

// ---- 键是 char 时：直接查表（与类里的两版对拍） ----

// 「点名册」：pos[c] = 字符 c 坐在中序第几号，-1 表示中序里没有。
// 登记时发现这一格已经有人，就是中序里有重复键。
void fill_position_table(int pos[256], const char* inorder, int count) {
    for (int c = 0; c < 256; ++c) {
        pos[c] = -1;
    }
    for (int i = 0; i < count; ++i) {
        const unsigned char c = static_cast<unsigned char>(inorder[i]);
        if (pos[c] != -1) {
            throw std::invalid_argument("duplicate key in inorder sequence");
        }
        pos[c] = i;
    }
}

// 与 build_impl 同一个递归，只把「从左往右数」换成查一次表：找根 O(1)，总代价 O(n)。
// 两棵子树先各自建成小树，再用 create_tree（【代码5.2】）接到根上。
void build_by_table(BinaryTree<char>& out, const int pos[256], const char* postorder,
                    int in_begin, int in_end, int post_begin) {
    if (in_begin == in_end) {
        out.clear();
        return;
    }
    const char key = postorder[post_begin + (in_end - in_begin) - 1];   // 后序段的最后一个是根
    const int k = pos[static_cast<unsigned char>(key)];                 // 查表：它坐在中序第几号
    if (k < in_begin || k >= in_end) {                                  // 不在本段（-1 也落在这里）
        throw std::invalid_argument("root lies outside its inorder range");
    }
    const int left_size = k - in_begin;
    BinaryTree<char> left, right;
    build_by_table(left, pos, postorder, in_begin, k, post_begin);
    build_by_table(right, pos, postorder, k + 1, in_end, post_begin + left_size);
    out.create_tree(key, left, right);
}

bool rebuild_by_table(BinaryTree<char>& tree, const std::string& in, const std::string& post) {
    try {
        if (post.size() != in.size()) {
            throw std::invalid_argument("sequences differ in length");
        }
        int pos[256];
        fill_position_table(pos, in.data(), static_cast<int>(in.size()));
        BinaryTree<char> fresh;                 // 先建在临时树里：出错时 tree 不动
        build_by_table(fresh, pos, post.data(), 0, static_cast<int>(in.size()), 0);
        tree = fresh;
    } catch (const std::invalid_argument&) {
        return false;
    }
    return true;
}

// 图 5.5：中序 DBGEACHFI、后序 DGEBHIFCA，与课件逐帧推演的是同一棵树。
// 变异：左右子树的后序起点算错 / 取后序段的第一个当根 → 这里会红。
void test_rebuild_figure_5_5() {
    BinaryTree<char> tree;
    check(rebuild(tree, "DBGEACHFI", "DGEBHIFCA"), "重建：图 5.5 的合法序列被接受");
    check(all_orders(tree) == "ABDEGCFHI/DBGEACHFI/DGEBHIFCA/ABCDEFGHI",
          "重建：图 5.5 前序 ABDEGCFHI，层次 ABCDEFGHI");
    check(tree.size() == 9 && tree.height() == 4, "重建：图 5.5 共 9 个结点、高度 4");
}

std::string all_orders_of_sample() {
    BinaryTree<char> tree;
    build_sample(tree);
    return all_orders(tree);
}

// 样例树里 C 只有右孩子——单侧空子树的切分最容易差一。
void test_rebuild_sample_tree() {
    BinaryTree<char> tree;
    check(rebuild(tree, "DBEACF", "DEBFCA"), "重建：样例树的合法序列被接受");
    check(all_orders(tree) == all_orders_of_sample(), "重建：样例树四种周游与原树一致");
}

// 随机形状往返：建一棵树 → 取中序、后序 → 重建 → 四种周游逐一比对。
unsigned g_seed = 20261013u;
unsigned next_random() {
    g_seed = g_seed * 1103515245u + 12345u;
    return g_seed >> 16;
}

// 用 create_tree 拼一棵 n 个结点的随机树，关键码从 next_key 起依次取，互不相同。
void random_tree(BinaryTree<char>& tree, int n, char& next_key) {
    if (n == 0) {
        tree.clear();
        return;
    }
    const int left_n = static_cast<int>(next_random() % static_cast<unsigned>(n));
    BinaryTree<char> left, right;
    random_tree(left, left_n, next_key);
    const char key = next_key++;
    random_tree(right, n - 1 - left_n, next_key);
    tree.create_tree(key, left, right);
}

void test_rebuild_random_round_trip() {
    bool accepted = true, same = true;
    for (int round = 0; round < 300; ++round) {
        const int n = static_cast<int>(next_random() % 41);     // 0..40 个结点
        char next_key = '0';                                     // '0' 起 40 个可打印字符
        BinaryTree<char> original;
        random_tree(original, n, next_key);
        std::string in, post;
        original.inorder([&in](char c) { in.push_back(c); });
        original.postorder([&post](char c) { post.push_back(c); });

        BinaryTree<char> rebuilt;
        const bool ok = rebuild(rebuilt, in, post);
        accepted = accepted && ok;
        same = same && ok && all_orders(rebuilt) == all_orders(original);
    }
    check(accepted, "重建：300 棵随机树的合法序列全部被接受");
    check(same, "重建：300 棵随机树（n≤40）重建后四种周游全一致");
}

void test_rebuild_empty() {
    BinaryTree<char> tree;
    build_sample(tree);
    check(rebuild(tree, "", ""), "重建：空序列被接受");
    check(tree.empty(), "重建：空序列得空树（原来的树被替换掉）");
}

// 两个序列对不上：抛 invalid_argument，且原树不动。
// 中序 123、后序 312：根 2 已经建好，左子树才发现 3 不在区间 [1] 里——
// 半成品若没人释放，LeakSanitizer 会报泄漏；若直接建在本树上，原树就没了。
// 变异：去掉 k < in_end 的边界 → 越界读（ASan 红）；直接建进 root_ → 「原树不动」红。
void test_rebuild_rejects_inconsistent() {
    BinaryTree<char> tree;
    build_sample(tree);
    const std::string before = all_orders(tree);
    check(!rebuild(tree, "123", "312"), "重建拒绝：中序 123、后序 312 不对应任何树");
    check(all_orders(tree) == before, "重建拒绝后原树不动");
    check(!rebuild(tree, "123", "124"), "重建拒绝：后序出现中序没有的键");
    check(all_orders(tree) == before, "连续两次被拒后原树仍不动");
}

// 显式栈版、查表版与递归版对拍：同一批随机树，三版建出的树四种周游全一致。
// 变异：显式栈版右子树后序起点算错 / 忘了 *f.link = node；查表版去掉 k >= in_end → 这里会红。
void test_rebuild_three_versions_agree() {
    bool iterative_ok = true, table_ok = true;
    for (int round = 0; round < 300; ++round) {
        const int n = static_cast<int>(next_random() % 61);     // 0..60 个结点
        char next_key = '0';
        BinaryTree<char> original;
        random_tree(original, n, next_key);
        std::string in, post;
        original.inorder([&in](char c) { in.push_back(c); });
        original.postorder([&post](char c) { post.push_back(c); });
        const std::string want = all_orders(original);

        BinaryTree<char> a, b;
        iterative_ok = iterative_ok && rebuild_iterative(a, in, post) && all_orders(a) == want;
        table_ok = table_ok && rebuild_by_table(b, in, post) && all_orders(b) == want;
    }
    check(iterative_ok, "重建（显式栈版）：300 棵随机树（n≤60）与原树四种周游全一致");
    check(table_ok, "重建（查表版）：300 棵随机树（n≤60）与原树四种周游全一致");

    BinaryTree<char> fig;
    check(rebuild_iterative(fig, "DBGEACHFI", "DGEBHIFCA") && all_orders(fig) == "ABDEGCFHI/DBGEACHFI/DGEBHIFCA/ABCDEFGHI",
          "重建（显式栈版）：图 5.5");
    check(rebuild_by_table(fig, "DBGEACHFI", "DGEBHIFCA") && all_orders(fig) == "ABDEGCFHI/DBGEACHFI/DGEBHIFCA/ABCDEFGHI",
          "重建（查表版）：图 5.5");
    check(rebuild_iterative(fig, "", "") && fig.empty(), "重建（显式栈版）：空序列得空树");
}

// 不自洽输入：两版都拒绝，且原树不动。
// 显式栈版中序 1234、后序 3412：根 2 建好后，栈里还压着右子树那一帧，左子树才失败——
// 剩下的帧与半成品若没还，LeakSanitizer 会报泄漏。
void test_rebuild_other_versions_reject() {
    BinaryTree<char> tree;
    build_sample(tree);
    const std::string before = all_orders(tree);
    check(!rebuild_iterative(tree, "1234", "3412"), "重建拒绝（显式栈版）：中序 1234、后序 3412");
    check(!rebuild_iterative(tree, "123", "124"), "重建拒绝（显式栈版）：后序出现中序没有的键");
    check(all_orders(tree) == before, "重建拒绝（显式栈版）后原树不动");
    check(!rebuild_by_table(tree, "123", "312"), "重建拒绝（查表版）：中序 123、后序 312");
    check(!rebuild_by_table(tree, "123", "124"), "重建拒绝（查表版）：后序出现中序没有的键");
    check(!rebuild_by_table(tree, "ABA", "AAB"), "重建拒绝（查表版）：中序有重复键");
    // 重复键即使不在登记时查，也会因「根落在区间外」被拒（抽屉原理）；
    // 登记时单独查，是为了让报错说对原因。所以直接对点名册断言。
    // 变异：去掉 fill_position_table 里的重复检查 → 这里会红。
    {
        int pos[256];
        bool duplicate_reported = false;
        try {
            fill_position_table(pos, "ABA", 3);
        } catch (const std::invalid_argument& e) {
            duplicate_reported = std::string(e.what()).find("duplicate") != std::string::npos;
        }
        check(duplicate_reported, "查表版：登记点名册时就报出重复键");
    }
    check(all_orders(tree) == before, "重建拒绝（查表版）后原树不动");
}

// 显式栈版：第 1、2、……次 new 依次失败（每次只让一处失败），看每一处都收拾干净。
// 图 5.5 共 9 个结点、19 帧（第一帧 + 每个结点压两帧），也就是 28 次 new。
// 变异：去掉 FrameStack 的析构，或退回「只在找不到根时手动清栈」的写法 → 「全部还掉」会红。
void test_rebuild_iterative_survives_bad_alloc() {
    const std::string in = "DBGEACHFI", post = "DGEBHIFCA";
    BinaryTree<char> tree;
    tree.create_leaf('Z');                       // 原树：只有一个 Z

    int failures = 0;
    bool all_released = true, unchanged = true;
    for (long n = 0;; ++n) {
        const long live_before = g_live_allocations;
        g_allocations_until_failure = n;         // 前 n 次 new 成功，第 n+1 次抛
        bool failed = false;
        try {
            tree.build_from_inorder_postorder_iterative(in.data(), post.data(), in.size());
        } catch (const std::bad_alloc&) {
            failed = true;
        }
        g_allocations_until_failure = -1;
        if (!failed) {
            break;                               // n 次就够用了：建成了
        }
        ++failures;
        all_released = all_released && g_live_allocations == live_before;
        unchanged = unchanged && tree.size() == 1 && tree.root()->value == 'Z';
    }
    check(failures == 28, "显式栈版：图 5.5 共 28 次 new（9 个结点 + 19 帧），每一处都注入过失败");
    check(all_released, "显式栈版：任何一次 new 失败，已建结点与栈里剩下的帧全部还掉");
    check(unchanged, "显式栈版：new 失败后原树不动");
    check(all_orders(tree) == "ABDEGCFHI/DBGEACHFI/DGEBHIFCA/ABCDEFGHI", "显式栈版：不再注入失败时正常建成图 5.5");
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
        (void)rebuild(tree, "DBEACF", "DEBFCA");
        (void)rebuild(tree, "123", "312");
        (void)rebuild_iterative(tree, "1234", "3412");
        (void)rebuild_by_table(tree, "123", "312");

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

    test_rebuild_figure_5_5();
    test_rebuild_sample_tree();
    test_rebuild_random_round_trip();
    test_rebuild_empty();
    test_rebuild_rejects_inconsistent();
    test_rebuild_three_versions_agree();
    test_rebuild_other_versions_reject();
    test_rebuild_iterative_survives_bad_alloc();

    test_no_console_output();

    std::printf("BinaryTree(教学版): %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

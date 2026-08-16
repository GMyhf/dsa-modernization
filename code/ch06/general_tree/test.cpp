#include "modern.hpp"

#include <cstdio>
#include <stdexcept>
#include <string>
#include <utility>
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

dsa::GeneralTree<char> make_tree() {
    dsa::GeneralTree<char> tree;
    tree.create_root('A');
    auto* b = tree.insert_first(tree.root(), 'B');
    auto* c = tree.insert_next(b, 'C');
    tree.insert_first(b, 'D');
    tree.insert_first(c, 'E');
    return tree;
}

void test_tree() {
    auto tree = make_tree();
    std::vector<char> preorder;
    std::vector<char> postorder;
    std::vector<char> breadth;
    tree.preorder([&](char value) { preorder.push_back(value); });
    tree.postorder([&](char value) { postorder.push_back(value); });
    tree.breadth_first([&](char value) { breadth.push_back(value); });

    check(preorder == std::vector<char>({'A', 'B', 'D', 'C', 'E'}),
          "算法6.3 preorder");
    check(postorder == std::vector<char>({'D', 'B', 'E', 'C', 'A'}),
          "算法6.4 postorder");
    check(breadth == std::vector<char>({'A', 'B', 'C', 'D', 'E'}),
          "算法6.5 breadth first");

    auto* b = tree.root()->child;
    auto* c = b->sibling;
    check(tree.parent_of(b) == tree.root(), "代码6.7 first child parent");
    check(tree.parent_of(c) == tree.root(), "代码6.7 sibling parent");
    check(tree.parent_of(tree.root()) == nullptr, "代码6.2 root parent");
    check(tree.parent_of(nullptr) == nullptr, "代码6.2 null parent");

    auto* new_first = tree.insert_first(tree.root(), 'F');
    check(tree.root()->child == new_first, "代码6.6 insert first prepends");
    check(new_first->sibling == b, "代码6.6 old first follows new");
    check(new_first->parent == tree.root(), "代码6.6 inserted parent");
    tree.delete_subtree(new_first);
    check(tree.root()->child == b, "代码6.7 delete prepended child");

    tree.delete_subtree(b);
    check(tree.root()->child == c, "代码6.7 delete first child reconnects");
    check(c->parent == tree.root(), "代码6.7 surviving child parent");
    tree.delete_subtree(c);
    check(tree.root()->child == nullptr, "代码6.7 delete last child");

    tree.create_root('X');
    auto* second_root = tree.insert_next(tree.root(), 'Y');
    tree.delete_subtree(second_root);
    check(tree.root()->value == 'X', "代码6.7 delete forest sibling root");
    check(tree.root()->sibling == nullptr, "代码6.7 forest tail unlinked");
    auto* restored_second = tree.insert_next(tree.root(), 'Y');
    tree.delete_subtree(tree.root());
    check(tree.root() == restored_second, "代码6.7 delete first forest root");
    check(tree.root()->parent == nullptr, "代码6.7 successor remains forest root");

    auto copy = tree;
    copy.root()->value = 'Z';
    check(tree.root()->value == 'Y', "代码6.1 deep copy source unchanged");
    check(copy.root()->value == 'Z', "代码6.1 copied node independent");
    dsa::GeneralTree<char> assigned;
    assigned = tree;
    auto& alias = assigned;
    assigned = alias;
    check(assigned.root()->value == 'Y', "代码6.2 self assignment");
    auto moved = std::move(copy);
    check(copy.root() == nullptr, "代码6.2 move clears source");
    check(moved.root()->value == 'Z', "代码6.2 move owns tree");

    bool bad_parent = false;
    try {
        tree.insert_first(nullptr, '!');
    } catch (const std::invalid_argument&) {
        bad_parent = true;
    }
    check(bad_parent, "代码6.6 rejects null parent");
    bool bad_sibling = false;
    try {
        tree.insert_next(nullptr, '!');
    } catch (const std::invalid_argument&) {
        bad_sibling = true;
    }
    check(bad_sibling, "代码6.6 rejects null sibling");
    tree.delete_subtree(nullptr);
    check(tree.root() != nullptr, "代码6.7 null deletion is no-op");
    tree.clear();
    check(tree.root() == nullptr, "代码6.7 clear tree");
}

void test_disjoint_set() {
    dsa::DisjointSet sets(8);
    for (std::size_t index = 0; index < 8; ++index) {
        check(sets.find(index) == index, "代码6.8 singleton find");
    }
    check(sets.unite(0, 1), "代码6.8 unite pair one");
    check(sets.unite(2, 3), "代码6.8 unite pair two");
    check(sets.unite(4, 5), "代码6.8 unite pair three");
    check(sets.unite(0, 2), "代码6.8 weighted union");
    check(sets.unite(0, 4), "代码6.8 merge classes");
    check(sets.same(1, 3), "代码6.8 equivalence class");
    check(sets.same(5, 0), "算法6.9 path compression find");
    check(!sets.same(1, 6), "代码6.8 distinct sets");
    const auto root = sets.find(5);
    check(root == sets.find(0), "算法6.9 compressed root");
    check(root == sets.find(1), "算法6.9 sibling compressed root");
    check(!sets.unite(1, 5), "代码6.8 duplicate union false");

    bool bad_find = false;
    try {
        (void)sets.find(8);
    } catch (const std::out_of_range&) {
        bad_find = true;
    }
    check(bad_find, "代码6.8 find bounds check");
    bool bad_unite = false;
    try {
        (void)sets.unite(0, 9);
    } catch (const std::out_of_range&) {
        bad_unite = true;
    }
    check(bad_unite, "代码6.8 unite bounds check");
}


// 【算法6.10】带双标记位的先根次序表示 → 「左子/右兄」树。
//
// 用原书图6.5(a) 那片森林的双标记序列（图6.15）：
//   先根次序 A B C E F D G H J I
//   ltag(有孩子) 0 0 0 0 1 1 1 0 1 1  →  has_child = (ltag == 0)
//   rtag(有兄弟) 0 1 0 1 1 1 0 0 1 1  →  has_sibling = (rtag == 0)
//
// 判据是**先根周游必须还原成同一串**——序列进、序列出，中间那套栈机制若错，
// 顺序立刻乱。变异：把「扫到没有孩子的结点才弹栈」改成「每个结点都弹」，这里会红。
void test_dual_tag_construction() {
    using Node = dsa::GeneralTree<char>::DualTagNode;
    const Node nodes[] = {
        {'A', true,  true },   // ltag=0 rtag=0
        {'B', true,  false},   // ltag=0 rtag=1
        {'C', true,  true },   // ltag=0 rtag=0
        {'E', true,  false},   // ltag=0 rtag=1
        {'F', false, false},   // ltag=1 rtag=1
        {'D', false, false},   // ltag=1 rtag=1
        {'G', false, true },   // ltag=1 rtag=0
        {'H', true,  true },   // ltag=0 rtag=0
        {'J', false, false},   // ltag=1 rtag=1
        {'I', false, false},   // ltag=1 rtag=1
    };
    auto tree = dsa::GeneralTree<char>::from_dual_tag(nodes, 10);

    std::string pre;
    tree.preorder([&pre](char c) { pre.push_back(c); });
    check(pre == "ABCEFDGHJI", "算法6.10 先根周游还原出原序列");

    // 结构本身也要对：A 的第一个孩子是 B，B 的第一个孩子是 C。
    const auto* a = tree.root();
    check(a != nullptr && a->value == 'A', "算法6.10 根是 A");
    check(a->child != nullptr && a->child->value == 'B', "算法6.10 A 的长子是 B");
    check(a->child->child != nullptr && a->child->child->value == 'C', "算法6.10 B 的长子是 C");
    // A 的 rtag 为 0，所以它在森林里还有下一棵树
    check(a->sibling != nullptr, "算法6.10 A 有右兄弟（这是一片森林，不是一棵树）");

    // 父指针也要接对：兄弟共享父结点
    const auto* c = a->child->child;
    check(c->parent == a->child, "算法6.10 C 的父是 B");
    check(c->sibling != nullptr && c->sibling->parent == a->child,
          "算法6.10 C 的右兄弟与 C 同父");
}

void test_dual_tag_edge_cases() {
    using Node = dsa::GeneralTree<char>::DualTagNode;

    auto empty_tree = dsa::GeneralTree<char>::from_dual_tag(nullptr, 0);
    check(empty_tree.root() == nullptr, "算法6.10 空序列得到空树");

    const Node single[] = {{'X', false, false}};
    auto one = dsa::GeneralTree<char>::from_dual_tag(single, 1);
    check(one.root() != nullptr && one.root()->value == 'X', "算法6.10 单结点序列");
    check(one.root()->child == nullptr && one.root()->sibling == nullptr,
          "算法6.10 单结点没有孩子也没有兄弟");

    bool null_rejected = false;
    try {
        (void)dsa::GeneralTree<char>::from_dual_tag(nullptr, 3);
    } catch (const std::invalid_argument&) {
        null_rejected = true;
    }
    check(null_rejected, "算法6.10 count 非零而数组为空指针时抛异常");

    // 标志位不自洽：两个结点都说「没有孩子、没有兄弟」，第二个结点无处安放。
    // 原书这里对空栈取顶，是未定义行为；这里必须抛异常。
    const Node inconsistent[] = {{'A', false, false}, {'B', false, false}};
    bool bad_rejected = false;
    try {
        (void)dsa::GeneralTree<char>::from_dual_tag(inconsistent, 2);
    } catch (const std::invalid_argument&) {
        bad_rejected = true;
    }
    check(bad_rejected, "算法6.10 标志位不自洽时抛异常，而不是对空栈取顶");

    // 最后一个结点若声称还有孩子，序列没有正常收尾
    const Node unfinished[] = {{'A', true, false}, {'B', true, false}};
    bool unfinished_rejected = false;
    try {
        (void)dsa::GeneralTree<char>::from_dual_tag(unfinished, 2);
    } catch (const std::invalid_argument&) {
        unfinished_rejected = true;
    }
    check(unfinished_rejected, "算法6.10 末结点仍声称有孩子时抛异常");
}

// 代码6.8 的**重量权衡合并规则**：小树挂到大树下，比的是元素个数不是树高。
//
// 这一组用例专门分辨「按重量」和「按秩」——两者在这组等价对上长出不同的父指针数组。
// 变异：改回按秩合并，下面「4 挂到 0 下面」那条会红。
void test_weighted_union_rule() {
    dsa::DisjointSet sets(8);

    // 先把 {0,1,2} 并成一棵 3 个元素、高度 1 的树
    check(sets.unite(0, 1), "并 0-1");
    check(sets.unite(0, 2), "并 0-2");
    check(sets.set_size(0) == 3, "{0,1,2} 规模为 3");

    // 再把 {4,5} 并成一棵 2 个元素、高度 1 的树
    check(sets.unite(4, 5), "并 4-5");
    check(sets.set_size(4) == 2, "{4,5} 规模为 2");

    // 关键一步：两棵树**高度相同**（都是 1），但规模不同（3 vs 2）。
    //   按重量 → 小的挂到大的下面，根是 0；
    //   按秩   → 秩相同，取决于实现的先后手，原实现会让 4 当根。
    check(sets.unite(4, 0), "并 {4,5} 与 {0,1,2}");
    check(sets.find(4) == 0, "重量权衡：规模小的 {4,5} 挂到规模大的 {0,1,2} 下，根是 0");
    check(sets.set_size(0) == 5, "合并后规模累加为 5");
}

// 并列时的口径：规模相同，值大的根挂到值小的根下（课程第 6 章习题 8 的原话）。
void test_weighted_union_tie_break() {
    dsa::DisjointSet sets(4);
    check(sets.unite(2, 3), "并 2-3，规模同为 1");
    check(sets.find(3) == 2, "并列时值大的 3 挂到值小的 2 下");
    check(sets.parents()[3] == 2, "父指针数组里 3 的父是 2");

    dsa::DisjointSet reversed(4);
    check(reversed.unite(3, 2), "换个参数顺序再并一次");
    check(reversed.find(3) == 2, "结果与参数顺序无关，仍是 3 挂到 2 下");
}

}  // namespace

int main() {
    test_tree();
    test_disjoint_set();
    test_dual_tag_construction();
    test_dual_tag_edge_cases();
    test_weighted_union_rule();
    test_weighted_union_tie_break();
    std::printf("GeneralTree: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

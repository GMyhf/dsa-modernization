#include "modern.hpp"

#include <cstdio>
#include <stdexcept>
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

}  // namespace

int main() {
    test_tree();
    test_disjoint_set();
    std::printf("GeneralTree: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

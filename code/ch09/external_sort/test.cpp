#include "modern.hpp"

#include <cstdio>
#include <stdexcept>
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

void test_replacement_selection() {
    check(dsa::external_sort::replacement_selection({3, 1, 2}) ==
              std::vector<int>({1, 2, 3}),
          "算法9.1 basic order");
    check(dsa::external_sort::replacement_selection({5, -1, 5, 0}) ==
              std::vector<int>({-1, 0, 5, 5}),
          "算法9.1 duplicates and negative values");
    check(dsa::external_sort::replacement_selection({}) == std::vector<int>({}),
          "算法9.1 empty input");
    check(dsa::external_sort::replacement_selection({1}) == std::vector<int>({1}),
          "算法9.1 singleton input");
}

void test_tournament_tree() {
    dsa::external_sort::WinnerTree tree({4, 1, 3, 1, 2});
    check(tree.winner() == 1, "代码9.2 initial winner");
    check(tree.winner_index() == 1, "代码9.2 tie uses left player");
    tree.replace(1, 9);
    check(tree.winner() == 1, "代码9.2 replay finds other winner");
    check(tree.winner_index() == 3, "代码9.2 replay index");
    tree.replace(3, 8);
    check(tree.winner() == 2, "代码9.2 successive replay");
    tree.replace(4, 10);
    check(tree.winner() == 3, "代码9.3 loser-tree compatible replay");

    dsa::external_sort::LoserTree empty({});
    check(!empty.winner() && !empty.winner_index(), "代码9.3 empty tree optional");
    bool bad = false;
    try {
        tree.replace(5, 0);
    } catch (const std::out_of_range&) {
        bad = true;
    }
    check(bad, "代码9.2 invalid player");
}
}  // namespace

int main() {
    test_replacement_selection();
    test_tournament_tree();
    std::printf("ExternalSort: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

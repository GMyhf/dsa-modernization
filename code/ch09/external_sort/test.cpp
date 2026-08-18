#include "modern.hpp"
#include "support/shared_cases.hpp"

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

bool is_sorted(const std::vector<int>& values) {
    for (std::size_t index = 1; index < values.size(); ++index) {
        if (values[index] < values[index - 1]) {
            return false;
        }
    }
    return true;
}

void test_replacement_selection() {
    const std::vector<int> textbook{50, 49, 35, 45, 30, 25, 15, 60, 16, 27, 1};
    const auto runs = dsa::external_sort::replacement_selection(textbook, 7);
    check(runs.size() == 2, "算法9.1 textbook input makes two runs");
    check(runs[0] == std::vector<int>({15, 25, 30, 35, 45, 49, 50, 60}),
          "算法9.1 first run is longer than memory");
    check(runs[1] == std::vector<int>({1, 16, 27}), "算法9.1 frozen keys form next run");
    check(runs[0].size() > 7, "算法9.1 first run exceeds heap size M");

    const auto heapsort_like = dsa::external_sort::replacement_selection({3, 1, 2}, 3);
    check(heapsort_like.size() == 1 && heapsort_like[0] == std::vector<int>({1, 2, 3}),
          "算法9.1 memory covering all input yields one run");
    check(dsa::external_sort::replacement_selection({}, 1).empty(), "算法9.1 empty input");
    check(dsa::external_sort::replacement_selection({1}, 1) ==
              std::vector<std::vector<int>>({{1}}),
          "算法9.1 singleton input");
    check(dsa::external_sort::replacement_selection({5, -1, 5, 0}, 2).size() >= 1,
          "算法9.1 duplicates and negatives produce runs");
    for (const auto& run : dsa::external_sort::replacement_selection({5, -1, 5, 0}, 2)) {
        check(is_sorted(run), "算法9.1 every run is sorted");
    }

    bool bad = false;
    try {
        (void)dsa::external_sort::replacement_selection({1}, 0);
    } catch (const std::invalid_argument&) {
        bad = true;
    }
    check(bad, "算法9.1 rejects zero memory");

    // 若退化成整表堆排序，只能交出一个顺串；冻结才是本节的教学内容。
    const auto tiny = dsa::external_sort::replacement_selection({9, 8, 7, 6, 5}, 2);
    check(tiny.size() > 1, "算法9.1 decreasing input freezes into multiple runs");
}

void test_winner_tree() {
    dsa::external_sort::WinnerTree tree({4, 1, 3, 1, 2});
    check(tree.winner() == 1, "代码9.2 initial winner");
    check(tree.winner_index() == 1, "代码9.2 tie uses left player");
    tree.replace(1, 9);
    check(tree.winner() == 1, "代码9.2 replay finds other winner");
    check(tree.winner_index() == 3, "代码9.2 replay index");
    tree.replace(3, 8);
    check(tree.winner() == 2, "代码9.2 successive replay");

    dsa::external_sort::WinnerTree empty({});
    check(!empty.winner() && !empty.winner_index(), "代码9.2 empty tree optional");
    bool bad = false;
    try {
        tree.replace(5, 0);
    } catch (const std::out_of_range&) {
        bad = true;
    }
    check(bad, "代码9.2 invalid player");
}

void test_loser_tree() {
    dsa::external_sort::LoserTree tree({4, 1, 3, 1, 2});
    check(tree.winner() == 1, "代码9.3 initial winner");
    check(tree.winner_index() == 1, "代码9.3 tie uses left player");
    check(tree.loser_at(1).has_value(), "代码9.3 records a loser at the root");
    tree.replace(1, 9);
    check(tree.winner() == 1, "代码9.3 replay finds other winner");
    check(tree.winner_index() == 3, "代码9.3 replay index");
    tree.replace(3, 8);
    check(tree.winner() == 2, "代码9.3 successive replay");
    tree.replace(4, 10);
    check(tree.winner() == 3, "代码9.3 later replay");

    dsa::external_sort::LoserTree empty({});
    check(!empty.winner() && !empty.winner_index(), "代码9.3 empty tree optional");
    bool bad = false;
    try {
        tree.replace(5, 0);
    } catch (const std::out_of_range&) {
        bad = true;
    }
    check(bad, "代码9.3 invalid player");
}
}  // namespace

int main() {
    test_replacement_selection();
    test_winner_tree();
    test_loser_tree();
    const auto shared = dsa::shared_cases::load();
    for (const auto& item : shared) {
        if (item.operation == "winner") { dsa::external_sort::WinnerTree tree(dsa::shared_cases::integers(item.input)); check(tree.winner() == std::stoi(item.expected), "T-047 winner"); }
        else { const auto split = item.input.find('|'); const auto memory = std::stoul(item.input.substr(0, split)); const auto values = dsa::shared_cases::integers(item.input.substr(split + 1)); if (item.expected_error.empty()) check(dsa::external_sort::replacement_selection(values, memory).size() == std::stoul(item.expected), "T-047 replacement"); else { bool raised = false; try { (void)dsa::external_sort::replacement_selection(values, memory); } catch (const std::invalid_argument&) { raised = true; } check(raised, "T-047 replacement exception"); } }
    }
    std::printf("共享用例: %zu\n", shared.size());
    std::printf("ExternalSort: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

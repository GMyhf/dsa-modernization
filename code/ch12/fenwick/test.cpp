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

void test_prefix_and_ranges() {
    dsa::fenwick::FenwickTree tree(8);
    const std::vector<long long> values{3, -2, 7, 0, 5, 1, -4, 6};
    for (std::size_t i = 0; i < values.size(); ++i) tree.add(i, values[i]);
    for (std::size_t end = 0; end <= values.size(); ++end) {
        long long expected = 0;
        for (std::size_t i = 0; i < end; ++i) expected += values[i];
        check(tree.prefix_sum(end) == expected, "前缀和对拍");
    }
    for (std::size_t left = 0; left <= values.size(); ++left) {
        for (std::size_t right = left; right <= values.size(); ++right) {
            long long expected = 0;
            for (std::size_t i = left; i < right; ++i) expected += values[i];
            check(tree.range_sum(left, right) == expected, "半开区间对拍");
        }
    }
}

void test_updates_and_boundaries() {
    dsa::fenwick::FenwickTree tree(3);
    tree.set(0, 10);
    tree.set(1, -2);
    tree.add(1, 5);
    check(tree.value_at(0) == 10 && tree.value_at(1) == 3, "set 与 add 的当前值");
    check(tree.range_sum(0, 2) == 13, "更新后的区间和");
    bool a = false, b = false, c = false;
    try { tree.add(3, 1); } catch (const std::out_of_range&) { a = true; }
    try { (void)tree.prefix_sum(4); } catch (const std::out_of_range&) { b = true; }
    try { (void)tree.range_sum(2, 1); } catch (const std::out_of_range&) { c = true; }
    check(a && b && c, "越界或反向区间必须拒绝");
    check(dsa::fenwick::FenwickTree::lowbit(12) == 4, "lowbit");
}

void test_shared_cases() {
    const auto cases = dsa::shared_cases::load();
    for (const auto& item : cases) {
        const auto split = item.input.find('|');
        const auto values = dsa::shared_cases::integers(item.input.substr(0, split));
        dsa::fenwick::FenwickTree tree(values.size());
        for (std::size_t i = 0; i < values.size(); ++i) tree.add(i, values[i]);
        if (item.expected_error == "out_of_range") {
            bool raised = false;
            try { (void)tree.prefix_sum(static_cast<std::size_t>(std::stoul(item.input.substr(split + 1)))); }
            catch (const std::out_of_range&) { raised = true; }
            check(raised, "T-047 Fenwick exception");
        } else if (item.operation == "prefix") {
            check(tree.prefix_sum(static_cast<std::size_t>(std::stoul(item.input.substr(split + 1)))) ==
                      std::stoll(item.expected), "T-047 Fenwick prefix");
        } else {
            const auto bounds = dsa::shared_cases::integers(item.input.substr(split + 1));
            check(tree.range_sum(bounds[0], bounds[1]) == std::stoll(item.expected),
                  "T-047 Fenwick range");
        }
    }
    std::printf("共享用例: %zu\n", cases.size());
}
}  // namespace

int main() {
    test_prefix_and_ranges();
    test_updates_and_boundaries();
    test_shared_cases();
    std::printf("Fenwick: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

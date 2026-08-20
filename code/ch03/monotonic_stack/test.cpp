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

std::vector<std::size_t> brute_next(const std::vector<int>& values, bool smaller) {
    std::vector<std::size_t> result(values.size(), values.size());
    for (std::size_t i = 0; i < values.size(); ++i) {
        for (std::size_t j = i + 1; j < values.size(); ++j) {
            if ((smaller && values[j] < values[i]) || (!smaller && values[j] > values[i])) {
                result[i] = j;
                break;
            }
        }
    }
    return result;
}

std::vector<std::size_t> as_indices(const std::vector<int>& values) {
    std::vector<std::size_t> result;
    for (int value : values) result.push_back(static_cast<std::size_t>(value));
    return result;
}

long long brute_histogram(const std::vector<int>& heights) {
    long long best = 0;
    for (std::size_t left = 0; left < heights.size(); ++left) {
        int current = heights[left];
        for (std::size_t right = left; right < heights.size(); ++right) {
            if (heights[right] < current) current = heights[right];
            const long long width = static_cast<long long>(right - left + 1);
            if (static_cast<long long>(current) * width > best) best = current * width;
        }
    }
    return best;
}

void test_next_indices() {
    for (const std::vector<int>& values : {std::vector<int>{}, {2}, {2, 2, 2},
                                           {4, 1, 3, 2, 5}, {8, 7, 6, 5, 4, 3, 2, 1, 0}}) {
        check(dsa::monotonic_stack::next_greater_indices(values) == brute_next(values, false),
              "下一个更大值对拍");
        check(dsa::monotonic_stack::next_smaller_indices(values) == brute_next(values, true),
              "下一个更小值对拍");
    }
}

void test_histograms() {
    for (const std::vector<int>& heights : {std::vector<int>{}, {2}, {2, 1, 2},
                                            {2, 1, 5, 6, 2, 3}, {1, 2, 3, 4}}) {
        check(dsa::monotonic_stack::largest_rectangle_area(heights) == brute_histogram(heights),
              "直方图暴力对拍");
    }
    bool raised = false;
    try { (void)dsa::monotonic_stack::largest_rectangle_area({1, -1}); }
    catch (const std::invalid_argument&) { raised = true; }
    check(raised, "负高度必须拒绝");
}

void test_shared_cases() {
    const auto cases = dsa::shared_cases::load();
    for (const auto& item : cases) {
        const auto values = dsa::shared_cases::integers(item.input);
        if (item.expected_error == "invalid_argument") {
            bool raised = false;
            try { (void)dsa::monotonic_stack::largest_rectangle_area(values); }
            catch (const std::invalid_argument&) { raised = true; }
            check(raised, "T-047 monotonic exception");
        } else if (item.operation == "nge") {
            check(dsa::monotonic_stack::next_greater_indices(values) ==
                      as_indices(dsa::shared_cases::integers(item.expected)), "T-047 monotonic nge");
        } else if (item.operation == "nse") {
            check(dsa::monotonic_stack::next_smaller_indices(values) ==
                      as_indices(dsa::shared_cases::integers(item.expected)), "T-047 monotonic nse");
        } else {
            check(dsa::monotonic_stack::largest_rectangle_area(values) == std::stoll(item.expected),
                  "T-047 monotonic histogram");
        }
    }
    std::printf("共享用例: %zu\n", cases.size());
}
}  // namespace

int main() {
    test_next_indices();
    test_histograms();
    test_shared_cases();
    std::printf("MonotonicStack: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

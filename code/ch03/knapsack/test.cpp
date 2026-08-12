// 背包问题的自带断言测试。
//
// 核心判据：**三种实现必须给出一致的结论**，且返回的下标集合必须真的能凑出承重。
// 只断言"返回了 true"是不够的——原书打印物品、不返回结果，正确性无从检验。
#include "modern.hpp"

#include <algorithm>
#include <cstdio>
#include <iostream>
#include <numeric>
#include <sstream>
#include <stdexcept>
#include <string>
#include <vector>

namespace {

int checks = 0, failures = 0;
void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::printf("  FAIL: %s\n", what.c_str()); }
}

/// 独立验算：把返回的下标加起来，必须恰好等于承重，且下标不重复、不越界。
bool solution_is_valid(const dsa::knapsack_solution& picked, int capacity,
                       const std::vector<int>& weights) {
    std::vector<std::size_t> sorted = picked;
    std::sort(sorted.begin(), sorted.end());
    if (std::adjacent_find(sorted.begin(), sorted.end()) != sorted.end()) return false;  // 有重复
    long long sum = 0;
    for (std::size_t i : picked) {
        if (i >= weights.size()) return false;
        sum += weights[i];
    }
    return sum == capacity;
}

/// 独立参照物：暴力枚举所有子集，判定是否存在和为 capacity 的子集。
bool brute_force_has_solution(int capacity, const std::vector<int>& weights) {
    const std::size_t n = weights.size();
    for (unsigned mask = 0; mask < (1u << n); ++mask) {
        long long sum = 0;
        for (std::size_t i = 0; i < n; ++i) {
            if (mask & (1u << i)) sum += weights[i];
        }
        if (sum == capacity) return true;
    }
    return false;
}

void expect_agreement(int capacity, const std::vector<int>& weights, const char* label) {
    const auto truth = brute_force_has_solution(capacity, weights);
    const auto r = dsa::knapsack_recursive(capacity, weights);
    const auto e = dsa::knapsack_with_explicit_stack(capacity, weights);
    const auto o = dsa::knapsack_optimized(capacity, weights);

    std::ostringstream d;
    d << label << "（承重 " << capacity << "）";
    check(r.has_value() == truth, "递归版与暴力枚举结论一致 " + d.str());
    check(e.has_value() == truth, "显式栈版与暴力枚举结论一致 " + d.str());
    check(o.has_value() == truth, "优化版与暴力枚举结论一致 " + d.str());
    if (r) check(solution_is_valid(*r, capacity, weights), "递归版给出的下标真能凑出承重 " + d.str());
    if (e) check(solution_is_valid(*e, capacity, weights), "显式栈版给出的下标真能凑出承重 " + d.str());
    if (o) check(solution_is_valid(*o, capacity, weights), "优化版给出的下标真能凑出承重 " + d.str());
}

void test_book_style_cases() {
    expect_agreement(10, {2, 3, 5, 7}, "恰好可解（2+3+5）");
    expect_agreement(11, {2, 3, 5, 7}, "另一组解（. . 5+. 或 2+. .）");
    expect_agreement(1, {2, 3, 5, 7}, "无解：最小物品都超了");
    expect_agreement(0, {2, 3, 5}, "承重为 0：什么都不选即为解（递归出口 1）");
    expect_agreement(17, {2, 3, 5, 7}, "全选恰好");
    expect_agreement(18, {2, 3, 5, 7}, "比全选还多，无解");
}

void test_edge_cases() {
    expect_agreement(0, {}, "空物品 + 承重 0：有解");
    expect_agreement(5, {}, "空物品 + 承重非 0：无解");
    expect_agreement(5, {5}, "单件恰好");
    expect_agreement(5, {6}, "单件超重");
    expect_agreement(6, {3, 3}, "两件同重");
}

// 穷举小规模输入，三种实现与暴力枚举四方对拍。
void test_exhaustive_agreement() {
    const std::vector<int> weights{1, 2, 3, 4, 6};
    int mismatches = 0;
    for (int capacity = 0; capacity <= 20; ++capacity) {
        const bool truth = brute_force_has_solution(capacity, weights);
        const auto r = dsa::knapsack_recursive(capacity, weights);
        const auto e = dsa::knapsack_with_explicit_stack(capacity, weights);
        const auto o = dsa::knapsack_optimized(capacity, weights);
        if (r.has_value() != truth || e.has_value() != truth || o.has_value() != truth) ++mismatches;
        if (r && !solution_is_valid(*r, capacity, weights)) ++mismatches;
        if (e && !solution_is_valid(*e, capacity, weights)) ++mismatches;
        if (o && !solution_is_valid(*o, capacity, weights)) ++mismatches;
    }
    check(mismatches == 0, "承重 0..20 全部穷举：三种实现与暴力枚举四方一致");
}

// 原书没有的检查：重量非正会让两个递归出口失去意义。
void test_invalid_input_is_rejected() {
    int thrown = 0;
    for (auto&& bad : std::vector<std::vector<int>>{{0}, {-1}, {2, 0, 3}}) {
        try { (void)dsa::knapsack_recursive(5, bad); } catch (const std::invalid_argument&) { ++thrown; }
    }
    try { (void)dsa::knapsack_recursive(-1, {2}); } catch (const std::invalid_argument&) { ++thrown; }
    check(thrown == 4, "重量非正或承重为负一律抛 invalid_argument");
}

// 缺陷 1：原书 `cout << w[n-1]` 把解直接打印出来，调用方拿不到、也没法验算。
void test_no_console_output() {
    std::ostringstream captured;
    std::streambuf* old_out = std::cout.rdbuf(captured.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(captured.rdbuf());
    (void)dsa::knapsack_recursive(10, {2, 3, 5, 7});
    (void)dsa::knapsack_with_explicit_stack(10, {2, 3, 5, 7});
    (void)dsa::knapsack_optimized(10, {2, 3, 5, 7});
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(captured.str().empty(), "三种实现全程不向 cout/cerr 写任何东西");
}

}  // namespace

int main() {
    test_book_style_cases();
    test_edge_cases();
    test_exhaustive_agreement();
    test_invalid_input_is_rejected();
    test_no_console_output();
    std::printf("Knapsack: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

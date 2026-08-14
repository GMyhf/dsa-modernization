// 递归与栈空间的自带断言测试。
//
// 本单元的判据核心：**三种实现必须逐个给出相同答案**，且原书会静默出错的地方
// （溢出、负数）必须抛异常。只测"能算出个数"的用例，在原书那份实现下同样全绿。
#include "modern.hpp"

#include <cstdio>
#include <iostream>
#include <sstream>
#include <stdexcept>
#include <string>

namespace {

int checks = 0, failures = 0;
void check(bool ok, const std::string& what) {
    ++checks;
    if (!ok) { ++failures; std::printf("  FAIL: %s\n", what.c_str()); }
}

// 三种实现互为参照物：递归（算法3.6）、迭代（算法3.8）、显式栈（算法3.9）。
void test_three_implementations_agree() {
    // 20! = 2432902008176640000，是 64 位无符号能容纳的最大阶乘
    const dsa::factorial_type expected[] = {
        1, 1, 2, 6, 24, 120, 720, 5040, 40320, 362880, 3628800};
    bool all_ok = true;
    for (long long n = 0; n <= 10; ++n) {
        const auto r = dsa::factorial_recursive(n);
        const auto i = dsa::factorial_iterative(n);
        const auto s = dsa::factorial_with_explicit_stack(n);
        all_ok = all_ok && r == expected[n] && i == expected[n] && s == expected[n];
    }
    check(all_ok, "0! 到 10!：递归/迭代/显式栈三者与手算值逐个一致");

    bool agree_high = true;
    for (long long n = 11; n <= 20; ++n) {
        const auto r = dsa::factorial_recursive(n);
        agree_high = agree_high && r == dsa::factorial_iterative(n)
                                && r == dsa::factorial_with_explicit_stack(n);
    }
    check(agree_high, "11! 到 20!：三种实现互相一致");
    check(dsa::factorial_iterative(20) == 2432902008176640000ULL, "20! 的值精确正确");
    // 边界：递归出口本身
    check(dsa::factorial_recursive(0) == 1 && dsa::factorial_iterative(0) == 1
          && dsa::factorial_with_explicit_stack(0) == 1, "0! = 1（递归出口）");
    check(dsa::factorial_recursive(1) == 1 && dsa::factorial_iterative(1) == 1
          && dsa::factorial_with_explicit_stack(1) == 1, "1! = 1（出口的邻居）");
    // 上限那一点三者必须完全一致，差一位都不行
    check(dsa::factorial_recursive(20) == dsa::factorial_iterative(20)
          && dsa::factorial_recursive(20) == dsa::factorial_with_explicit_stack(20),
          "在 64 位上限 20! 处三种实现逐位一致");
}

// 缺陷 1：原书用 long 且不查溢出。实测 factorial(21) = -4249290049419214848，
// factorial(66) = 0，且 UBSan 判定为 signed integer overflow（未定义行为）。
void test_overflow_is_rejected_not_silently_wrong() {
    int thrown = 0;
    for (long long n : {21LL, 25LL, 66LL, 1000LL}) {
        try { (void)dsa::factorial_recursive(n); } catch (const std::overflow_error&) { ++thrown; }
    }
    check(thrown == 4, "勘误E14 算法3.6/3.8/3.9：21! 起一律抛 overflow_error，而不是返回负数或 0");

    int thrown_all = 0;
    try { (void)dsa::factorial_iterative(21); } catch (const std::overflow_error&) { ++thrown_all; }
    try { (void)dsa::factorial_with_explicit_stack(21); } catch (const std::overflow_error&) { ++thrown_all; }
    check(thrown_all == 2, "三种实现在溢出边界上行为一致");
    check(dsa::kMaxFactorialInput == 20, "20 是 64 位无符号的阶乘上限");
}

// 缺陷 2：原书 `if (n <= 0) return 1;` 把负数静默当成 0，返回 1。
void test_negative_is_rejected() {
    int thrown = 0;
    for (long long n : {-1LL, -5LL, -1000LL}) {
        try { (void)dsa::factorial_recursive(n); } catch (const std::invalid_argument&) { ++thrown; }
        try { (void)dsa::factorial_iterative(n); } catch (const std::invalid_argument&) { ++thrown; }
        try { (void)dsa::factorial_with_explicit_stack(n); } catch (const std::invalid_argument&) { ++thrown; }
    }
    check(thrown == 9, "勘误E15 算法3.6：负数一律抛 invalid_argument，而不是静默返回 1");
}

// 3.1.5 的正题：递归的数据在运行栈上，显式栈的数据在堆上。
void test_explicit_stack_goes_far_beyond_recursion_depth() {
    const std::uint64_t n = 1000000;   // 100 万层
    const std::uint64_t expected = n * (n + 1) / 2;
    check(dsa::sum_to_with_explicit_stack(n) == expected,
          "显式栈版在 100 万层规模下正确——数据在堆上，不受运行栈上限约束");

    // 递归版在这个规模上会不会爆栈**取决于优化档**：本机实测 -O0 与 -O1+ASan 在
    // 50 万层崩溃，而 -O2 把递归转成了循环、100 万层照样通过（书稿 3.1.5 有表）。
    // 所以这里只在一个**任何档位都安全**的深度上验证两者一致，不去逼近边界——
    // 逼近边界就意味着让闸门崩。
    const std::uint64_t safe = 10000;
    check(dsa::sum_to_recursive(safe) == dsa::sum_to_with_explicit_stack(safe),
          "安全深度（1 万层）下递归与显式栈结果一致");
    check(dsa::sum_to_with_explicit_stack(safe) == safe * (safe + 1) / 2,
          "显式栈版与闭式公式 n(n+1)/2 一致");
    check(dsa::sum_to_recursive(0) == 0 && dsa::sum_to_with_explicit_stack(0) == 0,
          "n = 0 时两者都返回 0，不越界也不死循环");
}

// 容器与算法内部不做 I/O。原书栈的实现失败时会 cout 打提示。
void test_no_console_output() {
    std::ostringstream captured;
    std::streambuf* old_out = std::cout.rdbuf(captured.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(captured.rdbuf());
    (void)dsa::factorial_with_explicit_stack(10);
    try { (void)dsa::factorial_recursive(99); } catch (const std::overflow_error&) {}
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(captured.str().empty(), "全程不向 cout/cerr 写任何东西");
}

}  // namespace

int main() {
    test_three_implementations_agree();
    test_overflow_is_rejected_not_silently_wrong();
    test_negative_is_rejected();
    test_explicit_stack_goes_far_beyond_recursion_depth();
    test_no_console_output();
    std::printf("RecursionAndStack: %d 项断言，%d 失败\n", checks, failures);
    return failures == 0 ? 0 : 1;
}

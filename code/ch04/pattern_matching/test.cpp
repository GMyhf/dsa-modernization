// 模式匹配的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 本单元最要紧的一条：**原书的返回值差 1**。所以这里所有匹配用例都拿
// std::string_view::find 当独立参照物逐个比对，而不是只测"能找到"。
// 只断言 has_value() 的测试，在原书那份差一实现下同样全绿——那就等于没测。
#include "modern.hpp"
#include "support/shared_cases.hpp"

#include <cstdio>
#include <iostream>
#include <random>
#include <sstream>
#include <stdexcept>
#include <string>
#include <string_view>
#include <vector>

namespace {

int g_checks = 0;
int g_failed = 0;

void check(bool ok, const std::string& what) {
    ++g_checks;
    if (!ok) {
        ++g_failed;
        std::printf("  FAIL: %s\n", what.c_str());
    }
}

/// 独立参照物：标准库的 find。两个算法都必须与它逐字节一致。
std::optional<std::size_t> reference(std::string_view text, std::string_view pattern) {
    const auto pos = text.find(pattern);
    return pos == std::string_view::npos ? std::nullopt : std::optional<std::size_t>(pos);
}

void expect_same(std::string_view text, std::string_view pattern, const char* label) {
    const auto want = reference(text, pattern);
    const auto naive = dsa::naive_search(text, pattern);
    const auto kmp = dsa::kmp_search(text, pattern);
    std::ostringstream desc;
    desc << label << "（T=\"" << text.substr(0, 24) << "\" P=\"" << pattern << "\"）";
    check(naive == want, "勘误R10 算法4.6：朴素匹配的下标与标准库一致 " + desc.str());
    check(kmp == want, "勘误R13 算法4.8：KMP 的下标与标准库一致 " + desc.str());
}

// 缺陷 1：原书【算法4.6】【算法4.8】都写 `return (j - pLen + 1)`，0 起始下标下差 1。
void test_match_position_is_exact() {
    expect_same("abc", "abc", "整串相等");                    // 原书返回 1，正确 0
    expect_same("xabc", "abc", "匹配在下标 1");                // 原书返回 2，正确 1
    expect_same("aaab", "ab", "需要回溯");                     // 原书返回 3，正确 2
    // 书中图4.12 自己用的那组数据：匹配始于下标 10，原书两个算法都返回 11
    expect_same("abcddabcababcdaabcababcdaabcabaa", "abcdaabcab", "勘误E17 勘误E21 算法4.6/4.8：书中图4.12 的例子，0 起始下标返回 j-pLen 而不是加 1");
    expect_same("aaaaa", "aa", "重叠匹配取最左");
    expect_same("abcabcabd", "abcabd", "长回溯");
}

void test_not_found() {
    expect_same("abcdef", "xyz", "完全不含");
    expect_same("abc", "abcd", "模式比目标长");
    expect_same("", "a", "空目标");
    expect_same("aaaa", "aaab", "只差最后一个字符");
}

void test_empty_pattern() {
    // 约定与 std::string::find("") 一致：返回 0。原书用 assert(m>0) 挡，
    // 而 assert 在 NDEBUG 下会被整个编译掉——release 构建里就是越界写。
    check(dsa::naive_search("abc", "") == std::optional<std::size_t>(0), "空模式在朴素匹配下返回 0");
    check(dsa::kmp_search("abc", "") == std::optional<std::size_t>(0), "空模式在 KMP 下返回 0");
    check(dsa::build_next("").empty(), "空模式的 next 数组为空，不越界写");
}

// 【算法4.7】：与书中图4.11 的最终结果逐个比对。
void test_next_matches_the_book_figure() {
    const auto next = dsa::build_next("abcdaabcab");
    const std::vector<dsa::next_type> from_figure{-1, 0, 0, 0, -1, 1, 0, 0, 3, 0};
    check(next.size() == 10, "next 数组长度等于模式长度");
    check(next == from_figure, "勘误R12 算法4.7：next 数组与书中图4.11 最后一行逐个一致");
    // 注意：书中**正文**写的是 {-1,0,0,0,0,-1,1,0,0,3,0}，11 个值，比模式还长一位。
    // 正文与图4.11 自相矛盾，算法实算的结果站在图这一边。详见 legacy.md 缺陷 4。
    check(from_figure.size() == 10, "模式 \"abcdaabcab\" 只有 10 个字符，正文那 11 个值必有一处错");
}

void test_next_basic_properties() {
    for (std::string_view p : {"a", "aa", "aaaa", "abab", "abcabc", "aabaaab", "mississippi"}) {
        const auto next = dsa::build_next(p);
        check(next.size() == p.size(), std::string("next 长度匹配：") + std::string(p));
        check(next[0] == -1, std::string("next[0] 恒为 -1：") + std::string(p));
        bool bounded = true;
        for (std::size_t i = 0; i < next.size(); ++i) {
            bounded = bounded && next[i] < static_cast<dsa::next_type>(i) && next[i] >= -1;
        }
        check(bounded, std::string("next[i] 严格小于 i 且不小于 -1（保证回退会终止）：") + std::string(p));
    }
}

void test_kmp_reuses_next_across_targets() {
    const std::string_view pattern = "abcab";
    const auto next = dsa::build_next(pattern);  // 只算一次
    check(dsa::kmp_search("zzabcab", pattern, next) == std::optional<std::size_t>(2), "复用 next：第一个目标");
    check(dsa::kmp_search("abcabx", pattern, next) == std::optional<std::size_t>(0), "复用 next：第二个目标");
    check(!dsa::kmp_search("abcba", pattern, next).has_value(), "复用 next：第三个目标不匹配");

    bool threw = false;
    try {
        (void)dsa::kmp_search("abc", pattern, dsa::build_next("xy"));
    } catch (const std::invalid_argument&) {
        threw = true;
    }
    check(threw, "next 与模式不配套时抛 invalid_argument，而不是默默读错数组");
}

// 随机对拍：两个算法与标准库三方一致。差一错误、回溯错误都躲不过这一关。
void test_randomised_agreement() {
    std::mt19937 rng(20260812);  // 固定种子，失败可复现
    std::uniform_int_distribution<int> alphabet(0, 2);   // 只用 a/b/c，制造大量重复与回溯
    std::uniform_int_distribution<int> tlen(0, 40);
    std::uniform_int_distribution<int> plen(1, 6);

    int mismatches = 0;
    int found = 0;
    for (int round = 0; round < 3000; ++round) {
        std::string text, pattern;
        for (int i = 0, n = tlen(rng); i < n; ++i) text += static_cast<char>('a' + alphabet(rng));
        for (int i = 0, n = plen(rng); i < n; ++i) pattern += static_cast<char>('a' + alphabet(rng));
        const auto want = reference(text, pattern);
        if (want) ++found;
        if (dsa::naive_search(text, pattern) != want || dsa::kmp_search(text, pattern) != want) {
            if (++mismatches <= 3) {
                std::printf("    对拍不一致: T=\"%s\" P=\"%s\"\n", text.c_str(), pattern.c_str());
            }
        }
    }
    check(mismatches == 0, "3000 组随机对拍：两个算法与标准库完全一致");
    check(found > 500, "随机样本里确实有大量成功匹配（否则这轮对拍没测到匹配路径）");
}

// 原书强调 KMP 的目标下标 j 只增不减，因此是线性时间。这里用一个朴素匹配的
// 最坏情况来把两者分开：P="aaaa...b" 在 T="aaaa...a" 上，朴素要 O(n·m)。
void test_kmp_is_linear_on_the_naive_worst_case() {
    const std::size_t n = 200000;
    const std::size_t m = 2000;
    std::string text(n, 'a');
    std::string pattern(m - 1, 'a');
    pattern += 'b';  // 每趟都在最后一个字符失配

    const auto next = dsa::build_next(pattern);
    check(!dsa::kmp_search(text, pattern, next).has_value(), "最坏情况下 KMP 正确报告未找到");
    // 朴素匹配在这组数据上要约 n×m = 4×10^8 次比较；这里不跑它，
    // 只让 KMP 跑——若 KMP 退化成回溯，本单元会撞上闸门 120 秒超时而变红。
    text += pattern;
    check(dsa::kmp_search(text, pattern, next) == std::optional<std::size_t>(n),
          "把模式接在目标末尾后，KMP 找到它且下标精确");
}

// 容器/算法内部不做 I/O。
void test_no_console_output() {
    std::ostringstream captured;
    std::streambuf* old_out = std::cout.rdbuf(captured.rdbuf());
    std::streambuf* old_err = std::cerr.rdbuf(captured.rdbuf());
    (void)dsa::naive_search("abc", "z");
    (void)dsa::kmp_search("abc", "z");
    (void)dsa::build_next("abc");
    std::cout.rdbuf(old_out);
    std::cerr.rdbuf(old_err);
    check(captured.str().empty(), "匹配算法全程不向 cout/cerr 写任何东西");
}

// ── T-072：最小周期 ────────────────────────────────────────────────────────
//
// 独立参照物是**定义本身**：逐个试 p = 1, 2, …，第一个满足 s[i] == s[i+p] 的就是。
// O(n²)，与 border_lengths 的回退链没有共用任何一行。
std::size_t brute_minimal_period(std::string_view s) {
    for (std::size_t p = 1; p <= s.size(); ++p) {
        bool ok = true;
        for (std::size_t i = 0; i + p < s.size() && ok; ++i) {
            ok = s[i] == s[i + p];
        }
        if (ok) {
            return p;
        }
    }
    return 0;
}

/// 最大 K：逐个试能整除 n 的真约数 d，看 s 是否等于 s[0..d) 重复 n/d 次。
std::size_t brute_repetition_count(std::string_view s) {
    const std::size_t n = s.size();
    for (std::size_t d = 1; d < n; ++d) {
        if (n % d != 0) {
            continue;
        }
        std::string built;
        for (std::size_t t = 0; t < n / d; ++t) {
            built += std::string(s.substr(0, d));
        }
        if (built == s) {
            return n / d;
        }
    }
    return n == 0 ? 0 : 1;
}

void test_minimal_period_brute_force() {
    std::size_t strings = 0;
    std::size_t period_bad = 0;
    std::size_t power_bad = 0;
    std::size_t border_bad = 0;
    std::size_t proper_repetitions = 0;
    for (std::size_t len = 0; len <= 10; ++len) {
        for (std::size_t mask = 0; mask < (std::size_t{1} << len); ++mask) {
            std::string s;
            for (std::size_t i = 0; i < len; ++i) {
                s += ((mask >> i) & 1U) ? 'b' : 'a';
            }
            ++strings;
            if (dsa::minimal_period(s) != brute_minimal_period(s)) {
                if (++period_bad <= 3) std::printf("    最小周期不一致: \"%s\"\n", s.c_str());
            }
            const std::size_t k = brute_repetition_count(s);
            if (dsa::repetition_count(s) != k || dsa::is_repetition(s) != (k > 1)) {
                if (++power_bad <= 3) std::printf("    乘方不一致: \"%s\"\n", s.c_str());
            }
            proper_repetitions += k > 1 ? 1 : 0;
            // 每个前缀的边界长度 == 前缀长度 − 该前缀的最小周期
            const auto border = dsa::border_lengths(s);
            for (std::size_t i = 0; i < len; ++i) {
                if (border[i] != i + 1 - brute_minimal_period(std::string_view(s).substr(0, i + 1))) {
                    ++border_bad;
                }
            }
        }
    }
    check(strings == 2047, "{a,b} 上长度 0..10 的串全部穷举（2047 个）");
    check(period_bad == 0, "T-072 minimal_period 与按定义逐个试 p 的暴力解在 2047 个串上一致");
    check(power_bad == 0, "T-072 repetition_count / is_repetition 与逐个试约数的暴力解一致");
    check(border_bad == 0, "T-072 border_lengths 的每个前缀值 == 前缀长 − 前缀最小周期");
    check(proper_repetitions > 50, "穷举里确有足够多的循环串（否则乘方分支没被测到）");
}

// 陷阱一：原书优化版 next 不是边界长度，拿它求周期会错。
// 未优化时「长度为 i 的前缀的最小周期 = i − next[i]」；把优化版代进这条公式：
void test_optimized_next_gives_wrong_periods() {
    const auto next = dsa::build_next("aaaa");
    const std::vector<dsa::next_type> optimized{-1, -1, -1, -1};
    check(next == optimized, "\"aaaa\" 的优化版 next 全是 −1");
    const auto wrong = 3 - next[3];  // 前缀 "aaa" 用优化版 next 算出的「周期」
    check(wrong == 4, "陷阱：用优化版 next 算前缀 \"aaa\" 的周期得 4，比前缀本身还长");
    check(dsa::minimal_period("aaa") == 1, "前缀 \"aaa\" 的最小周期实为 1");
    check(3 - dsa::border_lengths("aaaa")[2] == 1, "用未优化的 border_lengths 算同一个前缀得 1");

    // 不是个例：在 {a,b} 长度 1..10 的全部串、全部前缀上统计。
    std::size_t wrong_count = 0;
    std::size_t right_count = 0;
    for (std::size_t len = 1; len <= 10; ++len) {
        for (std::size_t mask = 0; mask < (std::size_t{1} << len); ++mask) {
            std::string s;
            for (std::size_t i = 0; i < len; ++i) s += ((mask >> i) & 1U) ? 'b' : 'a';
            const auto nx = dsa::build_next(s);
            const auto border = dsa::border_lengths(s);
            for (std::size_t i = 1; i < len; ++i) {
                const std::size_t truth = brute_minimal_period(std::string_view(s).substr(0, i));
                if (static_cast<dsa::next_type>(i) - nx[i] != static_cast<dsa::next_type>(truth)) ++wrong_count;
                if (i - border[i - 1] != truth) ++right_count;
            }
        }
    }
    check(wrong_count > 1000, "优化版 next 在上千个前缀上给出错误周期——陷阱是普遍的，不是个例");
    check(right_count == 0, "未优化的 border_lengths 在同一批前缀上一个都不错");
}

// 陷阱二：边界为 0 时 p == n，n % n == 0 恒成立，但这不是循环串。
void test_zero_border_is_not_a_repetition() {
    check(dsa::border_lengths("abcd").back() == 0, "\"abcd\" 的整串边界为 0");
    check(dsa::minimal_period("abcd") == 4, "边界为 0 时最小周期等于串长");
    check(!dsa::is_repetition("abcd"), "陷阱：\"abcd\" 满足 4 % 4 == 0，但不是循环串");
    check(!dsa::is_repetition("a"), "单字符串不是循环串");
    check(dsa::repetition_count("abcd") == 1, "\"abcd\" 的乘方次数是 1");
    check(!dsa::is_repetition("ababa"), "周期 2 不整除 5：\"ababa\" 不是循环串");
    check(dsa::is_repetition("abab"), "\"abab\" 是 \"ab\" 重复 2 次");
    check(dsa::minimal_period("") == 0 && dsa::repetition_count("") == 0 && !dsa::is_repetition(""),
          "空串：周期 0、乘方 0、不是循环串");
    check(dsa::border_lengths("").empty(), "空串的边界数组为空");
}

}  // namespace

int main() {
    test_match_position_is_exact();
    test_not_found();
    test_empty_pattern();
    test_next_matches_the_book_figure();
    test_next_basic_properties();
    test_kmp_reuses_next_across_targets();
    test_randomised_agreement();
    test_kmp_is_linear_on_the_naive_worst_case();
    test_no_console_output();
    test_minimal_period_brute_force();
    test_optimized_next_gives_wrong_periods();
    test_zero_border_is_not_a_repetition();
    const auto shared = dsa::shared_cases::load();
    for (const auto& item : shared) {
        if (item.operation == "period") {
            check(dsa::minimal_period(item.input) == std::stoul(item.expected),
                  "T-072 最小周期（共享用例 " + item.name + "）");
            continue;
        }
        if (item.operation == "repetition") {
            check(dsa::repetition_count(item.input) == std::stoul(item.expected),
                  "T-072 字符串乘方（共享用例 " + item.name + "）");
            continue;
        }
        check(item.operation == "search" || item.operation == "bad_next",
              "共享用例的 operation 可识别：" + item.operation);
        const auto split = item.input.find('|');
        const std::string text = item.input.substr(0, split);
        const std::string pattern = item.input.substr(split + 1);
        if (item.expected_error == "invalid_argument") {
            bool raised = false;
            try { (void)dsa::kmp_search(text, pattern, {}); }
            catch (const std::invalid_argument&) { raised = true; }
            check(raised, "T-047 KMP exception");
        } else {
            const auto found = dsa::kmp_search(text, pattern);
            const long actual = found ? static_cast<long>(*found) : -1;
            check(actual == std::stol(item.expected), "T-047 KMP result");
        }
    }
    std::printf("共享用例: %zu\n", shared.size());

    std::printf("PatternMatching: %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

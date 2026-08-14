// 模式匹配的自带断言测试。零框架：断言失败就返回非零退出码。
//
// 本单元最要紧的一条：**原书的返回值差 1**。所以这里所有匹配用例都拿
// std::string_view::find 当独立参照物逐个比对，而不是只测"能找到"。
// 只断言 has_value() 的测试，在原书那份差一实现下同样全绿——那就等于没测。
#include "modern.hpp"

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

    std::printf("PatternMatching: %d 项断言，%d 失败\n", g_checks, g_failed);
    return g_failed == 0 ? 0 : 1;
}

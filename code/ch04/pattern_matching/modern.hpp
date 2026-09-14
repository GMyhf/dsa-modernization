// 字符串模式匹配 —— 原书【算法4.6】【算法4.7】【算法4.8】的现代化实现。
//
// 本节的教学内容是**匹配算法本身**（朴素回溯、特征向量、KMP 的线性时间论证），
// 不是字符串容器——容器是 4.2 节的事（见 code/ch04/string_class）。
// 因此这里用 std::string_view 接收输入：不拷贝、不拥有，把注意力留给算法。
//
// 遵循 collab/DECISION_LOG.md 的 D-001：C++17；容器内零 I/O；越界与非法输入抛标准异常；
// 可预期的"没找到"用 std::optional。
#pragma once

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <string_view>
#include <vector>

namespace dsa {

/// next 数组的元素要能取 -1，所以是有符号的。
using next_type = std::ptrdiff_t;

// >>> naive
/// 朴素模式匹配：返回 pattern 在 text 中首次出现的**起始下标**；没有则 std::nullopt。
///
/// 与原书【算法4.6】的关键差别是返回值：原书写的是 `return (j - pLen + 1);`，
/// 而在 0 起始的下标体系里正确的是 `j - pLen`——**原书这里差了 1**。
/// 用书中自己的例子可以当场看出来：T="abcddabcab..."、P="abcdaabcab" 匹配始于下标 10，
/// 原书返回 11（证据见 legacy.md 缺陷 1）。
///
/// 空模式约定返回 0（与 std::string::find("") 一致）；原书用 assert(m>0) 把它挡在门外，
/// 而 assert 在 NDEBUG 下会被整个编译掉。
[[nodiscard]] inline std::optional<std::size_t> naive_search(std::string_view text,
                                                             std::string_view pattern) {
    const std::size_t n = text.size();
    const std::size_t m = pattern.size();
    if (m == 0) {
        return std::size_t{0};
    }
    if (n < m) {
        return std::nullopt;
    }
    std::size_t i = 0;  // 模式下标
    std::size_t j = 0;  // 目标下标
    while (i < m && j < n) {
        if (text[j] == pattern[i]) {
            ++i;
            ++j;
        } else {
            j = j - i + 1;  // 回退到本趟起点的下一个位置
            i = 0;
        }
    }
    return i >= m ? std::optional<std::size_t>(j - m) : std::nullopt;
}
// <<< naive

// >>> build-next
/// 计算模式的特征向量（next 数组），原书【算法4.7】的"优化版"。
///
/// 与原书的差别只有所有权：原书 `int* findNext(String P)` 用 `new int[m]` 返回裸数组，
/// 而书中**从未展示过与之配对的 delete[]**——每调用一次泄漏一个数组。
/// 计算过程一字未改，包括 `next[i] = next[k]` 这一步优化。
///
/// 空模式返回空向量；原书是 `assert(m > 0)`，而 assert 在 NDEBUG 下会被编译掉，
/// 于是 release 构建里 `new int[0]` 加 `next[0] = -1` 就是一次越界写。
[[nodiscard]] inline std::vector<next_type> build_next(std::string_view pattern) {
    const std::size_t m = pattern.size();
    std::vector<next_type> next(m);
    if (m == 0) {
        return next;
    }
    next_type i = 0;
    next_type k = -1;
    next[0] = -1;
    while (i < static_cast<next_type>(m)) {
        while (k >= 0 && pattern[static_cast<std::size_t>(i)] != pattern[static_cast<std::size_t>(k)]) {
            k = next[static_cast<std::size_t>(k)];  // 沿已算好的特征值回退
        }
        ++i;
        ++k;
        if (i == static_cast<next_type>(m)) {
            break;
        }
        const auto ui = static_cast<std::size_t>(i);
        const auto uk = static_cast<std::size_t>(k);
        // P[i] 与 P[k] 相等时可以直接借用 next[k]，省掉一次注定失败的比较——这就是"优化版"。
        next[ui] = (pattern[ui] == pattern[uk]) ? next[uk] : k;
    }
    return next;
}
// <<< build-next

// >>> kmp
/// KMP 模式匹配。失配时不再把模式右移一位，而是按特征值决定右移多少。
///
/// 返回值与 naive_search 一致，也修正了原书【算法4.8】同样的差一错误。
/// next 由调用方传入：同一个模式可以只算一次、多次匹配复用——
/// 这正是原书强调的性质，接口把它显式表达出来。
[[nodiscard]] inline std::optional<std::size_t> kmp_search(std::string_view text,
                                                           std::string_view pattern,
                                                           const std::vector<next_type>& next) {
    const std::size_t n = text.size();
    const std::size_t m = pattern.size();
    if (m == 0) {
        return std::size_t{0};
    }
    if (next.size() != m) {
        throw std::invalid_argument("kmp_search: next 数组长度与模式不符");
    }
    if (n < m) {
        return std::nullopt;
    }
    next_type i = 0;    // 模式下标，可以退到 -1
    std::size_t j = 0;  // 目标下标，只增不减
    while (i < static_cast<next_type>(m) && j < n) {
        if (i == -1 || text[j] == pattern[static_cast<std::size_t>(i)]) {
            ++i;
            ++j;
        } else {
            i = next[static_cast<std::size_t>(i)];
        }
    }
    return i >= static_cast<next_type>(m) ? std::optional<std::size_t>(j - m) : std::nullopt;
}

/// 便利重载：模式只用一次时，自己把 next 算掉。
[[nodiscard]] inline std::optional<std::size_t> kmp_search(std::string_view text,
                                                           std::string_view pattern) {
    return kmp_search(text, pattern, build_next(pattern));
}
// <<< kmp

// >>> border-lengths
/// 未优化的失效函数：border[i] 是前缀 s[0..i] 的**最长真边界**长度——
/// 既是它的真前缀、又是它的真后缀的最长那一段有多长。
///
/// 这就是 build_next 去掉「优化」那一步之后的数，只是下标错开一位、没有 −1：
/// 未优化的 next[i+1] == border[i]。回退 `k = border[k - 1]` 与 build_next 的
/// `k = next[k]` 是同一个动作。
///
/// **求周期不能拿 build_next 代替它。** 优化版的 `next[i] = next[k]` 专为匹配服务：
/// 当 P[i] == P[k] 时它跳过一个注定失配的落点，于是 next[i] 不再是边界长度。
/// 例如 "aaaa" 的优化版 next 是 {−1,−1,−1,−1}，而边界长度是 {0,1,2,3}。
[[nodiscard]] inline std::vector<std::size_t> border_lengths(std::string_view s) {
    const std::size_t n = s.size();
    std::vector<std::size_t> border(n);
    for (std::size_t i = 1; i < n; ++i) {
        std::size_t k = border[i - 1];  // 先试着把上一个前缀的最长边界延长一个字符
        while (k > 0 && s[i] != s[k]) {
            k = border[k - 1];  // 延长不了，退到「边界的边界」再试
        }
        if (s[i] == s[k]) {
            ++k;
        }
        border[i] = k;
    }
    return border;
}
// <<< border-lengths

// >>> minimal-period
/// 最小周期 p：满足 s[i] == s[i+p]（对所有 0 ≤ i < n−p）的最小正整数。
/// 定理：p = n − （整串的最长真边界长度）。空串约定返回 0。
///
/// 注意 p 不一定整除 n："ababa" 的最小周期是 2，但它不是某个串重复若干次。
[[nodiscard]] inline std::size_t minimal_period(std::string_view s) {
    const std::size_t n = s.size();
    if (n == 0) {
        return 0;
    }
    return n - border_lengths(s)[n - 1];
}

/// s 能否写成某个**更短**的串重复至少两次（「循环串」问题）。
///
/// 两个条件缺一不可：
///   n % p == 0 —— 周期不整除长度就拼不回整串（"ababa"）；
///   p < n      —— 边界为 0 时 p == n，而 n % n == 0 **恒成立**，
///                 漏掉这一条会把 "abcd" 这种毫无重复的串也判成循环串。
[[nodiscard]] inline bool is_repetition(std::string_view s) {
    const std::size_t n = s.size();
    const std::size_t p = minimal_period(s);
    return p < n && n % p == 0;
}

/// 最大的 K，使 s 恰好是某个串重复 K 次（「字符串乘方」问题）。
/// 不是循环串时 K = 1（s 就是它自己重复一次）；空串约定返回 0。
[[nodiscard]] inline std::size_t repetition_count(std::string_view s) {
    if (s.empty()) {
        return 0;
    }
    return is_repetition(s) ? s.size() / minimal_period(s) : 1;
}
// <<< minimal-period

}  // namespace dsa

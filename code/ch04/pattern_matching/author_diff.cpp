// author_diff.cpp —— 与作者代码包对拍（由 tools/authorsrc.py --check 编译运行，不归 check_code 管）
//
// 作者包 ch04_String/alg6/naive.h（算法4.6）与 alg7-8/kmp.h（算法4.7、4.8），
// 在同一批文本/模式上与本单元的 naive_search / build_next / kmp_search 对拍。
//
// 两条结论由这里的断言钉住：
//   * 匹配位置：作者包里两种匹配也都返回 j - pLen + 1——**勘误 E17 / R10 / R13 不是排印问题，
//     作者的代码本来就按 1 起始报位置**，而第 4.3 节正文说下标从 0 起。于是找到时恒有
//     「作者 = 本单元 + 1」，没找到时两边都报没找到。
//   * 特征向量：作者 findNext 与本单元 build_next 逐项相等（都是带 next[i] = next[k] 优化的版本）。
//     作者包写的是 while (i < m) 加 if (i == m) break——break 在这里是必需的；
//     原书印成 while (i < m-1) 又留着那个 break，才有了勘误 R12「多余的 break」。
#include <cstdlib>
#include <iostream>
#include <random>
#include <string>
#include <vector>

#include "modern.hpp"

#include "alg6/naive.h"
#include "alg7-8/kmp.h"

namespace {

int failures = 0;

void expect(bool ok, const std::string& what) {
    if (!ok) {
        ++failures;
        std::cerr << "  ✗ " << what << "\n";
    }
}

std::string random_string(std::mt19937& rng, std::size_t length, char alphabet_end) {
    std::uniform_int_distribution<int> pick('a', alphabet_end);
    std::string out(length, 'a');
    for (char& c : out) c = static_cast<char>(pick(rng));
    return out;
}

}  // namespace

int main() {
    std::vector<std::pair<std::string, std::string>> cases = {
        {"aaaaaaaaab", "aaab"},           // 朴素匹配的坏情况
        {"abcdaabcab", "abcdaabcab"},     // 算法4.7 旁的特征向量例子，整串自匹配
        {"ababababca", "abababca"},
        {"abcabcabd", "abcabd"},
        {"aaaa", "b"},
        {"ab", "abc"},                    // 目标比模式短
        {"a", "a"},
    };
    std::mt19937 rng(20080601);
    for (int round = 0; round < 400; ++round) {
        std::string text = random_string(rng, 1 + rng() % 40, round % 2 ? 'b' : 'c');
        std::string pattern = random_string(rng, 1 + rng() % 6, round % 2 ? 'b' : 'c');
        cases.emplace_back(text, pattern);
    }

    int found = 0;
    for (const auto& [text, pattern] : cases) {
        const std::string label = "T=\"" + text + "\" P=\"" + pattern + "\"";
        const auto ours = dsa::naive_search(text, pattern);
        const auto ours_kmp = dsa::kmp_search(text, pattern);
        const int author_naive = NaiveStrMatching(text, pattern);
        int* author_next = findNext(pattern);
        const int author_kmp = KMPStrMatching(text, pattern, author_next);

        const auto next = dsa::build_next(pattern);
        bool same_next = next.size() == pattern.size();
        for (std::size_t i = 0; same_next && i < next.size(); ++i) {
            same_next = next[i] == author_next[i];
        }
        delete[] author_next;  // 作者 findNext 用 new int[m] 返回，从不释放
        expect(same_next, "算法4.7 特征向量不一致：" + label);

        expect(ours == ours_kmp, "本单元朴素与 KMP 不一致：" + label);
        if (ours) {
            ++found;
            const int expected = static_cast<int>(*ours) + 1;  // 勘误 E17：作者按 1 起始报位置
            expect(author_naive == expected, "算法4.6 作者朴素匹配应恰好比 0 起始位置大 1：" + label);
            expect(author_kmp == expected, "算法4.8 作者 KMP 应恰好比 0 起始位置大 1：" + label);
        } else {
            expect(author_naive == -1 && author_kmp == -1, "没找到时作者应返回 -1：" + label);
        }
    }
    if (failures) {
        std::cerr << "❌ 模式匹配对拍：" << failures << " 处不一致\n";
        return 1;
    }
    std::cout << "✅ 模式匹配对拍：" << cases.size() << " 组（其中 " << found
              << " 组找到），特征向量逐项相等；找到时作者两种匹配都恰好报 0 起始位置 + 1（勘误 E17）\n";
    return 0;
}

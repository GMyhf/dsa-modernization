#include "modern.hpp"

#include <iostream>

int main() {
    const char* text = "abcddabcababcdaabcababcdaabcabaa";
    const char* pattern = "abcdaabcab";
    const auto naive = dsa::naive_search(text, pattern);
    const auto kmp = dsa::kmp_search(text, pattern);
    std::cout << "图4.12 的串，正确起始下标是 10\n";
    std::cout << "朴素: " << (naive ? static_cast<long>(*naive) : -1) << '\n';
    std::cout << "KMP:  " << (kmp ? static_cast<long>(*kmp) : -1) << '\n';
    std::cout << "原书返回 11，一律差 1\n";
}

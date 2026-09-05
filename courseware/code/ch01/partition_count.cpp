// 上机题 1：把 m 写成若干个不超过 n 的自然数之和，有多少种写法？
//
//   c++ -std=c++17 -Wall -Wextra -Werror partition_count.cpp -o /tmp/partition
//   /tmp/partition
//
// 递推：按「这一堆里用不用一个 n」分两类
//     f(m, n) = f(m - n, n)   +   f(m, n - 1)
//               用掉一个 n        一个 n 都不用
// 边界：f(0, n) = 1（空拆法算一种）；m > 0 且 n == 0 时为 0；n > m 时按 f(m, m) 算。

#include <cassert>
#include <iostream>
#include <vector>

namespace dsa::ch01 {

// >>> partition
// 二维递推版：时间 O(mn)，空间 O(mn)。直接照抄递推式写递归会重复计算同一个
// (m, n) 无数次 —— 表格法把每个子问题只算一次，这就是第 12 章动态规划的雏形。
[[nodiscard]] long long partition_count(int m, int n) {
    if (m < 0 || n < 0) {
        return 0;
    }
    std::vector<std::vector<long long>> f(
        static_cast<std::size_t>(m) + 1,
        std::vector<long long>(static_cast<std::size_t>(n) + 1, 0));

    for (int part = 0; part <= n; ++part) {
        f[0][static_cast<std::size_t>(part)] = 1;   // 和为 0：只有「什么都不取」一种
    }
    for (int total = 1; total <= m; ++total) {
        for (int part = 1; part <= n; ++part) {
            const auto t = static_cast<std::size_t>(total);
            const auto p = static_cast<std::size_t>(part);
            f[t][p] = f[t][p - 1];                  // 一个 part 都不用
            if (total >= part) {
                f[t][p] += f[t - static_cast<std::size_t>(part)][p];   // 用掉一个 part
            }
        }
    }
    return f[static_cast<std::size_t>(m)][static_cast<std::size_t>(n)];
}
// <<< partition

}  // namespace dsa::ch01

int main() {
    using dsa::ch01::partition_count;

    // 题面给的例子：5 = 3+2 = 3+1+1 = 2+2+1 = 2+1+1+1 = 1+1+1+1+1
    assert(partition_count(5, 3) == 5);
    // n >= m 时就是 m 的全部分拆数 p(m)：1, 1, 2, 3, 5, 7, 11, 15, 22, 30, 42
    assert(partition_count(0, 5) == 1);
    assert(partition_count(1, 5) == 1);
    assert(partition_count(4, 4) == 5);
    assert(partition_count(10, 10) == 42);
    // 每份只能是 1：无论 m 多大都只有一种写法
    assert(partition_count(7, 1) == 1);
    // 一个数都不许用：m > 0 时无解
    assert(partition_count(7, 0) == 0);

    std::cout << "f(5, 3)   = " << partition_count(5, 3) << '\n';
    std::cout << "f(10, 10) = " << partition_count(10, 10) << '\n';
    std::cout << "f(50, 50) = " << partition_count(50, 50) << '\n';
}

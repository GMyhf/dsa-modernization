// 递归与栈空间 —— 原书【算法3.6】【算法3.7】【算法3.8】【算法3.9】的现代化实现。
//
// 本节（3.1.5 栈与递归）的教学内容是：**递归吃的是运行栈，而显式栈把同一份数据放到堆上**。
// 因此这里同时给出三种阶乘实现——递归、迭代、显式栈——让三者互为参照物。
// 显式栈那版直接用本章自己的 ArrayStack，正是原书 `Stack<long> s;` 的意思。
//
// 遵循 collab/DECISION_LOG.md 的 D-001：C++17；零 I/O；非法输入与溢出抛标准异常。
#pragma once

#include "ch03/array_stack/modern.hpp"

#include <cstdint>
#include <limits>
#include <stdexcept>

namespace dsa {

// >>> factorial-types
/// 阶乘增长极快，用无符号 64 位并**显式检查溢出**。
///
/// 原书三个版本都是 `long factorial(long n)`，既不检查负数也不检查溢出。
/// 实测（64 位 long）：`factorial(21)` 返回 **-4249290049419214848**，
/// `factorial(66)` 返回 **0**；而且这不只是"答案错"——UBSan 判定
/// `signed integer overflow ... cannot be represented in type 'long int'`，
/// **有符号溢出是未定义行为**。详见 legacy.md 缺陷 1。
using factorial_type = std::uint64_t;

/// 64 位无符号能容纳的最大阶乘是 20!；21! 起必然溢出。
inline constexpr factorial_type kMaxFactorialInput = 20;
// <<< factorial-types

// >>> factorial-recursive
/// 【算法3.6】递归实现。保留原书的递归形状——那正是本节要教的东西。
///
/// 加了原书没有的两道检查：负数是定义域错误，溢出是真错误（D-001 §3）。
/// 原书 `if (n <= 0) return 1;` 把负数静默当成 0 处理，返回 1。
[[nodiscard]] inline factorial_type factorial_recursive(long long n) {
    if (n < 0) {
        throw std::invalid_argument("factorial: 负数没有阶乘");
    }
    if (static_cast<factorial_type>(n) > kMaxFactorialInput) {
        throw std::overflow_error("factorial: 结果超出 64 位无符号范围（20! 是上限）");
    }
    if (n <= 1) {
        return 1;  // 递归出口
    }
    return static_cast<factorial_type>(n) * factorial_recursive(n - 1);
}
// <<< factorial-recursive

// >>> factorial-iterative
/// 【算法3.8】迭代实现。不用栈，也不占运行栈深度。
[[nodiscard]] inline factorial_type factorial_iterative(long long n) {
    if (n < 0) {
        throw std::invalid_argument("factorial: 负数没有阶乘");
    }
    if (static_cast<factorial_type>(n) > kMaxFactorialInput) {
        throw std::overflow_error("factorial: 结果超出 64 位无符号范围（20! 是上限）");
    }
    factorial_type m = 1;
    for (long long i = 2; i <= n; ++i) {
        m *= static_cast<factorial_type>(i);
    }
    return m;
}
// <<< factorial-iterative

// >>> factorial-explicit-stack
/// 【算法3.9】用显式栈模拟递归。
///
/// 这一版存在的意义不是"更快"——它比迭代版慢——而是**演示编译系统处理递归的机制**：
/// 遇到递归规则就压栈，遇到递归出口就出栈返回。原书的话是
/// 「模拟编译系统处理递归的机制，使用栈等数据结构保存回溯点」。
///
/// 关键差别在**数据放在哪**：递归版把每层的返回地址与局部变量放在**运行栈**上，
/// 大小由进程栈上限决定；这一版把待处理的数据压进 ArrayStack，**在堆上**，
/// 只受内存限制。实测数字见书稿 3.1.5 节。
///
/// 原书写的是 `while (s.pop(&tmp))`——传的是**指针**，而同书代码3.1 的栈 ADT
/// 声明的是 `bool pop(T& item)`（引用）。两处对不上（legacy.md 缺陷 3）。
[[nodiscard]] inline factorial_type factorial_with_explicit_stack(long long n) {
    if (n < 0) {
        throw std::invalid_argument("factorial: 负数没有阶乘");
    }
    if (static_cast<factorial_type>(n) > kMaxFactorialInput) {
        throw std::overflow_error("factorial: 结果超出 64 位无符号范围（20! 是上限）");
    }
    ArrayStack<factorial_type> pending;
    for (long long i = n; i > 1; --i) {  // 按递归规则压栈
        pending.push(static_cast<factorial_type>(i));
    }
    factorial_type m = 1;  // 递归出口的返回值
    while (auto top = pending.pop()) {   // 出栈即"递归返回"
        m *= *top;
    }
    return m;
}
// <<< factorial-explicit-stack

// >>> depth-demo
/// 本书补充（不对应原书清单）：把「递归吃运行栈、显式栈吃堆」这句话变成可测的东西。
///
/// 阶乘在 21 就溢出了，深度根本走不远，演示不了栈深度。所以这里用累加：
/// 同一个计算，一个递归、一个显式栈，各能走多深，书稿 3.1.5 节给了实测数字。
///
/// **注意这两个函数在不同优化档下行为不同**——`-O2` 可能把递归整个转成循环。
/// 这正是要说的：「递归会不会爆栈」不是源码单独决定的。
[[nodiscard]] inline std::uint64_t sum_to_recursive(std::uint64_t n) {
    if (n == 0) {
        return 0;
    }
    return n + sum_to_recursive(n - 1);
}

[[nodiscard]] inline std::uint64_t sum_to_with_explicit_stack(std::uint64_t n) {
    ArrayStack<std::uint64_t> pending;
    for (std::uint64_t i = n; i > 0; --i) {
        pending.push(i);
    }
    std::uint64_t total = 0;
    while (auto top = pending.pop()) {
        total += *top;
    }
    return total;
}
// <<< depth-demo

}  // namespace dsa

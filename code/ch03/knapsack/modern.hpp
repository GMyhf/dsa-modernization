// 背包问题 —— 原书【算法3.10】【算法3.11】【算法3.12】的现代化实现。
//
// 这一组是 3.1.5 节的压轴：把一个**有两条递归规则**的算法，机械地改写成
// 显式栈驱动的循环。原书用 goto + 三个标号模拟"返回地址"，这套机制本身
// 就是教学内容，本书保留它的骨架——只是把 goto 换成显式的状态机，
// 因为 goto 跳进跳出会让编译器无法保证对象生命周期。
//
// 问题本身是**子集和判定**：能否从若干物品中选出一部分，使重量之和恰好等于背包承重。
#pragma once

#include "ch03/array_stack/modern.hpp"

#include <cstddef>
#include <optional>
#include <stdexcept>
#include <vector>

namespace dsa {

/// 选中物品的下标集合（按原书的输出顺序：从最后一件倒推回来）。
using knapsack_solution = std::vector<std::size_t>;

namespace detail {
inline void validate(int capacity, const std::vector<int>& weights) {
    if (capacity < 0) {
        throw std::invalid_argument("背包：承重量不能为负");
    }
    for (int w : weights) {
        // 原书没有这道检查。物品重量为 0 会让"选它"与"不选它"无从区分，
        // 为负则整个 s<0 的递归出口失去意义。
        if (w <= 0) {
            throw std::invalid_argument("背包：物品重量必须为正");
        }
    }
}
}  // namespace detail

// >>> recursive
/// 【算法3.10】递归解法。两条递归规则、两个递归出口，原书的结构一字未改：
///   出口 1：承重恰为 0 → 有解（什么都不再选）
///   出口 2：承重为负，或承重为正但已无物品可选 → 无解
///   规则 1：选第 n-1 件 → 求解 knap(s - w[n-1], n-1)
///   规则 2：不选第 n-1 件 → 求解 knap(s, n-1)
///
/// 与原书的差别只有两处：
/// 1. **原书直接 `cout << w[n-1]`** 把选中的物品打印出来——算法与终端焊死，
///    调用方拿不到结果，也没法测试。这里把下标收集进 `chosen` 返回。
/// 2. **原书的 `w[]` 是一个从未声明的全局数组**（legacy.md 缺陷 3）。这里作参数传入。
[[nodiscard]] inline std::optional<knapsack_solution> knapsack_recursive(
    int capacity, const std::vector<int>& weights) {
    detail::validate(capacity, weights);
    knapsack_solution chosen;

    // 返回 true 表示 weights[0..n) 中存在一个子集，其和恰为 s
    const auto solve = [&weights, &chosen](auto&& self, int s, std::size_t n) -> bool {
        if (s == 0) {
            return true;  // 递归出口 1
        }
        if (s < 0 || n == 0) {
            return false;  // 递归出口 2
        }
        if (self(self, s - weights[n - 1], n - 1)) {  // 规则 1：选它
            chosen.push_back(n - 1);
            return true;
        }
        return self(self, s, n - 1);  // 规则 2：不选它
    };

    return solve(solve, capacity, weights.size())
               ? std::optional<knapsack_solution>(std::move(chosen))
               : std::nullopt;
}
// <<< recursive

// >>> explicit-stack
/// 【算法3.11】把上面的递归机械地改写成显式栈驱动。
///
/// 原书用 `goto label0/1/2/3` 表示"执行到哪一步"，本书把同一件事写成栈帧里的
/// 一个 `stage` 字段——**语义完全对应**，只是不用 goto：goto 跳进跳出会让编译器
/// 无法保证局部对象的构造与析构配对，在有 RAII 的 C++ 里不能这么写。
///
///   `Enter`      ↔ label0，递归调用入口：判出口，否则按规则 1 展开
///   `AfterRule1` ↔ label1，规则 1（选第 n-1 件）返回后的处理
///   `AfterRule2` ↔ label2，规则 2（不选第 n-1 件）返回后的处理
///
/// 每一帧存原书说的四个域：参数 s 与 n、返回地址（这里是 stage）、结果单元 k。
[[nodiscard]] inline std::optional<knapsack_solution> knapsack_with_explicit_stack(
    int capacity, const std::vector<int>& weights) {
    detail::validate(capacity, weights);

    enum class Stage { Enter, AfterRule1, AfterRule2 };
    struct Frame {
        int s = 0;
        std::size_t n = 0;
        Stage stage = Stage::Enter;
    };

    ArrayStack<Frame> stack;
    stack.push(Frame{capacity, weights.size(), Stage::Enter});
    knapsack_solution chosen;
    bool child_result = false;   // 下层刚刚返回的结果单元 k

    while (!stack.empty()) {
        Frame frame = *stack.pop();

        if (frame.stage == Stage::Enter) {
            if (frame.s == 0) {            // 递归出口 1
                child_result = true;
                continue;                  // 相当于 goto label3：直接向上返回
            }
            if (frame.s < 0 || frame.n == 0) {   // 递归出口 2
                child_result = false;
                continue;
            }
            frame.stage = Stage::AfterRule1;     // 记下"回来时该走哪一步"
            stack.push(frame);
            stack.push(Frame{frame.s - weights[frame.n - 1], frame.n - 1, Stage::Enter});
            continue;
        }

        if (frame.stage == Stage::AfterRule1) {
            if (child_result) {            // 规则 1 成功：第 n-1 件被选中
                chosen.push_back(frame.n - 1);
                continue;                  // k 已是 true，继续上传
            }
            frame.stage = Stage::AfterRule2;     // 回溯，改用规则 2
            stack.push(frame);
            stack.push(Frame{frame.s, frame.n - 1, Stage::Enter});
            continue;
        }

        // Stage::AfterRule2：规则 2 的结果就是本层的结果，原样上传
    }

    return child_result ? std::optional<knapsack_solution>(std::move(chosen)) : std::nullopt;
}
// <<< explicit-stack

// >>> optimized
/// 【算法3.12】原书"优化版"的两点观察，本书照单实现：
///
/// 1. **结果单元 k 可以提到栈外**——一旦某层为 true 就逐层上传且不再变化，
///    因此一个函数级变量即可，栈帧里的 `k` 域连同它的反复赋值、进出栈都省掉。
///    （上面那版其实已经这么做了：`child_result` 就在栈外。）
/// 2. **参数 n 可以由栈深推出**——每递归一层 n 减 1、栈深加 1，
///    所以 `n = n0 - 栈深`，栈帧里的 `n` 域也能省掉。
///
/// 于是栈帧从四个域缩到两个（s 与 stage）。这是本节真正的"优化"：
/// 不是让它更快，而是**让每层要记的东西更少**——这正是手工模拟递归的意义。
///
/// 原书这一版另有一处致命问题：它同时把 `stack.top` 当**数据成员**用
/// （`t = stack.top;`）又当**成员函数**用（`stack.top(&tmp);`）。
/// 这在任何一种解释下都编译不过，而且它恰恰依赖代码3.2/3.4 那个 `top` 重名缺陷。
[[nodiscard]] inline std::optional<knapsack_solution> knapsack_optimized(
    int capacity, const std::vector<int>& weights) {
    detail::validate(capacity, weights);

    enum class Stage { Enter, AfterRule1, AfterRule2 };
    struct Frame {
        int s = 0;              // 只剩两个域
        Stage stage = Stage::Enter;
    };

    const std::size_t n0 = weights.size();
    ArrayStack<Frame> stack;
    knapsack_solution chosen;
    bool child_result = false;

    stack.push(Frame{capacity, Stage::Enter});
    std::size_t depth = 1;      // 栈中帧数；当前帧的 n = n0 - (depth - 1)

    while (!stack.empty()) {
        Frame frame = *stack.pop();
        --depth;
        const std::size_t n = n0 - depth;   // 观察 2：n 由栈深推出，不再入栈

        if (frame.stage == Stage::Enter) {
            if (frame.s == 0) { child_result = true; continue; }
            if (frame.s < 0 || n == 0) { child_result = false; continue; }
            frame.stage = Stage::AfterRule1;
            stack.push(frame);
            stack.push(Frame{frame.s - weights[n - 1], Stage::Enter});
            depth += 2;
            continue;
        }

        if (frame.stage == Stage::AfterRule1) {
            if (child_result) { chosen.push_back(n - 1); continue; }
            frame.stage = Stage::AfterRule2;
            stack.push(frame);
            stack.push(Frame{frame.s, Stage::Enter});
            depth += 2;
            continue;
        }
        // AfterRule2：结果原样上传
    }

    return child_result ? std::optional<knapsack_solution>(std::move(chosen)) : std::nullopt;
}
// <<< optimized

}  // namespace dsa

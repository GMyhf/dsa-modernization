#pragma once

#include <cstddef>
#include <stdexcept>
#include <vector>

namespace dsa::monotonic_stack {

// >>> next-greater
/// 对每个位置返回右侧第一个严格更大值的位置；不存在时返回 n。
inline std::vector<std::size_t> next_greater_indices(const std::vector<int>& values) {
    const std::size_t n = values.size();
    std::vector<std::size_t> answer(n, n);
    std::vector<std::size_t> stack;
    for (std::size_t i = 0; i < n; ++i) {
        while (!stack.empty() && values[stack.back()] < values[i]) {
            answer[stack.back()] = i;
            stack.pop_back();
        }
        stack.push_back(i);
    }
    return answer;
}
// <<< next-greater

// >>> next-smaller
/// 对每个位置返回右侧第一个严格更小值的位置；不存在时返回 n。
inline std::vector<std::size_t> next_smaller_indices(const std::vector<int>& values) {
    const std::size_t n = values.size();
    std::vector<std::size_t> answer(n, n);
    std::vector<std::size_t> stack;
    for (std::size_t i = 0; i < n; ++i) {
        while (!stack.empty() && values[stack.back()] > values[i]) {
            answer[stack.back()] = i;
            stack.pop_back();
        }
        stack.push_back(i);
    }
    return answer;
}
// <<< next-smaller

// >>> histogram
/// 直方图最大矩形面积；在末尾放一个 0，统一清空仍有候选边界的栈。
inline long long largest_rectangle_area(const std::vector<int>& heights) {
    for (int height : heights) {
        if (height < 0) throw std::invalid_argument("histogram height must be non-negative");
    }
    std::vector<std::size_t> stack;
    long long best = 0;
    for (std::size_t i = 0; i <= heights.size(); ++i) {
        const int current = i == heights.size() ? 0 : heights[i];
        while (!stack.empty() && heights[stack.back()] > current) {
            const std::size_t top = stack.back();
            stack.pop_back();
            const std::size_t left = stack.empty() ? 0 : stack.back() + 1;
            const long long width = static_cast<long long>(i - left);
            const long long area = static_cast<long long>(heights[top]) * width;
            if (area > best) best = area;
        }
        stack.push_back(i);
    }
    return best;
}
// <<< histogram

}  // namespace dsa::monotonic_stack
